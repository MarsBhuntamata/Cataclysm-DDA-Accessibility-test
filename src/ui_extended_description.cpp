#include "ui_extended_description.h"

#include <imgui/imgui.h>

#include "accessibility.h"
#include "cata_utility.h"
#include "character.h"
#include "color.h"
#include "creature.h"
#include "creature_tracker.h"
#include "map.h"
#include "mapdata.h"
#include "point.h"
#include "string_formatter.h"
#include "text.h"
#include "translations.h"
#include "type_id.h"
#include "ui_manager.h"
#include "viewer.h"
#include "vpart_position.h"

static const trait_id trait_ILLITERATE( "ILLITERATE" );

static description_target &operator++( description_target &c )
{
    c = static_cast<description_target>( static_cast<int>( c ) + 1 );
    if( c == description_target::num_targets ) {
        c = static_cast<description_target>( 0 );
    }
    return c;
}

static description_target &operator--( description_target &c )
{
    if( c == static_cast<description_target>( 0 ) ) {
        c = description_target::num_targets;
    }
    c = static_cast<description_target>( static_cast<int>( c ) - 1 );
    return c;
}

static const Creature *seen_critter( const tripoint_bub_ms &p )
{
    const map &here = get_map();

    const Creature *critter = get_creature_tracker().creature_at( p, true );
    if( critter != nullptr && get_player_view().sees( here, *critter ) ) {
        return critter;
    }

    return nullptr;
}

static void draw_sign_text( const tripoint_bub_ms &p )
{
    std::string signage = get_map().get_signage( p );
    if( !signage.empty() ) {
        ImGui::Separator();
        cataimgui::draw_colored_text( get_player_character().has_trait( trait_ILLITERATE ) ?
                                      // NOLINTNEXTLINE(cata-text-style): the question mark does not end a sentence
                                      _( "Sign: ???" ) : string_format( _( "Sign: %s" ), signage ) );
    }
}

static void draw_graffiti_text( const tripoint_bub_ms &p )
{
    std::string graffiti = get_map().graffiti_at( p );
    if( !graffiti.empty() ) {
        ImGui::Separator();
        cataimgui::draw_colored_text( get_player_character().has_trait( trait_ILLITERATE ) ?
                                      // NOLINTNEXTLINE(cata-text-style): the question mark does not end a sentence
                                      _( "Inscription: ???" ) : string_format( _( "Inscription: %s" ), graffiti ) );
    }
}

void draw_extended_description( const std::vector<std::string> &description, cataimgui::scroll &s )
{
    cataimgui::set_scroll( s );

    for( const std::string &s : description ) {
        if( s == "--" ) {
            ImGui::Separator();
        } else {
            for( const std::string &line : string_split( s, '\n' ) ) {
                cataimgui::TextColoredParagraph( c_light_gray, line );
                ImGui::NewLine();
            }
        }
    }
}

