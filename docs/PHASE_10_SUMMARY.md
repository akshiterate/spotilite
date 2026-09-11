# Phase 10 Summary — Library

> MID-PHASE CHECKPOINT 1 — Liked Songs works (ABI + core + GUI + live
> verified). Remaining subsections: 2. Playlists, 3. Albums, 4. Artists.
> Per the plan, STOPPING here for user testing before continuing.

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

## Verified (subsection 1)

- `build/core_test.exe`: `liked tracks` ok, `liked total: 47`, 5 real
  rows printed, `liked total sane` ok, exit 0 (cached token, no browser).
- `build/gui.exe` relinked, 14s smoke alive. Button wiring is
  click-unverifiable from here — user tests at this checkpoint.
- `cargo build --release` exit 0. (`cmake --build` re-verify at close.)
- Scope: `include/`, `rust/` (required: network lives in Rust per 1.6,
  as in Phase 8), `cpp/core/**`, `cpp/ui/**`, `docs/`.

## How to test now

1. `cmake --build build --config Release` (cmake+cargo on PATH).
2. Relink core_test + gui.exe (g++ lines in file headers).
3. `build/core_test.exe` → liked rows + total.
4. `build/gui.exe` → Liked → page through with < >, Play liked, Add liked.
5. Report: rows correct? paging works? Play-from-liked audible?

## librespot rev

Unchanged: `a1b66d3c`, defaults. No manifest change (no new deps).

## Version control

- `phase-10: Liked Songs end-to-end`
- `phase-10: checkpoint Liked docs` (this file + `docs/DECISIONS.md`, `docs/API.md`)
- Pushed to `origin/main`.
