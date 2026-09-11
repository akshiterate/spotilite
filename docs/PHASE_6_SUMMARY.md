# Phase 6 Summary — Artwork

> MID-PHASE CHECKPOINT A (not the final Phase 6 report).
> Artwork fetch + cache + ABI done and building. Remaining: C++ proof
> via `core_test` (request → ARTWORK_READY → BMP files verified from the
> header), docs finalize, `PHASE 6 COMPLETE`.

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

## NOT yet done

C++ side proof (core `requestArtwork` helpers + `core_test` artwork
section reading BMP dims from headers) and any user-visible display —
no GUI exists yet, so "appears beside the track" is proven at the cache
level now and lands visually in Phase 7. No TUI image support (terminal).

## librespot rev

Unchanged: `a1b66d3c`, defaults. Manifest delta is only the `image` dep.

## Version control

- `phase-6: add artwork fetch+cache ABI`
- `phase-6: Cargo.lock for image dep`
- `phase-6: checkpoint A docs` (this file + `docs/DECISIONS.md`, `docs/API.md`)
- Pushed to `origin/main`.
