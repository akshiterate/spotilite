# spotilite

A very lightweight native Spotify Premium client for Windows (Linux support later).
C++ app + Rust/librespot bridge, no Electron/Chromium/WebView. See `plans.md` for the full plan.

## Download & run (no install)

1. Get `spotilite-vX.Y.Z-windows-x86_64.zip` from the
   [releases page](https://github.com/akshiterate/spotilite/releases) and
   unzip it anywhere. No installer, no admin rights, no extra runtimes.
2. Run `gui.exe`. First launch shows "Connecting..." — nothing is cached yet.
3. For device provisioning alternative, run `headless.exe`: it advertises
   as `spotilite`; pick it under "Connect to a device" in the official
   Spotify app to provision credentials.
4. Web search needs a one-time browser login: type anything in Search and
   press Find. It asks for `SPOTILITE_CLIENT_ID` once — create a free app
   at <https://developer.spotify.com/dashboard>, allowlist the redirect
   `http://127.0.0.1:8898/login`, and paste the Client ID. Afterwards a
   cached refresh token is used; the ID is never stored in the repo.

State (credentials, cache, config) lives under
`%LOCALAPPDATA%\spotilite\` — nothing is written next to the exe.

## Configuration

Edit `%LOCALAPPDATA%\spotilite\spotilite.toml` (auto-created on first
run), then restart. Keys: `device_name`, `bitrate` (96/160/320),
`normalisation`, `volume` (0..1), `cache_size_mb`. Bad values fall back
to defaults. The GUI Settings window edits the same file.

## Build from source

Requirements (Windows): MinGW-w64 GCC, portable CMake, rustup GNU
toolchain. Then:

```powershell
cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
```

Test/console binaries link directly against the Rust staticlib, e.g.:

```powershell
g++ -std=c++17 cpp/core/core_test.cpp cpp/core/player.cpp -Iinclude -Icpp target/release/liblibrespot_bridge.a -o build/core_test.exe -lws2_32 -luserenv -lbcrypt -lole32 -loleaut32 -lpropsys -lntdll
```

The GUI link line is in `cpp/ui/app.cpp` (header comment). Release
binaries add `-static` (no MinGW DLLs needed) and are `strip`ped:

```powershell
g++ -std=c++17 -static <same sources/libs as above> -o build/gui.exe <same libs>
strip build/gui.exe
```

## Troubleshooting

- `gui.exe` won't start on a fresh machine: you ran an unstripped or
  non-static dev build. Release zips are static; verify with
  `objdump -p gui.exe` (only system DLLs may appear).
- Rebuilding fails with "Access is denied / os error 5": close the
  running exe first — Windows locks running binaries.
- Search asks for a Client ID every time: the refresh token was revoked
  or deleted; complete one browser login again.
- Playlist tracks show 403 in old versions: fixed by resolving through
  the local context instead (current builds).
- Spotify rate-limiting or 503s: wait a minute and retry; the app
  reports server errors verbatim.

## Project state

Phases -1–11, 15 and the multi-window UI rework are done (see
`docs/PHASE_*_SUMMARY.md`, `docs/UI_REWORK.md`, `docs/DECISIONS.md`).
Remaining plan phases: 12 (Windows integration), 13 (headless flag),
14 (TUI parity), 16 (Linux).
