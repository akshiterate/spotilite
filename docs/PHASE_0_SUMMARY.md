# Phase 0 Summary — Repository + Build

> MID-PHASE CHECKPOINT 1 (not the final Phase 0 report).
> Milestones A+B done. Remaining: librespot rev pin, final full verify,
> push, final `PHASE 0 COMPLETE` report.

## Goal

Minimal buildable project: CMake builds the C++ executable AND the Rust
workspace (plans.md Phase 0).

## Done so far

- `CMakeLists.txt`: project `spotify-lite`, C++17, `spotify-lite` exe from
  `cpp/app/main.cpp`, plus `rust-bridge` custom target that runs
  `cargo build [--release]` so CMake orchestrates the Rust build.
- `cpp/app/main.cpp`: prints `Spotify-lite` / `Build successful.`, exit 0.
- `Cargo.toml`: workspace, member `rust/librespot-bridge`, resolver 2.
- `rust/librespot-bridge`: stub crate v0.1.0, `crate-type staticlib+rlib`,
  `bridge_version()` smoke symbol. No librespot dependency yet (pin is the
  next milestone).
- Toolchains (user-approved install, no admin): portable CMake 4.4.3,
  rustup stable-x86_64-pc-windows-gnu, rustc/cargo 1.98.1, MinGW GCC 16.1.0.
  Details + rejected alternatives in `docs/DECISIONS.md`.
- Generator: `MinGW Makefiles` (`-DCMAKE_BUILD_TYPE=Release`). Deviation:
  exe lands at `build/spotify-lite.exe`, not `build/Release/spotify-lite.exe`
  (latter is the MSVC multi-config layout). Recorded in `docs/DECISIONS.md`.

## Verified

- `cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release`
- `cmake --build build --config Release` → builds `rust-bridge` (cargo) +
  `spotify-lite` (C++), both succeed.
- `cargo build --release` standalone → succeeds.
- `build/spotify-lite.exe` → prints expected two lines, exit 0.

## librespot rev

Not pinned yet — next milestone. Then: record rev + feature flags in
`docs/DECISIONS.md`, show `cargo tree -i librespot`, commit `Cargo.lock`
separately per plans.md 1.16.

## How to test now

1. `cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release`
   (ensure `cmake` + `cargo` on PATH)
2. `cmake --build build --config Release`
3. `.\build\spotify-lite.exe` → expect `Spotify-lite` / `Build successful.`

## Version control

- `cf17069 phase-0: add CMake project + minimal C++ executable`
- `7f40ab1 phase-0: add Cargo workspace + bridge crate, CMake builds both`
- Not pushed yet (push happens at phase completion per 1.18 milestone flow;
  will push all Phase 0 commits together).
