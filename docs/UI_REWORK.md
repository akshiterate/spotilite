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
- Queue window as specified; main window keeps nav (full bar now),
  artwork, title/artist, slider, controls, volume. URI input dropped.
- Deviations from the proposal: main view stayed in `frame()` instead of
  `main_window.*` (less churn, same modularity for the new windows).
- Verified: link exit 0, 14s smoke alive, core_test green (unaffected).

## Phase 3 — Playlists + Settings + positioning (done, awaiting user test)

- `playlists_window.cpp`: Liked Songs entry + own-playlists list with
  paging, Open (multi-instance content window by id), Refresh.
- `content_window.cpp`: rows + Play Selected (top-play) + Add
  Playlist/All to Queue + Add Selected; per-window async fetch,
  per-window errors; closed windows destroyed.
- `settings_window.cpp`: placeholder text (Phase 11 owns settings).
- `window_placer.h/.cpp`: role-based rects from main HWND + work area
  (Queue left, Search above, Playlists right, contents right-cascaded,
  Settings below), clamped, overlap fallback. Applied once per open.
- Single-instance focus for Queue/Search/Playlists/Settings;
  re-clicking a playlist focuses its existing window.
- Verified: link exit 0, 14s smoke alive. Click paths need the user.
