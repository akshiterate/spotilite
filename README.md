# spotilite - my first fully "vibecoded" project for personal use

A very lightweight native Spotify Premium client for Windows (Linux support later).
C++ app + Rust/librespot bridge, no Electron/Chromium/WebView. See `plans.md` for the full plan.

## Download & run (no install)
1. download spotilite.exe from the release
2. open it up and open the spotify client(app) for windows, ideally you will see spotilite as a device u can play songs on in the device menu click on spotilite and close spotify app (you can even close it from the background in task manager)
3. hopefully after this when you try to search something or see your playlists a browser window opens for you to log in then the app saves the cache from the login and uses it to fetch the meta data of the songs
4. once u press play u can hear the playback from librespot[https://github.com/librespot-org/librespot] it has been integrated into the app.
5. you can use space for play/pause, ctrl+n for next, ctrl+p for prev.

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

Test/console binaries, the TUI and the GUI all build through one script
(static linking, so the exes run without MinGW installed, then stripped):

```powershell
powershell -ExecutionPolicy Bypass -File build.ps1
```

This configures + builds via CMake/Cargo and links + strips
`core_test`, `tui`, `bridge_test` and `gui` into `build/`.

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
