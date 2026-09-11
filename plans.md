# spotilite — Agent Development Plan

## 0. Project Definition

**Project:** spotilite

**Goal:** Build a very lightweight native Spotify Premium client for Windows, with Linux support later.

The application uses:

- **C++** for the application, core logic, UI, caching, configuration, and platform integration.
- **Rust + librespot** for Spotify session, playback, audio, and Spotify Connect functionality.
- A **small C-compatible ABI** between C++ and Rust.
- **No Electron.**
- **No Chromium.**
- **No Node.js runtime.**
- **No WebView.**

The application should eventually provide:

- Spotify authentication
- Spotify Connect
- Playback
- Search
- Track/album/artist metadata
- Queue
- Playlists
- Liked Songs
- Album artwork
- Volume
- Seeking
- Windows media controls
- Optional TUI/headless mode
- Disk caching

---

# 1. Agent Operating Rules

These rules are mandatory.

## 1.1 Work phase-by-phase

**The agent MUST only work on the current phase.**

It must not implement functionality belonging to a later phase unless explicitly instructed.

For example:

- During Phase 0, do not build the GUI.
- During Phase 1, do not build search.
- During Phase 3, do not implement playlists.
- During the GUI phase, do not spontaneously redesign the backend.

---

## 1.2 HARD STOP after every phase

**This is mandatory.**

After completing a phase:

1. Build the project.
2. Run the relevant tests.
3. Verify the phase's acceptance criteria.
4. Report exactly what was implemented.
5. Report how the user can test it.
6. Report any known issues.
7. **STOP.**

Do not begin the next phase automatically.

Do not continue coding after reporting completion.

Wait for the user to explicitly tell the agent to continue.

The user should be able to launch/test the project after every phase.

### Required completion format

At the end of every phase, report:

```text
PHASE X COMPLETE

Implemented:
- ...
- ...
- ...

How to test:
1. ...
2. ...
3. ...

Expected result:
...

Known issues:
- ...

NEXT PHASE: Phase X+1

WAITING FOR USER APPROVAL.
```

The agent must then stop.

---

## 1.3 Keep every phase buildable

At the end of every phase:

```text
cmake --build build --config Release
```

must succeed.

If the phase adds functionality that cannot be tested yet, provide the simplest available test.

Do not knowingly leave the repository broken between phases.

---

## 1.4 Do not over-engineer

Implement the smallest solution that satisfies the current phase.

Do NOT prematurely add:

- Databases
- Plugin systems
- Dependency injection frameworks
- RPC
- IPC
- Microservices
- Web servers
- Large application frameworks
- Complex event systems
- Custom audio pipelines
- Custom Spotify protocol implementations

---

## 1.5 Do not reinvent librespot

Do not implement:

- Spotify's audio protocol
- Spotify authentication protocol
- Spotify encryption/decryption
- Audio decoding

Use librespot wherever it already provides the functionality.

---

## 1.6 C++ owns the application

C++ owns:

- Application state
- UI
- Queue
- Search
- Library
- Metadata presentation
- Artwork cache
- Configuration
- User preferences
- Platform integration

Rust owns:

- librespot
- Spotify session
- Spotify authentication/session handling
- Audio streaming
- Audio decoding
- Spotify Connect

Keep Rust as a thin adapter around librespot.

---

## 1.7 Keep the ABI small

C++ and Rust communicate through a C-compatible interface.

Do not expose Rust-specific or C++-specific types across the boundary.

Use:

- Opaque handles
- Primitive types
- C-compatible structs
- UTF-8 strings
- Explicit ownership
- Explicit error codes

Do not introduce callbacks or complex event mechanisms until they are actually required.
Callbacks become required starting at Phase 3 (player events). When required:

- Prefer polling (`spotify_poll_event`) over callbacks across the C ABI.
- If callbacks are unavoidable, they must be: single-threaded, non-blocking,
  non-reentrant, documented with threading guarantees, and never call back into Rust from C++ on the same thread.
