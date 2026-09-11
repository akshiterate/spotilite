# Phase 3 Summary — C++ Player Core

> MID-PHASE CHECKPOINT A (not the final Phase 3 report).
> `spotify_poll_event` ABI addition done and building. Remaining: C++ core
> (`cpp/core/` player + queue + state), core test program, end-to-end
> verify, `PHASE 3 COMPLETE`.

## Goal

First real C++ application core: player, queue, playback state, commands,
events (plans.md Phase 3). Frontends (later) talk to the core, never to
Rust directly.

## Done so far

- `include/spotify_bridge.h`: added event polling surface — `SPOTIFY_EVENT_*`
  codes (NONE/TRACK_STARTED/PLAYING/PAUSED/TRACK_ENDED/SEEKED/
  VOLUME_CHANGED/POSITION), `SpotifyEvent` struct
  (`type/position_ms/volume/uri[128]`), `spotify_poll_event()` contract.
  Additive only; no existing signature touched.
- `rust/librespot-bridge/src/lib.rs`: handle owns a
  `Mutex<PlayerEventChannel>`; `PlayerConfig::position_update_interval`
  set to 1s; poll drains non-modelled events silently and maps
  Loading/Playing/Paused/EndOfTrack/Unavailable/Stopped/Seeked/
  VolumeChanged/PositionChanged/PositionCorrection. `cargo build --release`
  exit 0, no warnings.
- Header note (`read-only unless ABI bug`, Phase 3 scope): this is the
  plan-foreseen poll function (1.7 names `spotify_poll_event` explicitly),
  not a redesign — recorded in `docs/DECISIONS.md`.

## NOT yet done

C++ core, core test program, link + run. No user-runnable change yet.

## librespot rev

Unchanged: `a1b66d3c`, defaults. No manifest change.

## Version control

- `phase-3: add spotify_poll_event ABI` (header + lib)
- `phase-3: checkpoint A docs` (this file + `docs/DECISIONS.md`, `docs/API.md`)
- Pushed to `origin/main`.
