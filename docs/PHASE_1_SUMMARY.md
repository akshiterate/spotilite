# Phase 1 Summary — librespot Playback Proof

> MID-PHASE CHECKPOINT 2 (not the final Phase 1 report).
> Receiver code complete, builds, smoke-tested. Awaiting USER manual test:
> run the receiver, select it in the Spotify app, play a track, confirm
> audio. Final `PHASE 1 COMPLETE` + docs polish after that confirmation.

## Goal

Prove Spotify playback works before building the application around it
(plans.md Phase 1). Auth method per user decision: discovery provisioning
(no login on our side); Premium account confirmed by user.

## Done so far

- `rust/librespot-bridge/src/bin/headless.rs`: minimal headless Connect
  receiver. Flow: stable device id (`device-id` file) + `Cache` under
  `%LOCALAPPDATA%\spotilite\cache` (outside repo, no secrets committable);
  reuse cached credentials or advertise `Spotify-lite` via mDNS until the
  official app provisions credentials; `Session` + rodio default sink +
  soft mixer + `Player`; `Spirc` Connect device, `activate()`; Ctrl+C quits.
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

- `cmake --build build --config Release` → `rust-bridge` + `spotify-lite`
  succeed (exit 0), incl. new `headless` binary.
- Smoke test: `target\release\headless.exe` ran 20s, printed banner +
  advertising lines, stayed alive awaiting selection, no stderr errors.
- `git status`: only bridge manifest, `Cargo.lock`, `src/bin/` touched.
  Cache (`device-id`, `files/`) created only under `%LOCALAPPDATA%`.

## NOT yet verified (requires user)

Audio actually playing through Windows audio. See "How to test now".

## librespot rev

Unchanged: `a1b66d3c8a14e55a9572a9e17467150dca618c9a`, defaults. `Cargo.lock`
delta (tokio/log/env_logger/futures-util) committed separately per 1.16.

## How to test now

1. Ensure `cargo` on PATH, then `cmake --build build --config Release`.
2. Run `.\target\release\headless.exe` (first run: prints "Advertising as
   Spotify-lite ...").
3. In the official Spotify app (same network, Premium): Connect to a device
   -> `Spotify-lite`. Receiver prints "Credentials received..." then
   "Connected as <you>."
4. Play any track in the app. Sound must come from Windows audio.
5. Ctrl+C quits. Second run reuses cached credentials (no re-select needed).

Expected: device visible in app, playback transfers, audio audible.

## Version control

- `phase-1: add headless Connect receiver (discovery provisioning)`
- `phase-1: Cargo.lock for receiver deps`
- `phase-1: checkpoint 2 docs` (this file + `docs/DECISIONS.md`, `docs/API.md`)
- Pushed to `origin/main`.