- Audio/event threads must never block the UI thread. C++ owns thread dispatch.

---

## 1.8 Verify before assuming

The versions and APIs described in this plan may become outdated.

Always verify the currently resolved dependencies and APIs.

If the current librespot API differs from this document:

1. Inspect the actual API.
2. Make the smallest required adaptation.
3. Document the deviation.
4. Continue only if the current phase's goal remains unchanged.

Do not blindly follow outdated examples.

---

## 1.9 No destructive actions

Do not:

- Delete user files.
- Modify files outside the project.
- Modify system configuration unnecessarily.
- Install unrelated software.
- Change global environment variables unnecessarily.
- Modify unrelated projects.

Keep all project work inside the repository unless explicitly required.

---

## 1.10 Intra-phase work budget — STOP mid-phase if needed

Phases vary in size. A phase boundary alone does not prevent runaway work
inside a large phase (Phase 1, 7, 10).

The agent MUST stop and report for user review if any of these is hit
before the phase is complete:

- >20 tool/file operations without a user-visible checkpoint, OR
- >5 files created/modified, OR
- >300 lines added, OR
- blocked >15 minutes / 3 failed attempts on the same error, OR
- the task expands beyond the current phase's Allowed files.

Report format for a mid-phase stop:

```text
PHASE X — MID-PHASE CHECKPOINT
Done so far:
- ...
Blocked / uncertain:
- ...
How to test now:
- ...
Continue? WAITING FOR USER APPROVAL.
```

Then STOP. Do not continue coding.

---

## 1.11 STOP and ask on ambiguity

Stop conditions in 1.2 / 1.10 apply at phase end / budget limits.
Additionally the agent MUST stop immediately if:

- librespot API differs from this plan and adaptation would change the phase goal,
- credentials / auth flow are unclear,
- a dependency, feature flag, or build error requires a design choice,
- two reasonable implementations exist.

1.8 rule 4 is mandatory: continue only if the current phase's goal remains
unchanged. Otherwise STOP and ask. Never guess on auth, threading, ABI
ownership, or dependency selection.

---

## 1.12 Stateless start — no prior chat context assumed

Any phase must be startable with a fresh agent using only the repo + this file.
The agent MUST assume zero prior conversation history.

On receiving `start phase N`, the agent MUST run this bootstrap before coding:

1. Read `plans.md` sections 0–1 and ONLY Phase N section. Do not read ahead / implement later phases.
2. Read `README.md`, `docs/PHASE_N-1_SUMMARY.md` (or latest summary if N=0, skip),
   `docs/API.md` and `docs/DECISIONS.md` if they exist.
3. Read the source-of-truth interfaces: `include/spotify_bridge.h`,
   `rust/librespot-bridge/Cargo.toml`, `CMakeLists.txt`.
4. Run `git status --short` and `cmake --build build --config Release` (or
   `cmake -S . -B build` if no build dir). If broken, STOP and report — do not start new work.
5. Confirm Allowed / Forbidden files for Phase N (see 1.15).

Do not use phrases like "as before" / "current implementation" without a
`file:line` reference. All claims must be verifiable from files on disk.

---

## 1.13 Persistent handoff — required docs

Conversation memory is not state. Files on disk are state.
At the end of every phase (including mid-phase stops), the agent MUST
create/update:

```text
docs/PHASE_X_SUMMARY.md  — what was built, how to test, known issues, librespot rev used
docs/API.md              — current C ABI signatures + ownership + threading notes
docs/DECISIONS.md        — append-only log: date, decision, why, alternatives rejected
```

The next phase's agent reads only these + code. If the summary is missing,
the phase is NOT complete.

---

## 1.14 Machine-checkable completion gate

Prose HARD STOPs are easy to ignore. A phase is complete ONLY if:

1. `cmake --build build --config Release` succeeds, log pasted.
2. `git status --short` and `git diff --stat` are shown — only Allowed files touched.
3. The `PHASE X COMPLETE` block from 1.2 is printed verbatim, ending with
   `WAITING FOR USER APPROVAL.` as the last line.
