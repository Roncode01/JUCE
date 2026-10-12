# D2D Repro VST3 -- v1 occlusion fix test (direct2d-occlusion-didpresent.patch)

Both later revisions of the occlusion fix -- "v2" (`direct2d-occlusion-standby.patch`) and "v3"
(`direct2d-occlusion-standby-v3.patch`) -- have now been built against D2DReproVst3 and **both
flickered** on screensaver/lock-screen, even though v3 had earlier tested clean and v2 had never
been tested before. v2 and v3 share the same design: an explicit `occluded` flag, with
`canPresent()` stopping real presents entirely while occluded and polling recovery via a cheap
`DXGI_PRESENT_TEST` call instead.

v1 -- `direct2d-occlusion-didpresent.patch` -- doesn't share any of that design, and it's the
one actually deployed in the real plugin (StereoSplitBandPhaseAdjuster 1.20.0), where it does
not flicker on lock/screensaver. It has never been built against D2DReproVst3 before now.

## What v1 actually does

Much simpler than v2/v3: no standby state, no polling, no occlusion flag. It keeps attempting a
real present every frame, exactly as stock JUCE already did. The only change: `didPresent()`
(which just resets the present-throttle flag, `didWait = false`) is now called whenever
`Present1` returns *any* success code, not only a literal `S_OK`. The original code checked
`hr == S_OK` specifically, which left the throttle flag stuck `true` for the entire occluded
period (since `canPresent()` short-circuits on that flag without waiting) -- silently
re-opening the original un-throttled race this patch set out to fix in the first place, until
the editor was closed and reopened.

## Why build this

To isolate one variable at a time. v1 and v2/v3 differ in exactly the respect described above.
If this build:

- **Doesn't flicker** -- that confirms the standby/polling design itself (shared by v2 and v3)
  is what's causing the regression in D2DReproVst3, and the simpler always-present approach
  (already proven in the real plugin) is the way to go for this repro too.
- **Still flickers** -- that would mean something about D2DReproVst3 itself (as opposed to the
  real plugin) produces this flicker regardless of which occlusion patch is applied, which is a
  different and more fundamental finding -- it would mean the repro app isn't equivalent to the
  real plugin for this specific test, and the next step would be figuring out what differs
  between them (hosting context, window creation path, timer-driven paint cadence, etc.)
  rather than continuing to compare patch revisions.

## Chain

1. `direct2d-frame-latency-wait.patch` -- the base patch. Unchanged from every other build.
2. `direct2d-occlusion-didpresent.patch` -- byte-for-byte the same file already shipped in the
   real plugin's `v1_20_0/patches/` folder. Confirmed identical before packaging here.

## Verification so far (2026-10-12)

- Confirmed `direct2d-occlusion-didpresent.patch` here is byte-identical to the one in the
  `v1_20_0` plugin build that's already deployed and confirmed not flickering.
- Both patches apply cleanly in sequence to a fresh JUCE 9.0.3 checkout.
- Verified through a real CMake `FetchContent` configure (fetches JUCE 9.0.3 over the network
  and runs both patches through its own populate+patch step).
- `Source/` is your own D2DReproVst3 app code, copied as-is from your repo -- unmodified, same
  as every other D2DReproVst3 build.
- **Not compiled** (no Windows environment here) and **not run against the real repro**, same
  standing limitation as everything in this investigation.

## How to test

1. Build and install exactly like the other D2DReproVst3 builds -- same `PRODUCT_NAME`, so it
   replaces whichever one is currently installed.
2. Lock the screen / trigger the screensaver.
3. Note whether it flickers.
