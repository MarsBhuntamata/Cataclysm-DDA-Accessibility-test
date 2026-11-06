# NVDA accessibility guide for Cataclysm: Dark Days Ahead

This fork aims to make the ordinary CDDA game accessible through NVDA. It does **not** automatically explore, fight, or make survival decisions for the player.

## Opening this guide

- **F12**: Read the essential accessibility controls aloud while playing. The spoken guide is available in the normal gameplay screen.
- **?**: Open CDDA's ordinary help or keybinding controls. The in-game keybindings menu is the authority if you have remapped keys.

## Exploring the world

- **Shift+V**: Open the list of nearby monsters, items, terrain, and furniture. This is the key surroundings scanner; use the list's controls to browse the categories.
- **x** (lowercase): Open the look-around cursor. Move the cursor with directional keys to examine visible or otherwise legitimately known tiles.
- **Shift+X**: Peek around a corner. Select a direction and use the look-around cursor from the temporary peeking viewpoint. Peeking costs moves. The dedicated NVDA explanation of the peeking transition is still incomplete.
- **Ctrl+L** or **F8**: Describe the eight tiles adjacent to your character.
- **F6**: Describe the tile underfoot.
- **F11**: Toggle automatic announcement when the terrain underfoot changes. Off by default to avoid needless narration.

## Death and game-over narration

- The final tombstone screen announces itself and explains that Enter continues. If a last-words input is present, it can be edited before continuing.
- The game-over flow announces final messages, scores, epilogue checks, and completion. The scores window narrates its tab name and up to twelve entries when entering the Achievements, Conducts, or Scores tabs; the Kills tab announces totals. Use left and right arrows to change tabs and Escape to continue.
- The main menu retains its own spoken entry and focused selection after the game returns to it. These changes need an NVDA playtest to verify timing and focus.

## Loading saved characters and finding Abandon Character

- **Load Game → choose a world → choose a character**: selecting a saved character loads it directly. There is no custom character Delete/archive action; no `.deleted` files are created by the accessibility load menu.
- **Abandon Character** now always appears in the gameplay action menu even without a keyboard shortcut. It is the game's original death/abandon action, not the safe character-removal option.
- **View Help** announces its help topic shortcuts, then narrates the chosen help article.
- **Sidebar Options** announces focused panel or layout, including whether a panel is enabled. Tab toggles a panel, Enter or S begins/ends moving a panel or selects a layout, and left/right switches columns.

## Random world and character selection

- **Shift+R in the world picker during character creation**: Choose one existing, eligible world randomly. Tutorial and defense worlds are excluded by the ordinary picker. If there is only one eligible world, CDDA selects it automatically; with none, the ordinary create-world flow applies.
- **Shift+R on the main menu under Load Game**: Pick a random saved character from any normal world, announce the chosen character and world, and ask for confirmation before loading.
- **Random saved character** is also a selectable entry at the bottom of an individual world's Load Character list. This chooses only among that world's saved characters, and asks before loading.
- These commands select existing worlds or characters. They do not create new random worlds, delete saves, or change a character's progress.

## World mod selection

- **Shift+R** in the world mod manager: Propose a random selection of two to five optional mods. The selected core mod is preserved; the game attempts to resolve dependencies and avoid declared conflicts.
- NVDA announces the proposed mod list and asks for confirmation **before** replacing your existing selection. Declining leaves it unchanged.
- The mod manager now reads each mod's description, dependencies and declared conflicts when browsing. Authors may not declare every incompatibility. For major total conversions, consider separate worlds.
- This feature changes only the world's mod selection during setup. It does not explore or play the game automatically.

## Status and messages

- **Ctrl+S** or **F9**: Speak character status.
- **Ctrl+R** or **F10**: Speak the last five game messages.
- **F7**: Repeat the previous NVDA announcement.

## Important distinctions

**Shift+V** is the surroundings list; lowercase **v** is not the same command. **x** and **Shift+X** are different: looking around and peeking around a corner, respectively. **Ctrl+X** is not the accessibility look command. Capital letters in CDDA often represent Shift-modified commands; this guide explicitly writes out Shift when it matters.

Descriptions must respect the game's visibility and knowledge rules. Unexplored or unseen terrain should not be revealed through narration. Normal CDDA conveniences such as its existing auto-travel remain part of the game, but this fork does not add automatic survival assistance.

## Testing and reporting a freeze

If a tester encounters a hang or silent interface, please report the build version or commit, the steps taken, whether the whole game stopped or only NVDA speech, and whether NVDA was running. The diagnostic file **nvda-accessibility-diagnostics.txt** is beside **cataclysm-tiles.exe**. Copy it **before restarting the game**, because it is overwritten at startup. If present, **debug.log** or **crash.log** may also be helpful.

The guide describes the default bindings for this accessibility branch. Keyboard remaps and future game changes may alter them.
