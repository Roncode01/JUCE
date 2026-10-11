# D2D Repro VST3 -- Diagnostic build

This is **not a fix**. It's a one-shot instrument to answer a question the last three rounds of
analysis turned up: we don't actually know which `HRESULT` `Present1()` returns during
screensaver/lock-screen/hibernate. Every fix attempt so far (including the one shipped in the
1.20.0 candidate) assumed it was `DXGI_STATUS_OCCLUDED` -- but Microsoft's own docs say that code
is never returned for a flip-model swap chain, which is what this file actually creates. So that
assumption may be wrong, and this build exists to get the real answer before designing anything
else.

## What's different from the other D2DReproVst3 builds

- Same `Source/` files, same base patch (`direct2d-frame-latency-wait.patch`, i.e. "patch 1").
- **Does NOT include `direct2d-occlusion-didpresent.patch`** (the second patch from the 1.20.0
  candidate). That patch changes control flow, which would confound the data this build is meant
  to capture. So: **this build is expected to still flicker** under the same triggers you found
  before (screensaver, lock screen, hibernate). That's expected, not a regression -- flicker isn't
  what we're testing here.
- Adds `direct2d-present-diagnostic.patch`, which logs **two** probes every paint, each tracked
  and logged independently:
  - **"Present1 normal"** -- the real present this file already made. Its result and flags are
    unchanged from before; the logging hook just watches, placed before any of JUCE's own code
    reacts to the result.
  - **"Present1 test"** -- a second call added on review, made immediately after the real one,
    using `DXGI_PRESENT_TEST` (submits no frame -- Microsoft's documented way to check
    presentability without presenting:
    <https://learn.microsoft.com/en-us/windows/win32/api/dxgi/nf-dxgi-idxgiswapchain-present>).
    No code in this project currently calls `DXGI_PRESENT_TEST` for real (the "occlusion-standby"
    design that would have was deliberately kept out of this diagnostic build), so without this
    second probe there'd be nothing here to show what a test-present reports during an
    occluded/transitioning period -- this adds that second, independent data point. Its result
    is only logged, never acted on, so it can't change behaviour.
  Neither probe changes what any function returns or does -- both only watch and log.
- Distinct plugin code (`D2dg`) from the other D2DReproVst3 build (`D2dr`), so REAPER's own
  plugin-identity tracking (FX chains, saved projects) can tell them apart. `PRODUCT_NAME` is
  deliberately left unchanged, though, because your CI workflow's artifact-collection step has
  that exact filename hardcoded -- so **this build's `.vst3` has the same filename as your other
  D2DReproVst3 build and will replace whichever one is currently installed**, the same way
  1.19.0 replaces 1.18.0 for the main plugin. It won't sit side-by-side with your existing
  D2DReproVst3 instance; install this one, run the trigger list, then reinstall the other build
  afterwards if you still need it.

## What it logs

Every time either probe's returned `HRESULT` changes from its own previous call, it writes one
line to:

```
%TEMP%\d2d_present_diagnostic.log
```

(also mirrored via `OutputDebugStringA`, in case you have DebugView or a debugger attached). Each
line is tagged `[Present1 normal]` or `[Present1 test]` so you can tell which probe it's from (the
two are tracked completely separately -- one changing never resets the other's counter), and has
a timestamp, the previous code (decoded to a name where recognised), how many consecutive calls of
that probe held that code, the new code it's transitioning to, and that probe's sync interval and
present flags. Each probe also always logs its own "start" line on its first call, so you've got a
reference point for both at launch.

Recognised codes (anything else logs as "unrecognised -- look up this hex value", verbatim hex,
so it still captures codes we didn't anticipate):

| Hex | Name |
|---|---|
| `0x00000000` | `S_OK` |
| `0x087A0001` | `DXGI_STATUS_OCCLUDED` (flagged "not expected -- flip model") |
| `0x087A0007` | `DXGI_STATUS_MODE_CHANGED` |
| `0x087A0008` | `DXGI_STATUS_MODE_CHANGE_IN_PROGRESS` |
| `0x887A0001` | `DXGI_ERROR_INVALID_CALL` |
| `0x887A0005` | `DXGI_ERROR_DEVICE_REMOVED` |
| `0x887A0006` | `DXGI_ERROR_DEVICE_HUNG` |
| `0x887A0007` | `DXGI_ERROR_DEVICE_RESET` |
| `0x887A0026` | `DXGI_ERROR_ACCESS_LOST` |

## How to use it

1. Build and install this exactly like the other D2DReproVst3 builds (`cmake -S . -B build -A x64`
   then build the `D2DReproVst3_VST3` target). This replaces whichever D2DReproVst3 build you
   currently have installed (same bundle filename, by design -- see "What's different" above),
   so load it in REAPER in place of your existing instance rather than expecting both at once.
2. Before testing, delete any existing `%TEMP%\d2d_present_diagnostic.log` so you're starting
   clean (it appends, so an old run's lines would otherwise mix in).
3. Open the plugin's editor, leave it open and visible, and run through the same trigger list as
   before:
   - Screensaver
   - Lock screen (Ctrl-Alt-Del)
   - Sleep (S3)
   - Hibernate (S4)
   - Display off only
   For each one: trigger it, wait a few seconds, then resume/unlock, and watch for the flicker
   returning (expected -- confirms you hit the same state as before) before moving to the next
   trigger. Leave a short gap between triggers so the log's transitions stay easy to tell apart.
4. Send back `%TEMP%\d2d_present_diagnostic.log`. That's the actual deliverable from this build --
   once we can see the real transition (what code, held for how many calls, lasting how long) for
   each trigger, the next fix can be designed against the real mechanism instead of a guess.

## Verification so far (2026-10-11)

- Both patches (`direct2d-frame-latency-wait.patch` then `direct2d-present-diagnostic.patch`)
  apply cleanly in sequence to a fresh JUCE 9.0.3 checkout, verified via a real `git apply`
  against an independent clone -- not just `--check`.
- Also re-verified through a real CMake `FetchContent` configure (not just `git apply`), which
  actually fetches JUCE 9.0.3 and runs both patches through its own populate+patch step.
- Both probes' call sites and symbols (`onPresent1Result`, `onPresent1TestResult`, the
  `DXGI_PRESENT_TEST` call) were confirmed present in the actually-fetched, patched source
  afterwards (grepped the patched file directly, not just the patch text), with patch 1's
  original `if (hr == S_OK) swap.didPresent();` control flow confirmed untouched alongside them.
- The patched file is fully CRLF, matching JUCE's own convention, with no bare LF lines introduced.
- **Not yet done, same as every prior round: not compiled (Windows/Direct2D-only file, no Windows
  environment available here), and not run against the real triggers.** That's the whole point of
  sending this to you.

## What happens after this

Once the log comes back, the next step is to design the actual fix against whatever the log
shows -- not `DXGI_STATUS_OCCLUDED` by default, and not by assuming it's a single consistent code
across all five triggers (hibernate's resume, in particular, is a different kind of event from a
screensaver/lock-screen occlusion, and may produce something else entirely, e.g.
`DXGI_STATUS_MODE_CHANGED` or a transient `DXGI_ERROR_*`). No new fix design has been started
yet -- this build is deliberately just the instrument.
