#include "scene_radar.h"

#include <algorithm>
#include <cstdlib>
#include <map>
#include <optional>
#include <utility>

#include "accessibility.h"
#include "avatar.h"
#include "enums.h"
#include "field.h"
#include "lightmap.h"
#include "line.h"
#include "map.h"
#include "mapdata.h"
#include "options.h"
#include "output.h"
#include "point.h"
#include "scene_description.h"
#include "string_formatter.h"
#include "translations.h"
#include "trap.h"

namespace
{

// In dim light, a view this short counts as reduced sight.
constexpr int reduced_sight_tiles = 8;
// A jump farther than this between two snapshots is a teleport, not a step.
constexpr int max_step_distance = 3;
// Area changes are ignored when both views are this small.
constexpr int min_area_tiles = 6;

struct direction_step {
    int dx;
    int dy;
};

// Index matches the 0-7 direction convention: clockwise from north.
constexpr std::array<direction_step, 8> direction_steps = {{
        { 0, -1 }, { 1, -1 }, { 1, 0 }, { 1, 1 },
        { 0, 1 }, { -1, 1 }, { -1, 0 }, { -1, -1 }
    }
};

radar_kind kind_of( const map &here, const tripoint_bub_ms &p )
{
    if( here.has_flag( ter_furn_flag::TFLAG_GOES_UP, p ) ||
        here.has_flag( ter_furn_flag::TFLAG_GOES_DOWN, p ) ||
        here.has_flag( ter_furn_flag::TFLAG_LADDER, p ) ||
        here.has_flag( ter_furn_flag::TFLAG_RAMP_UP, p ) ||
        here.has_flag( ter_furn_flag::TFLAG_RAMP_DOWN, p ) ) {
        return radar_kind::stairs;
    }
    if( here.has_flag( ter_furn_flag::TFLAG_WINDOW, p ) ||
        here.has_flag( ter_furn_flag::TFLAG_BARRICADABLE_WINDOW_CURTAINS, p ) ) {
        return radar_kind::window;
    }
    return radar_kind::door;
}

std::string short_name_of( const map &here, const tripoint_bub_ms &p, radar_kind kind )
{
    switch( kind ) {
        case radar_kind::window:
            return _( "window" );
        case radar_kind::door:
            return _( "door" );
        case radar_kind::stairs:
            break;
    }
    const bool up = here.has_flag( ter_furn_flag::TFLAG_GOES_UP, p ) ||
                    here.has_flag( ter_furn_flag::TFLAG_RAMP_UP, p );
    const bool down = here.has_flag( ter_furn_flag::TFLAG_GOES_DOWN, p ) ||
                      here.has_flag( ter_furn_flag::TFLAG_RAMP_DOWN, p );
    if( here.has_flag( ter_furn_flag::TFLAG_RAMP_UP, p ) ) {
        return _( "ramp up" );
    }
    if( here.has_flag( ter_furn_flag::TFLAG_RAMP_DOWN, p ) ) {
        return _( "ramp down" );
    }
    if( here.has_flag( ter_furn_flag::TFLAG_LADDER, p ) ) {
        return up ? _( "ladder up" ) : down ? _( "ladder down" ) : _( "ladder" );
    }
    return up ? _( "stairs up" ) : down ? _( "stairs down" ) : _( "stairs" );
}

radar_probe probe_direction( map &here, const tripoint_bub_ms &origin,
                             const direction_step &step, int range )
{
    radar_probe result;
    for( int distance = 1; distance <= range; ++distance ) {
        const tripoint_bub_ms p = origin + tripoint( step.dx * distance, step.dy * distance, 0 );
        if( !here.inbounds( p ) || !here.pl_sees( p, -1 ) ) {
            result.distance = distance - 1;
            return result;
        }
        if( here.impassable( p ) ) {
            result.blocked = true;
            result.distance = distance;
            result.tile = here.get_abs( p );
            result.name = here.obstacle_name( p );
            return result;
        }
    }
    result.distance = range;
    return result;
}

void note_hazard( std::map<std::string, std::pair<int, tripoint_abs_ms>> &hazards,
                  const std::string &name, int distance, const tripoint_abs_ms &pos )
{
    const auto found = hazards.find( name );
    if( found == hazards.end() || distance < found->second.first ) {
        hazards[name] = std::make_pair( distance, pos );
    }
}

// "3 east" from the avatar's tile to `pos`.
std::string position_of( const radar_snapshot &now, const tripoint_abs_ms &pos )
{
    const int dx = pos.x() - now.origin.x();
    const int dy = pos.y() - now.origin.y();
    return scene_position_phrase( rl_dist( now.origin, pos ), scene_direction_index( dx, dy ) );
}

int distance_of( const radar_snapshot &now, const tripoint_abs_ms &pos )
{
    return rl_dist( now.origin, pos );
}

bool kind_enabled( radar_kind kind, const radar_settings &settings )
{
    switch( kind ) {
        case radar_kind::door:
            return settings.doors;
        case radar_kind::window:
            return settings.windows;
        case radar_kind::stairs:
            return settings.stairs;
    }
    return false;
}

const radar_feature *find_feature( const radar_snapshot &scene, const tripoint_abs_ms &pos )
{
    for( const radar_feature &feature : scene.features ) {
        if( feature.pos == pos ) {
            return &feature;
        }
    }
    return nullptr;
}

bool has_hazard( const radar_snapshot &scene, const std::string &name )
{
    return std::any_of( scene.hazards.begin(), scene.hazards.end(),
    [&]( const radar_hazard & hazard ) {
        return hazard.name == name;
    } );
}

struct placed_notice {
    int distance;
    std::string text;
};

void append_nearest_first( std::vector<std::string> &out, std::vector<placed_notice> &notices )
{
    std::stable_sort( notices.begin(), notices.end(),
    []( const placed_notice & a, const placed_notice & b ) {
        return a.distance < b.distance;
    } );
    for( const placed_notice &notice : notices ) {
        out.push_back( notice.text );
    }
}

std::optional<radar_snapshot> &last_snapshot()
{
    static std::optional<radar_snapshot> last;
    return last;
}

} // namespace