4. `docs/PHASE_X_SUMMARY.md`, `docs/API.md`, `docs/DECISIONS.md` are updated.
5. No new work after the report. Version control per 1.18 is done (committed + pushed, or stop reason reported).

If any check fails, the phase is incomplete — STOP anyway and report which check failed.

---

## 1.15 Phase file scope — default Allowed files

Unless a phase section says otherwise, this scope applies:

```text
Phase -1: README.md, .gitignore, docs/ (no code)
Phase 0:  CMakeLists.txt, Cargo.toml, cpp/, rust/, include/, .gitignore, README.md, docs/
Phase 1:  rust/librespot-bridge/**, docs/
Phase 2:  rust/librespot-bridge/**, include/spotify_bridge.h, cpp/bridge_test.cpp, docs/
Phase 3:  cpp/core/**, include/spotify_bridge.h (read-only unless ABI bug), docs/
Phase 4:  cpp/app/tui.*, cpp/core/**, docs/
Phase 5+: cpp/core/**, cpp/cache/**, cpp/ui/** per phase, docs/
Windows-only: cpp/platform/windows/**
```

Forbidden always unless phase explicitly allows: `.env`, `*token*`, `*credential*`,
system dirs, files outside repo, large new dependencies.

Phase 4 note: `cpp/core/` may be modified only for small integration fixes
required to expose existing functionality to the TUI. Do not redesign or add
new core functionality during Phase 4.

---

## 1.16 Dependency pinning

Before adding/updating librespot or any significant dependency:

1. Record exact rev/commit + feature flags in `docs/DECISIONS.md` and `docs/PHASE_X_SUMMARY.md`.
2. Commit `Cargo.lock` changes separately and show `cargo tree -i librespot` (or equivalent).
3. If the API drifted from this plan, document the deviation per 1.8 and STOP if the phase goal would change.

Do not upgrade dependencies mid-phase to "fix" unrelated issues.

---

# 1.17 Phase Contract Template

Every phase section below MUST be read as if it contains these headings,
even where abbreviated:

```text
Goal:
Prerequisites: [previous PHASE_SUMMARY + build green]
Allowed files: [see 1.15 unless overridden]
Forbidden: [later-phase features, unrelated refactors]
Tasks: [...]
Acceptance (exact commands):
Handoff: [update docs per 1.13 + PHASE X COMPLETE per 1.2 + gate per 1.14 + commit/push per 1.18]
```

If a phase lacks explicit acceptance commands, default to:
`cmake -S . -B build; cmake --build build --config Release` + the phase's
manual test steps.

---

## 1.18 Version control — commit + push regularly

Fresh agents reconstruct state from GitHub history + files on disk, so
history must be clean and current.

Canonical remote (`origin`):

```text
https://github.com/akshiterate/spotilite.git
```

The agent MUST:

1. Verify `git status --short` before committing — only Allowed files per 1.15.
   Never commit `.env`, `*token*`, `*credential*`, `build/`, `target/`,
   cache/ or auth files. STOP if secrets would be committed.
2. Commit at each logical milestone, at every mid-phase checkpoint (1.10),
   and at phase completion (1.14). Do not batch an entire phase into one giant commit.
3. Use small, descriptive messages:
   ```text
   phase-N: short description
   ```
   Example: `phase-4: add TUI playback loop`.
4. Push to `origin` (default branch) after each commit if a GitHub remote exists
   and credentials are configured. Verify `git remote -v` matches the canonical
   remote above — if it differs, STOP and ask, do not retarget it. If push fails
   (no remote / no auth), STOP and report it in the checkpoint summary — do not
   work around it by changing remotes, rewriting history, or storing credentials
   in the repo.
5. Never `push --force`, never amend pushed commits, never rebase unless the user
   explicitly asked. Prefer `git log --oneline -10` to confirm history.
