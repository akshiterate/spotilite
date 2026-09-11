# Phase 5 Summary — Metadata

Final report, pending user accuracy confirmation (see Verified).

## Goal

Expose track name, artist, album, duration, track ID, Spotify URI from
librespot metadata — no Web API (plans.md Phase 5).

## Done so far

- `include/spotify_bridge.h`: `SpotifyMetadata` struct (title/artist/album
  strings, `duration_ms`, uri, base62 track_id) +
  `spotify_current_metadata()` contract. Additive only.
- `rust/librespot-bridge/src/lib.rs`: handle keeps the last loaded URI
  (`Mutex<String>`, canonicalized at load); metadata fetched on demand via
  `Metadata::get` on the handle runtime. `Track` carries album + artists
  inline, so one request covers everything; episodes map to title-only.
  No track loaded → `NOT_CONNECTED` + message; fetch failure → `INTERNAL`.
- `cpp/core/player.h/.cpp`: `spotilite::TrackMetadata` +
  `Player::metadata()` (bool + `lastError()`).
- `cpp/core/core_test.cpp`: prints all six fields after load and checks
  title/artist/album non-empty, duration sane, URI echo.
- No Web API, no new deps, no new system libs. TUI render untouched
  (Phase 5 file scope is core + docs; display wiring is later UI work —
  the test app is the acceptance vehicle per the plan's "TUI/test
  application").

## Verified

- `cargo build --release` exit 0, first try; direct-g++ link unchanged.
- `build/core_test.exe spotify:track:4PJEK76V3A1S0XzZJuTWh7`:

```text
title: Love Is A Long Road
artist: Tom Petty
album: Full Moon Fever
duration_ms: 247600
uri: spotify:track:4PJEK76V3A1S0XzZJuTWh7
track_id: 4PJEK76V3A1S0XzZJuTWh7
```

  All metadata checks `ok`, exit 0. Duration units confirmed as
  milliseconds (247600 ≈ 4:07, matches the song).
- Accuracy confirmation asked of the user (they supplied the link);
  `PHASE 5 COMPLETE` follows a "correct" answer.

## librespot rev

Unchanged: `a1b66d3c`, defaults. No manifest change.

## Version control

- `phase-5: add metadata query (ABI + core + test)`
- `phase-5: finalize docs` (this file + `docs/DECISIONS.md`, `docs/API.md`)
- Pushed to `origin/main`.
