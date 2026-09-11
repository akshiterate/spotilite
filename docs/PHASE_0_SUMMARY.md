# Phase 0 Summary — Repository + Build

Final report. Mid-phase checkpoint 1 (milestones A+B, toolchain install)
was approved by the user before the librespot pin below.

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
  `bridge_version()` smoke symbol. librespot v0.8.0 pinned as a dependency
  (rev below); unused until Phase 1.
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

- Rev: `a1b66d3c8a14e55a9572a9e17467150dca618c9a` (librespot v0.8.0).
- Features: defaults (`native-tls`, `rodio-backend`, `with-libmdns`).
  Rationale + rejected alternatives in `docs/DECISIONS.md`.
- `cargo tree -i librespot --depth 1`:

```text
librespot v0.8.0 (https://github.com/librespot-org/librespot?rev=a1b66d3c8a14e55a9572a9e17467150dca618c9a#a1b66d3c)
└── librespot-bridge v0.1.0 (C:\Users\asac\Documents\spotilite\rust\librespot-bridge)
```

- 393 packages in `Cargo.lock`, committed separately per plans.md 1.16.

## How to test now

1. `cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release`
   (ensure `cmake` + `cargo` on PATH)
2. `cmake --build build --config Release`
3. `.\build\spotify-lite.exe` → expect `Spotify-lite` / `Build successful.`

## Version control

- `cf17069 phase-0: add CMake project + minimal C++ executable`
- `7f40ab1 phase-0: add Cargo workspace + bridge crate, CMake builds both`
- `2890bfd phase-0: checkpoint 1 docs (milestones A+B, toolchain decisions)`
- `2ba1a21 phase-0: pin librespot rev in bridge manifest`
- `2be1082 phase-0: Cargo.lock for librespot pin`
- plus `phase-0: finalize docs (librespot pin record)` (this file +
  `docs/DECISIONS.md`).
- All pushed to `origin/main` at phase completion.
