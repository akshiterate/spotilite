# Phase 10 Summary — Library

Final report. All four subsections live-verified (see Verified).

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

## Verified (all subsections)

- `build/core_test.exe`: liked (47), playlists (22) + context drill,
  albums (2) + drill (44 tracks), artists (4) + top-tracks drill — all
  real data, exit 0. Artists run triggered the auto re-login live.
- `build/gui.exe` relinked, smoke alive (all modes compile into the same
  LibView; click paths user-tested per checkpoint + final).
- `cargo build --release` + `cmake --build build --config Release` exit 0.
- `git status` clean; scope respected (`include/`, `rust/`,
  `cpp/core/**`, `cpp/ui/**`, `docs/`).

## Done in subsection 3 — Albums

- Probed first: `/v1/me/albums` AND `/v1/albums/{id}/tracks` both work
  (only playlist items are restricted), so albums stay pure Web API with
  real totals — no context detour needed.
- `spotify_albums` + `spotify_album_tracks` (URI or raw id),
  `Player::albums/albumTracks`, core_test albums section with drill-in.
- GUI: Albums button + ALBUM_TRACKS drill reusing the library view; Back
  generalized (remembers mode + page instead of playlists-only).
- Live: 2 saved albums listed; 44-track drill with real names; exit 0.
- GUI relinked + smoke alive.

## Done in subsection 4 — Artists

- Followed artists need `user-follow-read`, absent from the original
  login. Added to scopes + auto re-login: on 403 "Insufficient client
  scope", the bridge reuses the cached client id, opens the browser once,
  and retries — no env, no file deleting.
- `spotify_followed_artists` (cursor-walked internally, same offset paging
  outward) + `spotify_artist_tracks` (context top tracks, first page,
  total -1 — same machinery as playlist tracks).
- `Player::followedArtists/artistTracks`, core_test artists section with
  drill-in, GUI ARTISTS + ARTIST_TRACKS modes in the shared library view.
- Live coordinated re-login worked first try: 4 followed artists, top
  tracks of the first with correct names; exit 0.

## How to test now

1. `cmake --build build --config Release` (cmake+cargo on PATH).
2. Relink core_test + gui.exe (g++ lines in file headers).
3. `build/core_test.exe` → liked + playlists + drill + albums + drill +
   artists + drill. (First artists run opens the browser once for the new
   scope if the login predates it.)
4. `build/gui.exe` → Liked / Playlists / Albums / Artists, drill into
   rows, Back, Play/Add at every level.
5. Report: drill-in correct? Back returns right? Play-from-library audible?

## librespot rev

Unchanged: `a1b66d3c`, defaults. No manifest change (no new deps).

## Version control

- `phase-10: Liked Songs end-to-end`
- `phase-10: checkpoint Liked docs`
- `phase-10: Playlists end-to-end`
- `phase-10: checkpoint Playlists docs`
- `phase-10: Albums end-to-end`
- `phase-10: checkpoint Albums docs`
- `phase-10: Artists end-to-end`
- `phase-10: finalize docs` (this file + `docs/DECISIONS.md`, `docs/API.md`)
- All pushed to `origin/main` at phase completion.