6. Include the latest commit hash + `git log --oneline -5` in the
   `PHASE X COMPLETE` / mid-phase report, so the next fresh agent can
   `git pull` / `git log` to resume.

A phase is NOT complete unless its work is committed and pushed, or a push
failure is explicitly reported with reason.

---

# 2. Architecture

```text
                         spotilite
                              │
                 ┌────────────┴────────────┐
                 │                         │
             C++ Application          Rust Bridge
                 │                         │
        ┌────────┼────────┐                │
        │        │        │                │
       UI      Core     Cache          librespot
        │        │        │                │
        └────────┼────────┘                │
                 │                         │
                 └─────── C ABI ───────────┘
                              │
                           Spotify
```

Dependency direction:

```text
GUI/TUI
   ↓
C++ Core
   ↓
C ABI
   ↓
Rust Bridge
   ↓
librespot
```

The C++ application must not depend directly on librespot internals.

---

# 3. Repository Layout

```text
spotilite/
│
├── CMakeLists.txt
├── Cargo.toml
├── Cargo.lock
├── README.md
├── plans.md
│
├── cpp/
│   ├── app/
│   ├── core/
│   ├── ui/
│   ├── cache/
│   └── platform/
│       └── windows/
│
├── rust/
│   └── librespot-bridge/
│       ├── Cargo.toml
│       └── src/
│           └── lib.rs
│
├── include/
│   └── spotify_bridge.h
│
└── third_party/
```

The exact layout may evolve when technically necessary, but responsibilities must remain separated.

---

# 4. Build System

Use:

- **CMake** for C++.
- **Cargo** for Rust.
- CMake should orchestrate the Rust build.

Target developer workflow:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

The final executable should eventually be:

```text
build/Release/spotilite.exe
```

---

# 5. UI Philosophy

The UI is intentionally **bare-bones and minimal**.

The goal is not to recreate Spotify's visual design.

The goal is:

> **A tiny native interface that gets out of the way and lets the music player work.**

Avoid:

- Gradients
- Blur
- Large animations
- Animated backgrounds
- Shadows everywhere
- Excessive rounded cards
- Decorative panels
- Complex navigation
- Full-screen artwork
- Spotify-style visual effects
- Unnecessary icons
- Animated transitions

Prefer:

- Flat layout
- Simple text
- Small controls
- Minimal borders
- Small amount of artwork
- Dense information
- Keyboard-friendly interaction
- Fast rendering

The application should look closer to a **simple media player** than a Spotify clone.

---

# 6. Minimal GUI Layout

The initial GUI should eventually resemble:

```text
┌─────────────────────────────────────────────┐
│ spotilite                                │
├─────────────────────────────────────────────┤
│ Search: [________________________]           │
│                                             │
│ Results                                     │
│ ─────────────────────────────────────────── │
│ Track Name          Artist                  │
│ Another Track       Artist                  │
│                                             │
│                                             │
├─────────────────────────────────────────────┤
│ [art]  Track Name — Artist                  │
│        ───────────────  2:31 / 4:02         │
│        [<<] [▶] [>>]              Volume    │
└─────────────────────────────────────────────┘
```

That's enough.

Do not add Home, Discover, recommendations, podcasts, social features, or other Spotify UI sections unless explicitly requested later.

---

# 7. GUI Technology

Use **Dear ImGui** for the MVP unless a concrete technical limitation prevents it.

No:

- WebView
- HTML
- CSS
- JavaScript
- Electron
- Chromium

The GUI should consume the C++ core rather than talking directly to Rust.

---

# 8. Phase -1 — Git + GitHub Smoke Test

## Objective

Prove git + GitHub works before any code is written, so all later phases
can rely on version control per 1.18.

This is how the agent knows git is working: it must successfully commit
and push, then verify from both local git and the remote.

### Prerequisites

- None. Fresh repo or existing checkout. No build required.

### Allowed files

```text
README.md, .gitignore, docs/
```

