# Phase 8 Summary — Search

Final report. Live-tested end to end (see Verified).

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
- `cpp/core/player.h/.cpp`: `SearchKind` (+`SEARCH_ANY`; `SEARCH_ALL`
  collides with a Windows SDK macro — renamed), `SearchResult`,
  `Player::search()` into a `vector` (bool + `lastError()`).
- `cpp/core/core_test.cpp`: search section — without login expects the
  clean AUTH error; with login prints rows and checks non-empty.
- `cpp/ui/gui.h/.cpp`: Search input + Find (async like metadata),
  results listbox (`name — subtitle`), Play-result button (tracks load;
  other kinds report "only tracks can be played yet"). Window 660x700.
- Live-test fix: Spotify sometimes returns null playlist entries —
  empty-URI rows are now filtered in the bridge.

## Verified

- Error path (no env, no cache): `search without login fails clean`,
  browser never opens. Exit 0.
- LIVE coordinated run (user completed the browser login while the probe
  waited): `search radiohead` → 5 tracks + 5 artists + 5 albums +
  4 playlists, all real (Creep, OK Computer, ...), exit 0.
- Cache priming proven: second run with NO env used the cached refresh
  token (no browser) and played the searched track end to end —
  `spotify:track:70LcF31zb1H0PyJoS1Sx1r` (Creep, metadata
  Creep/Radiohead/Pablo Honey/3:58), full playback + artwork + queue
  checks all `ok`, exit 0.
- `build/gui.exe` relinked and smoke-tested alive; interactive search →
  play is user-testable in the GUI now.
- `cargo build --release` + `cmake --build build --config Release` exit 0.
- `git status` clean; scope respected (`include/`, `rust/`,
  `cpp/core/**`, `cpp/ui/**`, `docs/`; TUI/CMakeLists untouched).

## librespot rev

Unchanged: `a1b66d3c`, defaults. Manifest delta is only the three search deps.

## Version control

- `phase-8: add OAuth search ABI`
- `phase-8: Cargo.lock for search deps`
- `phase-8: checkpoint A docs`
- `phase-8: add core search + GUI results`
- `phase-8: empty-URI row filter + live-test green`
- `phase-8: finalize docs` (this file + `docs/DECISIONS.md`, `docs/API.md`)
- All pushed to `origin/main` at phase completion.
