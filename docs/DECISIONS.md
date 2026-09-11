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
- 2026-09-11 (Phase 9): Queue owns remove/reorder; index follows its track
  on both (removing the playing row doesn't stop playback — matches common
  players). `loadUri` (reset) split from queue-preserving `loadCurrent`;
  next/previous/play-selected navigate without collapsing the queue, which
  the old always-reset behavior defeated. GUI manipulates via the public
  queue + `playCurrent`; any search row can enqueue (play validates).
- 2026-09-11 (Phase 8): Spotify search `limit` caps at 10 (bisected live:
  10 ok, 11/15/20/50 → 400 "Invalid limit"); bridge clamps 1..10. Up to
  40 rows/call across types keeps the plan's 20–50 page.
- 2026-09-11 (Phase 8): Refresh responses often omit a new refresh token —
  the bridge preserved the cached one instead of overwriting with empty
  (the overwrite wiped a login mid-phase; fixed + re-logged-in).
- 2026-09-11 (post-Phase 9 GUI bugfix round): three fixes from user testing.
  (1) Seek slider never moved: live state overwrote the thumb every frame
  (snap-back). Fixed with an IsItemActive-held drag value. (2) Volume
  display lied (0.5 default vs cached mixer value): new
  `spotify_get_volume` ABI (mixer.volume()) mirrored in `connect()`.
  (3) Pause/Play toggle desync on event latency: `pause()`/`resume()` now
  wait up to 300ms for the confirming event (draining in order) with an
  optimistic fallback, so state is exact in the common case.
- 2026-09-11 (post-Phase 9 GUI polish): ImGui window fills the OS window
  (NoTitleBar/NoResize/NoMove) — no more window-inside-a-window.
- 2026-09-11 (post-Phase 9 GUI polish): Play-button dead end fixed —
  `resume()` cannot revive an ended track, so the toggle restarts
  (reloads current URI) when position is at/past duration, and resumes
  when paused. URI Play always (re)loads; the two buttons now behave
  distinctly by design.
- 2026-09-11 (post-Phase 9 GUI polish): ImGui duplicate-ID bug — the URI
  "Play" and the toggle "Play" shared one ID, so clicks landed on the
  wrong button (likely the original dead-Play root cause). Labels now
  carry hidden `##uri` / `##toggle` ID suffixes.
- 2026-09-11 (post-Phase 9 GUI polish): search Play-result used resetting
  `loadUri` (queue wipe). New `Queue::insertAt` + `Player::playFirst` put
  the track on top and play it, preserving the rest.
- 2026-09-11 (UI feedback round 2): queue highlight follows its track
  across auto-advance (selection was positional and went stale when the
  list shifted underneath it). Alt-Tab (or any focus loss to a foreign
  window) closes utility windows via WM_ACTIVATE + own-HWND check, per
  explicit user request; focus moving between our own windows is ignored.
- 2026-09-11 (Phase 11): `spotilite.toml` next to the cache, five real
  keys only (device name, bitrate, normalisation, volume, cache size);
  cache location/Connect/startup skipped per the plan's own rule. New dep
  `toml 0.8` (pure parser). Rust alone parses (single parser, no skew);
  C++ never touches the file — Settings window shows path + restart note,
  tests use `spotify_config_summary`. Bad values fall back per key.
- 2026-09-11 (Phase 11 follow-up): Settings window is editable —
  `spotify_config_get/set` ABI (strict validation with messages) +
  header-only `spotilite::Config` wrapper. Widgets save per edit
  (slider/input on release, combo/checkbox immediately); file rewritten
  canonically each save (no extra parser dep); values apply on restart,
  stated in-window. `set` is strict, the file loader lenient, by design.
- 2026-09-11 (Phase 15): measured first. Startup 1088ms (connect-bound)
  → async connect with UI adoption (474/350ms, Connecting.../Retry UI);
  binaries 34–44MB → `strip` (~44% off, all ≤25 except headless 27.6).
  Accepted as-is: idle CPU ~1%, headless WS 21.5MB, warm-start <200ms
  (needs deeper startup surgery for little gain). Network by
  construction (~1.2MB/min @160kbps); interface counters too noisy on a
  lived-in machine to attribute.
- 2026-09-11 (UI feedback round 3): queue shows row 0 = now playing
  (move target and delete exclude it; Play Selected on it replays);
  selection mapping is index-based so labels always match. Playlist,
  album and artist content windows page to completion (cap 500) instead
  of the first 50.
- 2026-09-11 (scoped Phase 12 per user: hotkeys only, nothing else):
  low-level keyboard hook for Space (play/pause), Ctrl+N (next), Ctrl+P
  (previous) — app-local by foreground-PID check, swallowed before ImGui
  so widgets never double-trigger; text inputs always let keys through.
  Hardware media keys via WM_APPCOMMAND (focused window only — background
  delivery needs a media session, not registered). All three funnel into
  shared App methods (toggle now extracted from the button).
