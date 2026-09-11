# Phase 2 Summary — C ABI Bridge

Final report. All acceptance criteria verified (see Verified).

## Goal

Control librespot from C++ through a small C ABI (plans.md Phase 2).

## Done so far

- `include/spotify_bridge.h`: opaque `SpotifyPlayer*`, error codes
  (`SPOTIFY_OK`/`NULL_ARG`/`NOT_CONNECTED`/`AUTH`/`BAD_URI`/`AUDIO`/
  `INTERNAL`), `create/destroy/connect/load_uri/play/pause/resume/seek/
  set_volume/last_error` with ownership + threading contract in comments.
- `rust/librespot-bridge/src/lib.rs`: full implementation. Handle owns a
  2-worker Tokio runtime, `Session`, `Player`, `Mixer`, credential `Cache`.
  `connect` uses the Phase 1 machine-local cache (same device id) and is
  idempotent; `load_uri` accepts playable URIs and autoplays;
  `resume == play`; volume float clamps to `u16`; per-thread last-error
  buffer behind `spotify_last_error()`.
- Pinned-API adaptations (1.8): no next/previous — librespot 0.8 `Player`
  has no C++-drivable queue (`load/play/pause/seek` only); queue ops belong
  to the Phase 3/9 C++ core. Auth vs transport failures distinguished via
  `AuthenticationError` downcast. `Cache` is `core::cache::Cache`;
  audio/mixer `find()` return `Option`. Phase goal unchanged.
- `cpp/bridge_test.cpp`: interactive console test (`connect`, `play <uri>`,
  `pause`, `resume`, `seek <ms>`, `volume <0-100>`, `status`, `help`,
  `quit`); prints result codes + `spotify_last_error()` on failure.
- Bug found by self-test, fixed: `Session::new` (and `Player::new`) must
  run inside a Tokio runtime context ("no reactor running" panic), so
  creation now happens under `rt.block_on`. Details in `docs/DECISIONS.md`.
- Link (direct `g++`, no CMake change — `CMakeLists.txt` is outside the
  Phase 2 file scope):

```text
g++ cpp/bridge_test.cpp -Iinclude target/release/liblibrespot_bridge.a
    -o build/bridge_test.exe -lws2_32 -luserenv -lbcrypt -lole32
    -loleaut32 -lpropsys -lntdll
```

  The COM/prop-sys libs are needed by cpal/rodio inside the staticlib.

## Verified

- `cargo build --release` → exit 0, no warnings. Staticlib
  `target/release/liblibrespot_bridge.a` produced.
- `cmake --build build --config Release` untouched config still green
  (rerun at close; log in phase report).
- `build/bridge_test.exe` scripted runs, all green:
  - error paths: `pause` before connect → `-2 NOT_CONNECTED`;
    `play garbage` → `-4 BAD_URI` ("does not belong to Spotify");
    `play spotify:album:...` → `-4` ("not playable audio").
  - `connect` → `Connected.` (cached creds, real auth); `volume 50` OK;
    `seek 60000` OK; `quit` destroys cleanly, exit 0, empty stderr.
  - audio path with user-supplied track
    (`spotify:track:4PJEK76V3A1S0XzZJuTWh7`, from their share link):
    `Playing.` → 14s → `Paused.` → `Resumed.` → 7s → `Seeked.` (30s) →
    destroy, exit 0. Same WASAPI sink already proven audible in Phase 1.
- `git status` clean; only Phase 2 Allowed files + docs touched
  (`include/`, `rust/`, `cpp/bridge_test.cpp`, `docs/`; CMakeLists
  untouched).

## librespot rev

Unchanged: `a1b66d3c`, defaults. No manifest change (no new deps).

## Version control

- `f1f4084 phase-2: add C ABI header + Rust bridge`
- `6a322e2 phase-2: checkpoint A docs`
- `phase-2: add bridge_test program + reactor fix`
  (`cpp/bridge_test.cpp` new, `lib.rs` runtime-context fix)
- `phase-2: finalize docs` (this file + `docs/DECISIONS.md`, `docs/API.md`)
- All pushed to `origin/main` at phase completion.
