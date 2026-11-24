#include <algorithm>
#include <string>
#include <vector>

#include "avatar.h"
#include "calendar.h"
#include "cata_catch.h"
#include "coordinates.h"
#include "map.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "map_scale_constants.h"
#include "player_helpers.h"
#include "point.h"
#include "scene_radar.h"
#include "type_id.h"

static const ter_str_id ter_t_door_c( "t_door_c" );
static const ter_str_id ter_t_wall( "t_wall" );
static const ter_str_id ter_t_window( "t_window" );

static const tripoint_abs_ms radar_origin( 100, 100, 0 );

static bool has_notice( const std::vector<std::string> &notices, const std::string &text )
{
    return std::find( notices.begin(), notices.end(), text ) != notices.end();
}

static tripoint_abs_ms offset( const tripoint_abs_ms &from, int dx, int dy )
{
    return from + tripoint( dx, dy, 0 );
}

static void block( radar_snapshot &scene, int direction, const tripoint_abs_ms &tile, int distance )
{
    scene.probes[direction].blocked = true;
    scene.probes[direction].distance = distance;
    scene.probes[direction].tile = tile;
    scene.probes[direction].name = "wall";
}

// A corridor running north to south: walls one tile east and west, open for
// twelve tiles north and south.
static radar_snapshot make_corridor( const tripoint_abs_ms &origin )
{
    radar_snapshot scene;
    scene.origin = origin;
    scene.open_tiles = 24;
    for( radar_probe &probe : scene.probes ) {
        probe.distance = 12;
    }
    block( scene, 2, offset( origin, 1, 0 ), 1 );
    block( scene, 6, offset( origin, -1, 0 ), 1 );
    return scene;
}

static radar_feature make_feature( const tripoint_abs_ms &pos, radar_kind kind,
                                   const std::string &name, const std::string &short_name )
{
    radar_feature feature;
    feature.pos = pos;
    feature.kind = kind;
    feature.name = name;
    feature.short_name = short_name;
    return feature;
}

// Rebuild light and vision caches after editing the map, as vision_test does.
static void refresh_radar_vision( int zlev )
{
    map &here = get_map();
    here.invalidate_visibility_cache();
    here.update_visibility_cache( zlev );
    for( int z = -2; z <= OVERMAP_HEIGHT; z++ ) {
        here.invalidate_map_cache( z );
    }
    here.build_map_cache( zlev );
    here.invalidate_visibility_cache();
    here.update_visibility_cache( zlev );
    here.invalidate_map_cache( zlev );
    here.build_map_cache( zlev );
}

TEST_CASE( "radar announces exits when they come into view", "[scene_radar][accessibility]" )
{
    const radar_settings settings;
    const radar_snapshot before = make_corridor( radar_origin );
    const tripoint_abs_ms door_pos = offset( radar_origin, 0, -4 );

    // One step north; a door is now in view three tiles ahead.
    radar_snapshot now = make_corridor( offset( radar_origin, 0, -1 ) );
    now.features.push_back( make_feature( door_pos, radar_kind::door, "closed wood door", "door" ) );

    SECTION( "a new door is spoken with its position" ) {
        const std::vector<std::string> notices = diff_radar( before, now, settings );
        REQUIRE( notices.size() == 1 );
        CHECK( notices[0] == "Door, 3 north." );
    }

    SECTION( "full wording uses the game's name" ) {
        radar_settings full = settings;
        full.full_names = true;
        CHECK( has_notice( diff_radar( before, now, full ), "Closed wood door, 3 north." ) );
    }

    SECTION( "a door already in view is not repeated after another step" ) {
        radar_snapshot later = make_corridor( offset( radar_origin, 0, -2 ) );
        later.features = now.features;
        CHECK( diff_radar( now, later, settings ).empty() );
    }

    SECTION( "a door that opens is reported as changed" ) {
        radar_snapshot later = now;
        later.features[0].name = "open wood door";
        CHECK( has_notice( diff_radar( now, later, settings ), "Now open wood door, 3 north." ) );
    }

    SECTION( "switching doors off silences them" ) {
        radar_settings no_doors = settings;
        no_doors.doors = false;
        CHECK( diff_radar( before, now, no_doors ).empty() );
    }

    SECTION( "windows and stairs follow their own switches" ) {
        now.features.clear();
        now.features.push_back( make_feature( offset( radar_origin, 0, -3 ), radar_kind::window,
                                              "window", "window" ) );
        now.features.push_back( make_feature( offset( radar_origin, 0, -6 ), radar_kind::stairs,
                                              "stairs leading down", "stairs down" ) );
        const std::vector<std::string> both = diff_radar( before, now, settings );
        REQUIRE( both.size() == 2 );
        // Nearest first.
        CHECK( both[0] == "Window, 2 north." );
        CHECK( both[1] == "Stairs down, 5 north." );

        radar_settings no_windows = settings;
        no_windows.windows = false;
        const std::vector<std::string> stairs_only = diff_radar( before, now, no_windows );
        REQUIRE( stairs_only.size() == 1 );
        CHECK( stairs_only[0] == "Stairs down, 5 north." );
    }
}