- 2026-09-11 (UI feedback round 5): auto-height locked in while async
  content was still loading, so search/playlists/content windows stayed
  tiny. They now refit once when rows land (size flags consumed before
  Begin).
- 2026-09-11 (release UX): normal users never touch headless —
  `spotify_connect` falls back to the search PKCE browser flow when no
  session cache exists (new `streaming` scope; `DEFAULT_CLIENT_ID` const
  for release baking, env/cache otherwise). Connected sessions persist
  blobs, so later runs skip the browser. Existing blob users unaffected.
- 2026-09-11 (UI feedback round 6): main window spread + centered
  (nav/artwork/title/slider/controls/volume), hero artwork 192px from
  the cached 256px file, 90%-width slider, large fixed-size transport
  buttons, bigger OS window (560x540).
- 2026-09-11 (UI feedback round 4): sizing pass — main OS window 660x900
  → 560x460; sub windows widened (queue 470, search/content 560,
  playlists 500) with more visible rows (8/8/10 → 10/10/12); heights stay
  auto. Play Selected "random song" reports traced to stale binaries +
  selection going stale at track boundaries (both fixed); core ordering
  proven by test.
- 2026-09-11 (UI feedback round): queue rows show `title - artist` via a
  background name cache (`spotify_metadata_for_uri`, 200-entry cap, URIs
  until names land). Secondary windows auto-height (fixed widths kept);
  `ConfigViewportsNoTaskBarIcon` keeps them in the main window's family
  (no taskbar buttons, no Alt-Tab entries; backend already parents them).
  Liked content windows page to completion (cap 500). Playlist/artist
  drill-ins follow context page_urls (10-page cap) with real totals.
- 2026-09-11 (UI rework phase 3): playlists + settings + placer.
  Content windows fetch own rows async (liked: 50; playlists: context
  page); closed ones are destroyed. `Add Playlist to Queue` enqueues all
  visible rows; queue/search/library event flows unchanged. Backend,
  network, playback, metadata untouched per the brief.
- 2026-09-11 (UI rework phase 1): `third_party/imgui` master v1.92.9b →
  docking snapshot `367b2c2` ("1.93.0 WIP"). Reason: master has no
  multi-OS-window support (verified absent: no UpdatePlatformWindows /
  ViewportsEnable); docking branch is the ImGui-sanctioned path, same
  dep, same license, same direct-g++ build. Current GUI relinked
  unmodified + smoked green. Progress tracked in `docs/UI_REWORK.md`.
- 2026-09-11 (UI rework phase 2): `Gui` split into `App` shell
  (`cpp/ui/app.*`: Player ownership, viewport loop, shared
  metadata/artwork) + per-window views (`main/queue/search_window.cpp`;
  main view stays in the frame). Queue window shows current + numbered
  upcoming; Play Selected = discard-before + play (`Player::playFrom`),
  Delete, Move-to-N input, Shuffle-upcoming-only
  (`Queue::shuffleUpcoming`), auto-advance on track end in the app layer
  (display-filtered, so Previous keeps working). URI input dropped from
  the GUI (play from Search; URIs still work in tests).
- 2026-09-11 (Phase 10.1 Liked Songs): Web API calls share `web_get`
  (token + one 401 retry); login failures keep AUTH via a dedicated
  variant. Library pages cap at 50 (official cap; search's 10 is
  search-only). Totals flow out for paging UI; null rows filtered as
  before. GUI library section mirrors the search async pattern; `rust/`
  + `include/` touched because network + ABI live there per 1.6/1.7
  (same as Phase 8).
- 2026-09-11 (Phase 10.2 Playlists): `/v1/playlists/{id}/tracks` is 403
  for new apps (own/others, all param variants) while sibling endpoints
  work — endpoint restriction, not scope. Tracks resolve via
  `spclient.get_context` (first page) + concurrent metadata names;
  total -1 (unknown). List subtitle is owner-only (per-playlist totals
  read 0). GUI library generalized to LIKED/PLAYLISTS/TRACKS modes with
  drill-in Play, Back (page remembered), mode-aware Play/Add.
- 2026-09-11 (Phase 10.3 Albums): probed first — both album endpoints
  work, so pure Web API with real totals (no context detour). GUI drill
  generalized with a remembered return mode/page; Back works from both
  track views.
- 2026-09-11 (Phase 10.4 Artists): followed list needs `user-follow-read`
  (scope added; refresh keeps old grants, so a re-login is mandatory).
  Auto re-login on 403 "Insufficient client scope" using the cached client
  id — no env, no file deleting. Followed paging is cursor-based, walked
  internally to keep the offset ABI uniform. Artist top tracks via context
  (shared helper with playlist tracks), first page, total -1.
