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
