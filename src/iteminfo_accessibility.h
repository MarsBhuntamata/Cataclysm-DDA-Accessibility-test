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

#endif // CATA_SRC_ITEMINFO_ACCESSIBILITY_H
