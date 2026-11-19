# Prism routing experiment

This branch was created directly from `balloon-accessibility-framework`, independently of the Tolk test branch.

## What changes

- All existing game UI, item details, crafting, map and message narration stays in our established accessibility framework.
- Direct NVDA Controller remains the first and preferred output route.
- If NVDA is unavailable, Prism's *explicit* non-NVDA screen-reader adapters are tried. The experiment never calls `prism_registry_create_best()`; it does not select NVDA twice or automatically switch to SAPI/OneCore.
- One backend is attached to the narration service at a time.
- Prism v0.18.4 Windows x64 runtime is downloaded in the branch's GitHub Actions test package. No change to CDDA's C++ compiler standard or build dependencies is required for this runtime-loaded adapter.

## Test against Tolk

Run the two builds separately, from these branches:

- `balloon-prism-routing-test`
- `balloon-screenreader-routing-test`

Verify with NVDA alone, JAWS alone, simultaneous NVDA/JAWS, and no running screen reader. Watch for doubled lines, interruptions, braille availability, and startup/shutdown issues. Inspect `nvda-accessibility-diagnostics.txt` beside `cataclysm-tiles.exe` for backend selection.

Prism also offers speech-engine backends; we intentionally exclude these from this experiment to avoid unwanted synthesized output. The runtime is loaded only when direct NVDA initialization has failed.

**Known limitations:** Selecting the backend occurs at startup; switching screen readers during a game session does not automatically rebind output. The code has not been tested on a real JAWS installation. Prism's Windows runtime packaging may require additional dependencies or plugin modules, so a successful GitHub build does not guarantee successful operation on every reader. Do not merge into the working branch before testing.

Source: https://github.com/ethindp/prism (MPL-2.0), pinned release v0.18.4.
