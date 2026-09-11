# Phase 3 Summary — C++ Player Core

Final report. All acceptance criteria verified (see Verified).

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
- `cpp/core/queue.h` (header-only): URI list + index; add/clear/next/
  previous/select. Remove/reorder deferred to Phase 9.
- `cpp/core/player.h/.cpp`: RAII `spotilite::Player` over the ABI —
  connect/loadUri/play/pause/resume/seek/setVolume/next/previous/enqueue,
  `pollEvent`/`drainEvents` with a mirrored `PlaybackState`
  (connected/playing/currentUri/volume/positionMs). `loadUri` resets the
  queue to the single URI (solo play); `next`/`previous` advance then load.
  Errors: bool + `lastError()` (no exceptions except throwing ctor).
- `cpp/core/core_test.cpp`: scripted test through the core only (no Rust
  symbols). With a URI arg: load → 8s event watch → pause → resume →
  seek 30s → volume 0.4, printing every event. Always: connect, garbage-URI
  rejection, queue navigation incl. player next/previous wiring. Exit 0/1.
- Link (direct `g++ -std=c++17`, same sys libs as bridge_test, no CMake
  change): `core_test.cpp + core/player.cpp + staticlib → build/core_test.exe`.

## Verified

- `cargo build --release` and `cmake --build build --config Release` exit 0.
- `build/core_test.exe spotify:track:4PJEK76V3A1S0XzZJuTWh7`: connect,
  load, live `track_started/playing/position/...` events with advancing
  positions, pause (`paused` event), resume, seek 30s (`seeked pos=29987`
  then positions continue), volume 0.4 mirrored in state — all `ok`.
- `build/core_test.exe` (no URI): connect, garbage rejection, all queue +
  player next/previous wiring checks — `CORE TEST DONE: all passed`, exit 0.
- Test bug found and fixed (not core): expectations assumed an empty queue
  after `loadUri`, which resets it — cleared explicitly in the test.
- `git status` clean; Phase 3 scope respected (`cpp/core/**`,
  header ABI addition per checkpoint, `docs/`; CMakeLists untouched).

## librespot rev

Unchanged: `a1b66d3c`, defaults. No manifest change (no new deps).

## Version control

- `phase-3: add spotify_poll_event ABI` (header + lib)
- `phase-3: checkpoint A docs`
- `phase-3: add C++ player core + core test` (`cpp/core/` x5)
- `phase-3: finalize docs` (this file + `docs/DECISIONS.md`, `docs/API.md`)
- All pushed to `origin/main` at phase completion.
