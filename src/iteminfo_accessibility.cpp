#include "iteminfo_accessibility.h"

#include "output.h"
#include "string_formatter.h"

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
    if( entry.sType == "DESCRIPTION" ) {
        return entry.sName == "--" ? std::string() : entry.sName;
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

} // namespace

std::string make_item_narration_text( const std::string &item_name,
                                      const std::string &type_name,
                                      const std::vector<iteminfo> &info,
                                      const std::string &location )
{
    std::string result;
    append_paragraph( result, item_name );

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
