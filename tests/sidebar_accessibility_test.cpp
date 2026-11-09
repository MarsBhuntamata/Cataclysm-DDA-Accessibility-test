#include <algorithm>
#include <string>
#include <vector>

#include "avatar.h"
#include "bodypart.h"
#include "calendar.h"
#include "cata_catch.h"
#include "coordinates.h"
#include "effect.h"
#include "game.h"
#include "map.h"
#include "map_helpers.h"
#include "map_helpers_tests.h"
#include "monster.h"
#include "panels.h"
#include "player_helpers.h"
#include "point.h"
#include "sidebar_accessibility.h"
#include "string_formatter.h"
#include "type_id.h"
#include "widget.h"

static const efftype_id effect_bite( "bite" );
static const efftype_id effect_bleed( "bleed" );
static const efftype_id effect_hunger_blank( "hunger_blank" );
static const efftype_id effect_hunger_hungry( "hunger_hungry" );
static const efftype_id effect_hunger_starving( "hunger_starving" );

// Widgets from data/mods/TEST_DATA/widgets.json and data/json/ui
static const widget_id widget_test_compass_N( "test_compass_N" );
static const widget_id widget_test_hp_head_graph( "test_hp_head_graph" );
static const widget_id widget_test_hunger_clause( "test_hunger_clause" );
static const widget_id widget_test_stamina_graph( "test_stamina_graph" );
static const widget_id widget_test_str_color_num( "test_str_color_num" );
static const widget_id widget_ll_stats_layout( "ll_stats_layout" );
static const widget_id widget_status_hpg_layout_larm( "status_hpg_layout_larm" );

static bool contains( const std::string &haystack, const std::string &needle )
{
    return haystack.find( needle ) != std::string::npos;
}

TEST_CASE( "sidebar labels are expanded for speech", "[sidebar_accessibility][accessibility]" )
{
    const widget_id none;
    CHECK( expand_sidebar_label( "Stam", none ) == "Stamina" );
    CHECK( expand_sidebar_label( "STR", none ) == "Strength" );
    CHECK( expand_sidebar_label( "L ARM ", none ) == "Left arm" );
    CHECK( expand_sidebar_label( "HEAD ", none ) == "Head" );
    CHECK( expand_sidebar_label( "N:", none ) == "North" );
    CHECK( expand_sidebar_label( "Safe", none ) == "Safe mode" );
    CHECK( expand_sidebar_label( "Weather", none ) == "Weather" );
    CHECK( expand_sidebar_label( "<color_c_red>Pain</color>", none ) == "Pain" );
}

TEST_CASE( "sidebar colour bands become words", "[sidebar_accessibility][accessibility]" )
{
    CHECK( band_for_percent( 5.0 ) == "critical" );
    CHECK( band_for_percent( 30.0 ) == "low" );
    CHECK( band_for_percent( 50.0 ) == "moderate" );
    CHECK( band_for_percent( 90.0 ).empty() );

    CHECK( severity_word_for_color( c_red ) == "warning" );
    CHECK( severity_word_for_color( c_light_red ) == "warning" );
    CHECK( severity_word_for_color( c_yellow ) == "caution" );
    CHECK( severity_word_for_color( c_green ).empty() );
    CHECK( severity_word_for_color( c_white ).empty() );
}

TEST_CASE( "numeric sidebar widgets speak their normal band", "[sidebar_accessibility][accessibility]" )
{
    avatar &ava = get_avatar();
    clear_avatar();
    ava.set_str_base( 8 );

    SECTION( "normal strength has no band" ) {
        const std::vector<status_row> rows = accessible_widget_rows( widget_test_str_color_num, ava, 40 );
        REQUIRE( rows.size() == 1 );
        CHECK( rows[0].label == "Strength" );
        CHECK( rows[0].value == "8" );
        CHECK( rows[0].band.empty() );
        CHECK_FALSE( rows[0].abnormal );
        CHECK( rows[0].spoken() == "Strength: 8" );
    }

    SECTION( "reduced strength is below normal" ) {
        ava.set_str_bonus( -2 );
        const std::vector<status_row> rows = accessible_widget_rows( widget_test_str_color_num, ava, 40 );
        REQUIRE( rows.size() == 1 );
        CHECK( rows[0].value == "6" );
        CHECK( rows[0].band == "below normal" );
        CHECK( rows[0].abnormal );
    }

    SECTION( "boosted strength is above normal" ) {
        ava.set_str_bonus( 2 );
        const std::vector<status_row> rows = accessible_widget_rows( widget_test_str_color_num, ava, 40 );
        REQUIRE( rows.size() == 1 );
        CHECK( rows[0].band == "above normal" );
        CHECK( rows[0].abnormal );
    }
}