TEST_CASE( "radar reports exits leaving view only when asked", "[scene_radar][accessibility]" )
{
    radar_snapshot before = make_corridor( radar_origin );
    before.features.push_back( make_feature( offset( radar_origin, 0, 5 ), radar_kind::door,
                               "closed wood door", "door" ) );
    const radar_snapshot now = make_corridor( offset( radar_origin, 0, -1 ) );

    radar_settings settings;
    CHECK( diff_radar( before, now, settings ).empty() );

    settings.announce_leaving = true;
    CHECK( has_notice( diff_radar( before, now, settings ), "Lost door, 6 south." ) );

    // Leaving only: a door coming into view is not spoken.
    settings.announce_entering = false;
    CHECK( diff_radar( now, before, settings ).empty() );
}

TEST_CASE( "radar speaks wall changes only when the space really changes",
           "[scene_radar][accessibility]" )
{
    const radar_settings settings;

    SECTION( "walking along a wall is silent" ) {
        const radar_snapshot before = make_corridor( radar_origin );
        const radar_snapshot now = make_corridor( offset( radar_origin, 0, -1 ) );
        CHECK( diff_radar( before, now, settings ).empty() );
    }

    SECTION( "walking towards a wall is silent" ) {
        const tripoint_abs_ms end_wall = offset( radar_origin, 0, -6 );
        radar_snapshot before = make_corridor( radar_origin );
        block( before, 0, end_wall, 6 );
        // Three tiles closer to the same wall tile.
        radar_snapshot now = make_corridor( offset( radar_origin, 0, -3 ) );
        block( now, 0, end_wall, 3 );
        CHECK( diff_radar( before, now, settings ).empty() );
    }

    SECTION( "a side opening is announced" ) {
        const radar_snapshot before = make_corridor( radar_origin );
        radar_snapshot now = make_corridor( offset( radar_origin, 0, -1 ) );
        now.probes[2] = radar_probe();
        now.probes[2].distance = 8;
        CHECK( has_notice( diff_radar( before, now, settings ), "Opens east, 8." ) );
    }

    SECTION( "a wall closing in is announced with its name" ) {
        radar_snapshot before = make_corridor( radar_origin );
        before.probes[2] = radar_probe();
        before.probes[2].distance = 8;
        const radar_snapshot now = make_corridor( offset( radar_origin, 0, -1 ) );
        CHECK( has_notice( diff_radar( before, now, settings ), "Closes in east, wall 1." ) );
    }

    SECTION( "changes below the threshold are silent" ) {
        const radar_snapshot before = make_corridor( radar_origin );
        radar_snapshot now = make_corridor( offset( radar_origin, 0, -1 ) );
        block( now, 2, offset( now.origin, 3, 0 ), 3 );
        CHECK( diff_radar( before, now, settings ).empty() );

        radar_settings sensitive = settings;
        sensitive.wall_threshold = 2;
        CHECK( has_notice( diff_radar( before, now, sensitive ), "Opens east, 3." ) );
    }

    SECTION( "diagonals are tracked only with eight directions" ) {
        const radar_snapshot before = make_corridor( radar_origin );
        radar_snapshot now = make_corridor( offset( radar_origin, 0, -1 ) );
        block( now, 1, offset( now.origin, 2, -2 ), 2 );
        CHECK( diff_radar( before, now, settings ).empty() );

        radar_settings eight = settings;
        eight.eight_directions = true;
        CHECK( has_notice( diff_radar( before, now, eight ), "Closes in northeast, wall 2." ) );
    }

    SECTION( "switching walls off silences them" ) {
        const radar_snapshot before = make_corridor( radar_origin );
        radar_snapshot now = make_corridor( offset( radar_origin, 0, -1 ) );
        now.probes[2] = radar_probe();
        now.probes[2].distance = 8;
        radar_settings no_walls = settings;
        no_walls.walls = false;
        CHECK( diff_radar( before, now, no_walls ).empty() );
    }
}

