# Phase 7 Summary — Minimal GUI

> MID-PHASE CHECKPOINT (not the final Phase 7 report).
> GUI implemented, linked, smoke-tested (14s alive, no crash, no errors).
> Awaiting USER manual test (required by the plan): launch, play, pause,
> skip, seek, volume, metadata + artwork visible. Finalize + `PHASE 7
> COMPLETE` after that confirmation.

## Goal

First native GUI: input, results, current track, controls, progress,
volume, small artwork (plans.md Phase 7). Dear ImGui, sparse, core-only.

## Done so far

- `third_party/imgui` vendored at pinned tag `v1.92.9b` (full tree, zip
  from GitHub releases; committed for reproducible/stateless builds).
- `cpp/ui/gui.h` + `cpp/ui/gui.cpp` (with `main`): `spotilite::Gui`
  over the core `Player`. Win32 window + DirectX 11 (system libs only —
  no windowing dependency). Single `spotilite` window:
  - URI input + Play button (real search is Phase 8; see deviations).
  - Queue listbox (click select, double... `Play selected` button).
  - Current: 64px artwork (BMP→DX11 texture on ARTWORK_READY) + title /
    artist-album from async metadata fetch (applied only if still
    current) + elapsed/total + state + volume% + queue count.
  - Progress SliderInt with seek-on-release; Play/Pause + <</>> buttons;
    live volume slider.
  - `io.IniFilename = nullptr` (no imgui.ini droppings). Errors shown as
    a status line (and a MessageBox for fatal/connect failures).
- `cpp/core/queue.h`: added `at(i)` row accessor for the listbox (small,
  justified; no behavior change).
- Link (direct `g++ -std=c++17`, no CMake change): gui + player + imgui
  core/draw/tables/widgets + win32/dx11 backends + staticlib →
  `build/gui.exe` (+ `d3d11 d3dcompiler dwmapi gdi32 user32 kernel32
  imm32`; GDI was missing on the first attempt).
- Deviations (phase goal unchanged, all logged in `docs/DECISIONS.md`):
  - Input is URI + Play, results area is the queue list — Spotify search
    needs the Phase 8 Web API; acceptance doesn't require search.
  - Metadata fetch off the UI thread (`std::async`); artwork requested
    per track change, textured on READY.

## Verified

- `g++` link exit 0. `build/gui.exe` launched headless here: alive 14s
  (incl. blocking auto-connect), no crash, empty stdout/stderr, killed
  cleanly. No interaction possible from here — manual test required.
- `cmake --build build --config Release` still green (deferred to close).

## NOT yet verified (requires user)

Launch, playback state shown, play/pause, skip, seek, volume, metadata +
artwork displayed. Steps in "How to test now".

## How to test now

1. `cmake --build build --config Release` (cmake+cargo on PATH).
2. Relink GUI with the `g++` line in `cpp/ui/gui.cpp` header comment.
3. Run `build/gui.exe`. Paste a `spotify:track:...` URI, press Play.
4. Check: metadata + 64px artwork appear, progress advances, Play/Pause
   toggles, << / >> skip (after adding URIs via queue... note: queue is
   populated by played URIs — load 2 URIs, select in list, Play
   selected), seek slider jumps, Vol slider changes volume.
5. Report: what works / screenshots of anything wrong / exact error text.

## librespot rev

Unchanged: `a1b66d3c`, defaults. No manifest change.

## Version control

- `phase-7: vendor Dear ImGui v1.92.9b`
- `phase-7: add minimal GUI`
- `phase-7: checkpoint docs` (this file + `docs/DECISIONS.md`, `docs/API.md`)
- Pushed to `origin/main`.
