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

Phase 2 (current): C ABI in `include/spotify_bridge.h`, implemented in
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
