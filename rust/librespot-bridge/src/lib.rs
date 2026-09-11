//! C ABI bridge (plans.md Phase 2): control the librespot `Player` from C++.
//!
//! The handle owns a Tokio runtime, a `Session`, a `Player`, its `Mixer`
//! and the credential `Cache`. All `spotify_*` calls are synchronous; only
//! `connect` blocks on the network, the rest hand commands to the player
//! thread and return. No callbacks (polling arrives in Phase 3).
//!
//! Deliberately absent vs the plan's sketch: next/previous — librespot 0.8
//! exposes no C++-driven queue on `Player` (queue lives in the Phase 3/9
//! C++ core). `resume` is provided and maps to `play`.

use std::cell::RefCell;
use std::ffi::CStr;
use std::os::raw::{c_char, c_float};
use std::path::PathBuf;
use std::sync::{
    atomic::{AtomicBool, Ordering},
    Arc, Mutex,
};

use librespot::{
    core::{
        authentication::AuthenticationError, cache::Cache, config::SessionConfig,
        session::Session, SpotifyUri,
    },
    playback::{
        audio_backend,
        config::{AudioFormat, PlayerConfig},
        mixer::{self, Mixer, MixerConfig},
        player::{Player, PlayerEvent, PlayerEventChannel},
    },
};

const DEVICE_ID_FILE: &str = "device-id";

/// Opaque to C++; see `include/spotify_bridge.h` for the contract.
pub struct SpotifyPlayer {
    rt: tokio::runtime::Runtime,
    session: Session,
    player: Arc<Player>,
    mixer: Arc<dyn Mixer>,
    cache: Cache,
    connected: AtomicBool,
    event_rx: Mutex<PlayerEventChannel>,
}

// Last-error text, one buffer per calling thread. The pointer handed out is
// valid until the next failing call on the same thread (documented in the
// header). Query on the thread that made the call.
thread_local! {
    static LAST_ERROR_BUF: RefCell<Vec<u8>> = const { RefCell::new(Vec::new()) };
}

fn stash_error(msg: &str) -> *const c_char {
    LAST_ERROR_BUF.with(|buf| {
        let mut buf = buf.borrow_mut();
        buf.clear();
        buf.extend_from_slice(msg.as_bytes());
        buf.push(0);
        buf.as_ptr() as *const c_char
    })
}

fn fail(code: std::os::raw::c_int, msg: String) -> std::os::raw::c_int {
    stash_error(&msg);
    code
}

fn check_connected(handle: &SpotifyPlayer) -> Result<(), (std::os::raw::c_int, String)> {
    if handle.connected.load(Ordering::SeqCst) {
        Ok(())
    } else {
        Err((
            SPOTIFY_ERR_NOT_CONNECTED,
            "not connected; call spotify_connect() first".to_owned(),
        ))
    }
}

fn cache_dir() -> PathBuf {
    let base = std::env::var_os("LOCALAPPDATA")
        .map(PathBuf::from)
        .unwrap_or_else(std::env::temp_dir);
    base.join("spotilite").join("cache")
}

/// Stable device id shared with the headless receiver (credential blobs are
/// device-bound). Missing or empty file: generate and persist.
fn device_id(dir: &PathBuf) -> Result<String, librespot::core::Error> {
    let path = dir.join(DEVICE_ID_FILE);
    if let Ok(id) = std::fs::read_to_string(&path) {
        let id = id.trim().to_owned();
        if !id.is_empty() {
            return Ok(id);
        }
    }
    let id = SessionConfig::default().device_id;
    if let Some(parent) = path.parent() {
        std::fs::create_dir_all(parent)?;
    }
    std::fs::write(&path, &id)?;
    Ok(id)
}

const SPOTIFY_OK: std::os::raw::c_int = 0;
const SPOTIFY_ERR_NULL_ARG: std::os::raw::c_int = -1;
const SPOTIFY_ERR_NOT_CONNECTED: std::os::raw::c_int = -2;
const SPOTIFY_ERR_AUTH: std::os::raw::c_int = -3;
const SPOTIFY_ERR_BAD_URI: std::os::raw::c_int = -4;
// Reserved for header parity: creation-time failures have no code channel
// and surface via spotify_last_error() instead.
#[allow(dead_code)]
const SPOTIFY_ERR_AUDIO: std::os::raw::c_int = -5;
const SPOTIFY_ERR_INTERNAL: std::os::raw::c_int = -6;

