# Phase 7 Summary — Minimal GUI

Final report. User manual test passed (see Verified).

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
  - Queue listbox (click select + `Play selected` button).
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

- `g++` link exit 0. `build/gui.exe` smoke here: alive 14s (incl.
  blocking auto-connect), no crash, empty stdout/stderr.
- USER manual test (required by plan): launch, playback state, play/pause,
  skip, seek, volume, metadata + artwork — user confirmed working ("yes").
- `cmake --build build --config Release` exit 0 (rerun at close).
- `git status` clean; scope respected (`third_party` vendor,
  `cpp/ui/**`, one `Queue::at` accessor, `docs/`; TUI/CMakeLists
  untouched).

## How to test now

1. `cmake --build build --config Release` (cmake+cargo on PATH).
2. Relink GUI with the `g++` line in `cpp/ui/gui.cpp` header comment.
3. Run `build/gui.exe`. Paste a `spotify:track:...` URI, press Play.
4. Check: metadata + 64px artwork appear, progress advances, Play/Pause
   toggles, << / >> skip, seek slider jumps, Vol slider changes volume.

Expected: all of the above (user-verified).

## librespot rev

Unchanged: `a1b66d3c`, defaults. No manifest change.

## Version control

- `phase-7: vendor Dear ImGui v1.92.9b`
- `phase-7: add minimal GUI`
- `phase-7: checkpoint docs`
- `phase-7: finalize docs` (this file)
- All pushed to `origin/main` at phase completion.
