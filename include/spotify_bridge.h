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
//   Phase 3 events, not return codes).
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

// Human-readable description of the last failure on the calling thread.
// Never NULL. Pass NULL to read a creation-time failure.
const char* spotify_last_error(const SpotifyPlayer* player);

#ifdef __cplusplus
}
#endif

#endif // SPOTILITE_SPOTIFY_BRIDGE_H
