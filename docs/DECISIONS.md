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
- 2026-09-11 (Phase 2): C ABI surface mirrors pinned 0.8 exactly:
  create/destroy/connect/load_uri/play/pause/resume/seek/set_volume/
  last_error. Omitted next/previous (no Player queue in 0.8; queue is
  Phase 3/9 C++ scope). `resume` kept as its own symbol mapping to `play`.
  Rejected: callbacks/events (Phase 3 polling per 1.7), discovery inside
  the bridge (reuse Phase 1 cache; missing cache = AUTH error telling the
  user to run headless.exe once).
- 2026-09-11 (Phase 2): Handle owns a 2-worker Tokio runtime; `connect`
  `block_on`s, the rest hand off to the player thread. Last-error via
  per-thread buffer (pointer valid till next failure on same thread);
  `last_error(NULL)` reads creation failures. Rejected: mutex-held string
  (dangling pointer), errno-style codes without text (undebuggable from
  C++). Connect failures split AUTH vs INTERNAL via `AuthenticationError`
  downcast (`Error.error` is pub).
- 2026-09-11 (Phase 2): `Session::new`/`Player::new` must run inside the
  Tokio runtime context (self-test panic: "no reactor running" at
  `session.rs:159`), so `spotify_create` builds them under `rt.block_on`.
  Rejected: `rt.enter()` guard (block_on is simpler and creation is
  one-shot).
- 2026-09-11 (Phase 2): `bridge_test` links the staticlib with direct
  `g++`, no CMake change (`CMakeLists.txt` outside Phase 2 scope, 1.15).
  Extra system libs required beyond the obvious: `ole32 oleaut32 propsys
  ntdll` (cpal/rodio COM + prop-variant + `NtCreateNamedPipeFile`). Test
  exe goes to `build/` (gitignored) to keep artifacts in one place.
- 2026-09-11 (Phase 3): Core owns the queue; `loadUri` resets it to the
  single URI (predictable solo-play semantics). `Player` mirrors state from
  polled events (optimistic `playing=true` on load, corrected by events);
  errors as bool + `lastError()`, exceptions only from the throwing ctor.
  Queue is header-only; remove/reorder deferred to Phase 9. Core test is
  scripted (exit 0/1) rather than interactive so runs are reproducible;
  `bridge_test` stays interactive for manual probing.
- 2026-09-11 (Phase 4): TUI is line-based (`p` + Enter), not single-key:
  avoids Windows-only console APIs (`conio`) before Phase 12 owns platform
  code, and keeps the app portable toward Phase 16. Render-on-command (plus
  `s` refresh) instead of a live loop: stdin blocks, and continuous redraw
  would violate the perf rules. Rejected: `_kbhit` poll loop (platform
  API), `system("cls")` flicker.
- 2026-09-11 (Phase 4): Track/Artist/Album/duration are placeholders; no
  metadata plumbing smuggled in ahead of Phase 5. 300ms settle after
  mutating commands so the render drains the events they produce.
- 2026-09-11 (Phase 4): `Player::pause()` flushes stale queued events then
  pins `playing=false` — stale `Playing` events otherwise re-set the flag
  after the pause and invert play/pause toggles. `resume`/`play` stay
  event-driven (optimistic `true` could stick with nothing loaded).
- 2026-09-11 (Phase 5): Metadata via `Metadata::get` on the stored
  canonical URI, fetched on demand (no Web API, no new deps). `Track`
  embeds album + artists, so one request suffices; artist names joined
  with ", ". Duration taken as milliseconds (verified against a 4:07
  track). TUI display deliberately untouched — Phase 5 scope is core +
  docs, and the plan accepts the test app as the display vehicle.
  Rejected: event-driven capture from `TrackChanged` (fires unreliably on
  direct loads; fetch is deterministic).
- 2026-09-11 (Phase 3): Added `spotify_poll_event` + `SpotifyEvent` to the
  ABI. The Phase 3 scope marks the header "read-only unless ABI bug" — this
  is the poll function 1.7 foresees by name, additive only, so it is treated
  as sanctioned completion rather than redesign. Rejected: callbacks across
  the ABI (1.7 forbids until required; polling suffices), exposing raw
  librespot event enums (C++ must not depend on Rust types, 1.7).