No code. No CMake/Cargo changes. No secrets.

### Tasks

- [ ] Run `git status --short`, `git log --oneline -5`, `git remote -v`. Record output.
- [ ] If no repo: `git init -b main`. Expected `origin` is
  `https://github.com/akshiterate/spotilite.git` — if `origin` is missing, add it
  with `git remote add origin https://github.com/akshiterate/spotilite.git`.
  If `origin` exists but points elsewhere, STOP and ask — do not retarget it.
- [ ] Check `git config user.name` / `user.email` (local). If missing, STOP and report — do not set globally.
- [ ] Create/update `README.md` with repo title (`spotilite`) + one-line goal.
- [ ] Ensure `.gitignore` ignores `build/`, `target/`, `*.token`, `*credential*`, cache/, auth/.
- [ ] Create `docs/` stub if missing (empty `API.md`, `DECISIONS.md` allowed).
- [ ] Commit: `phase--1: init repo + README smoke test`.
- [ ] Push to `origin/main` (or default branch). Then verify:
  `git log --oneline -3`, `git status --short` (clean), `git ls-remote origin`.

### Acceptance criteria

```powershell
git log --oneline -3
git remote -v
git ls-remote origin
```

all succeed, working tree clean, and the commit is visible at
`https://github.com/akshiterate/spotilite`.
Record commit hash + `git log --oneline -5` in `docs/PHASE_-1_SUMMARY.md`.

### HARD STOP

Do not proceed to Phase 0.

Wait for user testing and approval. Verify in GitHub UI that README rendered.

---

# 9. Phase 0 — Repository + Build

## Objective

Create the minimal buildable project.

### Tasks

- [ ] Create repository structure.
- [ ] Create CMake project.
- [ ] Create Cargo workspace.
- [ ] Create Rust bridge crate.
- [ ] Create C++ executable.
- [ ] Create `.gitignore` (build/, target/, *.token, *credential*, cache/, auth/).
- [ ] Pin initial librespot rev (even if unused yet) + record in `docs/DECISIONS.md`.
- [ ] Create `docs/` with empty `API.md`, `DECISIONS.md` templates.
- [ ] Verify C++ builds.
- [ ] Verify Rust builds.
- [ ] Verify CMake builds both.
- [ ] Launch executable.
- [ ] Write `docs/PHASE_0_SUMMARY.md` per 1.13.

### Expected application

```text
spotilite
Build successful.
```

### Acceptance criteria

These work:

```powershell
cmake -S . -B build
cmake --build build --config Release
```

and:

```powershell
build\Release\spotilite.exe
```

launches successfully.

### HARD STOP

Do not proceed to Phase 1.

Wait for user testing and approval.

---

# 10. Phase 1 — librespot Playback Proof

## Objective

Prove that Spotify playback works before building the application around it.

### Tasks

- [ ] Add the minimum required librespot dependencies.
- [ ] Determine current librespot feature flags.
- [ ] Configure Windows audio.
- [ ] Configure authentication.
- [ ] Configure Spotify Connect/discovery.
- [ ] Build a minimal headless receiver.
- [ ] Authenticate.
- [ ] Make spotilite visible to the official Spotify application.
- [ ] Play a track.

### Success condition

```text
Official Spotify
      ↓
Connect to a device
      ↓
spotilite
      ↓
Play
      ↓
Windows audio
```

### HARD STOP

The agent must not start GUI development.

The user must manually verify that audio actually plays.

---

# 11. Phase 2 — C ABI Bridge

## Objective

Control librespot from C++.

Initial conceptual API:

```c
SpotifyPlayer* spotify_create();
void spotify_destroy(SpotifyPlayer*);

int spotify_connect(SpotifyPlayer*);

int spotify_load_uri(
    SpotifyPlayer*,
    const char* uri
);

int spotify_play(SpotifyPlayer*);
int spotify_pause(SpotifyPlayer*);
int spotify_resume(SpotifyPlayer*);
int spotify_next(SpotifyPlayer*);
int spotify_previous(SpotifyPlayer*);

int spotify_seek(
    SpotifyPlayer*,
    uint32_t position_ms
);

int spotify_set_volume(
    SpotifyPlayer*,
    float volume
);
```

