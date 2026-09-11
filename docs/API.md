# API

No C ABI yet. The C ABI (`include/spotify_bridge.h`) is introduced in Phase 2.

Ownership / threading notes: none yet.

Phase 0: Rust crate `librespot-bridge` v0.1.0 exists with a
`bridge_version()` smoke symbol. No C symbols exported yet — the C ABI
(`include/spotify_bridge.h`) is introduced in Phase 2.

Phase 1 (no C ABI yet): `headless` binary (`src/bin/headless.rs`) runs a
Spirc Connect device driven entirely from the official Spotify app
(discovery provisioning or cached credentials; stdout status lines;
Ctrl+C quits). No callable Rust API surface added beyond Phase 0.

Phase 2: C ABI in `include/spotify_bridge.h`, implemented in
`rust/librespot-bridge/src/lib.rs` against pinned librespot 0.8.

```c
SpotifyPlayer* spotify_create(void);
void spotify_destroy(SpotifyPlayer*);
int spotify_connect(SpotifyPlayer*);
int spotify_load_uri(SpotifyPlayer*, const char* uri);
int spotify_play(SpotifyPlayer*);
int spotify_pause(SpotifyPlayer*);
int spotify_resume(SpotifyPlayer*);            // == play
int spotify_seek(SpotifyPlayer*, uint32_t position_ms);
int spotify_set_volume(SpotifyPlayer*, float volume);  // [0,1] clamped
const char* spotify_last_error(const SpotifyPlayer*);
```

Return codes: `SPOTIFY_OK 0`, `NULL_ARG -1`, `NOT_CONNECTED -2`,
`AUTH -3`, `BAD_URI -4`, `AUDIO -5`, `INTERNAL -6`.
Deliberately no next/previous: no C++-drivable queue on Player in 0.8.

Ownership: create/destroy pairing, NULL-safe destroy; strings copied in;
last-error pointer borrowed until next failing call on the same thread.
Threading: synchronous calls, safe from any thread, no concurrent destroy;
connect blocks on network, other calls return after handing commands to
the player thread (async playback errors via Phase 3 polling, not codes).
Credentials: machine-local Phase 1 cache, same device id; connect is
idempotent; missing cache fails with AUTH + message.

Phase 3: added event polling (foreseen by 1.7, additive only —
no existing signature changed):

```c
#define SPOTIFY_EVENT_NONE 0
#define SPOTIFY_EVENT_TRACK_STARTED 1
#define SPOTIFY_EVENT_PLAYING 2
#define SPOTIFY_EVENT_PAUSED 3
#define SPOTIFY_EVENT_TRACK_ENDED 4
#define SPOTIFY_EVENT_SEEKED 5
#define SPOTIFY_EVENT_VOLUME_CHANGED 6
#define SPOTIFY_EVENT_POSITION 7
#define SPOTIFY_EVENT_URI_MAX 128
typedef struct SpotifyEvent {
    int32_t type; uint32_t position_ms; uint16_t volume; uint16_t reserved;
    char uri[SPOTIFY_EVENT_URI_MAX];
} SpotifyEvent;
int spotify_poll_event(SpotifyPlayer*, SpotifyEvent*);  // 1 event / 0 none / <0 error
```

Poll drains unmodelled player events silently (preload hints, session/
cluster updates, shuffle/repeat flags, queue dumps). Position events flow
at 1s while playing (`position_update_interval`). Channel behind a Mutex;
poison/disconnect surface as INTERNAL.

Phase 3: C++ core in `cpp/core/` — the only layer frontends may
use. `spotilite::Player` (RAII over the ABI above):
connect/loadUri/play/pause/resume/seek/setVolume/next/previous/enqueue,
`pollEvent`/`drainEvents`, mirrored `PlaybackState`
(connected/playing/currentUri/volume/positionMs), bool + `lastError()`
errors. `spotilite::Queue` (header-only): add/clear/next/previous/select;
`loadUri` resets it to the single URI. No Rust symbols leak past the core.

Phase 4: terminal frontend `build/tui.exe` (`cpp/app/tui.*`,
`main` included) over the core only. Commands: `play <uri>`, `p`
toggle, `n`/`b` skip, `add <uri>`, `v <0-100>`, `s` refresh, `h`, `q`.
Renders title, current URI, elapsed mm:ss, state, volume %, queue size +
index after every command. Metadata fields are placeholders until Phase 5.

Phase 5 (current): metadata query over librespot metadata (no Web API):

```c
typedef struct SpotifyMetadata {
    char title[256]; char artist[256]; char album[256];
    uint32_t duration_ms; char uri[128]; char track_id[32];
} SpotifyMetadata;
int spotify_current_metadata(SpotifyPlayer*, SpotifyMetadata*);
```

C++: `spotilite::TrackMetadata` + `Player::metadata()` (bool +
`lastError()`). Fails when not connected / nothing loaded / fetch fails.
Handle stores the canonical loaded URI; `Track` fetch covers
title/artist/album/duration in one request.
