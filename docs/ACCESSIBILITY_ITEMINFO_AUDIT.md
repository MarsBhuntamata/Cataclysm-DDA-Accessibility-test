# Accessibility Audit: First Item-Information Vertical Slice

This audit records the source paths examined on the `balloon-accessibility-framework` branch. It is deliberately limited to observed source behavior; it does not claim to reproduce the unavailable source of the separate accessible executable.

## Confirmed inventory path

The inventory selector registers an `EXAMINE` action. When received, it obtains the highlighted item's `item_location` and calls `inventory_selector::action_examine()`.

That function:
1. Creates a vector of `iteminfo` entries.
2. Calls `sitem->info( true, vThisItem )`.
3. Prepends a location description.
4. Creates `item_info_data` containing the item name, type name, and structured info entries.
5. Enables scrolling.
6. Opens an `iteminfo_window` and calls `execute()`.

The inventory examiner has a separate details path. It calls the same item information API, builds `item_info_data`, and draws it through `draw_item_info()`. Therefore, fixing only the selector's examine popup would not cover every item-details presentation.

## Existing structured item content

The `iteminfo` structure in `item.h` already contains semantic-ish fields including:
- `sType`: information category
- `sName`: property label or description text
- `sFmt`: formatting text
- `sValue`: display value
- `dValue` and `dUnitAdjustedVal`: numeric values for comparisons
- `bNewLine`, `bDrawName`, `bIsArt`, and `isTable`: presentation hints

The item API exposes `get_info()` and `info()` overloads that return or populate vectors of these entries. This is a better starting point for speech than extracting text from the rendered window.

Important limitation: the current `iteminfo` structure mixes content, display formatting, and comparison metadata. A first implementation should adapt it into a speech-friendly representation without immediately changing every producer of item information.

## Presentation paths

- `iteminfo_window::draw_controls()` draws the item name/type and delegates the body to `display_item_info()`.
- `iteminfo_window::execute()` redraws the UI and waits for input, handling scrolling and dismissal.
- `display_item_info()` renders the ImGui version of item details.
- `format_item_info()` creates a text buffer from the same `iteminfo` entries for the curses renderer.
- `draw_item_info()` folds and draws that formatted buffer into a curses window.

The speech adapter should consume structured item data before either visual renderer runs. It should not call `format_item_info()` and assume that renderer-oriented formatting is a stable accessibility contract.

## Existing accessibility setting

`SCREEN_READER_MODE` is currently described in `options.cpp` as a display optimization for supported screens, targeted toward Orca using curses. Its description does not represent a complete speech-service setting or a Windows tiles screen-reader integration.

Do not overload this option into the new architecture without deciding how to preserve its existing meaning. The new service should have a clear, independently named setting group or a deliberate migration plan.

## Build observations

The project uses a CMake source glob for `src/*.cpp` and `src/*.h`, so a new source/header pair in that directory is automatically included in the source lists. The tiles target defines `TILES`; Windows-specific system libraries are linked in the existing Windows target configuration. Any Windows speech backend still needs appropriate platform guards and explicit dependency/link decisions.

## Immediate implementation recommendation

The first code change should be a small, backend-neutral narration model and service boundary, plus a test/no-op backend. Then connect the inventory `EXAMINE` path to submit one item-description event. Keep actual Windows speech backend implementation as a separate next step after confirming the desired backend APIs and packaging strategy.

This avoids embedding DLL loading, COM calls, or screen-reader-specific behavior inside inventory UI code.