The actual API should reflect the current librespot implementation.

### Test program

Create a tiny C++ console test:

```text
spotilite bridge test

Connected.

> play spotify:track:...
Playing.

> pause
Paused.

> play
Playing.
```

### Acceptance criteria

C++ successfully:

- Creates the Rust player.
- Connects.
- Loads a track/URI.
- Plays.
- Pauses.
- Resumes.
- Changes volume.
- Destroys the player safely.

### HARD STOP

Wait for user testing.

---

# 12. Phase 3 — C++ Player Core

## Objective

Build the first real C++ application core.

Implement:

- Player
- Queue
- Playback state
- Basic commands
- Basic events

Conceptually:

```text
GUI/TUI
   ↓
Command
   ↓
C++ Core
   ↓
Rust
   ↓
librespot
```

and:

```text
librespot
   ↓
Event
   ↓
C++ Core
   ↓
Frontend
```

### Required player operations

```text
Play
Pause
Resume
Next
Previous
Seek
Volume
Load URI
```

### Acceptance criteria

A simple C++ test program can control playback through the C++ core without directly accessing the Rust implementation.

### HARD STOP

Do not build GUI yet.

---

# 13. Phase 4 — Minimal TUI

## Objective

Create a tiny terminal frontend.

The TUI should display only:

```text
SPOTILITE

Track
Artist
Album

00:42 / 03:47

[P] Play/Pause
[N] Next
[B] Previous
[V] Volume
[Q] Quit
```

Optional:

```text
URI: spotify:track:...
```

### Requirements

- Uses the C++ core.
- Does not duplicate playback logic.
- Updates from player state/events.
- No unnecessary animations.

### Acceptance criteria

The user can:

- Launch TUI.
- Connect.
- Play.
- Pause.
- Resume.
- Skip.
- Change volume.
- Quit.

### HARD STOP

Wait for manual testing.

---

# 14. Phase 5 — Metadata

## Objective

Expose useful track metadata.

Implement:

- Track name
- Artist
- Album
- Duration
- Track ID
- Spotify URI

Use librespot-provided metadata where practical.

Do not add the Web API unless necessary.

### Acceptance criteria

The TUI/test application displays accurate metadata for the currently playing track.

### HARD STOP

Wait for user testing.

---

# 15. Phase 6 — Artwork

## Objective

Add basic album artwork.

Requirements:

- Background loading.
- No UI blocking.
- Small disk cache.
- Small memory cache.
- Reasonable image dimensions.

Initial cache sizes:

```text
128px
256px
```

No elaborate image management system.

### Acceptance criteria

Artwork appears beside the current track without noticeably blocking playback or UI interaction.

### HARD STOP

Wait for user testing.

---

# 16. Phase 7 — Minimal GUI

## Objective

Create the first native GUI.

The GUI must contain only:

1. Search/input area.
2. Results area.
3. Current track.
4. Playback controls.
5. Progress.
6. Volume.
7. Small artwork.

Example:

```text
┌─────────────────────────────────────────────┐
│ spotilite                                │
├─────────────────────────────────────────────┤
│ Search: [radiohead________________]         │
│                                             │
│ Radiohead                                   │
│ ├─ Creep                                    │
│ ├─ Karma Police                             │
│ └─ No Surprises                             │
│                                             │
├─────────────────────────────────────────────┤
│ [art]  Creep — Radiohead                    │
│        ─────────────  1:21 / 3:55           │
│        [<<] [▶] [>>]              Vol: 70% │
└─────────────────────────────────────────────┘
```

### GUI rules

Keep it visually sparse.

Do not add:

- Home
- Discover
- Recommendations
- Animated artwork
- Large album backgrounds
- Complex sidebars
- Social features
- Podcasts
- Decorative UI

