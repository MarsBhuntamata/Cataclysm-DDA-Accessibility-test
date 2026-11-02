# Accessibility Prototype Test Plan


## Keyboard-first gameplay review shortcuts

These are bound only in DEFAULTMODE, to avoid hijacking text-entry and NPC dialogue shortcuts:

- **Ctrl+L**: Survey the eight neighboring tiles. Also F8.
- **Ctrl+S**: Read current character condition (stamina, pain, hunger, thirst, and hit points). Also F9.
- **Ctrl+R**: Read the last five game messages. Also F10.

**F6** still describes terrain underfoot, **F7** repeats the last announcement, and **F11** toggles automatic terrain-change speech. Existing Ctrl+R in dialogue and Ctrl+S in the string editor are retained in their respective contexts. Ctrl+Shift combinations are deliberately not used.

Test all three Ctrl shortcuts while standing in the game world, then check that NPC dialogue and text input still behave normally. If custom user keybindings override the defaults, the new bindings may need to be restored in the keybindings menu.

## Expanded map and gameplay narration milestone

These keys are available in the regular game world, without spending a turn:

- **F6**: Describe terrain beneath the player.
- **F7**: Repeat the most recent speech (also in supported menus and look-around).
- **F8**: Survey the eight immediately adjacent tiles, clockwise from north. Include visible creatures and ground-item presence without revealing unseen tiles.
- **F9**: Read stamina, pain, hunger, thirst, and minimum/maximum body-part hit points (raw game values).
- **F10**: Read up to five recent gameplay messages, oldest first.
- **F11**: Toggle automatic terrain-change speech while walking. Off by default; only speaks when terrain names change.
- **Look around**: Move the usual look cursor; NVDA should read the relative direction/distance, visible terrain, creatures, and items. The cursor does not consume a turn.

If the function row controls hardware, use Fn plus the corresponding function key. Start from a fresh extracted package or ensure customized keybindings do not override these defaults.

### Additional regression checks

- Verify known-visible vs unseen tiles in the F8 survey and look cursor.
- Confirm look-around speaks only on cursor change and F7 repeats it.
- Check F9 in injured and healthy characters. Numbers are raw game data, not diagnoses.
- Confirm F10 reports actual recent messages and does not advance turns.
- Toggle F11 on, move between different terrain, then turn off and confirm silence on movement.
- Verify NPC dialogues, input popups, menus, inventory, and clean window close still work.

## Build checks

- Build the Windows x64 tiles target from the `balloon-accessibility-framework` branch.
- Build the tiles test target.
- Run the Catch tests tagged `[accessibility]`.
- Confirm that non-Windows builds still compile. The NVDA implementation is guarded by `_WIN32`.

## NVDA setup

- Place the matching-architecture NVDA Controller Client DLL in the same directory as `cataclysm-tiles.exe`.
- The loader checks for `nvdaControllerClient.dll` first, then the legacy architecture-suffixed filename.
- Start NVDA before or after launching the game. The backend checks whether NVDA is running when each request is sent.

## Inventory item-description test

1. Start NVDA.
2. Launch the Windows tiles build.
3. Open the inventory and move to an item with a non-empty description.
4. Use the inventory's Examine action.
5. Confirm that NVDA announces the item name, type when distinct, inventory location, and available item information.
6. Press F7 while the item-information window is open. Confirm that the last announcement is repeated and interrupts speech already in progress.
7. Close the item-information window and press F7 again from the inventory. Confirm that the same last announcement can be repeated there too.
8. Confirm that normal inventory interaction continues.

## Failure and regression checks

- Launch without the NVDA DLL. The game should continue without crashing; narration is unavailable.
- Launch with the DLL present but NVDA not running. The game should continue; speech requests should be safely ignored until NVDA is available.
- Confirm that item-information visual rendering and scrolling remain unchanged.
- Confirm that Ctrl+R retains its existing debug-dialogue binding. Repeat narration uses F7 to avoid that conflict.
- In gameplay, press F6 to describe the terrain underfoot. F7 repeats the last announcement.
- If the laptop uses media-key mode, try Fn+F6 and Fn+F7.

## Not covered by this prototype

- Automatic braille updates.
- System Access and SAPI backends.
- Narration for item selection changes outside the Examine window.
- Scroll-position announcements and focus restoration.
- Managed speech queues, verbosity controls, and event-specific announcement policies.


## Character, crafting and vehicle panels (Shift+number keys)

