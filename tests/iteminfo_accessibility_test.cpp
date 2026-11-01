#include "iteminfo_accessibility.h"

#include <string>
#include <vector>

#include "cata_catch.h"

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
