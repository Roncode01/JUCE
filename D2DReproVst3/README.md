# D2D Repro VST3 -- v6 (v1 occlusion fix + reentrant-teardown fix, combined)

Combines the two independent fixes from this investigation into one build, using the occlusion
fix now believed to actually work (v1) instead of the standby design (v2/v3) that turned out to
regress on screensaver/lock-screen.

## Why v1, not v2/v3

v3 (`direct2d-occlusion-standby-v3.patch`) tested clean initially, then a later test showed the
same screensaver/lock-screen flicker returning, with or without the separate teardown patch.
Isolating further: v2 (`direct2d-occlusion-standby.patch`, the never-before-built predecessor to
v3) flickered too, tested completely alone -- no diagnostic patch, no teardown patch -- which
ruled both of those out and implicated the standby design itself (the `occluded` flag +
`pollForRecovery()` polling loop, shared by v2 and v3). v1
(`direct2d-occlusion-didpresent.patch`) -- the patch actually deployed in the real plugin
(StereoSplitBandPhaseAdjuster 1.20.0), which has run clean there -- was the next, much simpler
alternative to test alone, and is what this build uses.

An independent code review of v1 vs. v2/v3 pinpointed a specific, credible defect in the standby
design's recovery path: `pollForRecovery()` sets `didWait = true` the moment its test-probe
confirms recovery, which bypasses the real frame-latency wait-object check (`canPresent()`'s
normal gate) for the very next real frame -- right at the lock/unlock transition, which matches
exactly where the flicker shows up. v1 never introduces that shortcut; it just resets the
present-throttle unconditionally after every successful present, occluded or not, so the normal
wait-object check always runs. See the chat history for the full writeup.

## What's in this build

1. **`direct2d-frame-latency-wait.patch`** -- the base patch (mixed-refresh-rate fix). Unchanged
   from every other build in this investigation.
2. **`direct2d-occlusion-didpresent.patch`** -- "v1". Byte-for-byte the same file already shipped
   and confirmed clean in the real plugin's `v1_20_0/patches/` folder.
3. **`direct2d-reentrant-teardown-fix-v1.patch`** -- the Stage 2 fix (the monitor-unplug/display-
   reshuffle crash, and the "ghost window" outline that could outlive even the host process).
   This is a mechanical rebuild of the same fix delivered in the v5 package, now based on top of
   v1's occlusion patch instead of v3's. The code change itself is unchanged -- confirmed by
   diffing just the added lines against the original: identical except for one comment sentence
   referencing a compile error that doesn't apply to this rebuild (that error was specific to an
   earlier mistake in how the fix was first written, already corrected, and not something this
   rebuild repeated). Everything else -- the reentrancy guard in
   `Direct2DGraphicsContext::Pimpl`, the explicit composition-teardown sequence in
   `CompositionTree`'s destructor, the guard usage in `HwndPimpl` -- is identical in substance.
   It's functionally independent of the occlusion fix regardless -- different functions,
   different classes in different modules, no shared state -- so rebasing it was purely a
   mechanical line-number exercise, not a logic change. Still named "v1" accordingly (per the
   earlier agreement: it only becomes "v2" if the fix itself changes).

## Verification so far (2026-10-12)

- All three patches apply cleanly in sequence to a fresh, independent JUCE 9.0.3 checkout
  (`git apply` against a separate clone, same method as every build in this project).
- Confirmed in the actually-patched source: `didPresent()`/`canPresent()` are back to the simple
  v1 shape (no `occluded` flag, no `pollForRecovery()`); the reentrancy guard
  (`enterReentrancyGuard`/`leaveReentrancyGuard`/`ScopedReentrancyGuard`) and the
  `CompositionTree` destructor are present and correctly wired.
- Verified through a real CMake `FetchContent` configure (fetches JUCE 9.0.3 over the network and
  runs all three patches through its own populate+patch step).
- `Source/` is your own D2DReproVst3 app code, copied as-is from your repo -- unmodified.
- **Not compiled** (no Windows environment here) and **not run against the real repro** -- same
  standing limitation as every build in this investigation. Your build is still the first real
  compile and the only real test.

## What this does NOT fix

Unchanged from the v5 README -- the reentrant-teardown fix never touched this:

- **GPU device-removed/driver crash** is still not handled at the patch level (ruled out of scope
  earlier in this investigation -- no swap-chain/device recreation code was wanted).
- **The adapter rebuild is still wholesale, not differential** -- unplugging one monitor in a
  dual-monitor setup still tears down and rebuilds every window's Direct2D state.
- **A factory-creation failure during the rebuild** can still leave the adapter list empty for a
  while, though it's self-healing on the next display-settings event.

## How to test

1. Build and install exactly like the other D2DReproVst3 builds -- same `PRODUCT_NAME`, so it
   replaces whichever one is currently installed.
2. Screensaver / lock-screen / hibernate, same scenario that regressed under v2 and v3 -- this is
   the main thing to confirm is actually clean this time.
3. Monitor unplug / dual-monitor reshuffle -- the crash/freeze and ghost-window scenario the
   reentrant-teardown fix targets, same as the v5 test.