- **Shift+2**: Open character information. Check arrow navigation through the Stats, Encumbrance, Speed, Skills, Traits, Bionics, Effects and Proficiencies tabs. NVDA should read the focused row with numbers and explanatory text. F7 repeats the current announcement.
- **Shift+7**: Open crafting. Check that selecting recipes announces the recipe name, list position and craftability. Check switching categories, filtering, and batch mode. F7 repeats.
- **Vehicle interaction**: Test the vehicle editor with a nearby vehicle, then move its part cursor and navigate its part overview lists. NVDA should identify the vehicle, selected part and mount location, and pronounce part names in the overview. F7 repeats.
- **Other Shift+number actions**: Shift+1 safe mode, Shift+3 factions, Shift+4 sleep, Shift+5 item-action menu, Shift+6 vehicle controls, Shift+8 construction, Shift+9 disassembly, and Shift+0 diary should retain their existing bindings. Some still use custom UIs that need further targeted speech coverage.

This milestone is a first accessible pass on these custom views; it does **not** guarantee full text and requirement navigation within every secondary panel. Keep existing saves backed up before testing a new build.

- **Shift+3**: In the faction screen, move through tabs for camps, followers, other factions, and known creatures. Confirm selected names and faction descriptions are spoken. F7 repeats.
- **Shift+0**: Open the diary. Navigate pages, change entries and page text. Confirm focused entries and explanatory text are spoken. F7 repeats.


## Spoken text polish

- In Look Around (X + arrows), moving one tile west onto a table should speak "1 west. table", with any observed item cue appended. Do not prepend "Look cursor".
- In inventory focus and item descriptions, a name such as `<color_c_green>++</color> animal boxer shorts` should speak "green animal boxer shorts". The display remains unmodified.
- Check light-green and red durability indicators, plain names, and names with regular color tags. F7 must repeat the cleaned-up wording.


## Loading screens and character creation readiness

- Starting a save load should announce **Loading** and the displayed random loading **Tip**. Only one announcement is issued per loading session, not for each rapidly changing substep.
- At the actual transition into gameplay, NVDA should announce **Game ready** after both an existing save and a new game. Do not announce ready on a failed load or during initial world setup.
- Starting Custom Character or Random Character should announce **Character creation ready**, the active section (Scenario or Summary), and its currently selected entry. Tab/Shift+Tab switches character-creation sections. Navigate a list with arrow keys and confirm the selected entry and changed tab are spoken.
- F7 should repeat the most recent character-creation speech, including on the Summary page.
- Verify loading tips are stripped of raw color tags; no repeated progress spam; failure/return-to-main-menu does not falsely announce ready.

## Armor layering, zones and overmap navigation

- **Shift+= / plus**: Open armor layering. NVDA should announce the body-part category, focused worn item, ordering selection, hidden sprite status, layering and available coverage/encumbrance/warmth properties. Use arrows to select items, left/right to switch body part, and F7 to repeat the focused announcement. Other actions must retain their original behavior.
- **Shift+Y**: Open zones. For each focused zone, read its position in the list, name, type, enabled state, distance/direction, personal/vehicle status and custom options. Confirm empty-zone output, edits/reordering and F7 repeat.
- **M**: Open overmap. Hear a brief position offset and terrain identification as the cursor moves. Unseen tiles must report "Unexplored", not underlying map terrain. Existing notes and danger markers should speak. Elevation changes must be reported. F7 repeats; no speech on blinking/timeouts or automated route playback.
- Regression: game movement and world map interactions remain unchanged, no repeated narration on redraw, and no unearned visibility of uncharted terrain.


## Advanced armor, zone editing and overmap reviews

- Armor layering (Shift+=): **F8** reads the selected armor's complete properties, flags and protection rows. **F9** speaks the next detail line and **F10** the previous. Switching clothing or body part resets the detail line. F7 repeats the last announcement.
- Overmap (M): cursor speech remains brief. **F8** reads sidebar details: visible terrain and vision level, coordinates, explored state, notes/danger, available weather, active mission or point of interest, and known vehicles. F7 repeats it. Confirm unseen terrain is *never* revealed.
- Zones (Shift+Y): **F8** reviews exact area corners, enabled state, type and options. Selecting a zone and pressing Enter exposes labeled, described edit actions. During area selection and movement, the look cursor now speaks "First zone corner", "Second zone corner" or "Move zone" with relative direction and visible tile type. F7 repeats.
- Regression checks: no accidental modifications from review keys; entering Zone Manager, Armor Layering or Overmap still uses original keys; other Ctrl review bindings work in gameplay.
