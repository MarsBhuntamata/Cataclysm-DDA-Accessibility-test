#!/usr/bin/env python3
"""Port reviewed NVDA map, dialogue, and item-use changes after upstream reorganization.
Only run against the isolated Nov 21 merge candidate. Refuse unexpected structure.
"""
from pathlib import Path

def rep(s, old, new, title):
    n=s.count(old)
    if n!=1:
        raise RuntimeError(f"{title}: expected exactly one anchor, got {n}")
    return s.replace(old,new,1)

def edit(path, f):
    p=Path(path)
    original=p.read_text(encoding="utf-8")
    updated=f(original)
    if updated==original:
        raise RuntimeError(f"{path}: no changes")
    p.write_text(updated,encoding="utf-8")

def iuse(s):
    s=rep(s,'#include "action.h"','#include "accessibility.h"\n#include "action.h"',"item accessibility include")
    s=rep(s,'    it->spill_contents( suitable.value() );\n    item_location::form_loc( p, here, pos, *it ).remove_item();',
      '    it->spill_contents( suitable.value() );\n    // Accessible confirmation when furniture is deployed.\n    p->add_msg_if_player( m_info, _( "You deploy the %s." ), it->tname() );\n    item_location::form_loc( p, here, pos, *it ).remove_item();',"furniture deploy announcement")
    s=rep(s,'    p->map_revealed_omts.clear();',
      '    // Keep previously discovered points of interest across roadmap readings.\n    const size_t discoveries_before = p->map_revealed_omts.size();',"roadmap discoveries preserved")
    s=rep(s,'    if( p->map_revealed_omts.empty() ) {',
      '    if( p->map_revealed_omts.size() == discoveries_before ) {',"roadmap no-new-discoveries detection")
    s=rep(s,'    const std::optional<tripoint_bub_ms> pos_ = choose_adjacent( string_format(\n                _( "Place %s where?" ),\n                it->tname() ) );',
      '    const std::string placement_prompt = string_format( _( "Place %s where?" ), it->tname() );\n    get_narration_service().announce( {\n        narration_event::contextual_help,\n        placement_prompt + " " + _( "Choose an adjacent tile with a direction key." ), true\n    } );\n    const std::optional<tripoint_bub_ms> pos_ = choose_adjacent( placement_prompt );',
      "trap placement narration")
    return s

def dialogue(s):
    s=rep(s,'#include "auto_pickup.h"','#include "accessibility.h"\n#include "auto_pickup.h"',"dialogue accessibility include")
    s=rep(s,'    d_img.sel_response = 0;\n    size_t response_ind = response_hotkeys.size();',
      '    d_img.sel_response = 0;\n    // Speak the NPC/computer line and initial response, not the whole menu.\n    const std::string spoken_line = remove_color_tags( challenge );\n    if( !spoken_line.empty() && !response_lines.empty() ) {\n        get_narration_service().announce( {\n            narration_event::ui_opened,\n            spoken_line + ". " + remove_color_tags( response_lines.front().text ), true\n        } );\n    }\n    size_t response_ind = response_hotkeys.size();',
      "dialogue initial narration")
    s=rep(s,'            action = ctxt.handle_input();\n',
      '            action = ctxt.handle_input();\n            const int previous_spoken_response = d_img.sel_response;\n',"dialogue previous selection")
    s=rep(s,'            if( action == "HELP_KEYBINDINGS" ) {',
      '            if( action == "REPEAT_NARRATION" ) {\n                get_narration_service().repeat_last();\n            } else if( action == "HELP_KEYBINDINGS" ) {',"dialogue repeat action")
    s=rep(s,'                d_img.sel_response = static_cast<int>( response_ind ) - 1;\n            }\n\n        } while',
      '                d_img.sel_response = static_cast<int>( response_ind ) - 1;\n            }\n            if( d_img.sel_response != previous_spoken_response &&\n                d_img.sel_response >= 0 &&\n                static_cast<size_t>( d_img.sel_response ) < response_lines.size() ) {\n                get_narration_service().announce( {\n                    narration_event::selection_changed,\n                    remove_color_tags( response_lines[d_img.sel_response].text ), true\n                } );\n            }\n\n        } while',
      "dialogue changed-selection narration")
    return s