extended_description_window::extended_description_window( tripoint_bub_ms &p ) :
    cataimgui::window( "ext_desc",
                       ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoResize |
                       ImGuiWindowFlags_NoNavInputs ),
    p( p )
{
    const map &here = get_map();

    ctxt = input_context( "EXTENDED_DESCRIPTION" );
    ctxt.register_action( "NEXT_TAB" );
    ctxt.register_action( "PREV_TAB" );
    ctxt.register_action( "CREATURE" );
    ctxt.register_action( "FURNITURE" );
    ctxt.register_action( "TERRAIN" );
    ctxt.register_action( "VEHICLE" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    ctxt.register_action( "REPEAT_NARRATION" );
    ctxt.register_action( "READ_EXTENDED_DESCRIPTION" );
    ctxt.register_action( "UP" );
    ctxt.register_action( "DOWN" );
    ctxt.register_action( "PAGE_UP" );
    ctxt.register_action( "PAGE_DOWN" );
    ctxt.set_timeout( 10 );

    const Creature *critter = seen_critter( p );
    if( critter ) {
        creature_description = critter->extended_description();
    }
    bool sees = get_player_character().sees( here, p );
    const furn_id &furn = here.furn( p );
    if( sees && furn ) {
        furniture_description = furn->extended_description();
    }
    const ter_id &ter = here.ter( p );
    if( sees && ter ) {
        terrain_description = ter->extended_description();
    }
    const optional_vpart_position &vp = here.veh_at( p );
    if( sees && vp ) {
        veh_app_description = vp.extended_description();
    }

    if( critter ) {
        switch_target = description_target::creature;
    } else if( here.has_furn( p ) ) {
        switch_target = description_target::furniture;
    } else if( here.veh_at( p ) ) {
        switch_target = description_target::vehicle;
    }


}

void extended_description_window::draw_controls()
{
    if( ImGui::BeginTabBar( "description tabs" ) ) {
        draw_creature_tab();
        draw_furniture_tab();
        draw_terrain_tab();
        draw_vehicle_tab();
        ImGui::EndTabBar();
    }
}

void extended_description_window::draw_creature_tab()
{
    ImGuiTabItemFlags_ flags = ImGuiTabItemFlags_None;
    if( switch_target == description_target::creature ) {
        flags = ImGuiTabItemFlags_SetSelected;
        switch_target = description_target::num_targets;
    }
    std::string title = string_format( _( "[%s] Creature" ), ctxt.get_desc( "CREATURE" ) );
    if( ImGui::BeginTabItem( title.c_str(), nullptr, flags ) ) {
        cur_target = description_target::creature;
        if( !creature_description.empty() ) {
            draw_extended_description( creature_description, info_scroll );
        } else {
            cataimgui::draw_colored_text( _( "You do not see any creature here." ), c_light_gray );
        }
        ImGui::EndTabItem();
    }
}

void extended_description_window::draw_furniture_tab()
{
    ImGuiTabItemFlags_ flags = ImGuiTabItemFlags_None;
    if( switch_target == description_target::furniture ) {
        flags = ImGuiTabItemFlags_SetSelected;
        switch_target = description_target::num_targets;
    }
    std::string title = string_format( _( "[%s] Furniture" ), ctxt.get_desc( "FURNITURE" ) );
    if( ImGui::BeginTabItem( title.c_str(), nullptr, flags ) ) {
        cur_target = description_target::furniture;
        if( !furniture_description.empty() ) {
            draw_extended_description( furniture_description, info_scroll );
        } else {
            cataimgui::draw_colored_text( _( "You do not see any furniture here." ), c_light_gray );
        }
        draw_sign_text( p );
        ImGui::EndTabItem();
    }
}

void extended_description_window::draw_terrain_tab()
{
    ImGuiTabItemFlags_ flags = ImGuiTabItemFlags_None;
    if( switch_target == description_target::terrain ) {
        flags = ImGuiTabItemFlags_SetSelected;
        switch_target = description_target::num_targets;
    }
    std::string title = string_format( _( "[%s] Terrain" ), ctxt.get_desc( "TERRAIN" ) );
    if( ImGui::BeginTabItem( title.c_str(), nullptr, flags ) ) {
        cur_target = description_target::terrain;
        if( !terrain_description.empty() ) {
            draw_extended_description( terrain_description, info_scroll );
        } else {
            cataimgui::draw_colored_text( _( "You can't see the terrain here." ), c_light_gray );
        }
        draw_graffiti_text( p );
        ImGui::EndTabItem();
    }
}

void extended_description_window::draw_vehicle_tab()
{
    ImGuiTabItemFlags_ flags = ImGuiTabItemFlags_None;
    if( switch_target == description_target::vehicle ) {
        flags = ImGuiTabItemFlags_SetSelected;
        switch_target = description_target::num_targets;
    }
    std::string title = string_format( _( "[%s] Vehicle/Appliance" ), ctxt.get_desc( "VEHICLE" ) );
    if( ImGui::BeginTabItem( title.c_str(), nullptr, flags ) ) {
        cur_target = description_target::vehicle;
        if( !veh_app_description.empty() ) {
            draw_extended_description( veh_app_description, info_scroll );
        } else {
            cataimgui::draw_colored_text( _( "You can't see vehicles or appliances here." ), c_light_gray );
        }
        ImGui::EndTabItem();
    }
}

cataimgui::bounds extended_description_window::get_bounds()
{
    return bounds;
}

void extended_description_window::show()
{
    description_target spoken_tab = description_target::num_targets;
    size_t spoken_line = 0;
    while( true ) {
        ui_manager::redraw_invalidated();

        const std::vector<std::string> *description = nullptr;
        std::string heading;
        switch( cur_target ) {
            case description_target::creature:
                heading = _( "Creature" );
                description = &creature_description;
                break;
            case description_target::furniture:
                heading = _( "Furniture" );
                description = &furniture_description;
                break;
            case description_target::terrain:
                heading = _( "Terrain" );
                description = &terrain_description;
                break;
            case description_target::vehicle:
                heading = _( "Vehicle or appliance" );
                description = &veh_app_description;
                break;
            case description_target::num_targets:
                break;
        }
        std::vector<std::string> readable_lines;
        if( description != nullptr ) {
            for( const std::string &paragraph : *description ) {
                if( paragraph == "--" ) {
                    continue;
                }
                for( const std::string &raw_line : string_split( paragraph, '\n' ) ) {
                    const std::string plain = remove_color_tags( raw_line );
                    if( !plain.empty() ) {
                        readable_lines.push_back( plain );
                    }
                }
            }
        }
        if( spoken_tab != cur_target ) {
            spoken_tab = cur_target;
            spoken_line = 0;
            std::string report = heading + ". ";
            if( readable_lines.empty() ) {
                report += _( "No visible description." );
            } else {
                report += readable_lines.front();
                if( readable_lines.size() > 1 ) {
                    report += ". " + std::string(
                                  _( "Use Down and Up for additional description lines, or F8 for all." ) );
                }
            }
            get_narration_service().announce( { narration_event::ui_opened, report, true } );
        }

        std::string action = ctxt.handle_input();
        if( action == "REPEAT_NARRATION" ) {
            get_narration_service().repeat_last();
            continue;
        }
        if( action == "READ_EXTENDED_DESCRIPTION" ) {
            std::string report = heading + ". ";
            if( readable_lines.empty() ) {
                report += _( "No visible description." );
            } else {
                for( const std::string &line : readable_lines ) {
                    report += line + ". ";
                }
            }
            get_narration_service().announce( {
                narration_event::contextual_help, report, true
            } );
            continue;
        }
        if( action == "NEXT_TAB" ) {
            switch_target = cur_target;
            ++switch_target;
        } else if( action == "PREV_TAB" ) {
            switch_target = cur_target;
            --switch_target;
        }  else if( action == "UP" ) {
            info_scroll = cataimgui::scroll::line_up;
            if( spoken_line > 0 ) {
                --spoken_line;
            }
            if( !readable_lines.empty() ) {
                get_narration_service().announce( {
                    narration_event::selection_changed,
                    readable_lines[spoken_line], true
                } );
            }
        }  else if( action == "DOWN" ) {
            info_scroll = cataimgui::scroll::line_down;
            if( !readable_lines.empty() ) {
                spoken_line = std::min( spoken_line + 1, readable_lines.size() - 1 );
                get_narration_service().announce( {
                    narration_event::selection_changed,
                    readable_lines[spoken_line], true
                } );
            }
        }  else if( action == "PAGE_UP" ) {
            info_scroll = cataimgui::scroll::page_up;
        }  else if( action == "PAGE_DOWN" ) {
            info_scroll = cataimgui::scroll::page_down;
        } else if( action == "CREATURE" ) {
            switch_target = description_target::creature;
        } else if( action == "FURNITURE" ) {
            switch_target = description_target::furniture;
        } else if( action == "TERRAIN" ) {
            switch_target = description_target::terrain;
        } else if( action == "VEHICLE" ) {
            switch_target = description_target::vehicle;
        } else if( action == "CONFIRM" || action == "QUIT" ) {
            return;
        }
    }
}
