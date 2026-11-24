#pragma once
#ifndef CATA_SRC_SCENE_RADAR_H
#define CATA_SRC_SCENE_RADAR_H

#include <array>
#include <string>
#include <vector>

#include "coordinates.h"

class avatar;

/**
 * Surroundings radar: spoken notices when what the avatar can see changes.
 *
 * After each player action the visible surroundings are gathered into a plain
 * snapshot and compared with the previous one. Doors, windows and stairs
 * coming into view, the way opening up or closing in along a direction, and
 * the visible floor area growing or shrinking are turned into short notices.
 *
 * Everything is keyed by absolute map position, so a door is only "new" when
 * its tile was not in the previous snapshot, however the avatar has moved.
 *
 * Gathering, comparing and announcing are separate so the comparison can be
 * unit tested with hand-built snapshots. Only tiles the avatar can see right
 * now are reported.
 */

enum class radar_kind {
    door,
    window,
    stairs
};

/** A door, window, flight of stairs, ladder or ramp in view. */
struct radar_feature {
    tripoint_abs_ms pos;
    radar_kind kind = radar_kind::door;
    // The game's displayed name: "closed wooden door".
    std::string name;
    // Generic name: "door", "window", "stairs up".
    std::string short_name;
};

/** What lies in one of the eight directions, 0-7 clockwise from north. */
struct radar_probe {
    // True when an impassable tile ended the probe; tile and name describe it.
    bool blocked = false;
    // Distance to the blocking tile, or the number of open tiles seen.
    int distance = 0;
    tripoint_abs_ms tile;
    std::string name;
};

/** A kind of hazard in view, such as fire. Position is that of the nearest. */
struct radar_hazard {
    std::string name;
    tripoint_abs_ms pos;
};

struct radar_snapshot {
    tripoint_abs_ms origin;
    bool outdoors = false;
    // True when darkness, not walls, is what limits the view.
    bool sight_reduced = false;
    // Visible walkable tiles within range.
    int open_tiles = 0;
    std::array<radar_probe, 8> probes;
    std::vector<radar_feature> features;
    std::vector<radar_hazard> hazards;
};

enum class radar_outdoors {
    all,
    exits_only,
    off
};

/** One field per option on the Accessibility options page. */
struct radar_settings {
    bool doors = true;
    bool windows = true;
    bool stairs = true;
    bool walls = true;
    bool area = true;
    bool hazards = false;
    bool announce_entering = true;
    bool announce_leaving = false;
    int range = 12;
    int wall_threshold = 3;
    bool eight_directions = false;
    bool full_names = false;
    int max_items = 3;
    radar_outdoors outdoors = radar_outdoors::exits_only;
};

/** Scan what the avatar can see within `range` tiles on its z-level. */
radar_snapshot gather_radar( const avatar &ava, int range );

/**
 * Notices for the change from `before` to `now`, most important first.
 * Uncapped; see render_radar. Empty when nothing worth saying changed.
 */
std::vector<std::string> diff_radar( const radar_snapshot &before, const radar_snapshot &now,
                                     const radar_settings &settings );

/** Join notices into one utterance, replacing those past `max_items` with "N more." */
std::string render_radar( const std::vector<std::string> &notices, int max_items );

/** Read the radar options. */
radar_settings radar_settings_from_options();

/**
 * Called once per player action. Gathers, compares with the last snapshot,
 * speaks any notices, and remembers the new snapshot. Does nothing but forget
 * the last snapshot when the radar option is off. When `travelling` is true
 * and the travel option is off, the snapshot is updated without speaking.
 */
void radar_update( const avatar &ava, bool travelling );

/** Forget the last snapshot, so the next update is silent. */
void radar_reset();

#endif // CATA_SRC_SCENE_RADAR_H
