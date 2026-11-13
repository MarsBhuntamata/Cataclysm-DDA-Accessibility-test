#include "game.h" // IWYU pragma: associated

#include <algorithm>
#include <map>
#include <string>
#include <vector>

#include "accessibility.h"
#include "avatar.h"
#include "calendar.h"
#include "cata_imgui.h"
#include "color.h"
#include "dialogue.h"
#include "dialogue_helpers.h"
#include "display.h"
#include "game_constants.h"
#include "imgui/imgui.h"
#include "input_context.h"
#include "line.h"
#include "mission.h"
#include "npc.h"
#include "faction.h"
#include "output.h"
#include "overmapbuffer.h"
#include "point.h"
#include "string_formatter.h"
#include "talker.h"
#include "timed_event.h"
#include "translation.h"
#include "translations.h"
#include "ui_manager.h"
#include "units.h"
#include "units_utility.h"

static const dimension_id dimension_world_default( "default" );

static const faction_id faction_no_faction( "no_faction" );

namespace
{
enum class mission_ui_tab_enum : int {
    ACTIVE = 0,
    COMPLETED,
    FAILED,
    POINTS_OF_INTEREST,
    MAP_DISCOVERIES,
    num_tabs
};
} // namespace

static mission_ui_tab_enum &operator++( mission_ui_tab_enum &c )
{
    c = static_cast<mission_ui_tab_enum>( static_cast<int>( c ) + 1 );
    if( c == mission_ui_tab_enum::num_tabs ) {
        c = static_cast<mission_ui_tab_enum>( 0 );
    }
    return c;
}

static mission_ui_tab_enum &operator--( mission_ui_tab_enum &c )
{
    if( c == static_cast<mission_ui_tab_enum>( 0 ) ) {
        c = mission_ui_tab_enum::num_tabs;
    }
    c = static_cast<mission_ui_tab_enum>( static_cast<int>( c ) - 1 );
    return c;
}

namespace
{
class mission_ui
{
        friend class mission_ui_impl;
    public:
        void draw_mission_ui();
};

class mission_ui_impl : public cataimgui::window
{
    public:
        std::string last_action;
        explicit mission_ui_impl() : cataimgui::window( _( "Your missions" ),
                    ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoNav ) {
        }

    private:
        void draw_mission_names( std::vector<mission *> missions, int &selected_mission,
                                 bool &need_adjust ) const;
        void draw_point_of_interest_names( const std::vector<point_of_interest> &points_of_interest,
                                           int &selected_mission,
                                           bool &need_adjust ) const;
        void draw_selected_description( std::vector<mission *> missions, int &selected_mission );
        void draw_selected_description( const std::vector<point_of_interest> &points_of_interest,
                                        const int &selected_mission ) const;
        void draw_label_with_value( const std::string &label, const std::string &value ) const;
        void draw_location( const std::string &label, const tripoint_abs_omt &loc ) const;

        mission_ui_tab_enum selected_tab = mission_ui_tab_enum::ACTIVE;
        mission_ui_tab_enum switch_tab = mission_ui_tab_enum::num_tabs;
        std::string last_spoken_mission;
        bool mission_screen_announced = false;
        // Roadmap terrain names and distance ordering are only computed once
        // per mission-screen opening, not on every ImGui redraw.
        bool discoveries_loaded = false;
        std::vector<point_of_interest> discovered_locations;

        float window_width = std::clamp( float( str_width_to_pixels( EVEN_MINIMUM_TERM_WIDTH ) ),
                                         ImGui::GetMainViewport()->Size.x / 2,
                                         ImGui::GetMainViewport()->Size.x );
        float window_height = std::clamp( float( str_height_to_pixels( EVEN_MINIMUM_TERM_HEIGHT ) ),
                                          ImGui::GetMainViewport()->Size.y / 2,
                                          ImGui::GetMainViewport()->Size.y );
        float table_column_width = window_width / 2;

        cataimgui::scroll s = cataimgui::scroll::none;