TEST_CASE( "radar says when the area opens out or narrows", "[scene_radar][accessibility]" )
{
    radar_settings settings;
    const radar_snapshot corridor = make_corridor( radar_origin );
    radar_snapshot room = make_corridor( offset( radar_origin, 0, -1 ) );
    room.open_tiles = 60;

    CHECK( has_notice( diff_radar( corridor, room, settings ), "Area opens out." ) );
    CHECK( has_notice( diff_radar( room, corridor, settings ), "Area narrows." ) );

    // A modest change says nothing.
    radar_snapshot similar = room;
    similar.open_tiles = 45;
    CHECK( diff_radar( room, similar, settings ).empty() );

    settings.area = false;
    CHECK( diff_radar( corridor, room, settings ).empty() );
}

TEST_CASE( "radar handles darkness, level changes and the outdoors",
           "[scene_radar][accessibility]" )
{
    radar_settings settings;
    const radar_snapshot before = make_corridor( radar_origin );
    radar_snapshot now = make_corridor( offset( radar_origin, 0, -1 ) );
    now.open_tiles = 60;
    now.features.push_back( make_feature( offset( radar_origin, 0, -4 ), radar_kind::door,
                                          "closed wood door", "door" ) );
    now.probes[2] = radar_probe();
    now.probes[2].distance = 8;

    SECTION( "losing the light replaces everything else" ) {
        now.sight_reduced = true;
        const std::vector<std::string> notices = diff_radar( before, now, settings );
        REQUIRE( notices.size() == 1 );
        CHECK( notices[0] == "Sight reduced." );
        CHECK( has_notice( diff_radar( now, before, settings ), "Sight restored." ) );
    }

    SECTION( "a new level is not compared with the old one" ) {
        now.origin = tripoint_abs_ms( radar_origin.x(), radar_origin.y(), -1 );
        CHECK( diff_radar( before, now, settings ).empty() );
    }

    SECTION( "a teleport is not compared with the old place" ) {
        now.origin = offset( radar_origin, 30, 0 );
        CHECK( diff_radar( before, now, settings ).empty() );
    }

    SECTION( "outdoors reports exits only by default" ) {
        now.outdoors = true;
        const std::vector<std::string> notices = diff_radar( before, now, settings );
        REQUIRE( notices.size() == 1 );
        CHECK( notices[0] == "Door, 3 north." );
    }

    SECTION( "outdoors can report everything or nothing" ) {
        now.outdoors = true;
        settings.outdoors = radar_outdoors::all;
        const std::vector<std::string> everything = diff_radar( before, now, settings );
        REQUIRE( everything.size() == 3 );
        // Area first, then exits, then walls.
        CHECK( everything[0] == "Area opens out." );
        CHECK( everything[1] == "Door, 3 north." );
        CHECK( everything[2] == "Opens east, 8." );

        settings.outdoors = radar_outdoors::off;
        CHECK( diff_radar( before, now, settings ).empty() );
    }
}

