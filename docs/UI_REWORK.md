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

## Phase 2 — Main + Queue + Search (next)

- `App` owns one `Player`; per-window structs with open flags.
- Viewports enabled; main loop gains Update/RenderPlatformWindows.
- Queue window: current + numbered upcoming, Play Selected (discard
  before + play), Delete, Move-to-N input, Shuffle upcoming, auto-advance
  on track end (app layer, display-filtered so Previous keeps working).
- Core additions: `Player::playFrom(i)`, `Queue::shuffleUpcoming()`.
- Search window: existing search UI relocated.

## Phase 3 — Playlists + Settings + positioning (later)

- Playlists window + multi-instance content windows (Liked = content
  window); Settings placeholder.
- Placer: Queue left, Search above, Playlists right, contents right,
  Settings below; single-instance focus, multi-instance by id.