def cache(s):
    s=rep(s,'#include <bitset>','#include <bitset>\n#include <chrono>',"cache chrono")
    s=rep(s,'#include "avatar.h"','#include "accessibility.h"\n#include "avatar.h"',"cache accessibility")
    s=rep(s,'#include "point.h"','#include "point.h"\n#include "string_formatter.h"',"cache string format")
    s=rep(s,'void map::update_visibility_cache( const int zlev )\n{\n    Character',
      'void map::update_visibility_cache( const int zlev )\n{\n    const bool profile = look_profiler_active();\n    const auto visibility_start = std::chrono::steady_clock::now();\n    Character',"visibility profile start")
    s=rep(s,'    visibility_variables_cache.variables_set = true; // Not used yet\n}',
      '    visibility_variables_cache.variables_set = true; // Not used yet\n    if( profile ) {\n        const long long elapsed_us = std::chrono::duration_cast<std::chrono::microseconds>(\n                                         std::chrono::steady_clock::now() - visibility_start ).count();\n        if( elapsed_us >= 50000 ) {\n            record_look_profiler_sample( string_format(\n                                            "LOOK2_VISIBILITY z=%d duration_us=%lld", zlev, elapsed_us ) );\n        }\n    }\n}',"visibility profile end")
    s=rep(s,'void map::build_map_cache( const int zlev, bool skip_lightmap )\n{\n    const int minz',
      'void map::build_map_cache( const int zlev, bool skip_lightmap )\n{\n    using profiler_clock = std::chrono::steady_clock;\n    const bool profile = look_profiler_active();\n    const auto start = profiler_clock::now();\n    const int minz',"map cache profile start")
    s=rep(s,'    build_scene_caches();\n    bool seen_cache_dirty',
      '    build_scene_caches();\n    const auto scene_done = profiler_clock::now();\n    bool seen_cache_dirty',"scene timer")
    s=rep(s,'        seen_cache_dirty |= build_vision_transparency_cache( z );\n    }\n\n    avatar',
      '        seen_cache_dirty |= build_vision_transparency_cache( z );\n    }\n    const auto transparency_done = profiler_clock::now();\n\n    avatar',"transparency timer")
    s=rep(s,'    // Light sources that change with no notice to the lightmap: characters,',
      '    const auto seen_done = profiler_clock::now();\n    // Light sources that change with no notice to the lightmap: characters,',"seen timer")
    s=rep(s,'    if( seen_cache_dirty ) {\n        refresh_vision_levels();\n    }\n    if( !skip_lightmap )',
      '    const auto sources_done = profiler_clock::now();\n    if( seen_cache_dirty ) {\n        refresh_vision_levels();\n    }\n    const auto levels_done = profiler_clock::now();\n    if( !skip_lightmap )',"source timer")
    s=rep(s,'                set_sunlight_only( z );\n            }\n        }\n    }\n}\n\nlevel_cache &map::access_cache',
      '            set_sunlight_only( z );\n            }\n        }\n    }\n    const auto done = profiler_clock::now();\n    if( profile ) {\n        const auto us = []( const auto &a, const auto &b ) {\n            return static_cast<long long>(\n                       std::chrono::duration_cast<std::chrono::microseconds>( b - a ).count() );\n        };\n        const long long total_us = us( start, done );\n        if( total_us >= 50000 ) {\n            record_look_profiler_sample( string_format(\n                                            "LOOK2_MAP_CACHE z=%d levels=%d total_us=%lld scene_us=%lld "\n                                            "transparency_us=%lld seen_us=%lld sources_us=%lld "\n                                            "vision_levels_us=%lld lighting_us=%lld dirty_seen=%d",\n                                            zlev, maxz - minz + 1, total_us, us( start, scene_done ),\n                                            us( scene_done, transparency_done ), us( transparency_done, seen_done ),\n                                            us( seen_done, sources_done ), us( sources_done, levels_done ),\n                                            us( levels_done, done ), static_cast<int>( seen_cache_dirty ) ) );\n        }\n    }\n}\n\nlevel_cache &map::access_cache',"cache profile end")
    return s

edit("src/iuse_actor.cpp", iuse)
edit("src/dialogue.cpp", dialogue)
edit("src/map_cache.cpp", cache)
print("All reviewed NVDA features ported into upstream's reorganized source files.")