- 2026-09-11 (Phase 6): New dependency `image 0.25` (decode JPEG + resize
  + BMP encode). Purpose: cover-art pipeline with zero C++ image code.
  Existing deps insufficient (nothing decodes/resizes images). Cost: pure
  Rust, `default-features = false` + `jpeg,bmp` only, small tree delta.
  Rejected: stb_image in C++ (pushes decode across the ABI), PNG disk
  format (C++ can't verify dims without a lib), direct CDN HTTPS (would
  need an HTTP client dep; `spclient.get_image` already does it).
- 2026-09-11 (Phase 6): Disk format BMP for both sizes so the C++ test
  verifies dimensions from the 54-byte header with no library (revisit in
  Phase 7 if the GUI prefers another format). Memory cache hand-rolled
  FIFO cap 8 (no `lru` dep, 1.4). Failures never-ready + warn (no ABI
  growth for error text across threads).
- 2026-09-11 (Phase 6): Acceptance adaptation — no UI exists yet, so
  "appears beside the track" is proven at cache level (READY event + valid
  files, non-blocking) and lands visually in the Phase 7 GUI. Phase goal
  (background, non-blocking, small caches, right sizes) unchanged.
- 2026-09-11 (Phase 6): C++ proves dims from BMP headers (no image lib);
  core exposes request/state/path; second-run cache reuse verified green.
- 2026-09-11 (Phase 7): New dependency Dear ImGui `v1.92.9b` (pinned tag).
  Purpose: MVP GUI per plan section 7. Existing deps insufficient (no UI
  toolkit in tree). Cost: vendored source under `third_party/imgui`
  (~150 files, committed — reproducible without submodule/network steps
  for stateless agents). Rejected: git submodule (friction for agents +
  fresh clones), GLFW/SDL (extra windowing deps; Win32 backend uses only
  system DX11 + GDI).
- 2026-09-11 (Phase 7): Backend = Win32 + DirectX 11 (most native, zero
  new runtime deps). ASCII window APIs explicitly (`A`/`W` suffixed) for
  MinGW predictability. `io.IniFilename = nullptr`.
- 2026-09-11 (Phase 7): Input area plays pasted URIs; results area lists
  the queue (click select + Play selected). Real Spotify search needs the
  Phase 8 Web API and isn't in the acceptance list. `Queue::at(i)` added
  for row rendering (no behavior change).
- 2026-09-11 (Phase 7): Metadata fetched via `std::async` off the UI
  thread, applied only if the track is still current (avoids cross-track
  races). Artwork textured on ARTWORK_READY from the deterministic disk
  path (128px).
- 2026-09-11 (Phase 8): Search via Web API + PKCE OAuth (user chose over
  tracks-only SpClient context search). New deps `librespot-oauth` (same
  pinned rev), `reqwest 0.12` (SChannel defaults), `serde_json 1` — all
  pre-compiled as transitive deps. Purpose: 4-type paged search now,
  reusable tokens for Phase 10 library later. Rejected: SpClient
  `spotify:search:` contexts (tracks only, quirky ranking), direct CDN
  HTTP (no client in tree), JSON across the ABI (structured items
  instead — no C++ JSON lib needed).
- 2026-09-11 (Phase 8): Secrets hygiene — client_id pasted by the user is
  runtime-only (`SPOTILITE_CLIENT_ID` env on first login, then cached
  `webapi.json` machine-locally); never in repo, logs, or docs. Scopes
  `user-library-read` + `playlist-read-private` pre-cover Phase 10 so one
  login suffices (search itself needs no scope).
- 2026-09-11 (Phase 8): `SEARCH_ALL` collides with a Windows SDK macro —
  the C++ enumerator is `SEARCH_ANY` (same bitmask 15); the C ABI keeps
  `SPOTIFY_SEARCH_ALL`. Null entries in playlist results are filtered
  (empty URI) after a live run showed a blank row.
- 2026-09-11 (Phase 8): GUI search is async (`std::async`, results +
  error pair); only track rows play, other kinds report it. Live test:
  coordinated browser login worked first try; cached refresh needs no env
  afterwards; searched track played end to end with correct metadata.
