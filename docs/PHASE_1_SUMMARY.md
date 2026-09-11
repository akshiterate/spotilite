# Phase 1 Summary — librespot Playback Proof

Final report. Audio playback verified by the user (see Verified).

## Goal

Prove Spotify playback works before building the application around it
(plans.md Phase 1). Auth method per user decision: discovery provisioning
(no login on our side); Premium account confirmed by user.

## Done so far

- `rust/librespot-bridge/src/bin/headless.rs`: minimal headless Connect
  receiver. Flow: stable device id (`device-id` file) + `Cache` under
  `%LOCALAPPDATA%\spotilite\cache` (outside repo, no secrets committable);
  reuse cached credentials or advertise `spotilite` via mDNS until the
  official app provisions credentials; `Session` + rodio default sink
  (WASAPI) + soft mixer + `Player`; `Spirc` Connect device named
  `spotilite`; no auto-activate (launch never hijacks existing playback);
  Ctrl+C quits.
- `rust/librespot-bridge/Cargo.toml`: added `tokio` (rt-multi-thread,
  macros, signal), `log`, `env_logger`, `futures-util`. librespot pin
  unchanged (`a1b66d3c`, defaults).
- Pinned-API deviations from the `play_connect` example: `Cache` import is
  `core::cache::Cache`; `audio_backend::find`/`mixer::find` return `Option`
  (mapped to `Error::unavailable`); `Discovery::builder(device_id,
  client_id).name().device_type(Computer).launch()` yields `Credentials`
  via `Stream`; `Cache::new` params must share one generic type (bound
  `files_dir` first). No phase-goal change (1.8 rule 4 satisfied).

## Verified

- `cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release` +
  `cmake --build build --config Release` → `rust-bridge` + `spotilite`
  succeed (exit 0); `build/spotilite.exe` prints `spotilite` /
  `Build successful.`
- `target\release\headless.exe` smoke-tested repeatedly: starts, uses
  cached credentials, authenticates, stays alive, exits cleanly on kill.
- USER manual test (required by plan): device visible in official app,
  playback transferred, **audio audible through Windows audio (Realtek /
  WASAPI)**. User confirmed "yes it works".
- Incidents during user testing: (1) first run played faintly then the
  process died — no log captured, never reproduced; if it recurs, capture
  full terminal text before anything else. (2) Device initially showed as
  `librespot` (pinned default `ConnectConfig.name`) — fixed by setting the
  name explicitly. (3) Launch used to interrupt app playback
  (`spirc.activate()`) — auto-activate removed. (4) Rebuilds fail with
  `os error 5` while the receiver is running (exe locked) — close it
  (Ctrl+C) before rebuilding.

## librespot rev

Unchanged: `a1b66d3c8a14e55a9572a9e17467150dca618c9a`, defaults. `Cargo.lock`
delta (tokio/log/env_logger/futures-util) committed separately per 1.16.

## How to test now

1. Ensure `cmake` + `cargo` on PATH, then
   `cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release`
   and `cmake --build build --config Release`.
2. Run `.\target\release\headless.exe` (close it with Ctrl+C before any
   rebuild — the running exe locks the file).
3. In the official Spotify app (same network, Premium): Connect to a device
   -> `spotilite`. (If a stale old name shows, restart the Spotify app
   once — it caches device names per device id.)
4. Play any track in the app. Sound comes from Windows audio.
5. Ctrl+C quits. Later runs reuse cached credentials.

Expected: device visible as `spotilite`, playback transfers, audio audible.

## Rename to `spotilite`

After audio verification the user requested the name `spotilite`
everywhere. Applied to `plans.md` (`Spotify-lite` x10, `spotify-lite` x7,
`SPOTIFY-LITE` x1), `README.md` title, CMake project/exe
(`build/spotilite.exe`), `main.cpp` banner, headless `DEVICE_NAME`/banner/
comments. Closed historical docs (`PHASE_-1/0` summaries, earlier
`DECISIONS.md` lines) intentionally left as-was (append-only records);
details in `docs/DECISIONS.md`.

## Version control

- `1eb49e6 phase-1: add headless Connect receiver (discovery provisioning)`
- `346cf7f phase-1: Cargo.lock for receiver deps`
- `6f0ac1a phase-1: checkpoint 2 docs`
- `b758cc1 phase-1: name Connect device Spotify-lite, no auto-activate on launch`
- `phase-1: rename project to spotilite` (plans.md, README, CMake target,
  main.cpp, headless device name)
- `phase-1: finalize docs` (this file + `docs/DECISIONS.md`)
- All pushed to `origin/main` at phase completion.
