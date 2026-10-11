# D2D Repro VST3 -- v5 (adds the monitor-unplug crash + ghost-window fix)

Stage 2 of the investigation: this build adds a fix for two separate, pre-existing JUCE defects
-- the crash/freeze you hit when unplugging a monitor (or reshuffling windows across monitors),
and the window-shaped "ghost" outline that could persist even after closing the host DAW -- on
top of everything v3 already confirmed working ("no flickering observed" on screensaver/lock/
hibernate). That fix is untouched here.

## What this is fixing

Not the same bug as v3. v3 fixed a flicker caused by `DXGI_STATUS_OCCLUDED` being returned from
`Present1()` -- a clean, documented success code the un-patched throttle logic mishandled. This
is two different failures, both traced to the same root cause by reading JUCE's own
adapter-management code (`DxgiAdapters`, `Direct2DGraphicsContext::Pimpl`,
`Direct2DHwndContext::HwndPimpl` -- all stock, unpatched JUCE 9.0.3, untouched by any of this
investigation's earlier patches):

1. Unplugging a monitor (or any display-topology change) makes Windows send `WM_DISPLAYCHANGE`/
   `WM_SETTINGCHANGE` to top-level windows, which JUCE's window proc turns into a call to
   `DxgiAdapters::updateAdapters()`. That tears down and rebuilds the Direct2D device/swap-chain/
   composition state for every window whose current adapter no longer matches, via
   `adapterCreated()`/`adapterRemoved()` calling `teardown()`.
2. Critically, `WM_DISPLAYCHANGE`/`WM_SETTINGCHANGE` are delivered with `SendMessage` -- a *sent*
   message, not a posted one. If this thread happens to be blocked inside a call that
   synchronises with the DWM -- `IDXGISwapChain::ResizeBuffers`, `IDXGISwapChain1::Present1`,
   `IDCompositionDevice::Commit`, all reachable from one ordinary paint cycle -- Windows can
   deliver that sent message *reentrantly*, nested inside the very call that's still using this
   object's Direct2D/DXGI members.
3. `teardown()` had no guard against this. It nulls out the device context and swap chain with
   no regard for whether a call higher up the same thread's stack is still holding and about to
   use those same members -- a null-pointer dereference. That's the crash.
4. Separately: `teardown()` also only ever dropped this process's own COM references to the
   DirectComposition visual/target, with no explicit "detach content, then commit the empty
   state" step first. `CreateTargetForHwnd`'s target isn't purely an app-side object -- it's tied
   to the window at the OS/compositor level -- so simply releasing references doesn't guarantee
   the DWM stops showing the last frame it was told to display. An incomplete teardown (made
   more likely by the same reentrancy above) could leave a static outline on screen that outlives
   the window and even the process, clearing only once Windows itself restarts.

Both would happen identically in a completely unpatched JUCE 9.0.3 app, with none of this
investigation's other patches applied. I checked whether either was already a known, fixed JUCE
bug; neither is. There's one related-but-different, already-fixed 2024 bug (an unchecked null
DXGI factory -- see https://forum.juce.com/t/br-decent-number-of-crashes-in-vblankdispatcher-reconfiguredisplays-on-customer-machines/60917),
already present as a null check in the 9.0.3 source. Both of these are separate from that and,
as far as I could find, unreported.

## The fix

`direct2d-reentrant-teardown-fix-v1.patch` -- its own fix lineage, not a successor to the
occlusion-standby versions -- adds two things:

- **A reentrancy guard.** `Direct2DGraphicsContext::Pimpl` gains `enterReentrancyGuard()`/
  `leaveReentrancyGuard()` (a nesting counter, plus a `teardownDeferred` flag).
  `adapterCreated()`/`adapterRemoved()` now check the counter before calling `teardown()`: if
  it's held, they set `teardownDeferred = true` and return instead of tearing anything down
  immediately; `teardown()` runs for real the moment the guard is released, at a point where
  nothing is still using the members it resets. `Direct2DHwndContext::HwndPimpl` holds that
  guard across `prepare()` (via a small RAII wrapper, so every one of `prepare()`'s existing
  early-return paths releases it correctly) and across the whole `startFrame()` .. `finishFrame()`
  span (a manual matched enter/leave, since they're two separate calls) -- covering
  `setSize()`'s `ResizeBuffers`, `BeginDraw`/`EndDraw`, everything your UI draws in between, and
  `present()`'s `Present1()` call.
- **An explicit composition-teardown sequence.** `CompositionTree` now has a real destructor:
  `SetContent(nullptr)` -> `SetRoot(nullptr)` -> `Commit()`, before its COM references are
  released. One subtlety this required: `CompositionTree::create()` returns its local `result`
  by value into a `std::optional`, and `ComSmartPtr` (JUCE's COM wrapper) has no move
  constructor of its own -- only copy. With a real destructor now attached, a copy there would
  leave both the returned object and the about-to-be-destroyed local pointing at the same
  underlying composition objects, and the local's destructor would immediately clear the content
  it had just set up. `CompositionTree` now has a hand-written move constructor/assignment
  (via `std::swap` on each member, which correctly nulls out the source) and its copy operations
  are explicitly deleted, so this can't happen -- and any future accidental copy is a compile
  error instead of a silent bug.

Both parts are additive and inert for anything other than `HwndPimpl`/`CompositionTree`: the
guard counter defaults to zero, so any other `Pimpl` subclass that never calls
`enterReentrancyGuard()` behaves exactly as before.

## Build fix (2026-10-11, second pass)

The first version of this patch failed to compile on your CI with MSVC error C2248
("cannot access protected struct"), on two lines inside `juce_Direct2DHwndContext_windows.cpp`.
Cause: the `ScopedReentrancyGuard` RAII helper (the thing that calls `enterReentrancyGuard()`/
`leaveReentrancyGuard()` around `prepare()`) was originally written as a free-standing struct at
namespace scope, and its constructor/member referred to `Direct2DGraphicsContext::Pimpl&`.
`Pimpl` is forward-declared under a `protected:` access specifier inside `Direct2DGraphicsContext`
(see `juce_Direct2DGraphicsContext_windows.h`) -- so code outside that class's own hierarchy can't
even *name* the type, regardless of whether the specific methods being called on it
(`enterReentrancyGuard`/`leaveReentrancyGuard`) are themselves public. `HwndPimpl` itself can name
`Pimpl` fine, because it's declared as `struct HwndPimpl : public Pimpl` -- nested classes of a
derived class share that derived class's protected-access rights.

Fix: `ScopedReentrancyGuard` is now a private struct nested *inside* `HwndPimpl`, and its
constructor/member take `HwndPimpl&` instead of `Direct2DGraphicsContext::Pimpl&` -- so it never
needs to spell out the protected type at all; it just calls the (public) guard methods, inherited
into `HwndPimpl`, directly on the reference it's given. Functionally identical, just relocated so
it's legal C++.

This patch file has been replaced with the corrected version. The CMakeLists.txt chain, the other
three patches, and everything else in this package are unchanged.

Re-verified the same way as before (fresh independent clone, `git apply` of all four patches in
sequence, plus a real CMake `FetchContent` configure) -- both now succeed, and the previously
reported `fopen` warning (`C4996`, from the diagnostic patch, line ~100) is unrelated and harmless;
it's a pre-existing warning from `direct2d-present-diagnostic.patch`, not an error, and not touched
here. **Important caveat: neither `git apply` nor CMake's configure step invokes a compiler, so
this verification could not have caught the C2248 error in the first place -- only your actual
Windows/MSVC build did, which is what caught it.** This correction fixes the specific error you
reported; it is not a guarantee no other compile issue remains, since nothing on this end can
actually compile Direct2D/Windows code. Your next build is still the real test.

## What this should do under test

The crash/freeze from unplugging a monitor, or reshuffling windows across two monitors when one
is unplugged, should no longer happen. The ghost outline that previously persisted even after
closing the DAW should no longer appear (this one's harder to verify -- you'd need to watch for
its absence specifically, not just crash/no-crash).

## What this does NOT fix

- **GPU device-removed/driver crash.** A separate, narrower issue. v3's `pollForRecovery()`
  already stops it from hanging in an infinite standby loop, but there's still no real recovery
  path -- it falls through to JUCE's existing `jassertfalse; return;` in `paint()`. Not touched
  by this patch. (We looked at three possible app-level mitigations for this -- triggering
  `updateAdapters()` on failure, a `Desktop::Displays::Listener`, and a repaint watchdog -- and
  concluded none of them actually reach the real point of failure: the shared `DxgiAdapter`'s
  underlying D3D/D2D device, which only `DxgiAdapters::updateAdapters()` can rebuild, and that
  function's own `IsCurrent()` guard most likely won't fire for a pure device-removed event with
  no accompanying display-topology change. Left alone for now, per your call not to go near
  swap-chain/device recreation code.)
- **The adapter rebuild is still wholesale, not differential** -- unplugging one monitor in a
  dual-monitor setup still tears down and rebuilds every window's Direct2D state, not just the
  one actually affected. Harmless on its own, just unnecessary churn; not fixed here.
- **A factory-creation failure during the rebuild can still leave the adapter list empty for a
  while** if `CreateDXGIFactory2` fails at exactly the wrong moment -- though this one turns out
  to be partly self-healing: `updateAdapters()`'s own guard means a failed factory doesn't block
  a retry on the *next* display-settings event, whenever that happens to occur. No guaranteed
  timing, but not permanently stuck either. Not fixed here.

## Verification so far (2026-10-11)

- All four patches (`direct2d-frame-latency-wait.patch`, `direct2d-present-diagnostic.patch`,
  `direct2d-occlusion-standby-v3.patch`, then `direct2d-reentrant-teardown-fix-v1.patch`) apply
  cleanly in sequence to a fresh, independent JUCE 9.0.3 checkout -- verified via a real
  `git apply` against a separate clone, under their final filenames.
- `direct2d-reentrant-teardown-fix-v1.patch` was also confirmed to apply independently of the
  other three -- on top of patch 1 alone, and even directly against completely stock, unpatched
  JUCE 9.0.3 with nothing else applied. It doesn't depend on the occlusion-standby fix or the
  diagnostic logging, and they don't depend on it, so it can be included or left out of a build's
  patch chain on its own.
- Also verified through a real CMake `FetchContent` configure (fetches JUCE 9.0.3 over the
  network and runs all four patches through its own populate+patch step, not just a dry-run
  `git apply --check`).
- All expected symbols confirmed present in the actually-fetched, four-patch source afterwards,
  read directly: `enterReentrancyGuard`, `leaveReentrancyGuard`, `reentrancyGuardCount`,
  `teardownDeferred` in the patched header; the guard checks in `adapterCreated()`/
  `adapterRemoved()`; `ScopedReentrancyGuard` and both guard call sites in `HwndPimpl`; the
  `CompositionTree` destructor, move constructor/assignment, and deleted copy operations. The v3
  occlusion-standby symbols (`occluded`, `pollForRecovery`, etc.) are also still present and
  unmodified.
- The patched files are fully CRLF, matching JUCE's own convention.
- **Not yet done, same as every round before it: not compiled (no Windows environment here), and
  not run against a real monitor-unplug.** This is a reasoned fix for a reentrancy/cleanup defect
  found by reading the code -- I don't have a crash dump confirming which exact call pumps the
  reentrant message on your machine, so treat this as a strong, well-evidenced hypothesis fix,
  not a lab-confirmed one, until you've actually reproduced the unplug under this build.

## How to test

1. Build and install exactly like the other D2DReproVst3 builds. Same bundle filename as the
   others (by design) -- this replaces whichever one is currently installed.
2. Re-run the screensaver/lock-screen/hibernate triggers first, to confirm nothing regressed from
   v3 (should still show no flicker).
3. Then try the monitor-unplug / dual-monitor-reshuffle scenario that crashed/froze before. Note
   whether it crashes, freezes, or recovers, and whether any ghost outline remains after closing
   the plugin/host. If it still misbehaves, the diagnostic log
   (`%TEMP%\d2d_present_diagnostic.log`) plus whatever Windows Event Viewer / crash dump you can
   get would help narrow down whether it's hitting a different path than the one this patch
   guards.