### Acceptance criteria

The GUI can:

- Launch.
- Show current playback state.
- Play/pause.
- Skip.
- Seek.
- Change volume.
- Display metadata.
- Display artwork.

### HARD STOP

The user must manually test the GUI before development continues.

---

# 17. Phase 8 — Search

## Objective

Implement Spotify search.

Search:

- Tracks
- Artists
- Albums
- Playlists

Results should be paginated.

Initial page:

```text
20–50 results
```

Do not load an entire search result set.

### Web API

Only now introduce Spotify Web API functionality if required.

Keep it isolated behind a C++ abstraction.

### Acceptance criteria

The user can search and play a track from results.

### HARD STOP

Wait for testing.

---

# 18. Phase 9 — Queue

## Objective

Implement queue management.

Required:

- Add
- Remove
- Reorder
- Clear
- Play selected
- Next
- Previous

The queue belongs to the C++ core.

The GUI only displays/manipulates it.

### Acceptance criteria

The user can create and modify a queue and playback follows it correctly.

### HARD STOP

Wait for testing.

---

# 19. Phase 10 — Library

Implement incrementally:

1. Liked Songs
2. Playlists
3. Albums
4. Artists

Everything must be lazy-loaded/paginated.

Do not load the entire library at startup.

### HARD STOP

After each library subsection works, stop and allow testing.

---

# 20. Phase 11 — Configuration

Eventually support:

```toml
bitrate = 160
normalisation = true
device_name = "spotilite"
cache_size_mb = 1024
connect = true
```

Potential settings:

- Bitrate
- Volume
- Normalisation
- Device name
- Cache location
- Cache size
- Connect
- Startup behaviour

Do not implement settings that don't correspond to an actual feature.

### HARD STOP

Wait for testing.

---

# 21. Phase 12 — Windows Integration

Implement only after the player is stable.

Potential features:

- Media Play/Pause
- Next
- Previous
- Windows media session
- Global media keys
- System tray
- Taskbar integration

Keep Windows-specific implementation under:

```text
cpp/platform/windows/
```

### HARD STOP

Wait for testing.

---

# 22. Phase 13 — Headless Mode

Create:

```powershell
spotilite.exe --headless
```

Headless mode should:

- Run without GUI.
- Authenticate.
- Advertise Spotify Connect.
- Receive playback.
- Play audio.
- Consume minimal resources.

### Acceptance criteria

The machine behaves as a lightweight Spotify Connect speaker.

### HARD STOP

Wait for testing.

---

# 23. Phase 14 — TUI Parity

Bring the TUI closer to GUI functionality.

Do not duplicate the core.

Both frontends must use:

```text
C++ Core
```

### HARD STOP

Wait for testing.

---

# 24. Phase 15 — Performance Pass

Only optimize after functionality is complete.

Measure:

```text
Startup time
RAM
Idle CPU
Playback CPU
Binary size
Network usage
```

Targets:

| Metric | Target |
|---|---:|
| Cold startup | <500 ms |
| Warm startup | <200 ms |
| GUI idle CPU | ~0% |
| Playback CPU | <3% |
| GUI RAM | ~40–60 MB |
| Headless RAM | <20 MB |
| Binary | Prefer <25 MB |

These are targets, not reasons to sacrifice maintainability.

### Optimization order

1. Measure.
2. Identify actual bottleneck.
3. Optimize.
4. Measure again.
5. Keep the optimization only if it provides meaningful benefit.

### HARD STOP

Report before/after measurements and wait for approval.

---

# 25. Phase 16 — Linux

Only begin Linux support after the Windows application is stable.

Reuse:

```text
C++ Core
Rust librespot bridge
```

Replace only platform-specific components.

Keep platform-specific code isolated.

### HARD STOP

Each major Linux milestone must be separately testable.

---

# 26. Final Application Architecture

The intended final structure is:

