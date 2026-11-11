#include "status_browser.h"

#include <algorithm>
#include <map>
#include <optional>
#include <string>
#include <vector>

#include "accessibility.h"
#include "avatar.h"
#include "input_context.h"
#include "input_enums.h"
#include "panels.h"
#include "sidebar_accessibility.h"
#include "translations.h"
#include "uilist.h"

namespace
{

// Each entry's retval encodes its section and row so the section can be
// recovered from the selected entry (uilist keeps its category private).
constexpr int rows_per_section = 1000;

// Remembered between openings so a repeated check of one section is quick.
std::string last_layout_id;
std::string last_section_id;
std::map<std::string, int> last_row_in_section;

class status_browser_callback : public uilist_callback
{
    public:
        explicit status_browser_callback( const std::vector<status_section> &sections )
            : sections( sections ) {}

        bool key( const input_context &, const input_event &, int /*entnum*/,
                  uilist *menu ) override {
            if( menu == nullptr ) {
                return false;
            }
            if( menu->ret_act == "READ_SECTION" ) {
                const int section_index = current_section( *menu );
                if( section_index >= 0 ) {
                    get_narration_service().announce( {
                        narration_event::contextual_help,
                        speak_status_section( sections[section_index] ), true
                    } );
                }
                return true;
            }
            if( menu->ret_act == "READ_ALL_SECTIONS" ) {
                std::string everything = _( "Status, all sections." );
                for( const status_section &section : sections ) {
                    everything += " " + speak_status_section( section );
                }
                get_narration_service().announce( {
                    narration_event::contextual_help, everything, true
                } );
                return true;
            }
            // NEXT_SECTION / PREV_SECTION are returned to the caller as
            // UILIST_ADDITIONAL so it can switch categories.
            return false;
        }

        int current_section( const uilist &menu ) const {
            if( menu.selected < 0 || menu.selected >= static_cast<int>( menu.entries.size() ) ) {
                return -1;
            }
            const int index = menu.entries[menu.selected].retval / rows_per_section;
            return index >= 0 && index < static_cast<int>( sections.size() ) ? index : -1;
        }

    private:
        const std::vector<status_section> &sections;
};

} // namespace

void show_status_browser( const avatar &ava )
{
    const std::vector<status_section> sections = build_status_sections( ava );
    if( sections.empty() ) {
        get_narration_service().announce( {
            narration_event::contextual_help,
            _( "No sidebar sections are enabled. Turn panels on in the sidebar options." ), true
        } );
        return;
    }

    const std::string layout_id = panel_manager::get_manager().get_current_layout_id();
    if( layout_id != last_layout_id ) {
        last_layout_id = layout_id;
        last_section_id.clear();
        last_row_in_section.clear();
    }

    int section_index = 0;
    for( size_t i = 0; i < sections.size(); ++i ) {
        if( sections[i].id == last_section_id ) {
            section_index = static_cast<int>( i );
            break;
        }
    }

    status_browser_callback callback( sections );
    while( true ) {
        uilist menu;
        menu.title = _( "Status" );
        menu.input_category = "STATUS_BROWSER";
        menu.allow_confirm = false;
        menu.allow_cancel = true;
        menu.allow_additional = true;
        menu.callback = &callback;
        menu.additional_actions = {
            { "READ_SECTION", to_translation( "Read the whole section" ) },
            { "READ_ALL_SECTIONS", to_translation( "Read every section" ) },
            { "NEXT_SECTION", to_translation( "Next section" ) },
            { "PREV_SECTION", to_translation( "Previous section" ) }
        };

        int first_entry_of_section = 0;
        for( size_t s = 0; s < sections.size(); ++s ) {
            const status_section &section = sections[s];
            menu.add_category( section.id, section.name );
            if( static_cast<int>( s ) == section_index ) {
                first_entry_of_section = static_cast<int>( menu.entries.size() );
            }
            for( size_t r = 0; r < section.rows.size(); ++r ) {
                const status_row &row = section.rows[r];
                const int retval = static_cast<int>( s ) * rows_per_section + static_cast<int>( r );
                const std::string text = row.label.empty() ? row.value : row.label + ": " + row.value;
                menu.addentry_col( retval, true, std::optional<input_event>( input_event() ), text,
                                   row.band );
            }
        }
        menu.set_category_filter( [&sections]( const uilist_entry & entry, const std::string & key ) {
            const int index = entry.retval / rows_per_section;
            return index >= 0 && index < static_cast<int>( sections.size() ) && sections[index].id == key;
        } );

        const status_section &opening = sections[section_index];
        menu.set_category( opening.id );
        const auto remembered = last_row_in_section.find( opening.id );
        int row = remembered == last_row_in_section.end() ? 0 : remembered->second;
        row = std::max( 0, std::min( row, static_cast<int>( opening.rows.size() ) - 1 ) );
        menu.set_selected( first_entry_of_section + row );

        menu.query();

        // Remember where the player was, whichever way they left.
        const int left_section = callback.current_section( menu );
        if( left_section >= 0 ) {
            last_section_id = sections[left_section].id;
            last_row_in_section[last_section_id] =
                menu.entries[menu.selected].retval % rows_per_section;
            section_index = left_section;
        }

        if( menu.ret == UILIST_ADDITIONAL && menu.ret_act == "NEXT_SECTION" ) {
            section_index = ( section_index + 1 ) % static_cast<int>( sections.size() );
            continue;
        }
        if( menu.ret == UILIST_ADDITIONAL && menu.ret_act == "PREV_SECTION" ) {
            section_index = ( section_index + static_cast<int>( sections.size() ) - 1 ) %
                            static_cast<int>( sections.size() );
            continue;
        }
        break;
    }
}