radar_snapshot gather_radar( const avatar &ava, int range )
{
    map &here = get_map();
    const tripoint_bub_ms origin = ava.pos_bub();
    radar_snapshot scene;
    scene.origin = here.get_abs( origin );
    scene.outdoors = here.is_outside( origin );

    for( size_t i = 0; i < direction_steps.size(); ++i ) {
        scene.probes[i] = probe_direction( here, origin, direction_steps[i], range );
    }

    std::map<std::string, std::pair<int, tripoint_abs_ms>> hazards;
    // Look far enough to tell a short view from a short radar range.
    const int scan = std::max( range, reduced_sight_tiles + 1 );
    int sight_range = 0;

    for( int dy = -scan; dy <= scan; ++dy ) {
        for( int dx = -scan; dx <= scan; ++dx ) {
            if( dx == 0 && dy == 0 ) {
                continue;
            }
            const tripoint_bub_ms p = origin + tripoint( dx, dy, 0 );
            if( !here.inbounds( p ) || !here.pl_sees( p, -1 ) ) {
                continue;
            }
            const int distance = rl_dist( origin, p );
            sight_range = std::max( sight_range, distance );
            if( distance > range ) {
                continue;
            }
            const tripoint_abs_ms abs_pos = here.get_abs( p );

            if( !here.impassable( p ) ) {
                ++scene.open_tiles;
            }

            if( scene_is_exit_tile( here, p ) ) {
                radar_feature feature;
                feature.pos = abs_pos;
                feature.kind = kind_of( here, p );
                feature.name = scene_exit_name( here, p );
                feature.short_name = short_name_of( here, p, feature.kind );
                scene.features.push_back( feature );
            }

            // The same rules as the scene description: the field a sighted
            // player sees drawn, and only traps the avatar has spotted.
            const field &fields = here.field_at( p );
            if( fields.field_count() > 0 ) {
                const field_entry *shown = fields.find_field( fields.displayed_field_type() );
                if( shown != nullptr && shown->is_dangerous() ) {
                    note_hazard( hazards, shown->name(), distance, abs_pos );
                }
            }
            const trap &tr = here.tr_at( p );
            if( !tr.is_null() && !tr.is_benign() && tr.can_see( p, ava ) ) {
                note_hazard( hazards, tr.name(), distance, abs_pos );
            }
        }
    }

    for( const std::pair<const std::string, std::pair<int, tripoint_abs_ms>> &entry : hazards ) {
        radar_hazard hazard;
        hazard.name = entry.first;
        hazard.pos = entry.second.second;
        scene.hazards.push_back( hazard );
    }

    const lit_level light = here.light_at( origin );
    scene.sight_reduced = ( light == lit_level::DARK || light == lit_level::LOW ) &&
                          sight_range <= reduced_sight_tiles;
    return scene;
}

