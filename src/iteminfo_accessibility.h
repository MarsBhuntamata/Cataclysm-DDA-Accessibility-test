#pragma once
#ifndef CATA_SRC_ITEMINFO_ACCESSIBILITY_H
#define CATA_SRC_ITEMINFO_ACCESSIBILITY_H

#include <string>
#include <vector>

#include "item.h"

/// Convert a displayed inventory name to speech-only text. In particular,
/// replace a leading colored durability indicator (such as a green "++")
/// with a spoken color, while leaving the actual game item name unchanged.
std::string make_item_name_narration_text( const std::string &name );

/// Convert structured item information into plain text suitable for speech.
/// This deliberately does not depend on either the ImGui or curses renderer.
std::string make_item_narration_text( const std::string &item_name,
                                      const std::string &type_name,
                                      const std::vector<iteminfo> &info,
                                      const std::string &location = "" );

/// What a sighted player sees on one inventory row, reduced to the parts
/// worth speaking.
struct inventory_focus_parts {
    /// Displayed name, may contain color tags.
    std::string name;
    /// Number marked for the pending action; zero when unmarked.
    size_t marked = 0;
    /// Number that could be marked.
    size_t available = 0;
    bool enabled = true;
    /// Why the entry cannot be chosen.  May be empty even when disabled.
    std::string denial;
    /// Whether the row has an expand/collapse marker at all.
    bool has_chevron = false;
    bool collapsed = false;
};

/// Speech for a newly focused inventory row.  Most important first: name,
/// then short row details, then expanded/collapsed state.
std::string make_inventory_focus_narration( const inventory_focus_parts &parts );

/// Speech for a change of marked count on the row that already has focus.
std::string make_inventory_mark_narration( size_t marked, size_t available );

/// Outcome of pressing the show/hide contents key on an inventory row.
enum class inventory_toggle_result {
    expanded,
    collapsed,
    nothing_to_expand,
    cannot_collapse,
    filter_active,
    overridden_show_all,
    overridden_hide_all
};

/// Speech for the show/hide contents key.  @p count is the number of rows
/// revealed and is only used for inventory_toggle_result::expanded.
std::string make_inventory_toggle_narration( inventory_toggle_result result, int count = 0 );

#endif // CATA_SRC_ITEMINFO_ACCESSIBILITY_H
