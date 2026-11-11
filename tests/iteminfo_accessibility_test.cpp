#include "iteminfo_accessibility.h"

#include <string>
#include <vector>

#include "cata_catch.h"

TEST_CASE( "item durability colors become spoken words, without markup", "[accessibility]" )
{
    CHECK( make_item_name_narration_text( "<color_c_green>++</color> animal boxer shorts" ) ==
           "green animal boxer shorts" );
    CHECK( make_item_name_narration_text( "<color_c_light_green>++</color> jacket" ) ==
           "light green jacket" );
    CHECK( make_item_name_narration_text( "<color_c_red>--</color> damaged boots" ) ==
           "red damaged boots" );
    CHECK( make_item_name_narration_text( "plain backpack" ) == "plain backpack" );
    CHECK( make_item_name_narration_text( "<color_c_red>dangerous</color> item" ) ==
           "dangerous item" );
    CHECK( make_item_name_narration_text( "<color_c_green>++</color>" ) == "green" );
    CHECK( make_item_name_narration_text( "<color_unknown>++</color> object" ) == "++ object" );

    const std::string info = make_item_narration_text(
                                 "<color_c_green>++</color> animal boxer shorts", "", {} );
    CHECK( info == "green animal boxer shorts" );
}

TEST_CASE( "item information is converted to plain narration text", "[accessibility]" )
{
    std::vector<iteminfo> info;
    info.emplace_back( "DESCRIPTION", "A dependable tool." );
    info.emplace_back( "WEAPON", "Damage: ", "<num>", iteminfo::no_flags, 0 );
    info.back().sValue = "12";

    const std::string narration = make_item_narration_text(
                                      "iron hammer", "tool", info, "in your inventory" );

    CHECK( narration.find( "iron hammer" ) != std::string::npos );
    CHECK( narration.find( "Type: tool" ) != std::string::npos );
    CHECK( narration.find( "Location: in your inventory" ) != std::string::npos );
    CHECK( narration.find( "A dependable tool." ) != std::string::npos );
    CHECK( narration.find( "Damage: 12" ) != std::string::npos );
}

TEST_CASE( "item narration omits visual separator entries", "[accessibility]" )
{
    std::vector<iteminfo> info;
    info.emplace_back( "DESCRIPTION", "--" );
    info.emplace_back( "DESCRIPTION", "A plain description." );

    const std::string narration = make_item_narration_text( "test item", "", info );

    CHECK( narration.find( "--" ) == std::string::npos );
    CHECK( narration.find( "A plain description." ) != std::string::npos );
}

TEST_CASE( "item narration respects hidden item information entries", "[accessibility]" )
{
    std::vector<iteminfo> info;
    info.emplace_back( "DESCRIPTION", "Internal-only text", iteminfo::no_name );
    info.emplace_back( "DESCRIPTION", "Player-facing description." );

    const std::string narration = make_item_narration_text( "test item", "", info );

    CHECK( narration.find( "Internal-only text" ) == std::string::npos );
    CHECK( narration.find( "Player-facing description." ) != std::string::npos );
}

TEST_CASE( "inventory focus speaks the name first, then details, then state", "[accessibility]" )
{
    inventory_focus_parts parts;
    parts.name = "<color_c_green>++</color> backpack";
    CHECK( make_inventory_focus_narration( parts ) == "green backpack" );

    // Expanded or collapsed is only spoken for rows that can be toggled.
    parts.collapsed = true;
    CHECK( make_inventory_focus_narration( parts ) == "green backpack" );
    parts.has_chevron = true;
    CHECK( make_inventory_focus_narration( parts ) == "green backpack. collapsed" );
    parts.collapsed = false;
    CHECK( make_inventory_focus_narration( parts ) == "green backpack. expanded" );

    parts.marked = 3;
    parts.available = 5;
    parts.enabled = false;
    parts.denial = "<color_c_red>too heavy</color>";
    CHECK( make_inventory_focus_narration( parts ) ==
           "green backpack. Marked 3 of 5. Unavailable: too heavy. expanded" );

    parts.denial.clear();
    parts.marked = 0;
    CHECK( make_inventory_focus_narration( parts ) == "green backpack. Unavailable. expanded" );
}

TEST_CASE( "inventory marking is spoken briefly", "[accessibility]" )
{
    CHECK( make_inventory_mark_narration( 0, 5 ) == "Unmarked" );
    CHECK( make_inventory_mark_narration( 1, 1 ) == "Marked" );
    CHECK( make_inventory_mark_narration( 2, 5 ) == "Marked 2 of 5" );
}

TEST_CASE( "show or hide contents always has a spoken outcome", "[accessibility]" )
{
    using result = inventory_toggle_result;
    CHECK( make_inventory_toggle_narration( result::expanded, 4 ) == "Expanded, 4 items" );
    CHECK( make_inventory_toggle_narration( result::expanded, 1 ) == "Expanded, 1 item" );
    CHECK( make_inventory_toggle_narration( result::expanded ) == "Expanded" );
    CHECK( make_inventory_toggle_narration( result::collapsed ) == "Collapsed" );
    for( const result r : {
             result::nothing_to_expand, result::cannot_collapse, result::filter_active,
             result::overridden_show_all, result::overridden_hide_all
         } ) {
        CHECK_FALSE( make_inventory_toggle_narration( r ).empty() );
    }
}