/// Crate version smoke-test symbol (kept from Phase 0).
pub fn bridge_version() -> &'static str {
    env!("CARGO_PKG_VERSION")
}

#[no_mangle]
pub unsafe extern "C" fn spotify_create() -> *mut SpotifyPlayer {
    let build = || -> Result<*mut SpotifyPlayer, String> {
        let rt = tokio::runtime::Builder::new_multi_thread()
            .worker_threads(2)
            .enable_all()
            .build()
            .map_err(|e| format!("runtime: {e}"))?;

        let dir = cache_dir();
        let id = device_id(&dir).map_err(|e| format!("device id: {e}"))?;
        let files_dir = dir.join("files");
        let cache =
            Cache::new(Some(&dir), Some(&dir), Some(&files_dir), None).map_err(|e| format!("cache: {e}"))?;
        // Session takes ownership of a Cache; a second instance over the
        // same paths is cheap (directory creation is idempotent) and keeps
        // credential reads available on the handle.
        let session_cache =
            Cache::new(Some(&dir), Some(&dir), Some(&files_dir), None).map_err(|e| format!("cache: {e}"))?;

        let mut session_config = SessionConfig::default();
        session_config.device_id = id;

        let mixer = mixer::find(None)
            .ok_or_else(|| "no mixer backend".to_owned())?(MixerConfig::default())
            .map_err(|e| format!("mixer: {e}"))?;
        let sink_builder =
            audio_backend::find(None).ok_or_else(|| "no audio backend".to_owned())?;
        let audio_format = AudioFormat::default();

        // Session::new (and Player::new) require a Tokio runtime context:
        // they spawn background tasks via tokio primitives at construction.
        // Position updates feed the C++ core's playback-state polling.
        let (session, player, event_rx) = rt.block_on(async {
            let session = Session::new(session_config, Some(session_cache));
            let mut player_config = PlayerConfig::default();
            player_config.position_update_interval =
                Some(std::time::Duration::from_millis(1000));
            let player = Player::new(
                player_config,
                session.clone(),
                mixer.get_soft_volume(),
                move || sink_builder(None, audio_format),
            );
            let event_rx = player.get_player_event_channel();
            (session, player, event_rx)
        });

        let handle = SpotifyPlayer {
            rt,
            session,
            player,
            mixer,
            cache,
            connected: AtomicBool::new(false),
            event_rx: Mutex::new(event_rx),
        };
        Ok(Box::into_raw(Box::new(handle)))
    };

    match build() {
        Ok(ptr) => ptr,
        Err(msg) => {
            stash_error(&msg);
            std::ptr::null_mut()
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn spotify_destroy(player: *mut SpotifyPlayer) {
    if !player.is_null() {
        drop(Box::from_raw(player));
    }
}

fn handle_ref<'a>(player: *const SpotifyPlayer) -> Result<&'a SpotifyPlayer, (std::os::raw::c_int, String)> {
    if player.is_null() {
        Err((SPOTIFY_ERR_NULL_ARG, "null player handle".to_owned()))
    } else {
        // SAFETY: by contract the pointer comes from spotify_create() and is
        // not used after spotify_destroy() (documented in the header).
        Ok(unsafe { &*player })
    }
}

#[no_mangle]
pub unsafe extern "C" fn spotify_connect(player: *mut SpotifyPlayer) -> std::os::raw::c_int {
    let handle = match handle_ref(player) {
        Ok(h) => h,
        Err((code, msg)) => return fail(code, msg),
    };
    if handle.connected.load(Ordering::SeqCst) {
        return SPOTIFY_OK;
    }
    let credentials = match handle.cache.credentials() {
        Some(c) => c,
        None => {
            return fail(
                SPOTIFY_ERR_AUTH,
                "no cached credentials; run the headless receiver once to provision".to_owned(),
            );
        }
    };
    match handle
        .rt
        .block_on(handle.session.connect(credentials, true))
    {
        Ok(()) => {
            handle.connected.store(true, Ordering::SeqCst);
            SPOTIFY_OK
        }
        Err(e) => {
            // Login rejections vs transport failures get distinct codes;
            // the full message is always in spotify_last_error().
            let code = if e.error.downcast_ref::<AuthenticationError>().is_some() {
                SPOTIFY_ERR_AUTH
            } else {
                SPOTIFY_ERR_INTERNAL
            };
            fail(code, format!("connect: {e}"))
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn spotify_load_uri(
    player: *mut SpotifyPlayer,
    uri: *const c_char,
) -> std::os::raw::c_int {
    let handle = match handle_ref(player) {
        Ok(h) => h,
        Err((code, msg)) => return fail(code, msg),
    };
    if let Err((code, msg)) = check_connected(handle) {
        return fail(code, msg);
    }
    if uri.is_null() {
        return fail(SPOTIFY_ERR_NULL_ARG, "null uri".to_owned());
    }
    // SAFETY: null-checked above; caller passes valid UTF-8 per header.
    let uri = unsafe { CStr::from_ptr(uri) };
    let uri = match uri.to_str() {
        Ok(s) => s,
        Err(_) => return fail(SPOTIFY_ERR_BAD_URI, "uri is not valid UTF-8".to_owned()),
    };
    let parsed = match SpotifyUri::from_uri(uri) {
        Ok(u) => u,
        Err(e) => return fail(SPOTIFY_ERR_BAD_URI, format!("bad uri: {e}")),
    };
    if !parsed.is_playable() {
        return fail(
            SPOTIFY_ERR_BAD_URI,
            format!("uri is not playable audio: {uri}"),
        );
    }
    handle.player.load(parsed, true, 0);
    SPOTIFY_OK
}

#[no_mangle]
pub unsafe extern "C" fn spotify_play(player: *mut SpotifyPlayer) -> std::os::raw::c_int {
    let handle = match handle_ref(player) {
        Ok(h) => h,
        Err((code, msg)) => return fail(code, msg),
    };
    if let Err((code, msg)) = check_connected(handle) {
        return fail(code, msg);
    }
    handle.player.play();
    SPOTIFY_OK
}

#[no_mangle]
pub unsafe extern "C" fn spotify_pause(player: *mut SpotifyPlayer) -> std::os::raw::c_int {
    let handle = match handle_ref(player) {
        Ok(h) => h,
        Err((code, msg)) => return fail(code, msg),
    };
    if let Err((code, msg)) = check_connected(handle) {
        return fail(code, msg);
    }
    handle.player.pause();
    SPOTIFY_OK
}

#[no_mangle]
pub unsafe extern "C" fn spotify_resume(player: *mut SpotifyPlayer) -> std::os::raw::c_int {
    // Resume is play on the librespot player.
    unsafe { spotify_play(player) }
}

#[no_mangle]
pub unsafe extern "C" fn spotify_seek(
    player: *mut SpotifyPlayer,
    position_ms: u32,
) -> std::os::raw::c_int {
    let handle = match handle_ref(player) {
        Ok(h) => h,
        Err((code, msg)) => return fail(code, msg),
    };
    if let Err((code, msg)) = check_connected(handle) {
        return fail(code, msg);
    }
    handle.player.seek(position_ms);
    SPOTIFY_OK
}

#[no_mangle]
pub unsafe extern "C" fn spotify_set_volume(
    player: *mut SpotifyPlayer,
    volume: c_float,
) -> std::os::raw::c_int {
    let handle = match handle_ref(player) {
        Ok(h) => h,
        Err((code, msg)) => return fail(code, msg),
    };
    if let Err((code, msg)) = check_connected(handle) {
        return fail(code, msg);
    }
    let clamped = volume.clamp(0.0, 1.0);
    let raw = (clamped * u16::MAX as c_float).round() as u16;
    handle.mixer.set_volume(raw);
    SPOTIFY_OK
}

// Event codes mirroring include/spotify_bridge.h.
const SPOTIFY_EVENT_TRACK_STARTED: i32 = 1;
const SPOTIFY_EVENT_PLAYING: i32 = 2;
const SPOTIFY_EVENT_PAUSED: i32 = 3;
const SPOTIFY_EVENT_TRACK_ENDED: i32 = 4;
const SPOTIFY_EVENT_SEEKED: i32 = 5;
const SPOTIFY_EVENT_VOLUME_CHANGED: i32 = 6;
const SPOTIFY_EVENT_POSITION: i32 = 7;
const SPOTIFY_EVENT_URI_MAX: usize = 128;

/// C layout twin of `SpotifyEvent` (field order and types must match).
#[repr(C)]
pub struct SpotifyEvent {
    pub event_type: i32,
    pub position_ms: u32,
    pub volume: u16,
    pub reserved: u16,
    pub uri: [c_char; SPOTIFY_EVENT_URI_MAX],
}

fn fill_uri(slot: &mut [c_char; SPOTIFY_EVENT_URI_MAX], uri: &SpotifyUri) {
    let text = uri.to_string();
    let bytes = text.as_bytes();
    let n = bytes.len().min(SPOTIFY_EVENT_URI_MAX - 1);
    for (i, b) in bytes[..n].iter().enumerate() {
        slot[i] = *b as c_char;
    }
    slot[n] = 0;
}

/// Map one player event to a C event. `None` = not modelled; the poll loop
/// drops it and keeps draining.
fn map_event(event: PlayerEvent) -> Option<SpotifyEvent> {
    let mut out = SpotifyEvent {
        event_type: 0,
        position_ms: 0,
        volume: 0,
        reserved: 0,
        uri: [0; SPOTIFY_EVENT_URI_MAX],
    };
    match event {
        PlayerEvent::Loading {
            track_id,
            position_ms,
            ..
        } => {
            out.event_type = SPOTIFY_EVENT_TRACK_STARTED;
            out.position_ms = position_ms;
            fill_uri(&mut out.uri, &track_id);
        }
        PlayerEvent::Playing {
            track_id,
            position_ms,
            ..
        } => {
            out.event_type = SPOTIFY_EVENT_PLAYING;
            out.position_ms = position_ms;
            fill_uri(&mut out.uri, &track_id);
        }
        PlayerEvent::Paused {
            track_id,
            position_ms,
            ..
        } => {
            out.event_type = SPOTIFY_EVENT_PAUSED;
            out.position_ms = position_ms;
            fill_uri(&mut out.uri, &track_id);
        }
        PlayerEvent::EndOfTrack { track_id, .. }
        | PlayerEvent::Unavailable { track_id, .. }
        | PlayerEvent::Stopped { track_id, .. } => {
            out.event_type = SPOTIFY_EVENT_TRACK_ENDED;
            fill_uri(&mut out.uri, &track_id);
        }
        PlayerEvent::Seeked {
            track_id,
            position_ms,
            ..
        } => {
            out.event_type = SPOTIFY_EVENT_SEEKED;
            out.position_ms = position_ms;
            fill_uri(&mut out.uri, &track_id);
        }
        PlayerEvent::VolumeChanged { volume } => {
            out.event_type = SPOTIFY_EVENT_VOLUME_CHANGED;
            out.volume = volume;
        }
        PlayerEvent::PositionChanged { position_ms, .. }
        | PlayerEvent::PositionCorrection { position_ms, .. } => {
            out.event_type = SPOTIFY_EVENT_POSITION;
            out.position_ms = position_ms;
        }
        // Drained silently: preload hints, request ids, session/cluster
        // updates, shuffle/repeat/autoplay/filter flags, queue dumps.
        _ => return None,
    }
    Some(out)
}

#[no_mangle]
pub unsafe extern "C" fn spotify_poll_event(
    player: *mut SpotifyPlayer,
    out: *mut SpotifyEvent,
) -> std::os::raw::c_int {
    let handle = match handle_ref(player) {
        Ok(h) => h,
        Err((code, msg)) => return fail(code, msg),
    };
    if out.is_null() {
        return fail(SPOTIFY_ERR_NULL_ARG, "null event out-pointer".to_owned());
    }
    let mut rx = match handle.event_rx.lock() {
        Ok(guard) => guard,
        Err(_) => return fail(SPOTIFY_ERR_INTERNAL, "event lock poisoned".to_owned()),
    };
    loop {
        match rx.try_recv() {
            Ok(event) => {
                if let Some(mapped) = map_event(event) {
                    // SAFETY: null-checked above; caller provides storage.
                    unsafe {
                        *out = mapped;
                    }
                    return 1;
                }
            }
            Err(tokio::sync::mpsc::error::TryRecvError::Empty) => return 0,
            Err(tokio::sync::mpsc::error::TryRecvError::Disconnected) => {
                return fail(
                    SPOTIFY_ERR_INTERNAL,
                    "player event channel closed".to_owned(),
                )
            }
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn spotify_last_error(player: *const SpotifyPlayer) -> *const c_char {
    let _ = player;
    LAST_ERROR_BUF.with(|buf| {
        let buf = buf.borrow();
        if buf.is_empty() {
            c"ok".as_ptr()
        } else {
            buf.as_ptr() as *const c_char
        }
    })
}
