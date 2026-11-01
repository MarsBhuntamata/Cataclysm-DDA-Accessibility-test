# Accessibility Prototype Test Plan

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