std::vector<std::string> diff_radar( const radar_snapshot &before, const radar_snapshot &now,
                                     const radar_settings &settings )
{
    std::vector<std::string> out;

    // A new level or a teleport is a new scene, not a change to this one.
    if( before.origin.z() != now.origin.z() ||
        std::abs( before.origin.x() - now.origin.x() ) > max_step_distance ||
        std::abs( before.origin.y() - now.origin.y() ) > max_step_distance ) {
        return out;
    }
    if( now.outdoors && settings.outdoors == radar_outdoors::off ) {
        return out;
    }
    // When the light goes, everything leaves view at once. Say that instead.
    if( before.sight_reduced != now.sight_reduced ) {
        out.emplace_back( now.sight_reduced ? _( "Sight reduced." ) : _( "Sight restored." ) );
        return out;
    }

    const bool structure = !( now.outdoors && settings.outdoors == radar_outdoors::exits_only );

    if( settings.hazards ) {
        std::vector<placed_notice> notices;
        for( const radar_hazard &hazard : now.hazards ) {
            if( !has_hazard( before, hazard.name ) ) {
                notices.push_back( { distance_of( now, hazard.pos ),
                                     string_format( _( "%1$s, %2$s." ), uppercase_first_letter( hazard.name ),
                                                    position_of( now, hazard.pos ) )
                                   } );
            }
        }
        append_nearest_first( out, notices );
    }

    if( structure && settings.area &&
        std::max( before.open_tiles, now.open_tiles ) >= min_area_tiles ) {
        if( now.open_tiles * 2 >= before.open_tiles * 3 ) {
            out.emplace_back( _( "Area opens out." ) );
        } else if( now.open_tiles * 2 <= before.open_tiles ) {
            out.emplace_back( _( "Area narrows." ) );
        }
    }

    if( settings.announce_entering ) {
        std::vector<placed_notice> notices;
        for( const radar_feature &feature : now.features ) {
            if( !kind_enabled( feature.kind, settings ) ) {
                continue;
            }
            const radar_feature *old = find_feature( before, feature.pos );
            if( old == nullptr ) {
                const std::string &name = settings.full_names ? feature.name : feature.short_name;
                notices.push_back( { distance_of( now, feature.pos ),
                                     string_format( _( "%1$s, %2$s." ), uppercase_first_letter( name ),
                                                    position_of( now, feature.pos ) )
                                   } );
            } else if( old->name != feature.name ) {
                // A door opened or closed, or a window was smashed.
                notices.push_back( { distance_of( now, feature.pos ),
                                     string_format( _( "Now %1$s, %2$s." ), feature.name,
                                                    position_of( now, feature.pos ) )
                                   } );
            }
        }
        append_nearest_first( out, notices );
    }

    if( structure && settings.walls ) {
        const int step = settings.eight_directions ? 1 : 2;
        for( int direction = 0; direction < 8; direction += step ) {
            const radar_probe &old = before.probes[direction];
            const radar_probe &cur = now.probes[direction];
            // Walking towards a wall changes the distance but not the wall.
            if( old.blocked && cur.blocked && old.tile == cur.tile ) {
                continue;
            }
            const int change = cur.distance - old.distance;
            if( std::abs( change ) < settings.wall_threshold ) {
                continue;
            }
            const std::string where = scene_direction_word( direction );
            if( change > 0 ) {
                out.push_back( string_format( _( "Opens %1$s, %2$d." ), where, cur.distance ) );
            } else if( cur.blocked ) {
                out.push_back( string_format( _( "Closes in %1$s, %2$s %3$d." ), where, cur.name,
                                              cur.distance ) );
            } else {
                out.push_back( string_format( _( "Closes in %1$s, %2$d." ), where, cur.distance ) );
            }
        }
    }

    if( settings.announce_leaving ) {
        std::vector<placed_notice> notices;
        for( const radar_feature &feature : before.features ) {
            if( !kind_enabled( feature.kind, settings ) ||
                find_feature( now, feature.pos ) != nullptr ) {
                continue;
            }
            const std::string &name = settings.full_names ? feature.name : feature.short_name;
            notices.push_back( { distance_of( now, feature.pos ),
                                 string_format( _( "Lost %1$s, %2$s." ), name,
                                                position_of( now, feature.pos ) )
                               } );
        }
        append_nearest_first( out, notices );
    }

    return out;
}

