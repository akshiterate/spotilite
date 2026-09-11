# UI Rework — Multi-Window Desktop Player

Living handoff for the out-of-plan UI overhaul (approved after Phase 10).
Plan phases are untouched by this work unless stated.

## Phase 1 — Vendor swap (done)

- `third_party/imgui` replaced: master v1.92.9b → docking snapshot
  `367b2c24f399988ddafc0bb4628da0106bcc09be` (branch tip 2026-09-11,
  reports as "1.93.0 WIP"). Full tree committed, same layout.
- Verified in-tree: `UpdatePlatformWindows`,
  `RenderPlatformWindowsDefault`, `ViewportsEnable` present; win32
  backend implements the viewport platform interface.
- Current single-window `build/gui.exe` relinked UNMODIFIED against the
  new snapshot: link exit 0, 14s smoke alive, no errors. Swap is safe.

## Phase 2 — Main + Queue + Search (done, awaiting user test)

- `App` (`cpp/ui/app.*`) owns one `Player`; `Gui` split via `git mv`
  (history kept). Views: main view in frame, `queue_window.cpp`,
  `search_window.cpp` (no extra headers — view methods on `App`).
- Viewports enabled; main loop runs Update/RenderPlatformWindows.
- Queue window as specified; main window keeps nav (Queue/Search only),
  artwork, title/artist, slider, controls, volume. URI input dropped.
- Deviations from the proposal: main view stayed in `frame()` instead of
  `main_window.*` (less churn, same modularity for the new windows);
  single-instance focus via `SetNextWindowFocus`; positioning manager
  stays Phase 3 (default cascade for now).
- Verified: link exit 0, 14s smoke alive, core_test green (unaffected).

## Phase 3 — Playlists + Settings + positioning (later)

- Playlists window + multi-instance content windows (Liked = content
  window); Settings placeholder.
- Placer: Queue left, Search above, Playlists right, contents right,
  Settings below; single-instance focus, multi-instance by id.