TEST_CASE( "radar announces a kind of hazard once", "[scene_radar][accessibility]" )
{
    radar_settings settings;
    const radar_snapshot before = make_corridor( radar_origin );
    radar_snapshot now = make_corridor( offset( radar_origin, 0, -1 ) );
    radar_hazard fire;
    fire.name = "fire";
    fire.pos = offset( radar_origin, 0, -5 );
    now.hazards.push_back( fire );

    // Off by default.
    CHECK( diff_radar( before, now, settings ).empty() );

    settings.hazards = true;
    CHECK( has_notice( diff_radar( before, now, settings ), "Fire, 4 north." ) );

    // Still burning, one step later: nothing new.
    radar_snapshot later = make_corridor( offset( radar_origin, 0, -2 ) );
    later.hazards = now.hazards;
    CHECK( diff_radar( now, later, settings ).empty() );
}

TEST_CASE( "radar notices are capped per step", "[scene_radar][accessibility]" )
{
    const std::vector<std::string> notices = { "One.", "Two.", "Three.", "Four.", "Five." };
    CHECK( render_radar( notices, 3 ) == "One. Two. Three. 2 more." );
    CHECK( render_radar( notices, 10 ) == "One. Two. Three. Four. Five." );
    CHECK( render_radar( { "One." }, 1 ) == "One." );
}

TEST_CASE( "radar gathers what the avatar can see", "[scene_radar][accessibility]" )
{
    map &here = get_map();
    avatar &ava = get_avatar();
    clear_avatar();
    clear_map_without_vision();
    set_time( calendar::turn_zero + 12_hours );
    const tripoint_bub_ms origin = ava.pos_bub();

    // A wall two tiles north with a door straight ahead and a window beside it.
    for( int dx = -3; dx <= 3; ++dx ) {
        here.ter_set( origin + tripoint( dx, -2, 0 ), ter_t_wall.id() );
    }
    here.ter_set( origin + tripoint( 0, -2, 0 ), ter_t_door_c.id() );
    here.ter_set( origin + tripoint( 1, -2, 0 ), ter_t_window.id() );
    // A second door beyond a short radar range.
    here.ter_set( origin + tripoint( 0, 8, 0 ), ter_t_door_c.id() );
    refresh_radar_vision( origin.z() );

    const auto find_at = []( const radar_snapshot & scene, const tripoint_abs_ms & pos ) {
        return std::find_if( scene.features.begin(), scene.features.end(),
        [&pos]( const radar_feature & feature ) {
            return feature.pos == pos;
        } );
    };
    const tripoint_abs_ms door_pos = here.get_abs( origin + tripoint( 0, -2, 0 ) );
    const tripoint_abs_ms window_pos = here.get_abs( origin + tripoint( 1, -2, 0 ) );
    const tripoint_abs_ms far_door_pos = here.get_abs( origin + tripoint( 0, 8, 0 ) );

    const radar_snapshot scene = gather_radar( ava, 12 );
    CHECK( scene.origin == here.get_abs( origin ) );
    CHECK_FALSE( scene.sight_reduced );
    CHECK( scene.open_tiles > 0 );

    CHECK( scene.probes[0].blocked );
    CHECK( scene.probes[0].distance == 2 );
    CHECK( scene.probes[0].tile == door_pos );
    CHECK( scene.probes[4].blocked );
    CHECK( scene.probes[4].distance == 8 );
    CHECK( scene.probes[4].tile == far_door_pos );
    CHECK_FALSE( scene.probes[2].blocked );
    CHECK( scene.probes[2].distance == 12 );

    const auto door = find_at( scene, door_pos );
    REQUIRE( door != scene.features.end() );
    CHECK( door->kind == radar_kind::door );
    CHECK( door->name == "closed wood door" );
    CHECK( door->short_name == "door" );

    const auto window = find_at( scene, window_pos );
    REQUIRE( window != scene.features.end() );
    CHECK( window->kind == radar_kind::window );

    CHECK( find_at( scene, far_door_pos ) != scene.features.end() );

    SECTION( "a shorter range leaves out what lies beyond it" ) {
        const radar_snapshot short_scan = gather_radar( ava, 5 );
        CHECK( find_at( short_scan, door_pos ) != short_scan.features.end() );
        CHECK( find_at( short_scan, far_door_pos ) == short_scan.features.end() );
        CHECK_FALSE( short_scan.probes[4].blocked );
        CHECK( short_scan.probes[4].distance == 5 );
        CHECK( short_scan.open_tiles < scene.open_tiles );
    }
}
