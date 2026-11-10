#include "ui_iteminfo.h"

#include <imgui/imgui.h>
#include <algorithm>
#include <string>
#include <vector>

#include "accessibility.h"
#include "color.h"
#include "input_enums.h"
#include "iteminfo_accessibility.h"
#include "translations.h"
#include "ui_manager.h"


iteminfo_window::iteminfo_window( item_info_data &info, point pos, int width, int height,
                                  ImGuiWindowFlags flags ) :
    cataimgui::window( _( "Item info" ), flags ),
    data( info ), pos( pos ), width( width ), height( height )
{
    ctxt = input_context( "default" );
    if( info.handle_scrolling ) {
        ctxt.register_action( "PAGE_UP" );
        ctxt.register_action( "PAGE_DOWN" );
        ctxt.register_action( "HOME" );
        ctxt.register_action( "END" );
        if( info.arrow_scrolling ) {
            ctxt.register_action( "UP" );
            ctxt.register_action( "DOWN" );
        }
    }
    ctxt.register_action( "HELP_KEYBINDINGS" );
    ctxt.register_action( "READ_ITEM_DESCRIPTION" );
    ctxt.register_action( "UP" );
    ctxt.register_action( "DOWN" );
    ctxt.register_action( "PAGE_UP" );
    ctxt.register_action( "PAGE_DOWN" );
    ctxt.register_action( "HOME" );
    ctxt.register_action( "END" );
    ctxt.register_action( "REPEAT_NARRATION" );
    ctxt.register_action( "CONFIRM" );
    ctxt.register_action( "QUIT" );
    if( info.any_input ) {
        ctxt.register_action( "ANY_INPUT" );
    }

    ctxt.set_timeout( 10 );
}

cataimgui::bounds iteminfo_window::get_bounds()
{
    return { pos.x < 0 ? -1.f : static_cast<float>( str_width_to_pixels( pos.x ) ),
             pos.y < 0 ? -1.f : static_cast<float>( str_height_to_pixels( pos.y ) ),
             static_cast<float>( str_width_to_pixels( width ) ),
             static_cast<float>( str_height_to_pixels( height ) ) };
}

void iteminfo_window::draw_controls()
{
    cataimgui::set_scroll( s );

    nc_color base_color = c_light_gray;
    if( !data.get_item_name().empty() ) {
        cataimgui::draw_colored_text( data.get_item_name(), base_color );
        ImGui::Spacing();
    }
    if( !data.get_type_name().empty() &&
        data.get_item_name().find( data.get_type_name() ) == std::string::npos ) {
        cataimgui::draw_colored_text( data.get_type_name(), base_color );
    }

    display_item_info( data.get_item_display(), data.get_item_compare() );
}

void iteminfo_window::execute()
{
    if( data.without_getch ) {
        ui_manager::redraw();
        return;
    }

    const std::string narration = data.accessibility_narration.empty() ?
                                  make_item_narration_text( data.get_item_name(), data.get_type_name(),
                                          data.get_item_display() ) : data.accessibility_narration;
    std::vector<std::string> lines;
    // The structured item-info generator separates each entry with a newline.
    // Break unusually long entries into sentences as well, so a rope or armor
    // description cannot consume the entire speech queue at once.
    size_t begin = 0;
    while( begin < narration.size() ) {
        const size_t end = narration.find( '\n', begin );
        const std::string paragraph = narration.substr( begin,
                                      end == std::string::npos ? std::string::npos : end - begin );
        size_t segment = 0;
        while( segment < paragraph.size() ) {
            size_t split = std::string::npos;
            if( paragraph.size() - segment > 180 ) {
                split = paragraph.find( ". ", segment );
            }
            const std::string text = paragraph.substr( segment,
                                     split == std::string::npos ? std::string::npos : split - segment + 1 );
            if( text.find_first_not_of( " \t\r" ) != std::string::npos ) {
                lines.push_back( text );
            }
            if( split == std::string::npos ) {
                break;
            }
            segment = split + 2;
        }
        if( end == std::string::npos ) {
            break;
        }
        begin = end + 1;
    }
    if( lines.empty() ) {
        lines.push_back( _( "No item description available." ) );
    }

    size_t spoken_line = 0;
    get_narration_service().announce( {
        narration_event::item_description_opened,
        lines.front() + " " +
        _( "Use Up and Down to read each description line. Page Up and Page Down skip ten lines. Home and End jump to the beginning or end. F8 reads the full description." ),
        true
    } );

    while( true ) {
        ui_manager::redraw();
        const std::string action = ctxt.handle_input();
        bool moved = false;

        if( action == "UP" ) {
            if( spoken_line > 0 ) {
                --spoken_line;
            }
            if( data.handle_scrolling && data.arrow_scrolling ) {
                s = cataimgui::scroll::line_up;
            }
            moved = true;
        } else if( action == "DOWN" ) {
            spoken_line = std::min( spoken_line + 1, lines.size() - 1 );
            if( data.handle_scrolling && data.arrow_scrolling ) {
                s = cataimgui::scroll::line_down;
            }
            moved = true;
        } else if( action == "PAGE_UP" ) {
            spoken_line = spoken_line > 10 ? spoken_line - 10 : 0;
            if( data.handle_scrolling ) {
                s = cataimgui::scroll::page_up;
            }
            moved = true;
        } else if( action == "PAGE_DOWN" ) {
            spoken_line = std::min( spoken_line + 10, lines.size() - 1 );
            if( data.handle_scrolling ) {
                s = cataimgui::scroll::page_down;
            }
            moved = true;
        } else if( action == "HOME" ) {
            spoken_line = 0;
            if( data.handle_scrolling ) {
                s = cataimgui::scroll::begin;
            }
            moved = true;
        } else if( action == "END" ) {
            spoken_line = lines.size() - 1;
            if( data.handle_scrolling ) {
                s = cataimgui::scroll::end;
            }
            moved = true;
        } else if( action == "READ_ITEM_DESCRIPTION" ) {
            get_narration_service().announce( {
                narration_event::contextual_help, narration, true
            } );
        } else if( action == "REPEAT_NARRATION" ) {
            get_narration_service().repeat_last();
        } else if( action == "CONFIRM" || action == "QUIT" ||
                   ( data.any_input && action == "ANY_INPUT" && !ctxt.get_raw_input().sequence.empty() ) ) {
            break;
        }
        if( moved ) {
            get_narration_service().announce( {
                narration_event::selection_changed, lines[spoken_line], true
            } );
        }
        if( !get_is_open() ) {
            break;
        }
    }
}