TEST_CASE( "graph sidebar widgets speak percentages instead of symbols",
           "[sidebar_accessibility][accessibility]" )
{
    avatar &ava = get_avatar();
    clear_avatar();

    SECTION( "full stamina" ) {
        ava.set_stamina( ava.get_stamina_max() );
        const std::vector<status_row> rows = accessible_widget_rows( widget_test_stamina_graph, ava, 40 );
        REQUIRE( rows.size() == 1 );
        CHECK( rows[0].label == "Stamina" );
        CHECK( rows[0].value == "100 percent" );
        CHECK( rows[0].band.empty() );
        CHECK_FALSE( rows[0].abnormal );
    }

    SECTION( "nearly exhausted stamina is critical" ) {
        ava.set_stamina( ava.get_stamina_max() / 10 );
        const std::vector<status_row> rows = accessible_widget_rows( widget_test_stamina_graph, ava, 40 );
        REQUIRE( rows.size() == 1 );
        CHECK( rows[0].value == "10 percent" );
        CHECK( rows[0].band == "critical" );
        CHECK( rows[0].abnormal );
    }
}

TEST_CASE( "limb hit point widgets speak current of maximum plus wounds",
           "[sidebar_accessibility][accessibility]" )
{
    avatar &ava = get_avatar();
    clear_avatar();
    const bodypart_id head( "head" );
    const int max_hp = ava.get_part_hp_max( head );

    SECTION( "healthy head" ) {
        ava.set_part_hp_cur( head, max_hp );
        const std::vector<status_row> rows = accessible_widget_rows( widget_test_hp_head_graph, ava, 40 );
        REQUIRE( rows.size() == 1 );
        CHECK( rows[0].label == "Head" );
        CHECK( rows[0].value == string_format( "%d of %d", max_hp, max_hp ) );
        CHECK( rows[0].band.empty() );
        CHECK_FALSE( rows[0].abnormal );
    }

    SECTION( "badly hurt and bleeding head" ) {
        ava.set_part_hp_cur( head, std::max( 1, max_hp / 10 ) );
        ava.add_effect( effect_bleed, 1_minutes, head );
        ava.get_effect( effect_bleed, head ).set_intensity( 15 );
        const std::vector<status_row> rows = accessible_widget_rows( widget_test_hp_head_graph, ava, 40 );
        REQUIRE( rows.size() == 1 );
        CHECK( contains( rows[0].band, "critical" ) );
        CHECK( contains( rows[0].band, "bleeding" ) );
        CHECK( rows[0].abnormal );
    }
}

TEST_CASE( "blank clause text is spoken as a word", "[sidebar_accessibility][accessibility]" )
{
    avatar &ava = get_avatar();
    clear_avatar();

    SECTION( "no hunger" ) {
        ava.add_effect( effect_hunger_blank, 1_minutes );
        const std::vector<status_row> rows = accessible_widget_rows( widget_test_hunger_clause, ava, 40 );
        REQUIRE( rows.size() == 1 );
        CHECK( rows[0].label == "Hunger" );
        CHECK( rows[0].value == "not hungry" );
        CHECK_FALSE( rows[0].abnormal );
    }

    SECTION( "hungry is caution, not an alarm" ) {
        ava.add_effect( effect_hunger_hungry, 1_minutes );
        const std::vector<status_row> rows = accessible_widget_rows( widget_test_hunger_clause, ava, 40 );
        REQUIRE( rows.size() == 1 );
        CHECK( rows[0].value == "Hungry" );
        CHECK( rows[0].band == "caution" );
        CHECK_FALSE( rows[0].abnormal );
    }

    SECTION( "starving is a warning" ) {
        ava.add_effect( effect_hunger_starving, 1_minutes );
        const std::vector<status_row> rows = accessible_widget_rows( widget_test_hunger_clause, ava, 40 );
        REQUIRE( rows.size() == 1 );
        CHECK( rows[0].value == "Starving!" );
        CHECK( rows[0].band == "warning" );
        CHECK( rows[0].abnormal );
    }
}

