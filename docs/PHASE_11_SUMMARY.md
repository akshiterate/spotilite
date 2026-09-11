# Phase 11 Summary — Configuration

Final report. All verified (see Verified).

## Goal

Machine-local `spotilite.toml` for real settings only (plans.md Phase 11).
Cache location, Connect and startup behaviour skipped: no corresponding
features exist yet (the plan itself forbids speculative settings).

## Done so far

- `%LOCALAPPDATA%\spotilite\spotilite.toml`, auto-created with commented
  defaults on first run. Every key optional; wrong types fall back per
  key with a warning, never a crash. Unknown keys ignored.
- Implemented keys (all take effect, all verified):
  - `device_name` → headless Discovery + Connect names.
  - `bitrate` (96/160/320, else 160) → `PlayerConfig` in bridge +
    headless (verified `Bitrate` enum at the pinned rev).
  - `normalisation` (bool) → `PlayerConfig` in both.
  - `volume` (0..1 clamped) → mixer init in both (display already
    mirrors the mixer since the post-Phase 9 fix).
  - `cache_size_mb` (≥64) → audio-cache `FsSizeLimiter` bytes in both.
- `rust/librespot-bridge/Cargo.toml`: `toml 0.8.23`
  (default-features off n/a — pure parser; only reverse dep is the
  bridge). Lock committed separately per 1.16.
- `pub load_config()` shared by lib + headless binary (no duplication);
  `spotify_config_summary()` ABI for tests/diagnostics.
- `cpp/core/core_test.cpp` prints the summary at startup; GUI Settings
  window shows the file path + restart note (no C++ TOML parser needed).
- Rust owns the file (C++ never parses it): single parser, no skew.

## Verified

- Deleted config → defaults file auto-created with helpful content;
  summary prints defaults; player creates fine.
- Custom values (device_name, 320, false, 0.33, 100, bogus keys) →
  summary reflects all, bogus ignored, player creates fine.
- `bitrate = 999` → falls back to 160.
- `cargo build --release` (86s for toml) + `cmake --build build
  --config Release` exit 0. Machine restored (test config deleted).
- `git status` clean; scope respected (`rust/`, `include/`,
  `cpp/core/**` test, `cpp/ui/**` settings text, `docs/`).

## librespot rev

Unchanged: `a1b66d3c`, defaults. Manifest delta is only the `toml` dep.

## Version control

- `phase-11: spotilite.toml configuration`
- `phase-11: Cargo.lock for toml`
- `phase-11: finalize docs` (this file + `docs/DECISIONS.md`, `docs/API.md`)
- All pushed to `origin/main` at phase completion.
