//! Thin adapter around librespot (plans.md 1.6).
//!
//! Phase 0: stub crate so the workspace + CMake-orchestrated Cargo build can
//! be verified. Real session/playback code lands in Phase 1; the C ABI in
//! Phase 2.

/// Crate version smoke-test symbol.
pub fn bridge_version() -> &'static str {
    env!("CARGO_PKG_VERSION")
}
