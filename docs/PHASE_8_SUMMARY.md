# Phase 8 Summary — Search

> MID-PHASE CHECKPOINT A (not the final Phase 8 report).
> OAuth + search ABI done and building. Remaining: C++ core search +
> GUI results list + LIVE browser-auth test (client_id received, needs a
> coordinated run: the login page opens on the user's desktop), then
> finalize + `PHASE 8 COMPLETE`.

## Goal

Spotify search (tracks, artists, albums, playlists), paginated, Web API
isolated behind the C++ core (plans.md Phase 8).

## Done so far

- Auth decision (1.11 stop-and-ask, user chose): full Web API via PKCE
  OAuth, not the tracks-only SpClient context hack. User created a
  Spotify app, allowlisted `http://127.0.0.1:8898/login`, pasted the
  client_id (used at runtime only — never committed).
- `rust/librespot-bridge/Cargo.toml`: `librespot-oauth` (same pinned
  rev), `reqwest 0.12` (defaults = SChannel TLS, per Phase 0), `serde_json
  1`. All already compiled as transitive deps — 49s incremental build.
  Lock committed separately per 1.16; trees recorded in this file's
  history (oauth/reqwest/serde_json all reverse-depend only via bridge).
- `include/spotify_bridge.h` (additive): `SPOTIFY_SEARCH_*` kind bits +
  ALL mask, `SpotifySearchItem` (kind/uri/name/subtitle/duration_ms),
  `spotify_search()` contract (paged, blocking).
- `rust/librespot-bridge/src/lib.rs`: PKCE browser login on first use
  (client_id from `SPOTILITE_CLIENT_ID` env, then cached); refresh token
  in `%LOCALAPPDATA%\spotilite\cache\webapi.json` (never repo) with
  in-memory access token + 60s expiry margin; 401 → single refresh +
  retry; type CSV in fixed order; per-type struct mapping (track artists,
  album artists, playlist owner); count-capped fill, never over-reads.
  Scopes: `user-library-read` + `playlist-read-private` (search needs
  none; pre-covers Phase 10 so one login suffices).
- `cargo build --release` exit 0, no warnings.

## NOT yet done

C++ `Player::search`, GUI results list + play-from-results, and the live
test: first `spotify_search` call opens the browser on the user's machine
for the one-time login, then results must return. Needs coordination.

## librespot rev

Unchanged: `a1b66d3c`, defaults. Manifest delta is only the three search deps.

## Version control

- `phase-8: add OAuth search ABI`
- `phase-8: Cargo.lock for search deps`
- `phase-8: checkpoint A docs` (this file + `docs/DECISIONS.md`, `docs/API.md`)
- Pushed to `origin/main`.
