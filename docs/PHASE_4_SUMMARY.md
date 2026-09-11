# Phase 4 Summary — Minimal TUI

Final report. All acceptance criteria verified (see Verified).

## Goal

Tiny terminal frontend over the C++ core (plans.md Phase 4). No GUI yet.

## Done so far

- `cpp/app/tui.h` + `cpp/app/tui.cpp` (with `main`): `spotilite::Tui`
  owns a core `Player`, auto-connects, then runs a line-based command
  loop with render-on-command. Commands: `play <uri>`, `p` (play/pause
  toggle), `n`/`b` (queue skip), `add <uri>`, `v <0-100>`, `s` (refresh),
  `h`, `q`. No playback logic, no Rust symbols — core only.
- Render: title, current URI, elapsed position, state, volume %, queue
  size/index. No animations, no continuous redraw.
- Small core integration fix (allowed by 1.15): `Player::pause()` now
  flushes stale events then pins `playing=false` optimistically, so
  toggles can't invert on event latency. Same for the initial `stopped`
  label (was `paused` with nothing loaded).
- Link (direct `g++ -std=c++17`, same sys libs, no CMake change):
  `tui.cpp + core/player.cpp + staticlib → build/tui.exe`.
- Deviations from the mock (documented, phase goal unchanged):
  - Track/Artist/Album/duration show placeholders — real metadata is
    Phase 5; the TUI shows the current URI + elapsed position today.
  - Line-based commands (`p` + Enter) instead of single-keypress: avoids
    Windows-only console APIs before Phase 12 (platform isolation).
  - Render-on-command instead of live refresh: stdin blocks, so there is
    no idle redraw loop; `s` refreshes any time.

## Verified

- `cargo build --release` and `cmake --build build --config Release` exit 0.
- Scripted `build/tui.exe` runs (timed stdin): launch+connect, `play`
  → `[playing]`, `s` shows advancing elapsed, `p` → `[paused]`
  immediately, `p` → `[playing]`, `v 70` → `vol 70%`, `add` + `n`
  skips (reload, pos reset, `queue 1`), `q` → `Bye.`, exit 0, empty
  stderr. Toggle inversion found during testing and fixed (see above).
- `git status` clean; Phase 4 scope respected (`cpp/app/tui.*`,
  one small `cpp/core` fix, `docs/`; `main.cpp`/CMakeLists untouched).

## Known issues

- Position may lag wall-clock for a second or two at track start
  (pipeline buffering); it tracks thereafter. Cosmetic for now.
- `resume`/`play` stay event-driven (no optimistic `playing=true`), so
  right after resuming the display can lag one refresh behind; `s`
  corrects it. Pause is exact.

## librespot rev

Unchanged: `a1b66d3c`, defaults. No manifest change.

## Version control

- `phase-4: add minimal TUI` (`cpp/app/tui.*`, core pause/label fix)
- `phase-4: finalize docs` (this file + `docs/DECISIONS.md`, `docs/API.md`)
- All pushed to `origin/main` at phase completion.
