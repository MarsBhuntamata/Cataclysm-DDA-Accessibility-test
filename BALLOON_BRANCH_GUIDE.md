# Balloon CDDA branch guide

## Build target

**Cumulative test build:** `balloon-nvda-combined-fixes-2026-11-15`.

**Stable known-good:** `balloon-accessibility-framework`. Do not change this until the cumulative build is successfully compiled and tested.

GitHub Actions: `NVDA accessibility test build` → `Run workflow` → select the cumulative branch.

## Cumulative test branch contains

- Stable framework fixes, including tested lighthouse/look-mode performance optimization (`78b30c7`).
- Vehicle look-mode parts, vehicle repair/install/remove/maintenance narration.
- Shift+P message-log narration.
- Computer terminal login/security/output narration.
- Overmap active-objective direction/distance on opening and F8.
- Remembered Vehicles tab; **currently loaded overmaps only**.

Source code is committed, but compilation and NVDA runtime testing remain necessary.

## Historical and experimental branches

The `balloon-vehicle-*-test`, `balloon-computer-terminal-nvda-test`, and `balloon-framework-*-test` branches are historical snapshots, **not** current cumulative builds. Do not build those when testing the latest work.

`balloon-upstream-integration-2026-11-14` is a separate, significantly diverged upstream integration experiment.

`balloon-randomizer-total-conversion-guard-test` includes an experimental `src/worldfactory.cpp` guard that is **not yet incorporated** into the cumulative branch. Review and test before integration.

## Going forward

New accessibility fixes belong on the cumulative branch. Any temporary experimental branch must be reconciled back into the cumulative branch after review, without altering the stable known-good branch until successful testing. Update this guide when the build branch changes.
