#pragma once
#ifndef CATA_SRC_SIDEBAR_ACCESSIBILITY_H
#define CATA_SRC_SIDEBAR_ACCESSIBILITY_H

#include <string>
#include <vector>

#include "bodypart.h"
#include "color.h"
#include "type_id.h"

class Character;
class avatar;
class widget;

/**
 * Spoken equivalent of the sighted sidebar.
 *
 * The sighted player reads the sidebar constantly. These helpers walk the
 * player's active sidebar layout and turn every widget into plain spoken
 * rows, so the screen-reader player hears exactly the sections and values a
 * sighted player with the same layout sees, in the same order. Graphs, the
 * body graph, the compass and the mini overmap are replaced with words since
 * their glyphs carry no meaning when spoken; colour-only cues become a short
 * "band" word such as "low" or "warning".
 *
 * This layer deliberately knows nothing about the renderer or the narration
 * backend, so it can be unit tested.
 */
struct status_row {
    // Expanded label: "Stamina", "Left arm", "North". May be empty.
    std::string label;
    // Spoken value with colour tags removed.
    std::string value;
    // Colour band spoken after the value: "low", "critical", "below normal",
    // "warning", "overloaded", "hostile sees you"... Empty when normal.
    std::string band;
    // True when the row deserves a mention in the glance summary.
    bool abnormal = false;
    // Internal: this row only carries a label (or limb status) for the next
    // label-less sibling in a columns layout. Never emitted as-is.
    bool label_only = false;

    // "Label: value, band"
    std::string spoken() const;
};

struct status_section {
    // window_panel id, stable across openings; used to remember the section.
    std::string id;
    // Panel name as shown in the sidebar options.
    std::string name;
    std::vector<status_row> rows;
};

/** Rows for one widget (recursing into layouts), in visual reading order. */
std::vector<status_row> accessible_widget_rows( const widget_id &wid, const avatar &ava,
        int width );

/** One section per enabled panel of the active sidebar layout, in order. */
std::vector<status_section> build_status_sections( const avatar &ava );

/** "Section name. row. row. ..." for reading a whole section at once. */
std::string speak_status_section( const status_section &section );

/** One-shot summary: visible creatures first, then every abnormal row. */
std::string build_status_glance( const avatar &ava );

/** "Creatures: 2 zombies north, hostile sees you; dog west." */
std::string describe_visible_threats( const avatar &ava );

// Helpers exposed for unit tests.
std::string expand_sidebar_label( const std::string &label, const widget_id &wid );
std::string describe_bodypart_status( const Character &u, const bodypart_id &bp );
std::string severity_word_for_color( const nc_color &color );
std::string band_for_numeric( widget &w, int value );
std::string band_for_percent( double percent );

#endif // CATA_SRC_SIDEBAR_ACCESSIBILITY_H
