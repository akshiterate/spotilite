# Handoff — spotilite, new agent session

Date: 2026-09-12. Read this + `plans.md` + `docs/UI_REWORK.md` +
`docs/DECISIONS.md` + `docs/API.md` + `docs/PHASE_15_SUMMARY.md`, then
`git log --oneline -10`. The user explicitly said: solve the open bug
(below) IN the new session, not now.

## Repo state

- Remote: `https://github.com/akshiterate/spotilite.git`, branch `main`,
  clean tree, everything pushed. Tip at time of writing: `0449f11`
  ("playback: watchdog auto-recovery for dead audio sink") — verify with
  `git log --oneline -5` and `git status --short` on start.
- Layout: `cpp/app/` (App shell), `cpp/ui/` (main/queue/search/
  playlists/content/settings windows + placer), `cpp/core/` (Player,
  Queue), `rust/librespot-bridge/` (librespot 0.8 pinned rev
  `a1b66d3c`, staticlib), `include/spotify_bridge.h` (C ABI),
  `third_party/imgui` (docking snapshot `367b2c2`), `docs/`.
- Binaries are gitignored build artifacts: `build/gui.exe` (the app),
  `build/dist/spotilite.exe` (release copy, keep in sync manually).
- One build command (repo root, PowerShell):
  `powershell -ExecutionPolicy Bypass -File build.ps1`
  Then `strip` is already handled inside it. Tool PATHs (portable CMake
  4.4.3, rustup GNU, MinGW GCC 16.1.0) are baked into the script; fresh
  shells otherwise lack them.
- NEVER `push --force`, never amend pushed commits. Small commits,
  `phase-N:` / `ui:` / `auth:` / `build:` prefixes.
- Runtime state (machine-local, never in repo): `%LOCALAPPDATA%\spotilite\`
  (`cache/credentials.json`, `cache/device-id`, `cache/webapi.json`,
  `spotilite.toml`, art/audio caches). A baked Spotify client ID lives in
  `rust/librespot-bridge/src/lib.rs` (`DEFAULT_CLIENT_ID`) — public PKCE
  client, intentional.
- Close running exes before relinking (Windows file locks: os error 5).

## Architecture ( condensed)

- `spotilite::Player` (`cpp/core`) owns the Rust handle, queue, mirrored
  state; frontends use only it. Async ops: connect (worker + adopt),
  metadata/artwork/search/library fetches (`std::async`).
- Events are polled (`spotify_poll_event`), never callbacks. Position
  events flow at 1 Hz while playing.
- Queue is index-based and includes history; the queue window displays
  row 0 = current + numbered upcoming. Auto-advance on track end lives
  in `App::updateShared` (display-filtered, history kept so Previous
  works). Stall watchdog (10 s frozen position → `recoverPlayback()`,
  once per track) also lives there.
- GUI = Dear ImGui docking branch, Win32 + DX11, one OS window per view
  via viewports; utility windows are owned toolwindows (no Alt-Tab entry)
  and close on foreign focus loss. Space / Ctrl+N / Ctrl+P hotkeys via a
  low-level hook, app-local only; hardware media keys via WM_APPCOMMAND.
- Auth model (hard-won, do not regress): session credentials MUST be
  discovery blobs. OAuth access tokens AP-connect fine but are denied by
  login5, silently killing metadata/context/audio. `spotify_connect`
  drops username-less blobs, pre-flights `login5().auth_token()`, and
  reports `SPOTIFY_ERR_NO_CREDENTIALS` for GUI provisioning (zeroconf
  tap). OAuth tokens stay Web-API-only. See DECISIONS entries.

## OPEN BUG (solve next session): Next keeps skipping without stopping

User report: pressing Next (the `>>` button, main window) skips
repeatedly — "next next next without stopping". Not yet reproduced by an
agent (no GUI clicking possible headlessly); do NOT implement a fix
blind. Reproduce first, then fix.

### Facts

- `>>` calls `player_.next()` once per click: queue advance + load.
  Single-shot by construction; nothing in the call path loops.
- Auto-advance fires on `TRACK_ENDED` events whose URI matches (or is
  empty), in `App::updateShared`.
- `Player::next()` at queue end fails cleanly ("at end of queue").
- Space is swallowed app-wide by the keyboard hook; Enter is NOT hooked.

### Hypotheses (ranked)

1. **Keyboard focus + Enter repeat (prime suspect).** After clicking `>>`
   it keeps focus; Enter re-activates focused buttons natively and is not
   intercepted (Space is). Holding Enter, or the OS key-repeat firing,
   machine-guns `next()`. Check: does it happen on single mouse clicks
   with hands off keyboard? Does holding Enter reproduce it deterministically?
   Fix direction: clear button focus after activation and/or handle Enter
   in the hook like Space (keeping text-input behavior intact).
2. **Stale auto-advance.** An `TRACK_ENDED` with empty URI always advances;
   bursts of such events (or rapid load→end→load cycles on failing tracks)
   could chain-skip. Check console/event flow around the incident.
3. **Optimistic-state vs late events.** `loadCurrent` pins
   `playing=true`; a late Paused/Ended for the previous track within the
   same drain could theoretically double-advance given the right
   interleaving — audit ordering if H1/H2 are excluded.

### Suggested first steps

1. `build.ps1`, launch `build/gui.exe` from a terminal, reproduce with a
   2–3 track queue: single mouse click on `>>` vs Enter presses vs held keys.
2. If reproducible: add temporary stderr traces in `Player::next()` and
   the auto-advance branch (who calls, with what index/URI), rebuild,
   re-run, read the log.
3. Fix the proven mechanism, keep the others honest (single-track queue +
   end-of-queue must still stop cleanly), update `docs/DECISIONS.md`,
   commit, push, report per the phase format in `plans.md` §1.2.