    protected:
        void draw_controls() override;
};

void mission_ui::draw_mission_ui()
{
    input_context ctxt( "MISSION_UI" );
    mission_ui_impl p_impl;

    ctxt.register_navigate_ui_list();
    ctxt.register_leftright();
    ctxt.register_action( "NEXT_TAB" );
    ctxt.register_action( "PREV_TAB" );
    ctxt.register_action( "SELECT" );
    ctxt.register_action( "MOUSE_MOVE" );
    ctxt.register_action( "CONFIRM",
                          to_translation( "Set selected mission/point of interest as current objective" ) );
    ctxt.register_action( "DELETE_POINT_OF_INTEREST", to_translation( "Delete Point of Interest" ) );
    // We don't actually have a way to remap this right now
    //ctxt.register_action( "DOUBLE_CLICK", to_translation( "Set selected mission as current objective" ) );
    ctxt.register_action( "HELP_KEYBINDINGS" );
    ctxt.register_action( "QUIT" );
    // Smooths out our handling, makes tabs load immediately after input instead of waiting for next.
    ctxt.set_timeout( 10 );

    while( true ) {
        ui_manager::redraw_invalidated();


        p_impl.last_action = ctxt.handle_input();

        if( p_impl.last_action == "QUIT" || !p_impl.get_is_open() ) {
            break;
        }
    }
}

void mission_ui_impl::draw_controls()
{
    ImGui::SetWindowSize( ImVec2( window_width, window_height ), ImGuiCond_Once );
    std::vector<mission *> umissions;
    std::vector<point_of_interest> upoints_of_interest;

    static int selected_mission = 0;
    bool adjust_selected = false;

    if( last_action == "QUIT" ) {
        return;
    } else if( last_action == "NEXT_TAB" || last_action == "RIGHT" ) {
        adjust_selected = true;
        selected_mission = 0;
        s = cataimgui::scroll::begin;
        switch_tab = selected_tab;
        ++switch_tab;
    } else if( last_action == "PREV_TAB" || last_action == "LEFT" ) {
        adjust_selected = true;
        selected_mission = 0;
        s = cataimgui::scroll::begin;
        switch_tab = selected_tab;
        --switch_tab;
    } else if( last_action == "PAGE_UP" ) {
        ImGui::SetWindowFocus(); // Dumb hack! Clear our focused item so listbox selection isn't nav highlighted.
        s = cataimgui::scroll::page_up;
    } else if( last_action == "PAGE_DOWN" ) {
        ImGui::SetWindowFocus(); // Dumb hack! Clear our focused item so listbox selection isn't nav highlighted.
        s = cataimgui::scroll::page_down;
    }

    ImGuiTabItemFlags_ flags = ImGuiTabItemFlags_None;

    if( ImGui::BeginTabBar( "##TAB_BAR" ) ) {
        flags = ImGuiTabItemFlags_None;
        if( switch_tab == mission_ui_tab_enum::ACTIVE ) {
            flags = ImGuiTabItemFlags_SetSelected;
            switch_tab = mission_ui_tab_enum::num_tabs;
        }
        if( ImGui::BeginTabItem( _( "ACTIVE" ), nullptr, flags ) ) {
            selected_tab = mission_ui_tab_enum::ACTIVE;
            umissions = get_avatar().get_active_missions();
            ImGui::EndTabItem();
        }
        flags = ImGuiTabItemFlags_None;
        if( switch_tab == mission_ui_tab_enum::COMPLETED ) {
            flags = ImGuiTabItemFlags_SetSelected;
            switch_tab = mission_ui_tab_enum::num_tabs;
        }
        if( ImGui::BeginTabItem( _( "COMPLETED" ), nullptr, flags ) ) {
            selected_tab = mission_ui_tab_enum::COMPLETED;
            umissions = get_avatar().get_completed_missions();
            ImGui::EndTabItem();
        }
        flags = ImGuiTabItemFlags_None;
        if( switch_tab == mission_ui_tab_enum::FAILED ) {
            flags = ImGuiTabItemFlags_SetSelected;
            switch_tab = mission_ui_tab_enum::num_tabs;
        }
        if( ImGui::BeginTabItem( _( "FAILED" ), nullptr, flags ) ) {
            selected_tab = mission_ui_tab_enum::FAILED;
            umissions = get_avatar().get_failed_missions();
            ImGui::EndTabItem();
        }
        flags = ImGuiTabItemFlags_None;
        if( switch_tab == mission_ui_tab_enum::POINTS_OF_INTEREST ) {
            flags = ImGuiTabItemFlags_SetSelected;
            switch_tab = mission_ui_tab_enum::num_tabs;
        }
        if( ImGui::BeginTabItem( _( "POINTS OF INTEREST" ), nullptr, flags ) ) {
            selected_tab = mission_ui_tab_enum::POINTS_OF_INTEREST;
            upoints_of_interest = get_avatar().get_points_of_interest();
            ImGui::EndTabItem();
        }
        flags = ImGuiTabItemFlags_None;
        if( switch_tab == mission_ui_tab_enum::MAP_DISCOVERIES ) {
            flags = ImGuiTabItemFlags_SetSelected;
            switch_tab = mission_ui_tab_enum::num_tabs;
        }
        if( ImGui::BeginTabItem( _( "MAP DISCOVERIES" ), nullptr, flags ) ) {
            selected_tab = mission_ui_tab_enum::MAP_DISCOVERIES;
            if( !discoveries_loaded ) {
                const tripoint_abs_omt player_pos = get_avatar().pos_abs_omt();
                discovered_locations.reserve( get_avatar().map_revealed_omts.size() );
                for( const tripoint_abs_omt &pos : get_avatar().map_revealed_omts ) {
                const om_vision_level vision = overmap_buffer.seen( pos );
                const std::string name = vision == om_vision_level::unseen ?
                                         _( "Map-revealed location" ) :
                                         overmap_buffer.ter( pos )->get_name( vision );
                    discovered_locations.push_back( { pos, name } );
                }
                std::sort( discovered_locations.begin(), discovered_locations.end(),
            [&player_pos]( const point_of_interest & lhs, const point_of_interest & rhs ) {
                const int lhs_dist = rl_dist( player_pos, lhs.pos );
                const int rhs_dist = rl_dist( player_pos, rhs.pos );
                if( lhs_dist != rhs_dist ) {
                    return lhs_dist < rhs_dist;
                }
                if( lhs.pos.x() != rhs.pos.x() ) {
                    return lhs.pos.x() < rhs.pos.x();
                }
                if( lhs.pos.y() != rhs.pos.y() ) {
                    return lhs.pos.y() < rhs.pos.y();
                }
                return lhs.pos.z() < rhs.pos.z();
                } );
                discoveries_loaded = true;
            }
            ImGui::EndTabItem();
        }
        ImGui::EndTabBar();
    }

    const std::vector<point_of_interest> &visible_points =
        selected_tab == mission_ui_tab_enum::MAP_DISCOVERIES ? discovered_locations : upoints_of_interest;
    size_t num_entries = umissions.size();
    if( selected_tab == mission_ui_tab_enum::POINTS_OF_INTEREST ||
        selected_tab == mission_ui_tab_enum::MAP_DISCOVERIES ) {
        num_entries = visible_points.size();
    }
    const int last_entry = num_entries == 0 ? 0 : ( static_cast<int>( num_entries ) - 1 );
    const int previous_selected_mission = selected_mission;
    if( last_action == "UP" ) {
        ImGui::SetKeyboardFocusHere( -1 );
        selected_mission--;
    } else if( last_action == "DOWN" ) {
        ImGui::SetKeyboardFocusHere( 1 );
        selected_mission++;
    } else if( last_action == "HOME" ) {
        selected_mission = 0;
    } else if( last_action == "END" ) {
        selected_mission = last_entry;
    }
    if( selected_mission < 0 ) {
        selected_mission = last_entry;
    } else if( selected_mission > last_entry ) {
        selected_mission = 0;
    }
    adjust_selected |= selected_mission != previous_selected_mission;

    // This action needs to be after umissions is populated
    if( last_action == "CONFIRM" ) {
        if( selected_tab == mission_ui_tab_enum::ACTIVE && !umissions.empty() ) {
            mission &chosen = *umissions[selected_mission];
            get_avatar().set_active_mission( chosen );
            std::string confirmation = string_format( _( "Current mission selected: %s." ),
                                        chosen.name() );
            const tripoint_abs_omt target = get_avatar().get_active_mission_target();
            if( !target.is_invalid() ) {
                const tripoint_abs_omt here = get_avatar().pos_abs_omt();
                confirmation += " " + string_format( _( "Target is %d overmap tiles away." ),
                                                   rl_dist( here, target ) );
                if( target.x() != here.x() ) {
                    confirmation += " " + std::string( target.x() > here.x() ? _( "East." ) : _( "West." ) );
                }
                if( target.y() != here.y() ) {
                    confirmation += " " + std::string( target.y() > here.y() ? _( "South." ) : _( "North." ) );
                }
                if( target.z() != here.z() ) {
                    confirmation += " " + std::string( target.z() > here.z() ? _( "Above." ) : _( "Below." ) );
                }
            } else {
                confirmation += " " + std::string( _( "No map target available for this mission." ) );
            }
            get_narration_service().announce( {
                narration_event::status_changed, confirmation, true
            } );
        } else if( ( selected_tab == mission_ui_tab_enum::POINTS_OF_INTEREST ||
                     selected_tab == mission_ui_tab_enum::MAP_DISCOVERIES ) &&
                   !visible_points.empty() ) {
            const point_of_interest &chosen = visible_points[selected_mission];
            if( selected_tab == mission_ui_tab_enum::MAP_DISCOVERIES ) {
                // A roadmap discovery is not automatically a mission POI. Save
                // only the selected discovery as a POI so it can be targeted.
                get_avatar().add_point_of_interest( chosen );
            } else {
                get_avatar().set_active_point_of_interest( chosen );
            }
            const tripoint_abs_omt here = get_avatar().pos_abs_omt();
            std::string confirmation = string_format( _( "Current point of interest selected: %s. %d overmap tiles away." ),
                                       chosen.text, rl_dist( here, chosen.pos ) );
            if( chosen.pos.x() != here.x() ) {
                confirmation += chosen.pos.x() > here.x() ? _( " East." ) : _( " West." );
            }
            if( chosen.pos.y() != here.y() ) {
                confirmation += chosen.pos.y() > here.y() ? _( " South." ) : _( " North." );
            }
            get_narration_service().announce( {
                narration_event::status_changed, confirmation, true
            } );
        }
    }

    if( last_action == "DELETE_POINT_OF_INTEREST" &&
        selected_tab == mission_ui_tab_enum::POINTS_OF_INTEREST && !visible_points.empty() ) {
        get_avatar().delete_point_of_interest( visible_points[selected_mission].pos );
    }

    // ImGui mission lists are not exposed to NVDA by ordinary keyboard focus.
    // Report the selected entry and description, but only on a meaningful change.
    std::string mission_report;
    switch( selected_tab ) {
        case mission_ui_tab_enum::ACTIVE:
            mission_report = _( "Active missions" );
            break;
        case mission_ui_tab_enum::COMPLETED:
            mission_report = _( "Completed missions" );
            break;
        case mission_ui_tab_enum::FAILED:
            mission_report = _( "Failed missions" );
            break;
        case mission_ui_tab_enum::POINTS_OF_INTEREST:
            mission_report = _( "Points of interest" );
            break;
        case mission_ui_tab_enum::MAP_DISCOVERIES:
            mission_report = _( "Map discoveries. Nearest first" );
            break;
        default:
            break;
    }
    if( num_entries == 0 ) {
        mission_report += ". " + std::string( _( "No entries." ) );
    } else if( selected_tab == mission_ui_tab_enum::POINTS_OF_INTEREST ||
               selected_tab == mission_ui_tab_enum::MAP_DISCOVERIES ) {
        const point_of_interest &poi = visible_points[selected_mission];
        mission_report += ". " + poi.text;
        const tripoint_abs_omt here = get_avatar().pos_abs_omt();
        mission_report += ". " + string_format( _( "%d overmap tiles away." ),
                                               rl_dist( here, poi.pos ) );
        if( poi.pos.x() != here.x() ) {
            mission_report += poi.pos.x() > here.x() ? _( " East." ) : _( " West." );
        }
        if( poi.pos.y() != here.y() ) {
            mission_report += poi.pos.y() > here.y() ? _( " South." ) : _( " North." );
        }
        if( get_avatar().get_active_point_of_interest().pos == poi.pos ) {
            mission_report += ". " + std::string( _( "Current destination." ) );
        }
    } else {
        const mission &miss = *umissions[selected_mission];
        mission_report += ". " + miss.name();
        if( get_avatar().get_active_mission() == &miss ) {
            mission_report += ". " + std::string( _( "Current objective." ) );
        }
        if( !miss.get_description().empty() ) {
            mission_report += ". " + remove_color_tags( miss.get_description() );
        }
    }
    if( mission_report != last_spoken_mission || !mission_screen_announced ) {
        get_narration_service().announce( {
            mission_screen_announced ? narration_event::selection_changed : narration_event::ui_opened,
            mission_screen_announced ? mission_report :
            std::string( _( "Your missions. " ) ) + mission_report, true
        } );
        last_spoken_mission = mission_report;
        mission_screen_announced = true;
    }

    if( ( selected_tab != mission_ui_tab_enum::POINTS_OF_INTEREST &&
          selected_tab != mission_ui_tab_enum::MAP_DISCOVERIES && umissions.empty() ) ||
        ( ( selected_tab == mission_ui_tab_enum::POINTS_OF_INTEREST ||
            selected_tab == mission_ui_tab_enum::MAP_DISCOVERIES ) && visible_points.empty() ) ) {
        static const std::map< mission_ui_tab_enum, translation > nope = {
            { mission_ui_tab_enum::ACTIVE, to_translation( "You have no active missions!" ) },
            { mission_ui_tab_enum::COMPLETED, to_translation( "You haven't completed any missions!" ) },
            { mission_ui_tab_enum::FAILED, to_translation( "You haven't failed any missions!" ) },
            { mission_ui_tab_enum::POINTS_OF_INTEREST, to_translation( "You don't have any points of interest.  Add those from the overmap." ) },
            { mission_ui_tab_enum::MAP_DISCOVERIES, to_translation( "No map discoveries. Read a roadmap to reveal locations." ) }
        };
        ImGui::TextWrapped( "%s", nope.at( selected_tab ).translated().c_str() );
        return;
    }

    if( get_avatar().get_active_mission() ) {
        ImGui::TextWrapped( _( "Current objective: %s" ),
                            get_avatar().get_active_mission()->name().c_str() );
    } else if( get_avatar().get_active_point_of_interest().pos != tripoint_abs_omt::invalid ) {
        ImGui::TextWrapped( _( "Current point of interest: %s" ),
                            get_avatar().get_active_point_of_interest().text.c_str() );
    }

    if( ImGui::BeginTable( "##MISSION_TABLE", 2, ImGuiTableFlags_None,
                           ImGui::GetContentRegionAvail() ) ) {
        // Missions selection is purposefully thinner than the description, it has less to convey.
        if( selected_tab != mission_ui_tab_enum::POINTS_OF_INTEREST &&
            selected_tab != mission_ui_tab_enum::MAP_DISCOVERIES ) {
            ImGui::TableSetupColumn( _( "Missions" ), ImGuiTableColumnFlags_WidthStretch,
                                     table_column_width * 0.8 );
        } else {
            ImGui::TableSetupColumn( selected_tab == mission_ui_tab_enum::MAP_DISCOVERIES ?
                                     _( "Map Discoveries" ) : _( "Points of Interest" ), ImGuiTableColumnFlags_WidthStretch,
                                     table_column_width * 0.8 );
        }
        ImGui::TableSetupColumn( _( "Description" ), ImGuiTableColumnFlags_WidthStretch,
                                 table_column_width * 1.2 );
        ImGui::TableHeadersRow();
        ImGui::TableNextColumn();
        if( selected_tab != mission_ui_tab_enum::POINTS_OF_INTEREST &&
            selected_tab != mission_ui_tab_enum::MAP_DISCOVERIES ) {
            draw_mission_names( umissions, selected_mission, adjust_selected );
        } else {
            draw_point_of_interest_names( visible_points, selected_mission, adjust_selected );
        }
        ImGui::TableNextColumn();
        if( selected_tab != mission_ui_tab_enum::POINTS_OF_INTEREST &&
            selected_tab != mission_ui_tab_enum::MAP_DISCOVERIES ) {
            draw_selected_description( umissions, selected_mission );
        } else {
            draw_selected_description( visible_points, selected_mission );
        }
        ImGui::EndTable();
    }

    cataimgui::set_scroll( s );
}

void mission_ui_impl::draw_mission_names( std::vector<mission *> missions, int &selected_mission,
        bool &need_adjust ) const
{
    const int num_missions = missions.size();

    if( ImGui::BeginListBox( "##LISTBOX", ImVec2( table_column_width * 0.75,
                             ImGui::GetContentRegionAvail().y ) ) ) {
        for( int i = 0; i < num_missions; i++ ) {
            const bool is_selected = selected_mission == i;
            ImGui::PushID( i );
            if( ImGui::Selectable( missions[i]->name().c_str(), is_selected,
                                   ImGuiSelectableFlags_AllowDoubleClick ) ) {
                selected_mission = i;
                if( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) {
                    get_avatar().set_active_mission( *missions[selected_mission] );
                }
            }

            if( is_selected && need_adjust ) {
                ImGui::SetScrollHereY();
                ImGui::SetItemDefaultFocus();
            }
            ImGui::PopID();
        }
        ImGui::EndListBox();
    }
}

void mission_ui_impl::draw_point_of_interest_names( const std::vector<point_of_interest> &
        points_of_interest, int &selected_mission,
        bool &need_adjust ) const
{
    const int num_missions = points_of_interest.size();

    if( ImGui::BeginListBox( "##LISTBOX", ImVec2( table_column_width * 0.75,
                             ImGui::GetContentRegionAvail().y ) ) ) {
        ImGuiListClipper clipper;
        clipper.Begin( num_missions );
        if( num_missions > 0 ) {
            clipper.IncludeItemsByIndex( selected_mission, selected_mission + 1 );
        }
        while( clipper.Step() ) {
            for( int i = clipper.DisplayStart; i < clipper.DisplayEnd; i++ ) {
                const bool is_selected = selected_mission == i;
                ImGui::PushID( i );
                if( ImGui::Selectable( points_of_interest[i].text.c_str(), is_selected,
                                   ImGuiSelectableFlags_AllowDoubleClick ) ) {
                selected_mission = i;
                if( ImGui::IsMouseDoubleClicked( ImGuiMouseButton_Left ) ) {
                    if( selected_tab == mission_ui_tab_enum::MAP_DISCOVERIES ) {
                        get_avatar().add_point_of_interest( points_of_interest[selected_mission] );
                    } else {
                        get_avatar().set_active_point_of_interest( points_of_interest[selected_mission] );
                    }
                }
            }

            if( is_selected && need_adjust ) {
                ImGui::SetScrollHereY();
                ImGui::SetItemDefaultFocus();
            }
                ImGui::PopID();
            }
        }
        ImGui::EndListBox();
    }
}

void mission_ui_impl::draw_label_with_value( const std::string &label,
        const std::string &value ) const
{
    ImGui::TextColored( c_white, "%s", label.c_str() );
    const float label_width = table_column_width * 0.3f;
    ImGui::SameLine( label_width );
    ImGui::TextColored( c_light_gray, "%s", value.c_str() );
}

void mission_ui_impl::draw_selected_description( std::vector<mission *> missions,
        int &selected_mission )
{
    mission *miss = missions[selected_mission];
    ImGui::TextWrapped( _( "Mission: %s" ), miss->name().c_str() );
    npc *mission_giver = nullptr;
    if( miss->get_npc_id().is_valid() ) {
        mission_giver = g->find_npc( miss->get_npc_id() );
        if( mission_giver ) {
            draw_label_with_value( _( "Given by:" ), mission_giver->disp_name() );
            if( mission_giver->get_faction() && mission_giver->get_fac_id() != faction_no_faction ) {
                draw_label_with_value( _( "Faction:" ), mission_giver->get_faction()->get_name() );
            }
            const tripoint_abs_omt npc_location = mission_giver->pos_abs_omt();
            draw_location( _( "Map location:" ), npc_location );
        }
    }
    ImGui::Separator();
    static std::string raw_description;
    static std::string parsed_description;
    // We do this to avoid replacing expanded snippets with other valid snippet text on redraw. The raw description includes un-parsed snippet tags.
    // The parsed description is, obviously, after they've been parsed. So "<zombie_name>" becomes "undead thing" or "zed" or whatever.
    // Whenever the user changes which mission they're looking at, this will update to parse the description *once* and keep displaying that parsed version
    // until the mission changes again. The raw_description string is stored for as long as the program is running.
    // This hinges on the 'static' keyword influencing storage duration. See https://en.cppreference.com/w/cpp/language/storage_duration.html etc
    if( raw_description != miss->get_description() ) {
        raw_description = miss->get_description();
        parsed_description = raw_description;
        // Handles(example)  <reward_count:item>   in description
        // Not handled in parse_tags for some reason!
        dialogue d( get_talker_for( get_avatar() ), nullptr, {} );
        const talk_effect_fun_t::likely_rewards_t &rewards = miss->get_likely_rewards();
        for( const auto &reward : rewards ) {
            std::string token = "<reward_count:" + itype_id( reward.second.evaluate( d ) ).str() + ">";
            parsed_description = string_replace( parsed_description, token, string_format( "%g",
                                                 reward.first.evaluate( d ) ) );
        }
        const Character &other_talker = mission_giver ? *mission_giver : get_player_character();
        // parse_tags() modifies the argument string directly, no return value.
        parse_tags( parsed_description, get_player_character(), other_talker );
    }
    cataimgui::draw_colored_text( parsed_description, c_unset, table_column_width * 1.15 );
    if( miss->has_deadline() ) {
        const time_point deadline = miss->get_deadline();
        if( selected_tab == mission_ui_tab_enum::ACTIVE ) {
            ImGui::TextWrapped( _( "Deadline: %s" ), to_string( deadline ).c_str() );
            const time_duration remaining = deadline - calendar::turn;
            std::string remaining_time;
            if( remaining <= 0_turns ) {
                remaining_time = _( "None!" );
            } else if( get_player_character().has_watch() ) {
                remaining_time = to_string( remaining );
            } else {
                remaining_time = to_string_approx( remaining );
            }
            ImGui::TextWrapped( _( "Time remaining: %s" ), remaining_time.c_str() );
        } else {
            const time_duration time_in_past = calendar::turn - deadline;
            std::string time_in_past_string;
            if( get_player_character().has_watch() ) {
                time_in_past_string = to_string( time_in_past );
            } else {
                time_in_past_string = to_string_approx( time_in_past );
            }
            if( deadline != calendar::turn_zero ) {
                if( selected_tab == mission_ui_tab_enum::COMPLETED ) {
                    //~The replaced string is a calendar date, such as "Year 1, May 12, 08:04:32"
                    cataimgui::draw_colored_text( string_format( _( "Completed: %s" ),
                                                  to_string( deadline ) ), c_green );
                } else if( selected_tab == mission_ui_tab_enum::FAILED ) {
                    //~The replaced string is a calendar date, such as "Year 1, May 12, 08:04:32"
                    cataimgui::draw_colored_text( string_format( _( "Failed at: %s" ),
                                                  to_string( deadline ).c_str() ), c_red );
                }
                //~The replaced string is a time duration, such as "12 hours", or "5 minutes"
                cataimgui::draw_colored_text( string_format( _( "%s ago" ), time_in_past_string ), c_unset );
            }
        }
    }
    if( miss->has_target() ) {
        // TODO: target does not contain a z-component, targets are assumed to be on z=0
        draw_location( _( "Target:" ), miss->get_target() );
    }
    dimension_id mission_dimension = miss->get_dimension();
    // If dimension isn't the default one
    if( mission_dimension != dimension_world_default ) {
        draw_label_with_value( _( "Dimension:" ), mission_dimension.str() );
    }
}

void mission_ui_impl::draw_selected_description( const std::vector<point_of_interest> &points_of_interest,
        const int &selected_mission ) const
{
    const point_of_interest &selected_point_of_interest = points_of_interest[selected_mission];
    ImGui::TextWrapped( selected_tab == mission_ui_tab_enum::MAP_DISCOVERIES ?
                       _( "Map discovery: %s" ) : _( "Point of Interest: %s" ),
                       selected_point_of_interest.text.c_str() );
    ImGui::Separator();
    draw_location( _( "Target:" ), selected_point_of_interest.pos );
}

void mission_ui_impl::draw_location( const std::string &label,
                                     const tripoint_abs_omt &loc ) const
{
    const tripoint_abs_omt pos = get_player_character().pos_abs_omt();
    draw_label_with_value( label, display::overmap_position_text( loc ) );
    if( !you_know_where_you_are() ) {
        // Don't display "Distance:" or direction arrow if we don't know where we are
        return;
    }
    int omt_distance = rl_dist( pos, loc );
    if( omt_distance > 0 ) {
        // One OMT is 24 tiles across, at 1x1 meters each, so we can simply do number of OMTs * 24
        units::length actual_distance = omt_distance * 24_meter;
        const std::string dir_arrow = direction_arrow( direction_from( pos.xy(), loc.xy() ) );
        //~Parenthesis is a real-world value for distance. Example string: "223 tiles (5.35km) ⇗"
        const std::string distance_str = string_format( _( "%1$d tiles (%2$s) %3$s" ),
                                         omt_distance, length_to_string_approx( actual_distance ), dir_arrow );
        draw_label_with_value( _( "Distance:" ), distance_str );
    }
}
} // namespace

void game::list_missions()
{
    mission_ui new_instance;
    new_instance.draw_mission_ui();
}
