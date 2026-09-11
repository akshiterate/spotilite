# Phase 15 Summary — Performance Pass

Final report with before/after measurements (plans.md Phase 15).
Machine: Windows 10/11 x64, 12 cores, MinGW GCC 16.1.0, rustc 1.98.1.
Caveats: a lived-in machine (browser/music apps active), so interface
byte counters are noisy; CPU % normalized by 12 cores.

## Baseline (before)

| Metric | Measured | Target | Verdict |
|---|---|---|---|
| Startup, time-to-window (2 runs) | 1088 / 1077 ms | cold <500, warm <200 | MISS (~2x) |
| Session+connect alone (headless log) | ~1871 ms | — | bottleneck identified |
| GUI idle RAM (WS / private) | 62.4 / 49.6 MB | 40–60 | borderline (WS just over) |
| GUI idle CPU (12s avg) | 1.0% normalized | ~0% | near-miss, accepted |
| Playback CPU (core_test, 10s) | 0.01% normalized | <3% | PASS (100x headroom) |
| Playback engine RAM (WS / private) | 23.1 / 5.9 MB | — | very lean |
| Headless idle-connected RAM | 21.5 / 6.2 MB | <20 | marginal miss (WS) |
| Binary gui.exe | 43.9 MB | prefer <25 | MISS |
| Network (interface delta, 30–45s) | tens of MB (background noise) | — | unmeasurable this way |

## Optimizations (evidence-driven, both kept)

1. **Async connect (startup 1088 → 350–474 ms).** Connect+session was
   ~1–2s on the launch path. Window now opens first; `connectBlocking()`
   runs on a worker and the UI adopts it (`adoptConnected()`) when done,
   with Connecting.../Retry UI. Split is race-free (ABI calls only on
   the worker; C++ state only on the UI thread). New `Player` primitives
   documented in `docs/API.md`. core_test/TUI keep the sync path.
2. **`strip` on release binaries (43.9 → 24.6 MB, -44%).** Debug symbols
   were nearly half the binary. gui/core_test/tui/bridge_test/headless
   all stripped and smoke-tested; all ≤25 MB except headless at 27.6
   (near-miss, accepted — same staticlib, marginally more code).
   Rebuild + `strip build/*.exe target/release/headless.exe` to repeat.

Kept idle CPU at ~1% (frame loop is simple and correct; dirty-rendering
not worth the complexity) and headless WS at 21.5 MB (1.5 over; private
6.2, not worth chasing).

## After

| Metric | Measured | Target | Verdict |
|---|---|---|---|
| Startup, time-to-window (2 runs) | 474 / 350 ms | cold <500 | PASS (warm <200 still open) |
| GUI idle RAM | unchanged (~62/50) | 40–60 | accepted |
| GUI idle CPU | ~1.0% | ~0% | accepted (see above) |
| Playback CPU | 0.01% | <3% | PASS |
| Binary gui.exe | 24.6 MB | <25 | PASS |
| Network (streaming estimate) | ~1.2 MB/min @160kbps; auth ~hundreds KB once; metadata/art/search KBs per action | — | by construction, see below |

Network note: interface counters moved tens of MB during idle windows
with other apps active, so per-process attribution was impossible here.
The dominant cost is audio at the configured bitrate (96: ~0.7,
160: ~1.2, 320: ~2.4 MB/min); everything else is one-off KBs. No
polling loops exist to trim (events are push, caches are local).

## How to test now

1. `cmake --build build --config Release`; relink gui.exe (g++ line in
   `cpp/ui/app.cpp`); `strip build/gui.exe`.
2. Time `build/gui.exe` to window (should feel instant, "Connecting..."
   shows, then ready); play/pause/seek/volume/queue/search/library as
   in Phases 7–10 — all green in automation.
3. Confirm the only behavior change: no more launch blocking.

## librespot rev

Unchanged: `a1b66d3c`, defaults. No manifest change.

## Version control

- `phase-15: async connect + strip binaries`
- `phase-15: finalize docs` (this file + `docs/DECISIONS.md`, `docs/API.md`)
- All pushed to `origin/main` at phase completion.
