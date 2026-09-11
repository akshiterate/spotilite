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