std::string render_radar( const std::vector<std::string> &notices, int max_items )
{
    const size_t shown = std::min( notices.size(), static_cast<size_t>( std::max( max_items, 1 ) ) );
    std::string out;
    for( size_t i = 0; i < shown; ++i ) {
        if( !out.empty() ) {
            out += " ";
        }
        out += notices[i];
    }
    if( notices.size() > shown ) {
        out += " " + string_format( _( "%d more." ), static_cast<int>( notices.size() - shown ) );
    }
    return out;
}

radar_settings radar_settings_from_options()
{
    radar_settings settings;
    settings.doors = get_option<bool>( "A11Y_RADAR_DOORS" );
    settings.windows = get_option<bool>( "A11Y_RADAR_WINDOWS" );
    settings.stairs = get_option<bool>( "A11Y_RADAR_STAIRS" );
    settings.walls = get_option<bool>( "A11Y_RADAR_WALLS" );
    settings.area = get_option<bool>( "A11Y_RADAR_AREA" );
    settings.hazards = get_option<bool>( "A11Y_RADAR_HAZARDS" );
    const std::string announce = get_option<std::string>( "A11Y_RADAR_ANNOUNCE" );
    settings.announce_entering = announce != "leave";
    settings.announce_leaving = announce != "enter";
    settings.range = get_option<int>( "A11Y_RADAR_RANGE" );
    settings.wall_threshold = get_option<int>( "A11Y_RADAR_WALL_THRESHOLD" );
    settings.eight_directions = get_option<std::string>( "A11Y_RADAR_DIRECTIONS" ) == "eight";
    settings.full_names = get_option<std::string>( "A11Y_RADAR_WORDING" ) == "full";
    settings.max_items = get_option<int>( "A11Y_RADAR_MAX_ITEMS" );
    const std::string outdoors = get_option<std::string>( "A11Y_RADAR_OUTDOORS" );
    settings.outdoors = outdoors == "all" ? radar_outdoors::all :
                        outdoors == "off" ? radar_outdoors::off : radar_outdoors::exits_only;
    return settings;
}

void radar_update( const avatar &ava, bool travelling )
{
    // The range the last snapshot was taken at; a change makes it incomparable.
    static int last_range = 0;
    std::optional<radar_snapshot> &last = last_snapshot();

    if( !get_option<bool>( "A11Y_RADAR" ) ) {
        last.reset();
        return;
    }
    const radar_settings settings = radar_settings_from_options();
    radar_snapshot now = gather_radar( ava, settings.range );

    const bool silent = travelling && !get_option<bool>( "A11Y_RADAR_TRAVEL" );
    if( last && last_range == settings.range && !silent ) {
        const std::vector<std::string> notices = diff_radar( *last, now, settings );
        if( !notices.empty() ) {
            get_narration_service().announce( {
                narration_event::movement, render_radar( notices, settings.max_items ), false
            } );
        }
    }
    last = std::move( now );
    last_range = settings.range;
}

void radar_reset()
{
    last_snapshot().reset();
}
