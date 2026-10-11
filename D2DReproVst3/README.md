# D2D Repro VST3 -- v3 fix candidate

This is the first real fix attempt built on **confirmed** data rather than a guess. Stage 1 of
two: this patch targets the screensaver/lock-screen/hibernate flicker specifically. The separate
monitor-unplug crash/freeze you found is stage 2 -- not addressed here, see "What this does NOT
fix" below.

## Why this design, now

The 2026-10-11 diagnostic run (on the previous build, patch 1 + logging only) showed:

| Trigger | Real HRESULT | `DXGI_PRESENT_TEST` probe |
|---|---|---|
| Screensaver | `DXGI_STATUS_OCCLUDED` (`0x087A0001`) | tracked it exactly |
| Lock screen | `DXGI_STATUS_OCCLUDED` | tracked it exactly |
| Hibernate | `DXGI_STATUS_OCCLUDED` | tracked it exactly |
| Sleep | never occluded (no `Present1` calls at all) | -- |
| Display off | never occluded | -- |

So the "occlusion-standby" design floated a few rounds back -- detect occlusion, stop presenting
for real, poll with `DXGI_PRESENT_TEST` until it clears -- is no longer a guess about which
HRESULT occurs or whether the test probe is trustworthy here. Both are confirmed, for exactly the
triggers that matter.

## What changed from patch 1 alone

`SwapChain::didPresent()` now takes the `HRESULT`:

- `S_OK` -- reset the present-throttle (`didWait = false`) exactly as before.
- Any other success code (confirmed to be `DXGI_STATUS_OCCLUDED` here) -- leave the throttle
  alone and set `occluded = true`. `canPresent()` then stops attempting real presents and calls
  `pollForRecovery()` instead, every readiness check, until it confirms recovery.

`pollForRecovery()` calls `Present1(0, DXGI_PRESENT_TEST, ...)` -- submits no frame, so it can't
hit the "frame might never retire" problem a real present would. On `S_OK`, it exits standby and
lets the very next real frame through immediately. **This is the one change from the design
reviewed a few rounds back:** if the probe itself returns a genuine failure (`FAILED(hr)`, e.g.
`DXGI_ERROR_DEVICE_REMOVED`) rather than "still occluded," it now exits standby instead of polling
forever -- a real failure isn't occlusion, and looping on it would hang the editor rather than
just flicker. The next real `Present1` attempt then runs normally and hits its own existing
`FAILED(hr)` handling (unchanged, further down in `paint()`).

## Diagnostic logging -- kept in, with a caveat

`direct2d-present-diagnostic.patch` is still chained in, as asked, so you can see what's actually
happening while testing this. One thing worth knowing: **the log will go quiet during standby
itself.** Both logging probes live inside `paint()`, and `canPresent()` -- which gates whether
`paint()` runs at all -- returns `false` while occluded (it's busy polling via
`pollForRecovery()`, which isn't logged). So you'll see the log capture the moment occlusion
begins, then nothing, then the moment it recovers and real painting resumes. That gap is expected
under this patch, not a sign something broke -- it's the opposite of patch-1-alone's behaviour,
where `paint()` kept running (and logging) the whole time because nothing was gating it.

## What this build should do under test

For screensaver, lock screen, and hibernate: **no flicker**, and no hang either (that's the thing
being added beyond the earlier design). The editor should go quiet while occluded (no visible
updates, which is correct -- there's nothing to update since the old frame is still valid) and
resume cleanly on recovery.

## What this does NOT fix

Your two extra tests -- unplugging the monitor showing the FX window, single- and dual-monitor --
produced crashes/freezes with no log entries at all, meaning whatever failed likely happened
before `Present1` was even called again. That's a different code path entirely (display-topology
change handling, not the swap-chain presentation logic this patch touches) and needs its own
investigation -- that's stage 2, after you've confirmed this patch on the original five triggers.

## Verification so far (2026-10-11)

- All three patches (`direct2d-frame-latency-wait.patch`, then `direct2d-present-diagnostic.patch`,
  then `direct2d-occlusion-standby-v3.patch`) apply cleanly in sequence to a fresh JUCE 9.0.3
  checkout, verified via a real `git apply` against an independent clone.
- Also verified through a real CMake `FetchContent` configure (not just `git apply`) -- fetches
  JUCE 9.0.3 and runs all three patches through its own populate+patch step.
- All expected symbols (`occluded`, `pollForRecovery`, `didPresent (HRESULT)`, both diagnostic
  probes) confirmed present in the actually-fetched, triple-patched source afterwards, read
  directly to confirm the logic matches what's described above.
- The patched file is fully CRLF, matching JUCE's own convention.
- **Not yet done, same as every round before it: not compiled (no Windows environment here), and
  not run against the real triggers.** That's what this build is for.

## How to test

1. Build and install exactly like the other D2DReproVst3 builds. Same bundle filename as the
   other builds (by design, so your CI's `Copy-Item` step still works) -- this replaces whichever
   one is currently installed.
2. Clear `%TEMP%\d2d_present_diagnostic.log` first if you want a clean run.
3. Run through screensaver, lock screen, and hibernate again. Confirm: no flicker, no hang, log
   shows the expected quiet gap during each occlusion period.
4. If you want to also probe the device-loss path this round added handling for, that's harder to
   trigger deliberately (it needs an actual GPU driver reset/removal) -- not expected to be
   testable on demand, just defended against now.
5. Leave the monitor-unplug tests for stage 2.
