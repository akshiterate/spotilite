# Phase 6 Summary — Artwork

Final report. All criteria verified at cache level (see Verified).

## Goal

Basic album artwork: background loading, no blocking, small disk +
memory caches, 128px + 256px (plans.md Phase 6).

## Done so far

- `rust/librespot-bridge/Cargo.toml`: `image 0.25.10`
  (`default-features = false`, `jpeg` + `bmp` only). Lock committed
  separately per 1.16; `cargo tree -i image --depth 1` shows only
  `librespot-bridge` as reverse dep.
- `include/spotify_bridge.h` (additive): `SPOTIFY_ART_128/256`,
  `SPOTIFY_EVENT_ARTWORK_READY 8`, `spotify_request_artwork`,
  `spotify_artwork_state`, `spotify_artwork_path` + cache-layout contract.
- `rust/librespot-bridge/src/lib.rs`: cover source = widest
  `Album`/`Episode` cover via `spclient.get_image` (librespot-native, no
  Web API, no HTTP client). Background `rt.spawn` task decodes, resizes
  (Triangle) to both sizes, stores BMPs under
  `%LOCALAPPDATA%\spotilite\cache\art\<track-id>-<size>.bmp`, reports via
  art channel drained inside `spotify_poll_event`. Memory: 128px bytes,
  FIFO cap 8, promoted disk→mem on sync queries (no cross-thread cache
  writes). Failures: `log::warn` + never-ready state (documented).
- `cargo build --release` exit 0. Two pinned-API fixes during build:
  `Images` lives at `metadata::image::Images`; `BmpEncoder::encode`
  needs no trait import.
- `cpp/core/player.h/.cpp`: `requestArtwork` / `artworkReady(uri, size)` /
  `artworkPath` (bool/string + `lastError()`, no Rust past the core).
- `cpp/core/core_test.cpp`: artwork section — request, ≤20s wait for
  `ARTWORK_READY` with echoing URI, ready-state for both sizes, BMP dims
  read from headers (expects exactly 128x128 + 256x256), garbage-URI and
  bad-size rejection. Same direct-g++ link into `build/core_test.exe`.

## Verified

- Full `build/core_test.exe spotify:track:4PJEK76V3A1S0XzZJuTWh7`: every
  artwork check `ok` (ready event, echo, both sizes cached, exact BMP
  dims, both rejections), playback audibly continuing throughout
  (position events flow during the fetch — non-blocking proven), then all
  pre-existing checks still `ok`; `CORE TEST DONE: all passed`, exit 0.
- Second run reused the disk cache (idempotent fast path) — also green.
- `cargo build --release` + `cmake --build build --config Release` exit 0.
- `git status` clean; scope respected (`include/`, `rust/`,
  `cpp/core/**`, `docs/`; TUI/CMakeLists untouched).
- User-visible display: none yet by design (no GUI until Phase 7; no
  terminal images). The acceptance adaptation from checkpoint A stands:
  cache-level proof now, visual proof in Phase 7.

## librespot rev

Unchanged: `a1b66d3c`, defaults. Manifest delta is only the `image` dep.

## Version control

- `phase-6: add artwork fetch+cache ABI`
- `phase-6: Cargo.lock for image dep`
- `phase-6: checkpoint A docs`
- `phase-6: add C++ artwork proof` (core helpers + test section)
- `phase-6: finalize docs` (this file + `docs/DECISIONS.md`, `docs/API.md`)
- All pushed to `origin/main` at phase completion.
