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
