# Phase 10 Summary — Library

> MID-PHASE CHECKPOINT 2 — Liked Songs + Playlists work (live verified).
> Remaining subsections: 3. Albums, 4. Artists. Per the plan, STOPPING
> here for user testing before continuing.

## Goal

Library, incrementally: Liked Songs, Playlists, Albums, Artists — all
lazy-loaded/paginated, never bulk-loaded at startup (plans.md Phase 10).

## Done so far (subsection 1 — Liked Songs)

- Shared Web API plumbing (refactor, no behavior change to search):
  `web_get(handle, endpoint, params)` with token + single 401-refresh
  retry; `SearchError::Auth` variant so login failures keep the AUTH
  code; `map_track` shared by search + liked; top-level `fill_items`.
- `include/spotify_bridge.h` (additive):
  `spotify_liked_tracks(player, limit, offset, items, cap, total_out)` —
  library cap 50 (unlike search's 10), `*total_out` = full size.
- `rust/librespot-bridge/src/lib.rs`: `/v1/me/tracks` fetch, `total`
  passthrough, `items[].track` mapping.
- `cpp/core/player.h/.cpp`: `Player::likedTracks(limit, offset, vec,
  total)` (bool + `lastError()`).
- `cpp/core/core_test.cpp`: liked section with the same login gating
  (clean AUTH error without login, real rows with login).
- `cpp/ui/gui.h/.cpp`: Liked section — Liked button (page 0), `<`/`>`
  page buttons with `page X of Y`, 6-row listbox, Play-liked (top-play)
  + Add-liked buttons, all async like search. Window 660x900.

## Done in subsection 2 — Playlists

- Finding: `/v1/playlists/{id}/tracks` is robustly 403 for new apps
  (own + others', with/without market/fields) while the playlist object,
  `/v1/me/*` and search all work — an endpoint restriction, not a scope
  gap (scope errors say "Insufficient client scope", seen separately on
  artists). Per-playlist `tracks.total` also reads 0, so counts are not
  shown.
- Consequence: playlist TRACKS resolve through librespot's own context
  machinery (`spclient.get_context("spotify:playlist:{id}")` — the same
  source Spirc plays from) with concurrent metadata fetches for names.
  First page only; total reported -1 (unknown). No Web API gap touched.
- `include/spotify_bridge.h` (additive): `spotify_playlists` (owner-only
  subtitle) + `spotify_playlist_tracks` (URI or raw id).
- `rust/librespot-bridge/src/lib.rs`: shared arg-check + total-write
  helpers; unsound `ptr::read` draft caught and replaced with a borrow
  before compiling.
- `cpp/core/player.h/.cpp`: `playlists()` + `playlistTracks()`; shared
  copy helper.
- `cpp/core/core_test.cpp`: playlists list + drill into the first one.
- `cpp/ui/gui.h/.cpp`: library view generalized (`LibMode` LIKED /
  PLAYLISTS / PLAYLIST_TRACKS, one listbox): Playlists button, drill-in
  Play (= Open), Back (remembers page), Play/Add for tracks, Add on a
  playlist row explains itself. Fixed a decl-order compile error
  (`LibMode` before first use) along the way.

## Verified (subsections 1–2)

- `build/core_test.exe`: liked (total 47, real rows), playlists (total
  22, real names/owners), playlist-tracks via context (5 real named
  tracks from the first playlist), exit 0 — all cached-token, no browser.
- `build/gui.exe` relinked, 14s smoke alive. Click paths (drill-in, Back,
  Play/Add per mode) are user-tested at this checkpoint.
- `cargo build --release` exit 0. (`cmake --build` re-verify at close.)
- Scope: `include/`, `rust/`, `cpp/core/**`, `cpp/ui/**`, `docs/`.

## How to test now

1. `cmake --build build --config Release` (cmake+cargo on PATH).
2. Relink core_test + gui.exe (g++ lines in file headers).
3. `build/core_test.exe` → liked + playlists + playlist tracks.
4. `build/gui.exe` → Liked (as before); Playlists → Open a row (Play) →
   tracks listed → Back; Play/Add tracks at both levels.
5. Report: drill-in correct? Back returns right? Play-from-playlist audible?

## librespot rev

Unchanged: `a1b66d3c`, defaults. No manifest change (no new deps).

## Version control

- `phase-10: Liked Songs end-to-end`
- `phase-10: checkpoint Liked docs`
- `phase-10: Playlists end-to-end`
- `phase-10: checkpoint Playlists docs` (this file + `docs/DECISIONS.md`, `docs/API.md`)
- Pushed to `origin/main`.
