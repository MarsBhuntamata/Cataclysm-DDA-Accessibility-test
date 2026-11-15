# Balloon CDDA: Two main branches

## 1. Working accessibility branch
`balloon-accessibility-framework`

This is **the** working branch for all NVDA fixes. New accessibility changes are built here. The latest set of cumulative fixes includes:
- lighthouse/look-mode performance optimization (`78b30c7`)
- NVDA vehicle parts in look mode and repair/install/remove/maintenance narration
- Shift+P message log narration
- computer terminal login/security/output narration
- active objective direction and distance on overmap opening and F8
- Remembered Vehicles tab (currently loaded overmaps only)

Note: recent additions may still need Windows compilation and in-game NVDA verification. Older verified commits remain in Git history for rollback.

## 2. Upstream-integrated accessibility branch
`balloon-upstream-integration-2026-11-14`

This branch combines newer CleverRaven CDDA upstream changes **and** our full NVDA working history. The sync workflow `.github/workflows/sync-balloon-working-to-upstream.yml` attempts a normal Git merge when the working branch is pushed, stopping on conflicts instead of silently discarding upstream or accessibility changes. **Verify the sync workflow completed successfully** before assuming this branch includes the very latest working fixes.

Build via the GitHub Actions workflow `NVDA upstream pre-test build`.

## Other branches

All `balloon-*-test` and `balloon-nvda-combined-fixes-2026-11-15` branches are temporary or historical snapshots, not primary build targets.

The experimental randomizer total-conversion guard on `balloon-randomizer-total-conversion-guard-test` has not yet been integrated into working and remains a separate untested candidate.

## Rules
1. Two permanent development targets only: working and upstream-integrated.
2. Every accepted accessibility change lands in working first and is then merged into upstream.
3. Never replace whole upstream source files with older working copies merely to settle conflicts.
4. Keep rollback through Git history; don't discard fixes when upstream changes.
5. A failed merge or build is reported, never presented as successful.
