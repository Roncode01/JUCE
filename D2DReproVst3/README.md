# D2D Repro VST3 -- v2 occlusion-standby test (minimal, no diagnostic, no teardown fix)

A deliberately stripped-down build, to isolate one specific question raised by the regression
report: the v3 build (`direct2d-occlusion-standby-v3.patch`, used in all the other D2DReproVst3
packages) showed the screensaver/lock-screen flicker you'd previously confirmed fixed coming
back -- and it came back whether or not the separate `direct2d-reentrant-teardown-fix-v1.patch`
was also applied.

This build keeps only the two patches below. No diagnostic-logging patch, no teardown/
reentrancy-guard patch:

1. `direct2d-frame-latency-wait.patch` -- the base patch. Unchanged from every other build.
2. `direct2d-occlusion-standby.patch` -- **this is "v2"**, not the same file as
   `direct2d-occlusion-standby-v3.patch` used elsewhere. It's the corrected-but-never-built
   revision from the chat history: an explicit `occluded`/`pollForRecovery()` design (same shape
   as v3), written up as analysis only and never actually wired into a D2DReproVst3 build or
   tested until now.

## Why this specific combination

`direct2d-occlusion-didpresent.patch` -- call it v1 -- is what's actually deployed in the real
plugin (StereoSplitBandPhaseAdjuster 1.20.0), and it does not flicker there on lock/screensaver.
v2 and v3 both superseded v1 because v1 had a theoretical flaw (treating any non-`FAILED` result,
including `DXGI_STATUS_OCCLUDED`, as "safely consumed" -- risking a frame-latency wait that never
gets satisfied). v2 and v3 are functionally identical to each other for a plain occlusion
transition: both gate real presents on a literal `hr == S_OK` and poll via
`Present1(0, DXGI_PRESENT_TEST, ...)` while occluded. The only difference is v3's extra handling
for a genuine `FAILED(hr)` seen *during* that poll (v2 just stays in standby forever on any
non-`S_OK` code, including a real failure; v3 clears the occluded flag on a real failure so the
next real present's own existing failure-handling runs instead of polling forever) -- a case a
plain lock/unlock shouldn't ever hit, since nothing's actually failing.

So this build should behave identically to the v3 build for this scenario. What differs is
everything *around* it: no diagnostic patch's extra per-frame `DXGI_PRESENT_TEST` probe, no
reentrancy-guard code wrapping `prepare()`/`startFrame()`/`finishFrame()`, no composition-
teardown destructor. If this build:

- **Still flickers** -- that implicates the occlusion-standby design itself (shared by v2 and
  v3), independent of the diagnostic or teardown patches or anything about how they interact
  with it. Worth then comparing carefully against v1's simpler logic to see what actually
  differs in practice, not just on paper.
- **Doesn't flicker** -- that points at something in the diagnostic patch, the teardown patch,
  or an interaction between either of them and the standby logic -- worth revisiting with that
  narrowed down, rather than the standby design itself.

## Verification so far (2026-10-12)

- Both patches apply cleanly in sequence to a fresh JUCE 9.0.3 checkout (confirmed previously,
  2026-10-10, and re-confirmed here).
- Verified through a real CMake `FetchContent` configure (fetches JUCE 9.0.3 over the network
  and runs both patches through its own populate+patch step).
- `Source/` here is your own D2DReproVst3 app code, copied as-is from your repo -- unmodified.
- **Not compiled** (no Windows environment here) and **not run against the real repro** -- same
  standing limitation as every build in this project. This is purely to get a clean, minimal
  binary in your hands to test against the lock-screen/screensaver scenario.

## How to test

1. Build and install exactly like the other D2DReproVst3 builds -- same `PRODUCT_NAME`, so it
   replaces whichever one is currently installed.
2. Lock the screen / trigger the screensaver, same as the test that showed the regression.
3. Note whether it flickers. If you still have the diagnostic patch's log file from a previous
   build, a fresh `%TEMP%\d2d_present_diagnostic.log` won't be produced by this build (no
   diagnostic patch here) -- so this round is a plain visual yes/no, not a log comparison.
