#include "iteminfo_accessibility.h"

#include <string_view>

#include "output.h"
#include "string_formatter.h"
#include "translations.h"

namespace
{

void append_paragraph( std::string &target, const std::string &paragraph )
{
    if( paragraph.empty() ) {
        return;
    }
    if( !target.empty() ) {
        target += "\n";
    }
    target += remove_color_tags( paragraph );
}

std::string iteminfo_entry_text( const iteminfo &entry )
{
    // ASCII diagrams have visual layout but no sensible spoken reading.
    // The item description already provides textual armor coverage details.
    if( entry.bIsArt ) {
        return {};
    }
    if( entry.sType == "DESCRIPTION" ) {
        return !entry.bDrawName || entry.sName == "--" ? std::string() : entry.sName;
    }

    if( entry.isTable ) {
        // Tables are meaningful as text too. Preserve row order and turn
        // their visual column separators into spoken pauses.
        std::string text = entry.sName;
        size_t pos = 0;
        while( ( pos = text.find( ';', pos ) ) != std::string::npos ) {
            text.replace( pos, 1, ", " );
            pos += 2;
        }
        return text;
    }

    std::string text;
    if( entry.bDrawName ) {
        text += entry.sName;
    }

    const bool has_value = entry.sValue != "-999";
    const size_t number_marker = entry.sFmt.find( "<num>" );
    if( number_marker != std::string::npos ) {
        text += entry.sFmt.substr( 0, number_marker );
        if( has_value ) {
            text += entry.sValue;
        }
        text += entry.sFmt.substr( number_marker + 5 );
    } else {
        text += entry.sFmt;
        if( has_value ) {
            text += entry.sValue;
        }
    }
    return text;
}

// Read an item-name durability marker as its color, not a string of pluses
// or literal markup. The status-colored marker is a visual prefix; don't
// interpret ordinary colored text elsewhere in a name as a colored item.
std::string spoken_durability_color( std::string_view color )
{
    if( color.size() >= 2 &&
        ( color.substr( 0, 2 ) == "c_" || color.substr( 0, 2 ) == "h_" ||
          color.substr( 0, 2 ) == "i_" ) ) {
        color.remove_prefix( 2 );
    }
    if( color == "green" ) {
        return _( "green" );
    }
    if( color == "light_green" ) {
        return _( "light green" );
    }
    if( color == "red" ) {
        return _( "red" );
    }
    if( color == "light_red" ) {
        return _( "light red" );
    }
    if( color == "yellow" ) {
        return _( "yellow" );
    }
    if( color == "blue" ) {
        return _( "blue" );
    }
    if( color == "light_blue" ) {
        return _( "light blue" );
    }
    if( color == "cyan" ) {
        return _( "cyan" );
    }
    if( color == "light_cyan" ) {
        return _( "light cyan" );
    }
    if( color == "magenta" ) {
        return _( "magenta" );
    }
    if( color == "pink" ) {
        return _( "pink" );
    }
    if( color == "brown" ) {
        return _( "brown" );
    }
    if( color == "dark_gray" ) {
        return _( "dark gray" );
    }
    if( color == "light_gray" ) {
        return _( "light gray" );
    }
    if( color == "white" ) {
        return _( "white" );
    }
    if( color == "black" ) {
        return _( "black" );
    }
    return {};
}

} // namespace

std::string make_item_name_narration_text( const std::string &name )
{
    static constexpr std::string_view color_open = "<color_";
    static constexpr std::string_view color_close = "</color>";
    const std::string clean_name = remove_color_tags( name );

    if( name.compare( 0, color_open.size(), color_open ) != 0 ) {
        return clean_name;
    }
    const size_t open_end = name.find( '>', color_open.size() );
    if( open_end == std::string::npos ) {
        return clean_name;
    }
    const size_t close_start = name.find( color_close, open_end + 1 );
    if( close_start == std::string::npos ) {
        return clean_name;
    }
    const std::string_view indicator( name.data() + open_end + 1,
                                      close_start - open_end - 1 );
    // Only convert standalone durability symbols (like ++ or --).
    // For genuinely colored words, strip markup without changing their sense.
    if( indicator.empty() || indicator.find_first_not_of( "+-=|.!" ) != std::string_view::npos ) {
        return clean_name;
    }
    const std::string_view color( name.data() + color_open.size(),
                                  open_end - color_open.size() );
    const std::string spoken_color = spoken_durability_color( color );
    if( spoken_color.empty() ) {
        return clean_name;
    }
    size_t remainder_start = close_start + color_close.size();
    while( remainder_start < name.size() && name[remainder_start] == ' ' ) {
        ++remainder_start;
    }
    const std::string remaining = remove_color_tags( name.substr( remainder_start ) );
    return remaining.empty() ? spoken_color : spoken_color + " " + remaining;
}

std::string make_item_narration_text( const std::string &item_name,
                                      const std::string &type_name,
                                      const std::vector<iteminfo> &info,
                                      const std::string &location )
{
    std::string result;
    append_paragraph( result, make_item_name_narration_text( item_name ) );

    if( !type_name.empty() && item_name.find( type_name ) == std::string::npos ) {
        append_paragraph( result, string_format( _( "Type: %s" ), type_name ) );
    }
    if( !location.empty() ) {
        append_paragraph( result, string_format( _( "Location: %s" ), location ) );
    }

    for( const iteminfo &entry : info ) {
        append_paragraph( result, iteminfo_entry_text( entry ) );
    }

    return result;
}
