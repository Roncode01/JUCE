# D2D Repro VST3 -- v5 diagnostic (adapter-change teardown logging)

Not a new fix. This is the v5 build (patches 1-4, byte-identical to what you already have) plus
one extra, purely observational patch, added to chase down a regression report: with the v3
build (patches 1-3), with or without patch 4, the screensaver/lock-screen flicker you'd
previously confirmed fixed ("no flickering observed") came back.

## What the first diagnostic log already told us

Your `%TEMP%\d2d_present_diagnostic.log` from the flickering session showed exactly two
`Present1` calls around the lock event -- one that returned `DXGI_STATUS_OCCLUDED`, and the
next one (2.1 seconds later) that returned `S_OK` again -- with nothing logged in between. That
is what v3's occlusion-standby logic (`canPresent()` / `pollForRecovery()` / `didPresent()`)
is *supposed* to produce: while occluded, real presents stop almost entirely and the code polls
via a cheap `DXGI_PRESENT_TEST` call instead (which isn't itself logged, by design -- it's not
the real present), until it sees recovery and lets the next real frame through. In other words:
the mechanism patches 1-3 added is doing exactly what it was built to do here. That rules it out
as the cause of what you're now seeing -- whatever's flickering, it isn't `Present1` returning
the wrong thing or the standby loop misbehaving.

## What this patch adds

`direct2d-adapter-change-diagnostic.patch` instruments the *other* mechanism already under
investigation in this project: `DxgiAdapters` -> `adapterCreated()`/`adapterRemoved()` ->
`teardown()`, which tears down and recreates the entire Direct2D device context, swap chain and
DirectComposition visual tree. This path is triggered by `WM_DISPLAYCHANGE`/`WM_SETTINGCHANGE`,
not by anything `Present1` returns -- and on some driver/display configurations, locking or
unlocking the screen (or a screensaver engaging) *can* fire one of those messages, even though
it's logically just a lock, not an actual monitor change. If that's happening here, a full
teardown-and-rebuild of the swap chain and composition visual slotting in asynchronously is a
plausible, completely independent source of a visible flicker -- and it would explain why it
shows up "with or without" patch 4: patch 4 only changes *when* `teardown()` runs relative to a
reentrant call (immediately vs. deferred, to avoid the crash), not *whether* it runs, or that
it's visually disruptive when it does.

It adds three log lines, written to the same log file patch 2 already writes to, with the same
timestamp format, so the two logs can be read side by side:

- `adapterCreated: adapter changed -- running teardown() now` (and the `adapterRemoved`
  equivalent) -- logged right before an *immediate* teardown().
- `adapterCreated: adapter changed while reentrancy guard held -- deferring teardown()` (and the
  `adapterRemoved` equivalent) -- logged when the reentrancy guard (patch 4) defers it instead.
- `leaveReentrancyGuard: running deferred teardown() now` -- logged when that deferred
  teardown() actually runs, once the guard releases.

Nothing here changes behaviour -- it's observation only, inert except for the three `fopen`/
`OutputDebugStringA` calls themselves. Same caveat as patch 2: remove it once it's done its job.

## How to test

1. Build and install exactly as before -- same `PRODUCT_NAME`, so it replaces whichever
   D2DReproVst3 build is currently installed. (`PLUGIN_CODE` is `D2d6` here, just so REAPER
   doesn't confuse it with the `D2d5` build, in case you still have both installed somewhere.)
2. Reproduce the flicker (screensaver or lock screen, same as before).
3. Grab `%TEMP%\d2d_present_diagnostic.log` afterwards and check: does an `[AdapterChange]` line
   appear at or near the same timestamp as the `DXGI_STATUS_OCCLUDED` transition in the
   `[Present1 normal]`/`[Present1 test]` lines?
   - **If yes** -- that confirms a display-change notification is firing on this lock/unlock and
     triggering a real teardown+rebuild, which is a solid, evidenced target for an actual fix
     (most likely: skip the teardown/rebuild when the "new" adapter is actually the same
     physical adapter, which `DxgiAdapters::updateAdapters()`'s own `uniqueIDMatches()` check
     almost gets there already -- worth understanding exactly why it doesn't match in this
     case).
   - **If no `[AdapterChange]` line appears at all** -- this mechanism isn't firing either, and
     the flicker is coming from something neither diagnostic currently instruments (worth
     reconsidering candidates: the composition commit itself, a DWM-level effect unrelated to
     this plugin's own Direct2D calls, or something in REAPER's own window handling around
     lock/unlock).

Either answer is useful -- this is designed to narrow it down, not to fix anything yet.

## Verification so far (2026-10-12)

- All five patches apply cleanly in sequence to a fresh, independent JUCE 9.0.3 checkout, same
  method as every patch before it (`git apply` against a separate clone).
- Confirmed in the actually-patched source: the new log call sites in `adapterCreated()`/
  `adapterRemoved()`, the forward-declared `logDeferredTeardownRun()` in the header, and its
  definition and call site in `leaveReentrancyGuard()`.
- Patches 1-4 are unchanged from the v5 build you already have -- confirmed byte-identical.
- **Not compiled** (same limitation as always -- no Windows environment here). This one is
  slightly higher-confidence than most: it only adds `fopen`/`OutputDebugStringA` calls using
  the exact same Windows APIs (`GetTempPathA`, `SYSTEMTIME`, `GetLocalTime`) patch 2 already
  uses successfully in a different module, plus one ordinary forward-declared free function to
  cross the header/cpp boundary cleanly (specifically to avoid repeating the protected-type
  mistake patch 4 hit) -- but it's still unverified by an actual compiler until your next build.
