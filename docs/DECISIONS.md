# Decisions

Append-only log: date, decision, why, alternatives rejected.

- 2026-09-11: Phase -1 uses `main` as default branch with remote
  `https://github.com/akshiterate/spotilite.git`. No code, no dependencies,
  no build system yet (build system is Phase 0 scope).
- 2026-09-11: `plans.md` is versioned in the repo per explicit user request
  during Phase -1 (exception to the default Phase -1 file scope
  `README.md, .gitignore, docs/`).
- 2026-09-11 (Phase 0): No C++/Rust toolchain on PATH (no cmake, cargo,
  rustc, cl/MSVC). Per 1.11 stopped and asked; user approved
  "Install CMake + Rust now". Installed without admin: portable CMake 4.4.3
  (Kitware release zip, `%LOCALAPPDATA%\spotilite-tools`, appended to user
  PATH) + rustup stable-x86_64-pc-windows-gnu (rustc/cargo 1.98.1).
  Rejected: Chocolatey system install (needs admin), manual MSVC install
  (large, blocks progress), changing global/system env.
- 2026-09-11 (Phase 0): Compiler = existing MinGW-w64 GCC 16.1.0, CMake
  generator `MinGW Makefiles`, `-DCMAKE_BUILD_TYPE=Release`. Rejected: MSVC
  (not installed). Consequence/deviation: single-config layout, exe at
  `build/spotify-lite.exe` instead of the plan's MSVC-style
  `build/Release/spotify-lite.exe`. Revisit generator choice if Phase 12
  (Windows integration) needs MSVC-only tooling.
- 2026-09-11 (Phase 0): CMake orchestrates Cargo via `rust-bridge`
  `add_custom_target(ALL ... cargo build)` + `add_dependencies`.
  Rejected: ExternalProject/corrosion (overkill for Phase 0, 1.4).
  Bridge `crate-type = staticlib+rlib` so Phase 2 can link without manifest
  churn.
- 2026-09-11 (Phase 0): Pinned initial librespot rev
  `a1b66d3c8a14e55a9572a9e17467150dca618c9a` (from
  `git ls-remote https://github.com/librespot-org/librespot HEAD`).
  Manifest at the pinned rev inspected per 1.8: librespot v0.8.0, edition
  2024, rust-version 1.85 (satisfied by rustc 1.98.1). No API use yet, so no
  drift to adapt; phase goal unchanged. Feature flags: defaults
  (`native-tls`, `rodio-backend`, `with-libmdns`) — exactly what Phase 1
  needs (SChannel TLS, WASAPI audio, pure-Rust mDNS; no external C libs on
  Windows). Rejected: `default-features = false` (drops audio/TLS/discovery
  Phase 1 requires), rustls variants (native-tls uses the Windows cert
  store). `cargo tree -i librespot` shows only `librespot-bridge` as reverse
  dep; 393 packages in `Cargo.lock`, committed separately per 1.16. Pinned
  dep release-builds clean (first compile 4m11s, warms cache for Phase 1).
- 2026-09-11 (Phase 1): Auth = discovery (zeroconf) provisioning, per user
  choice on 1.11 STOP-and-ask (Premium confirmed). Rationale: no login or
  secrets on our side; matches the plan's success flow (device visible in
  the official app before any login). Rejected: OAuth browser login
  (needs client-id/redirect-port UX, heavier for a proof). Device name
  `Spotify-lite`, `DeviceType::Computer` (shows correctly in app device
  list; default would be Speaker).
- 2026-09-11 (Phase 1): Stable device id persisted in a `device-id` file:
  `SessionConfig::default().device_id` is a fresh UUID per run and
  credential blobs are device-bound, so reuse requires stability. File
  lives with the cache (below), not in the repo.
- 2026-09-11 (Phase 1): Receiver state (credentials.json, volume, audio
  files) under `%LOCALAPPDATA%\spotilite\cache`, created at runtime —
  outside the repo so 1.15 (never commit credentials) holds by
  construction. Rejected: repo-relative `.cache/` (would risk committing
  secrets).
- 2026-09-11 (Phase 1): Receiver as `src/bin/headless.rs` binary in the
  bridge crate (Phase 1 Allowed files: `rust/**`). `cargo build` picks up
  bins automatically, so no CMake change needed. Pinned-API notes: `Cache`
  is `core::cache::Cache`; audio/mixer `find()` return `Option`;
  `Discovery` is a `Stream<Item = Credentials>`; `Cache::new` args share
  one generic type.
- 2026-09-11 (Phase 1): Connect device name set explicitly to the project
  name (`ConnectConfig::default().name` is literally `"librespot"`, which
  is what users saw). Auto-activate on launch removed: starting the
  receiver must not hijack playback already playing elsewhere; the user
  transfers playback explicitly. Matches the plan's success flow.
- 2026-09-11 (Phase 1, after audio verification): user requested the name
  `spotilite` everywhere. Applied to `plans.md`, `README.md`, CMake
  project/exe (`build/spotilite.exe`), `main.cpp` banner, headless
  `DEVICE_NAME`/banner/comments. Closed records (`PHASE_-1/0` summaries,
  earlier lines here) left as-was. Note: Spotify apps cache device names
  per device id — restart the app if a stale name lingers after upgrade.
