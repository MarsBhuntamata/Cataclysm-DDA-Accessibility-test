# Screen-reader routing test balloon

This experimental branch is based on `balloon-accessibility-framework`. It does **not** merge any old Tolk UI narration. All item, crafting, and map announcements still come from the established framework.

## Routing

1. Use the existing NVDA Controller backend if NVDA is available.
2. Otherwise, try loading `Tolk.dll` from beside `cataclysm-tiles.exe`.
3. Explicitly disable Tolk SAPI fallback and reject Tolk's NVDA driver.
4. If neither works, preserve announcements in the narration service but do not synthesize speech.

Only one backend is attached to the narration service.

## Test build

See GitHub Actions, **Screen reader routing test build**. Download the `cdda-screenreader-routing-test` artifact if compilation succeeds. The workflow attempts to include an x64 `Tolk.dll` from the published Tolk release; check that it actually exists in the artifact for non-NVDA testing. If it is absent, place a separately obtained compatible x64 `Tolk.dll` alongside the executable, with its applicable license and any required screen-reader API dependencies.

Check the `nvda-accessibility-diagnostics.txt` next to the executable for the selected backend.

### Test matrix

- NVDA running: speech and braille still work; no Tolk speech duplicates.
- JAWS (or another Tolk-supported reader) with NVDA stopped: game speech works through Tolk.
- Both screen readers running: exactly one backend active, NVDA preferred.
- Neither running: no unexpected SAPI voice.
- Verify long item descriptions, crafting recipe navigation, popup speech, repeating, interruption, and shutdown.

Tolk is not thread-safe and may require screen-reader-specific DLLs or COM setup. A successful Windows build does not itself demonstrate JAWS compatibility. Do not merge this into the working branch until it has been tested with the target screen reader.