TEST_CASE( "body part status is described in words", "[sidebar_accessibility][accessibility]" )
{
    avatar &ava = get_avatar();
    clear_avatar();
    const bodypart_id arm( "arm_l" );

    CHECK( describe_bodypart_status( ava, arm ).empty() );

    ava.add_effect( effect_bite, 1_minutes, arm );
    ava.add_effect( effect_bleed, 1_minutes, arm );
    ava.get_effect( effect_bleed, arm ).set_intensity( 15 );
    const std::string status = describe_bodypart_status( ava, arm );
    CHECK( contains( status, "bleeding" ) );
    CHECK_FALSE( contains( status, "lightly" ) );
    CHECK( contains( status, "bitten" ) );

    ava.get_effect( effect_bleed, arm ).set_intensity( 25 );
    CHECK( contains( describe_bodypart_status( ava, arm ), "bleeding heavily" ) );
}

TEST_CASE( "layouts are read in sidebar order with labels merged",
           "[sidebar_accessibility][accessibility]" )
{
    avatar &ava = get_avatar();
    clear_avatar();

    SECTION( "stats panel of the labels sidebar" ) {
        const std::vector<status_row> rows = accessible_widget_rows( widget_ll_stats_layout, ava, 44 );
        REQUIRE( rows.size() == 6 );
        CHECK( rows[0].label == "Strength" );
        CHECK( rows[1].label == "Dexterity" );
        CHECK( rows[2].label == "Bionic power" );
        CHECK( rows[3].label == "Intelligence" );
        CHECK( rows[4].label == "Perception" );
        CHECK( rows[5].label == "Safe mode" );
    }

    SECTION( "coloured limb name merges into the hit point row beside it" ) {
        const bodypart_id arm( "arm_l" );
        ava.add_effect( effect_bite, 1_minutes, arm );
        const std::vector<status_row> rows = accessible_widget_rows( widget_status_hpg_layout_larm, ava, 11 );
        REQUIRE( rows.size() == 1 );
        CHECK( rows[0].label == "Left arm" );
        CHECK( contains( rows[0].value, " of " ) );
        CHECK( contains( rows[0].band, "bitten" ) );
        CHECK( rows[0].abnormal );
    }
}

TEST_CASE( "compass and glance describe visible creatures", "[sidebar_accessibility][accessibility]" )
{
    const map &here = get_map();
    avatar &ava = get_avatar();
    clear_avatar();
    clear_map_without_vision();
    set_time( calendar::turn_zero + 12_hours );
    // The glance walks the active sidebar layout; game::setup() normally
    // initialises the panel manager, but the test harness does not run it.
    panel_manager::get_manager().init();

    SECTION( "nothing around" ) {
        g->mon_info_update();
        CHECK( describe_visible_threats( ava ) == "No creatures in view." );
        const std::vector<status_row> rows = accessible_widget_rows( widget_test_compass_N, ava, 36 );
        REQUIRE( rows.size() == 1 );
        CHECK( rows[0].label == "North" );
        CHECK( rows[0].value == "clear" );
        const std::string glance = build_status_glance( ava );
        CHECK( contains( glance, "Status glance." ) );
        CHECK( contains( glance, "No creatures in view." ) );
    }

    SECTION( "one monster to the northeast" ) {
        const tripoint_bub_ms northeast = ava.pos_bub() + tripoint( 10, -10, 0 );
        monster &mon = spawn_test_monster( "mon_test_CBM", northeast );
        g->mon_info_update();
        REQUIRE( ava.sees( here, mon ) );
        const std::string threats = describe_visible_threats( ava );
        CHECK( contains( threats, "monster producing CBMs when dissected" ) );
        CHECK_FALSE( contains( threats, "No creatures" ) );
        // The compass row for north stays clear; the creature is northeast.
        const std::vector<status_row> rows = accessible_widget_rows( widget_test_compass_N, ava, 36 );
        REQUIRE( rows.size() == 1 );
        CHECK( rows[0].value == "clear" );
    }

    SECTION( "wounds are always part of the glance" ) {
        g->mon_info_update();
        const bodypart_id arm( "arm_l" );
        ava.add_effect( effect_bleed, 1_minutes, arm );
        ava.get_effect( effect_bleed, arm ).set_intensity( 25 );
        const std::string glance = build_status_glance( ava );
        CHECK( contains( glance, "Left arm" ) );
        CHECK( contains( glance, "bleeding heavily" ) );
        CHECK_FALSE( contains( glance, "All sidebar values normal" ) );
    }
}
