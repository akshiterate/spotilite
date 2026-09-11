# Phase 2 Summary — C ABI Bridge

> MID-PHASE CHECKPOINT A (not the final Phase 2 report).
> C ABI header + Rust implementation done and building. Remaining:
> C++ `bridge_test` program, link, end-to-end verify (needs a track URI),
> push/finalize, `PHASE 2 COMPLETE`.

## Goal

Control librespot from C++ through a small C ABI (plans.md Phase 2).

## Done so far

- `include/spotify_bridge.h`: opaque `SpotifyPlayer*`, error codes
  (`SPOTIFY_OK`/`NULL_ARG`/`NOT_CONNECTED`/`AUTH`/`BAD_URI`/`AUDIO`/
  `INTERNAL`), `create/destroy/connect/load_uri/play/pause/resume/seek/
  set_volume/last_error` with ownership + threading contract in comments.
- `rust/librespot-bridge/src/lib.rs`: full implementation. Handle owns a
  2-worker Tokio runtime, `Session`, `Player`, `Mixer`, credential `Cache`.
  `connect` uses the Phase 1 machine-local cache (same device id) and is
  idempotent; `load_uri` accepts playable URIs and autoplays;
  `resume == play`; volume float clamps to `u16`; per-thread last-error
  buffer behind `spotify_last_error()`.
- Pinned-API adaptations (1.8): no next/previous — librespot 0.8 `Player`
  has no C++-drivable queue (`load/play/pause/seek` only); queue ops belong
  to the Phase 3/9 C++ core. Auth vs transport failures distinguished via
  `AuthenticationError` downcast. `Cache` is `core::cache::Cache`;
  audio/mixer `find()` return `Option`. Phase goal unchanged.

## Verified

- `cargo build --release` → exit 0, no warnings. Staticlib produced.
- CMake untouched; full `cmake --build` re-verify deferred to milestone B.

## NOT yet done

C++ test program, staticlib link, connect/load/play/pause/resume/seek/
volume/destroy end-to-end run. The run needs a `spotify:track:...` URI
(user supplies one at test time) and audible-output confirmation.

## librespot rev

Unchanged: `a1b66d3c`, defaults. No manifest change (no new deps).

## Version control

- `phase-2: add C ABI header + Rust bridge` (this checkpoint's code)
- `phase-2: checkpoint A docs` (this file + `docs/DECISIONS.md`, `docs/API.md`)
- Pushed to `origin/main`.