```text
                         spotilite
                              │
              ┌───────────────┴───────────────┐
              │                               │
          C++ Frontends                    C++ Core
          ┌────┴────┐                 ┌──────┼──────┐
          │         │                 │      │      │
         GUI       TUI              Player Queue Metadata
          │         │                 │      │      │
          └────┬────┘                 └──────┼──────┘
               │                             │
               └──────────────┬──────────────┘
                              │
                            C ABI
                              │
                        Rust Bridge
                              │
                         librespot
                              │
                           Spotify
```

---

# 27. Performance Rules

## UI

- No unnecessary continuous redraw.
- No unbounded lists.
- Virtualize large lists.
- Avoid expensive animation.
- Avoid unnecessary image decoding.
- Keep background work off the UI thread.

## Network

- No unnecessary polling.
- Cache metadata.
- Cache artwork.
- Avoid downloading unnecessary data.
- Prefetch only when useful.

## Startup

Prefer:

```text
Launch
 ↓
Initialize local state
 ↓
Display minimal UI
 ↓
Connect/authenticate asynchronously
 ↓
Hydrate
```

Do not unnecessarily block startup on network requests.

---

# 28. Security

Never:

- Commit credentials.
- Commit tokens.
- Put credentials in source code.
- Print tokens to logs.
- Store passwords.
- Upload authentication files.

Add appropriate `.gitignore` rules.

---

# 29. Dependency Policy

Before adding a dependency, ask:

> Do we actually need this?

Prefer existing dependencies.

Avoid duplicate libraries providing the same functionality.

For every significant dependency, document:

```text
Dependency:
Purpose:
Why existing dependencies are insufficient:
Expected cost:
```

Do not add `rspotify`, a database, or another large dependency merely because it appeared in an earlier version of this plan.

---

# 30. Testing Strategy

## Unit tests

Test:

- Queue
- URI parsing
- Player state
- Pagination
- Cache
- Configuration

## Integration tests

Test:

- C ABI
- Rust bridge
- Player lifecycle

## Manual tests

Required for:

- Spotify login
- Spotify Connect
- Actual audio
- Artwork
- GUI
- Windows media integration

---

# 31. Definition of Done

The project is complete when the user can launch:

```powershell
spotilite.exe
```

and obtain a lightweight native Spotify client capable of:

```text
Authentication
Playback
Search
Queue
Metadata
Artwork
Library
Volume
Seeking
Spotify Connect
```

with:

```text
C++ Application
       ↓
     C ABI
       ↓
Rust librespot bridge
       ↓
    librespot
       ↓
     Spotify
```

The application should remain:

- Native
- Small
- Responsive
- Resource-efficient
- Maintainable

---

# 32. MOST IMPORTANT AGENT INSTRUCTION

**Never continue to the next phase automatically.**

At the end of every phase:

```text
BUILD
 ↓
TEST
 ↓
REPORT
 ↓
STOP
```

The user will explicitly authorize the next phase.

A successful phase is **not** permission to start the next phase.

The agent must wait for a new instruction such as:

```text
continue
```

or:

```text
start phase 3
```

before making changes belonging to the next phase.

This rule applies even if the next phase appears trivial.

**The human controls phase progression.**

### Copy-paste starter prompt (stateless, recommended)

To start any phase with a fresh agent and no prior context, use:

```text
Read plans.md sections 1.1-1.18 and Phase N only. Follow Startup Protocol (1.12).
Allowed files per 1.15 unless Phase N overrides. Intra-phase budget per 1.10 applies.
Implement only Phase N. End with BUILD + TEST + docs update (1.13) + commit/push (1.18) + PHASE N COMPLETE
report (1.2) + gate checks (1.14). Then STOP with WAITING FOR USER APPROVAL as last line.
Do not start Phase N+1.
```

Replace N with the phase number. No other context is needed — the agent
must reconstruct state from `docs/PHASE_*_SUMMARY.md`, `docs/API.md`,
`docs/DECISIONS.md`, and code on disk.