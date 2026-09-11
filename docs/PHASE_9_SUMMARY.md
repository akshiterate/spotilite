# Phase 9 Summary — Queue

Final report. All criteria verified (see Verified).

## Goal

Full queue management in the C++ core: add, remove, reorder, clear, play
selected, next, previous. GUI displays/manipulates only (plans.md Phase 9).

## Done so far

- `cpp/core/queue.h`: added `removeAt(i)` (index follows its track;
  removing the playing row doesn't stop playback) and `move(from, to)`
  (clamped target, index re-resolves onto the same track). Joins existing
  add/clear/next/previous/select/at.
- `cpp/core/player.h/.cpp`: split loading into resetting `loadUri()` vs
  queue-preserving `loadCurrent()` (+ public `playCurrent()`). `next()` /
  `previous()` / GUI play-selected now navigate WITHOUT collapsing the
  queue — previously every skip reset it to one item, which defeated real
  queues. Solo URI plays still reset (predictable).
- `cpp/ui/gui.cpp` (no header change needed): queue row gained Add URI,
  Del, Up, Down; search results gained Add to queue (any kind enqueues;
  play validates). This answers the Phase 7 user complaint (couldn't hold
  2 tracks).
- `cpp/core/core_test.cpp`: remove/reorder logic checks (incl. bounds +
  index-following), optional second argv URI proving playback follows
  `next()` onto track 2 and `previous()` back onto track 1; wiring
  section made order-independent (rebuilds canonical order first).

## Verified

- `build/core_test.exe <uri1> <uri2>`: `queue next loads`, `playback
  followed queue`, `queue previous loads`, `playback back on first`, all
  remove/move/bounds checks `ok`; `CORE TEST DONE: all passed`, exit 0
  (two real tracks, audible).
- One test bug caught by the run (wiring section assumed pre-move order)
  and fixed in the test, not the core.
- `build/gui.exe` relinked, 14s smoke alive, no crash.
- `cargo build --release` + `cmake --build build --config Release` exit 0.
- `git status` clean; scope respected (`cpp/core/**`, `cpp/ui/**`,
  `docs/`; TUI/CMakeLists/Rust untouched).

## librespot rev

Unchanged: `a1b66d3c`, defaults. No manifest change.

## Version control

- `phase-9: full queue management`
- `phase-9: finalize docs` (this file + `docs/DECISIONS.md`, `docs/API.md`)
- All pushed to `origin/main` at phase completion.
