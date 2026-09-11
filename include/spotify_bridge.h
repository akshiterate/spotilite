// spotilite C bridge (plans.md Phase 2).
//
// Control the librespot player from C++. Mirrors the pinned librespot 0.8
// API: the player itself has load/play/pause/seek and volume lives on the
// mixer. There is deliberately no next/previous yet — librespot 0.8 keeps no
// C++-driven queue on Player (queue arrives with the C++ core in Phase 3/9).
//
// Ownership:
// - spotify_create() returns an opaque handle owned by the caller.
// - spotify_destroy() frees it; NULL is a safe no-op.
// - All strings are copied into Rust. The pointer from spotify_last_error()
//   is borrowed: valid until the next failing call on the same thread.
//
// Threading:
// - Calls are synchronous and safe from any thread, but do not destroy a
//   handle while another thread is using it.
// - No callbacks. Events arrive via polling in Phase 3.
// - spotify_connect() blocks on network I/O; the rest return once the
//   command is handed to the player thread (playback errors surface via
//   polled events, not return codes).
// - Player events are drained with non-blocking spotify_poll_event()
//   (preferred over callbacks, plans.md 1.7). Events the C layer does not
//   model are silently dropped by the poll call.
//
// Credentials:
// - spotify_connect() uses the cached credentials provisioned by the
//   headless receiver (Phase 1) from the same machine-local cache.
//   If none exist it fails; run headless.exe once to provision.

#ifndef SPOTILITE_SPOTIFY_BRIDGE_H
#define SPOTILITE_SPOTIFY_BRIDGE_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// Opaque player handle.
typedef struct SpotifyPlayer SpotifyPlayer;

// Return codes. 0 is success; anything else is a failure with details in
// spotify_last_error().
#define SPOTIFY_OK 0
#define SPOTIFY_ERR_NULL_ARG -1
#define SPOTIFY_ERR_NOT_CONNECTED -2
#define SPOTIFY_ERR_AUTH -3
#define SPOTIFY_ERR_BAD_URI -4
#define SPOTIFY_ERR_AUDIO -5
#define SPOTIFY_ERR_INTERNAL -6

// Create a player (runtime, session, mixer, audio sink). Returns NULL on
// failure; check spotify_last_error(NULL).
SpotifyPlayer* spotify_create(void);

// Destroy a player. NULL-safe.
void spotify_destroy(SpotifyPlayer* player);

// Connect with cached credentials. Idempotent: returns OK if already
// connected.
int spotify_connect(SpotifyPlayer* player);

// Load a playable URI (spotify:track:... / spotify:episode:...) and start
// playing it from the beginning. Requires a prior spotify_connect().
int spotify_load_uri(SpotifyPlayer* player, const char* uri);

// Start or resume playback.
int spotify_play(SpotifyPlayer* player);

// Pause playback.
int spotify_pause(SpotifyPlayer* player);

// Resume playback (same as spotify_play).
int spotify_resume(SpotifyPlayer* player);

// Seek to a position in milliseconds.
int spotify_seek(SpotifyPlayer* player, uint32_t position_ms);

// Set volume in [0.0, 1.0]; out-of-range values are clamped.
int spotify_set_volume(SpotifyPlayer* player, float volume);

// Player events. Drain with spotify_poll_event(); modelling is
// intentionally coarse (commands own the queue, Phase 3/9).
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
    int32_t type;
    uint32_t position_ms;
    uint16_t volume;    // valid for SPOTIFY_EVENT_VOLUME_CHANGED
    uint16_t reserved;
    char uri[SPOTIFY_EVENT_URI_MAX];  // set when the event carries a track
} SpotifyEvent;

// Copy out one pending player event without blocking. Returns 1 with *out
// filled, 0 when no event is pending, or a negative error code.
int spotify_poll_event(SpotifyPlayer* player, SpotifyEvent* out);

// Human-readable description of the last failure on the calling thread.
// Never NULL. Pass NULL to read a creation-time failure.
const char* spotify_last_error(const SpotifyPlayer* player);

#ifdef __cplusplus
}
#endif

#endif // SPOTILITE_SPOTIFY_BRIDGE_H
