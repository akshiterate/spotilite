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
use std::collections::{HashMap, VecDeque};
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
    metadata::{Episode, Metadata, Track},
    playback::{
        audio_backend,
        config::{AudioFormat, PlayerConfig},
        mixer::{self, Mixer, MixerConfig},
        player::{Player, PlayerEvent, PlayerEventChannel},
    },
};

use librespot_oauth::{OAuthClientBuilder, OAuthToken};
use serde_json::Value;

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
    current_uri: Mutex<String>,
    art_dir: PathBuf,
    art_tx: tokio::sync::mpsc::UnboundedSender<String>,
    art_rx: Mutex<tokio::sync::mpsc::UnboundedReceiver<String>>,
    art_mem: Mutex<ArtFifo>,
    web_token: Mutex<Option<OAuthToken>>,
}

// Small in-memory artwork cache: 128px BMP bytes keyed by track id, FIFO
// eviction. Disk holds both sizes; memory keeps only the small one.
struct ArtFifo {
    map: HashMap<String, Vec<u8>>,
    order: VecDeque<String>,
}

const ART_MEM_CAP: usize = 8;
const ART_SIZES: [u32; 2] = [128, 256];

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

        let (art_tx, art_rx) = tokio::sync::mpsc::unbounded_channel::<String>();

        let handle = SpotifyPlayer {
            rt,
            session,
            player,
            mixer,
            cache,
            connected: AtomicBool::new(false),
            event_rx: Mutex::new(event_rx),
            current_uri: Mutex::new(String::new()),
            art_dir: dir.join("art"),
            art_tx,
            art_rx: Mutex::new(art_rx),
            art_mem: Mutex::new(ArtFifo {
                map: HashMap::new(),
                order: VecDeque::new(),
            }),
            web_token: Mutex::new(None),
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
    if let Ok(mut slot) = handle.current_uri.lock() {
        *slot = parsed.to_string();
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
    fill_str(&mut slot[..], &uri.to_string());
}

fn fill_str(slot: &mut [c_char], text: &str) {
    let bytes = text.as_bytes();
    let n = bytes.len().min(slot.len() - 1);
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
    let mut art_rx = match handle.art_rx.lock() {
        Ok(guard) => guard,
        Err(_) => return fail(SPOTIFY_ERR_INTERNAL, "artwork lock poisoned".to_owned()),
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
            Err(tokio::sync::mpsc::error::TryRecvError::Empty) => {}
            Err(tokio::sync::mpsc::error::TryRecvError::Disconnected) => {
                return fail(
                    SPOTIFY_ERR_INTERNAL,
                    "player event channel closed".to_owned(),
                )
            }
        }
        match art_rx.try_recv() {
            Ok(uri) => {
                let mut mapped = SpotifyEvent {
                    event_type: SPOTIFY_EVENT_ARTWORK_READY,
                    position_ms: 0,
                    volume: 0,
                    reserved: 0,
                    uri: [0; SPOTIFY_EVENT_URI_MAX],
                };
                // Channel only carries URIs validated at request time.
                fill_str(&mut mapped.uri, &uri);
                // SAFETY: null-checked above; caller provides storage.
                unsafe {
                    *out = mapped;
                }
                return 1;
            }
            Err(tokio::sync::mpsc::error::TryRecvError::Empty) => return 0,
            Err(tokio::sync::mpsc::error::TryRecvError::Disconnected) => {
                return fail(
                    SPOTIFY_ERR_INTERNAL,
                    "artwork event channel closed".to_owned(),
                )
            }
        }
    }
}

const SPOTIFY_META_TITLE_MAX: usize = 256;
const SPOTIFY_META_ARTIST_MAX: usize = 256;
const SPOTIFY_META_ALBUM_MAX: usize = 256;
const SPOTIFY_META_URI_MAX: usize = 128;
const SPOTIFY_META_ID_MAX: usize = 32;

/// C layout twin of `SpotifyMetadata` (field order and types must match).
#[repr(C)]
pub struct SpotifyMetadata {
    pub title: [c_char; SPOTIFY_META_TITLE_MAX],
    pub artist: [c_char; SPOTIFY_META_ARTIST_MAX],
    pub album: [c_char; SPOTIFY_META_ALBUM_MAX],
    pub duration_ms: u32,
    pub uri: [c_char; SPOTIFY_META_URI_MAX],
    pub track_id: [c_char; SPOTIFY_META_ID_MAX],
}

fn fill_slot(slot: &mut [c_char], text: &str) {
    let bytes = text.as_bytes();
    let n = bytes.len().min(slot.len() - 1);
    for (i, byte) in bytes[..n].iter().enumerate() {
        slot[i] = *byte as c_char;
    }
    slot[n] = 0;
}

struct FetchedMetadata {
    title: String,
    artist: String,
    album: String,
    duration_ms: u32,
    track_id: String,
    uri: String,
}

// Single metadata fetch: Track carries album + artists inline, so one
// request covers everything. No Web API.
async fn fetch_metadata(
    session: &Session,
    uri: &SpotifyUri,
) -> Result<FetchedMetadata, String> {
    match uri {
        SpotifyUri::Track { .. } => {
            let track = Track::get(session, uri)
                .await
                .map_err(|e| format!("track metadata: {e}"))?;
            Ok(FetchedMetadata {
                title: track.name.clone(),
                artist: track.artists.0.iter().map(|a| a.name.as_str()).collect::<Vec<_>>().join(", "),
                album: track.album.name.clone(),
                duration_ms: track.duration.max(0) as u32,
                track_id: uri.to_id(),
                uri: uri.to_string(),
            })
        }
        SpotifyUri::Episode { .. } => {
            let episode = Episode::get(session, uri)
                .await
                .map_err(|e| format!("episode metadata: {e}"))?;
            Ok(FetchedMetadata {
                title: episode.name.clone(),
                artist: String::new(),
                album: String::new(),
                duration_ms: episode.duration.max(0) as u32,
                track_id: uri.to_id(),
                uri: uri.to_string(),
            })
        }
        _ => Err(format!("not playable audio: {uri}")),
    }
}

#[no_mangle]
pub unsafe extern "C" fn spotify_current_metadata(
    player: *mut SpotifyPlayer,
    out: *mut SpotifyMetadata,
) -> std::os::raw::c_int {
    let handle = match handle_ref(player) {
        Ok(h) => h,
        Err((code, msg)) => return fail(code, msg),
    };
    if let Err((code, msg)) = check_connected(handle) {
        return fail(code, msg);
    }
    if out.is_null() {
        return fail(SPOTIFY_ERR_NULL_ARG, "null metadata out-pointer".to_owned());
    }
    let uri_text = match handle.current_uri.lock() {
        Ok(guard) => guard.clone(),
        Err(_) => return fail(SPOTIFY_ERR_INTERNAL, "track lock poisoned".to_owned()),
    };
    if uri_text.is_empty() {
        return fail(
            SPOTIFY_ERR_NOT_CONNECTED,
            "no track loaded; call spotify_load_uri() first".to_owned(),
        );
    }
    let uri = match SpotifyUri::from_uri(&uri_text) {
        Ok(u) => u,
        Err(e) => return fail(SPOTIFY_ERR_BAD_URI, format!("stored uri invalid: {e}")),
    };
    let meta = match handle.rt.block_on(fetch_metadata(&handle.session, &uri)) {
        Ok(m) => m,
        Err(msg) => return fail(SPOTIFY_ERR_INTERNAL, msg),
    };
    // SAFETY: null-checked above; caller provides storage.
    let dest = unsafe { &mut *out };
    fill_slot(&mut dest.title, &meta.title);
    fill_slot(&mut dest.artist, &meta.artist);
    fill_slot(&mut dest.album, &meta.album);
    dest.duration_ms = meta.duration_ms;
    fill_slot(&mut dest.uri, &meta.uri);
    fill_slot(&mut dest.track_id, &meta.track_id);
    SPOTIFY_OK
}

const SPOTIFY_EVENT_ARTWORK_READY: i32 = 8;

fn art_path_for(dir: &PathBuf, track_id: &str, size: u32) -> PathBuf {
    dir.join(format!("{track_id}-{size}.bmp"))
}

fn check_art_size(size: std::os::raw::c_int) -> Result<u32, (std::os::raw::c_int, String)> {
    match size {
        128 => Ok(128),
        256 => Ok(256),
        _ => Err((
            SPOTIFY_ERR_BAD_URI,
            "unsupported artwork size (128 or 256)".to_owned(),
        )),
    }
}

fn parse_playable(uri_text: &str) -> Result<SpotifyUri, (std::os::raw::c_int, String)> {
    let uri = SpotifyUri::from_uri(uri_text)
        .map_err(|e| (SPOTIFY_ERR_BAD_URI, format!("bad uri: {e}")))?;
    if !uri.is_playable() {
        return Err((
            SPOTIFY_ERR_BAD_URI,
            format!("uri is not playable audio: {uri_text}"),
        ));
    }
    Ok(uri)
}

fn read_uri_arg(uri: *const c_char) -> Result<String, (std::os::raw::c_int, String)> {
    if uri.is_null() {
        return Err((SPOTIFY_ERR_NULL_ARG, "null uri".to_owned()));
    }
    // SAFETY: null-checked; caller passes valid UTF-8 per header.
    unsafe { CStr::from_ptr(uri) }
        .to_str()
        .map(str::to_owned)
        .map_err(|_| (SPOTIFY_ERR_BAD_URI, "uri is not valid UTF-8".to_owned()))
}

// Largest cover first: best source for downscaling.
fn widest_cover(covers: &librespot::metadata::image::Images) -> Option<librespot::core::FileId> {
    covers
        .iter()
        .max_by_key(|img| img.width)
        .map(|img| img.id)
}

// Fetch cover bytes, downscale to both sizes, store BMPs on disk + the 128px
// one in memory. Runs on the handle runtime, never on the caller thread.
async fn fetch_and_store(
    session: &Session,
    art_dir: PathBuf,
    track_id: String,
    uri_text: String,
) -> Result<(), String> {
    let uri = SpotifyUri::from_uri(&uri_text).map_err(|e| format!("bad uri: {e}"))?;
    let covers = match &uri {
        SpotifyUri::Track { .. } => {
            let track = Track::get(session, &uri)
                .await
                .map_err(|e| format!("track metadata: {e}"))?;
            track.album.covers.clone()
        }
        SpotifyUri::Episode { .. } => {
            let episode = Episode::get(session, &uri)
                .await
                .map_err(|e| format!("episode metadata: {e}"))?;
            episode.covers.clone()
        }
        _ => return Err(format!("not playable audio: {uri_text}")),
    };
    let file_id =
        widest_cover(&covers).ok_or_else(|| "no cover art in metadata".to_owned())?;
    let bytes = session
        .spclient()
        .get_image(&file_id)
        .await
        .map_err(|e| format!("image download: {e}"))?;
    let decoded =
        image::load_from_memory(&bytes).map_err(|e| format!("image decode: {e}"))?;
    std::fs::create_dir_all(&art_dir).map_err(|e| format!("art cache dir: {e}"))?;
    for size in ART_SIZES {
        let small = decoded.resize_exact(
            size,
            size,
            image::imageops::FilterType::Triangle,
        );
        let rgb = small.to_rgb8();
        let (w, h) = (rgb.width(), rgb.height());
        let mut buf = Vec::new();
        {
            let mut cursor = std::io::Cursor::new(&mut buf);
            let mut enc = image::codecs::bmp::BmpEncoder::new(&mut cursor);
            enc.encode(rgb.as_raw(), w, h, image::ExtendedColorType::Rgb8)
                .map_err(|e| format!("bmp encode: {e}"))?;
        }
        std::fs::write(art_path_for(&art_dir, &track_id, size), &buf)
            .map_err(|e| format!("art cache write: {e}"))?;
    }
    Ok(())
}

// Promote the on-disk 128px BMP into the memory cache (FIFO cap). Called
// from synchronous queries, so no cross-thread cache writes are needed.
fn mem_insert(handle: &SpotifyPlayer, track_id: &str) {
    let path = art_path_for(&handle.art_dir, track_id, 128);
    let bytes = match std::fs::read(&path) {
        Ok(b) => b,
        Err(_) => return,
    };
    let mut mem = match handle.art_mem.lock() {
        Ok(guard) => guard,
        Err(_) => return,
    };
    if mem.map.contains_key(track_id) {
        return;
    }
    while mem.map.len() >= ART_MEM_CAP {
        match mem.order.pop_front() {
            Some(old) => {
                mem.map.remove(&old);
            }
            None => break,
        }
    }
    mem.order.push_back(track_id.to_owned());
    mem.map.insert(track_id.to_owned(), bytes);
}

#[no_mangle]
pub unsafe extern "C" fn spotify_request_artwork(
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
    let uri_text = match read_uri_arg(uri) {
        Ok(s) => s,
        Err((code, msg)) => return fail(code, msg),
    };
    let parsed = match parse_playable(&uri_text) {
        Ok(u) => u,
        Err((code, msg)) => return fail(code, msg),
    };
    let track_id = parsed.to_id();
    if art_path_for(&handle.art_dir, &track_id, 128).exists()
        && art_path_for(&handle.art_dir, &track_id, 256).exists()
    {
        mem_insert(handle, &track_id);
        let _ = handle.art_tx.send(uri_text);
        return SPOTIFY_OK;
    }
    let session = handle.session.clone();
    let art_dir = handle.art_dir.clone();
    let tx = handle.art_tx.clone();
    handle.rt.spawn(async move {
        match fetch_and_store(&session, art_dir, track_id, uri_text.clone()).await {
            Ok(()) => {
                let _ = tx.send(uri_text);
            }
            Err(msg) => log::warn!("artwork fetch failed for {uri_text}: {msg}"),
        }
    });
    SPOTIFY_OK
}

#[no_mangle]
pub unsafe extern "C" fn spotify_artwork_state(
    player: *mut SpotifyPlayer,
    uri: *const c_char,
    size: std::os::raw::c_int,
    ready_out: *mut std::os::raw::c_int,
) -> std::os::raw::c_int {
    let handle = match handle_ref(player) {
        Ok(h) => h,
        Err((code, msg)) => return fail(code, msg),
    };
    if let Err((code, msg)) = check_connected(handle) {
        return fail(code, msg);
    }
    if ready_out.is_null() {
        return fail(SPOTIFY_ERR_NULL_ARG, "null ready out-pointer".to_owned());
    }
    let size_u = match check_art_size(size) {
        Ok(s) => s,
        Err((code, msg)) => return fail(code, msg),
    };
    let uri_text = match read_uri_arg(uri) {
        Ok(s) => s,
        Err((code, msg)) => return fail(code, msg),
    };
    let parsed = match parse_playable(&uri_text) {
        Ok(u) => u,
        Err((code, msg)) => return fail(code, msg),
    };
    let track_id = parsed.to_id();
    if size_u == 128 {
        mem_insert(handle, &track_id);
    }
    let mem_hit = match handle.art_mem.lock() {
        Ok(mem) => size_u == 128 && mem.map.contains_key(&track_id),
        Err(_) => return fail(SPOTIFY_ERR_INTERNAL, "artwork lock poisoned".to_owned()),
    };
    let ready = mem_hit || art_path_for(&handle.art_dir, &track_id, size_u).exists();
    // SAFETY: null-checked above; caller provides storage.
    unsafe {
        *ready_out = i32::from(ready);
    }
    SPOTIFY_OK
}

#[no_mangle]
pub unsafe extern "C" fn spotify_artwork_path(
    player: *mut SpotifyPlayer,
    uri: *const c_char,
    size: std::os::raw::c_int,
    out: *mut c_char,
    cap: std::os::raw::c_int,
) -> std::os::raw::c_int {
    let handle = match handle_ref(player) {
        Ok(h) => h,
        Err((code, msg)) => return fail(code, msg),
    };
    if let Err((code, msg)) = check_connected(handle) {
        return fail(code, msg);
    }
    if out.is_null() || cap <= 0 {
        return fail(SPOTIFY_ERR_NULL_ARG, "null/short path buffer".to_owned());
    }
    let size_u = match check_art_size(size) {
        Ok(s) => s,
        Err((code, msg)) => return fail(code, msg),
    };
    let uri_text = match read_uri_arg(uri) {
        Ok(s) => s,
        Err((code, msg)) => return fail(code, msg),
    };
    let parsed = match parse_playable(&uri_text) {
        Ok(u) => u,
        Err((code, msg)) => return fail(code, msg),
    };
    let text = art_path_for(&handle.art_dir, &parsed.to_id(), size_u).to_string_lossy().into_owned();
    if text.len() + 1 > cap as usize {
        return fail(SPOTIFY_ERR_INTERNAL, "path buffer too small".to_owned());
    }
    // SAFETY: bounds-checked above; caller provides `cap` bytes.
    unsafe {
        std::ptr::copy_nonoverlapping(text.as_ptr() as *const c_char, out, text.len());
        *out.add(text.len()) = 0;
    }
    SPOTIFY_OK
}

const SPOTIFY_SEARCH_TRACK: i32 = 1;
const SPOTIFY_SEARCH_ARTIST: i32 = 2;
const SPOTIFY_SEARCH_ALBUM: i32 = 4;
const SPOTIFY_SEARCH_PLAYLIST: i32 = 8;
const SPOTIFY_SEARCH_NAME_MAX: usize = 256;
const SPOTIFY_SEARCH_SUBTITLE_MAX: usize = 256;

const WEBAPI_REDIRECT: &str = "http://127.0.0.1:8898/login";
// Search needs no scope; the library ones pre-cover Phase 10 so one login
// suffices. NOTE: adding a scope later requires a fresh browser login —
// refresh keeps the originally granted set (seen live with
// user-follow-read in 10.4).
const WEBAPI_SCOPES: [&str; 3] =
    ["user-library-read", "playlist-read-private", "user-follow-read"];
const WEBAPI_CACHE_FILE: &str = "webapi.json";

/// C layout twin of `SpotifySearchItem` (field order and types must match).
#[repr(C)]
pub struct SpotifySearchItem {
    pub kind: i32,
    pub uri: [c_char; SPOTIFY_META_URI_MAX],
    pub name: [c_char; SPOTIFY_SEARCH_NAME_MAX],
    pub subtitle: [c_char; SPOTIFY_SEARCH_SUBTITLE_MAX],
    pub duration_ms: u32,
}

fn web_cache_path() -> PathBuf {
    cache_dir().join(WEBAPI_CACHE_FILE)
}

fn read_web_cache() -> Option<(String, String)> {
    let text = std::fs::read_to_string(web_cache_path()).ok()?;
    let value: Value = serde_json::from_str(&text).ok()?;
    Some((
        value.get("client_id")?.as_str()?.to_owned(),
        value.get("refresh_token")?.as_str()?.to_owned(),
    ))
}

fn write_web_cache(client_id: &str, refresh_token: &str) {
    let text = serde_json::json!({"client_id": client_id, "refresh_token": refresh_token}).to_string();
    let _ = std::fs::write(web_cache_path(), text);
}

fn browser_login(client_id: &str) -> Result<OAuthToken, String> {
    OAuthClientBuilder::new(client_id, WEBAPI_REDIRECT, WEBAPI_SCOPES.to_vec())
        .open_in_browser()
        .build()
        .map_err(|e| format!("oauth setup: {e}"))?
        .get_access_token()
        .map_err(|e| format!("browser login: {e}"))
}

// Valid cached access token, refreshing or logging in as needed. Refresh
// failures fall back to a browser login; a missing client id is an error
// telling the user exactly what to set.
fn ensure_access_token(handle: &SpotifyPlayer) -> Result<String, String> {
    if let Ok(guard) = handle.web_token.lock() {
        if let Some(token) = guard.as_ref() {
            if std::time::Instant::now() + std::time::Duration::from_secs(60) < token.expires_at {
                return Ok(token.access_token.clone());
            }
        }
    }
    let cached = read_web_cache();
    let client_id = std::env::var("SPOTILITE_CLIENT_ID")
        .ok()
        .filter(|s| !s.trim().is_empty())
        .or_else(|| cached.as_ref().map(|(id, _)| id.clone()))
        .ok_or_else(|| {
            "no Spotify app configured: set SPOTILITE_CLIENT_ID to your client id, then search again to log in".to_owned()
        })?;
    let client = OAuthClientBuilder::new(&client_id, WEBAPI_REDIRECT, WEBAPI_SCOPES.to_vec())
        .build()
        .map_err(|e| format!("oauth setup: {e}"))?;
    let token = match cached.map(|(_, refresh)| refresh) {
        Some(refresh) => match client.refresh_token(&refresh) {
            Ok(token) => token,
            Err(_) => browser_login(&client_id)?,
        },
        None => browser_login(&client_id)?,
    };
    // Spotify only sometimes rotates the refresh token: an empty one means
    // "keep using the previous", so never overwrite the cache with empty.
    if !token.refresh_token.is_empty() {
        write_web_cache(&client_id, &token.refresh_token);
    }
    if let Ok(mut guard) = handle.web_token.lock() {
        *guard = Some(token.clone());
    }
    Ok(token.access_token)
}

enum SearchError {
    Unauthorized,
    Auth(String),
    Other(String),
}

// Authenticated GET against api.spotify.com with a single 401-refresh
// retry. Shared by search + library fetches (Phase 8/10).
async fn web_get(
    handle: &SpotifyPlayer,
    endpoint: &str,
    params: &[(&str, String)],
) -> Result<Value, SearchError> {
    let url = format!("https://api.spotify.com{endpoint}");
    for attempt in 0..2 {
        let token = ensure_access_token(handle).map_err(SearchError::Auth)?;
        let response = reqwest::Client::new()
            .get(&url)
            .query(params)
            .bearer_auth(&token)
            .send()
            .await
            .map_err(|e| SearchError::Other(format!("request: {e}")))?;
        let status = response.status();
        let text = response
            .text()
            .await
            .map_err(|e| SearchError::Other(format!("body: {e}")))?;
        if status.as_u16() == 401 && attempt == 0 {
            if let Ok(mut guard) = handle.web_token.lock() {
                *guard = None;
            }
            continue;
        }
        if status.as_u16() == 401 {
            return Err(SearchError::Unauthorized);
        }
        // Scope gap (e.g. a scope added after the user's login): re-login
        // with the cached client id — no env needed — then retry once.
        if status.as_u16() == 403
            && attempt == 0
            && text.contains("Insufficient client scope")
        {
            if let Some((client_id, _)) = read_web_cache() {
                let _ = std::fs::remove_file(web_cache_path());
                if let Ok(mut guard) = handle.web_token.lock() {
                    *guard = None;
                }
                match browser_login(&client_id) {
                    Ok(token) => {
                        if !token.refresh_token.is_empty() {
                            write_web_cache(&client_id, &token.refresh_token);
                        }
                        if let Ok(mut guard) = handle.web_token.lock() {
                            *guard = Some(token);
                        }
                        continue;
                    }
                    Err(msg) => return Err(SearchError::Auth(msg)),
                }
            }
        }
        if !status.is_success() {
            return Err(SearchError::Other(format!("http {status}: {text}")));
        }
        return serde_json::from_str(&text)
            .map_err(|e| SearchError::Other(format!("parse: {e}")));
    }
    Err(SearchError::Unauthorized)
}

struct RawItem {
    kind: i32,
    uri: String,
    name: String,
    subtitle: String,
    duration_ms: u32,
}

fn str_field(value: &Value, key: &str) -> String {
    value
        .get(key)
        .and_then(|v| v.as_str())
        .unwrap_or("")
        .to_owned()
}

fn artists_of(value: &Value) -> String {
    value
        .get("artists")
        .and_then(|a| a.as_array())
        .map(|arr| {
            arr.iter()
                .map(|a| str_field(a, "name"))
                .filter(|s| !s.is_empty())
                .collect::<Vec<_>>()
                .join(", ")
        })
        .unwrap_or_default()
}

fn map_track(out: &mut Vec<RawItem>, item: &Value) {
    out.push(RawItem {
        kind: SPOTIFY_SEARCH_TRACK,
        uri: str_field(item, "uri"),
        name: str_field(item, "name"),
        subtitle: artists_of(item),
        duration_ms: item
            .get("duration_ms")
            .and_then(|d| d.as_u64())
            .unwrap_or(0) as u32,
    });
}

fn push_tracks(out: &mut Vec<RawItem>, value: &Value) {
    if let Some(items) = value.get("tracks").and_then(|t| t.get("items")).and_then(|i| i.as_array()) {
        for item in items {
            map_track(out, item);
        }
    }
}

fn map_artist(out: &mut Vec<RawItem>, item: &Value) {
    let genres = item
        .get("genres")
        .and_then(|g| g.as_array())
        .map(|arr| {
            arr.iter()
                .filter_map(|g| g.as_str())
                .collect::<Vec<_>>()
                .join(", ")
        })
        .unwrap_or_default();
    out.push(RawItem {
        kind: SPOTIFY_SEARCH_ARTIST,
        uri: str_field(item, "uri"),
        name: str_field(item, "name"),
        subtitle: genres,
        duration_ms: 0,
    });
}

fn push_artists(out: &mut Vec<RawItem>, value: &Value) {
    if let Some(items) = value.get("artists").and_then(|t| t.get("items")).and_then(|i| i.as_array()) {
        for item in items {
            map_artist(out, item);
        }
    }
}

fn push_albums(out: &mut Vec<RawItem>, value: &Value) {
    if let Some(items) = value.get("albums").and_then(|t| t.get("items")).and_then(|i| i.as_array()) {
        for item in items {
            out.push(RawItem {
                kind: SPOTIFY_SEARCH_ALBUM,
                uri: str_field(item, "uri"),
                name: str_field(item, "name"),
                subtitle: artists_of(item),
                duration_ms: 0,
            });
        }
    }
}

fn push_playlists(out: &mut Vec<RawItem>, value: &Value) {
    if let Some(items) = value.get("playlists").and_then(|t| t.get("items")).and_then(|i| i.as_array()) {
        for item in items {
            let owner = item
                .get("owner")
                .map(|o| str_field(o, "display_name"))
                .unwrap_or_default();
            out.push(RawItem {
                kind: SPOTIFY_SEARCH_PLAYLIST,
                uri: str_field(item, "uri"),
                name: str_field(item, "name"),
                subtitle: owner,
                duration_ms: 0,
            });
        }
    }
}

#[no_mangle]
pub unsafe extern "C" fn spotify_search(
    player: *mut SpotifyPlayer,
    query: *const c_char,
    types: std::os::raw::c_int,
    limit: std::os::raw::c_int,
    offset: std::os::raw::c_int,
    items: *mut SpotifySearchItem,
    cap: std::os::raw::c_int,
) -> std::os::raw::c_int {
    let handle = match handle_ref(player) {
        Ok(h) => h,
        Err((code, msg)) => return fail(code, msg),
    };
    if let Err((code, msg)) = check_connected(handle) {
        return fail(code, msg);
    }
    if query.is_null() {
        return fail(SPOTIFY_ERR_NULL_ARG, "null query".to_owned());
    }
    // SAFETY: null-checked; caller passes valid UTF-8 per header.
    let query_text = match unsafe { CStr::from_ptr(query) }.to_str() {
        Ok(s) if !s.trim().is_empty() => s.trim().to_owned(),
        _ => return fail(SPOTIFY_ERR_BAD_URI, "empty query".to_owned()),
    };
    if items.is_null() || cap <= 0 {
        return fail(SPOTIFY_ERR_NULL_ARG, "null/short items buffer".to_owned());
    }
    let mask = if types == 0 { 15 } else { types & 15 };
    if mask == 0 {
        return fail(SPOTIFY_ERR_BAD_URI, "no result types selected".to_owned());
    }
    let mut type_names = Vec::new();
    if mask & SPOTIFY_SEARCH_TRACK != 0 {
        type_names.push("track");
    }
    if mask & SPOTIFY_SEARCH_ARTIST != 0 {
        type_names.push("artist");
    }
    if mask & SPOTIFY_SEARCH_ALBUM != 0 {
        type_names.push("album");
    }
    if mask & SPOTIFY_SEARCH_PLAYLIST != 0 {
        type_names.push("playlist");
    }
    // Spotify rejects limit > 10 ("Invalid limit") despite older docs
    // saying 50; verified live 2026-09-11 (10 ok, 11+ fail).
    let limit_c = limit.clamp(1, 10) as i64;
    let offset_c = offset.max(0) as i64;

    let value = handle.rt.block_on(web_get(
        handle,
        "/v1/search",
        &[
            ("q", query_text.clone()),
            ("type", type_names.join(",")),
            ("limit", limit_c.to_string()),
            ("offset", offset_c.to_string()),
        ],
    ));
    let value = match value {
        Ok(v) => v,
        Err(SearchError::Unauthorized) => {
            return fail(SPOTIFY_ERR_AUTH, "search unauthorized even after refresh".to_owned())
        }
        Err(SearchError::Auth(msg)) => return fail(SPOTIFY_ERR_AUTH, msg),
        Err(SearchError::Other(msg)) => return fail(SPOTIFY_ERR_INTERNAL, msg),
    };

    let mut raw: Vec<RawItem> = Vec::new();
    if mask & SPOTIFY_SEARCH_TRACK != 0 {
        push_tracks(&mut raw, &value);
    }
    if mask & SPOTIFY_SEARCH_ARTIST != 0 {
        push_artists(&mut raw, &value);
    }
    if mask & SPOTIFY_SEARCH_ALBUM != 0 {
        push_albums(&mut raw, &value);
    }
    if mask & SPOTIFY_SEARCH_PLAYLIST != 0 {
        push_playlists(&mut raw, &value);
    }
    let count = fill_items(items, cap as usize, &mut raw);
    count as std::os::raw::c_int
}

// Drop null rows, copy at most `cap` into the caller's buffer.
fn fill_items(items: *mut SpotifySearchItem, cap: usize, raw: &mut Vec<RawItem>) -> usize {
    raw.retain(|item| !item.uri.is_empty());
    let count = raw.len().min(cap);
    // SAFETY: caller provides `cap` slots (checked by callers).
    let dest = unsafe { std::slice::from_raw_parts_mut(items, cap) };
    for (i, item) in raw.iter().take(count).enumerate() {
        dest[i].kind = item.kind;
        fill_str(&mut dest[i].uri, &item.uri);
        fill_str(&mut dest[i].name, &item.name);
        fill_str(&mut dest[i].subtitle, &item.subtitle);
        dest[i].duration_ms = item.duration_ms;
    }
    count
}
#[no_mangle]
pub unsafe extern "C" fn spotify_liked_tracks(
    player: *mut SpotifyPlayer,
    limit: std::os::raw::c_int,
    offset: std::os::raw::c_int,
    items: *mut SpotifySearchItem,
    cap: std::os::raw::c_int,
    total_out: *mut std::os::raw::c_int,
) -> std::os::raw::c_int {
    let handle = match handle_ref(player) {
        Ok(h) => h,
        Err((code, msg)) => return fail(code, msg),
    };
    if let Err((code, msg)) = check_connected(handle) {
        return fail(code, msg);
    }
    if items.is_null() || cap <= 0 || total_out.is_null() {
        return fail(SPOTIFY_ERR_NULL_ARG, "null items/total buffer".to_owned());
    }
    // Library endpoints cap at 50 (unlike search's 10).
    let limit_c = limit.clamp(1, 50) as i64;
    let offset_c = offset.max(0) as i64;
    let value = handle.rt.block_on(web_get(
        handle,
        "/v1/me/tracks",
        &[
            ("limit", limit_c.to_string()),
            ("offset", offset_c.to_string()),
        ],
    ));
    let value = match value {
        Ok(v) => v,
        Err(SearchError::Unauthorized) => {
            return fail(SPOTIFY_ERR_AUTH, "library unauthorized even after refresh".to_owned())
        }
        Err(SearchError::Auth(msg)) => return fail(SPOTIFY_ERR_AUTH, msg),
        Err(SearchError::Other(msg)) => return fail(SPOTIFY_ERR_INTERNAL, msg),
    };
    let total = value
        .get("total")
        .and_then(|t| t.as_u64())
        .unwrap_or(0);
    let mut raw: Vec<RawItem> = Vec::new();
    if let Some(entries) = value.get("items").and_then(|i| i.as_array()) {
        for entry in entries {
            if let Some(track) = entry.get("track") {
                map_track(&mut raw, track);
            }
        }
    }
    let count = fill_items(items, cap as usize, &mut raw);
    // SAFETY: null-checked above; caller provides storage.
    unsafe {
        *total_out = total.min(i32::MAX as u64) as std::os::raw::c_int;
    }
    count as std::os::raw::c_int
}

#[no_mangle]
pub unsafe extern "C" fn spotify_playlists(
    player: *mut SpotifyPlayer,
    limit: std::os::raw::c_int,
    offset: std::os::raw::c_int,
    items: *mut SpotifySearchItem,
    cap: std::os::raw::c_int,
    total_out: *mut std::os::raw::c_int,
) -> std::os::raw::c_int {
    let handle = match check_library_args(player, items, cap, total_out) {
        Ok(h) => h,
        Err((code, msg)) => return fail(code, msg),
    };
    let limit_c = limit.clamp(1, 50) as i64;
    let offset_c = offset.max(0) as i64;
    let value = handle.rt.block_on(web_get(
        handle,
        "/v1/me/playlists",
        &[
            ("limit", limit_c.to_string()),
            ("offset", offset_c.to_string()),
        ],
    ));
    let value = match value {
        Ok(v) => v,
        Err(SearchError::Unauthorized) => {
            return fail(SPOTIFY_ERR_AUTH, "library unauthorized even after refresh".to_owned())
        }
        Err(SearchError::Auth(msg)) => return fail(SPOTIFY_ERR_AUTH, msg),
        Err(SearchError::Other(msg)) => return fail(SPOTIFY_ERR_INTERNAL, msg),
    };
    let total = value
        .get("total")
        .and_then(|t| t.as_u64())
        .unwrap_or(0);
    let mut raw: Vec<RawItem> = Vec::new();
    if let Some(entries) = value.get("items").and_then(|i| i.as_array()) {
        for entry in entries {
            let owner = entry
                .get("owner")
                .map(|o| str_field(o, "display_name"))
                .unwrap_or_default();
            // NOTE: per-playlist tracks.total reads 0 for new apps, so no
            // count is shown (observed live; cause unclear, likely related
            // to the items-endpoint restriction below).
            raw.push(RawItem {
                kind: SPOTIFY_SEARCH_PLAYLIST,
                uri: str_field(entry, "uri"),
                name: str_field(entry, "name"),
                subtitle: owner,
                duration_ms: 0,
            });
        }
    }
    let count = fill_items(items, cap as usize, &mut raw);
    write_total(total_out, total);
    count as std::os::raw::c_int
}

// First-page context tracks with concurrent metadata names. Shared by
// playlist + artist drill-ins (Web API items are restricted for new apps).
async fn context_track_items(
    handle: &SpotifyPlayer,
    context_uri: &str,
    limit_c: usize,
    offset_c: usize,
) -> Result<Vec<RawItem>, String> {
    let session = &handle.session;
    let ctx = session
        .spclient()
        .get_context(context_uri)
        .await
        .map_err(|e| format!("context resolve: {e}"))?;
    let uris: Vec<String> = ctx
        .pages
        .iter()
        .flat_map(|page| page.tracks.iter())
        .filter_map(|track| track.uri.clone())
        .filter(|uri| !uri.is_empty())
        .collect();
    let page: Vec<String> = uris.into_iter().skip(offset_c).take(limit_c).collect();
    let metas = futures_util::future::join_all(page.iter().map(|uri_text| async {
        let parsed = match SpotifyUri::from_uri(uri_text).ok() {
            // Tracks and episodes both play; anything else is skipped.
            Some(p @ SpotifyUri::Track { .. }) | Some(p @ SpotifyUri::Episode { .. }) => p,
            _ => return None,
        };
        let meta = fetch_metadata(session, &parsed).await.ok()?;
        Some((parsed, meta))
    }))
    .await;
    let mut raw: Vec<RawItem> = Vec::new();
    for entry in metas.into_iter().flatten() {
        let (parsed, meta) = entry;
        // Tracks and episodes both play through loadUri; the kind flag
        // only gates the GUI play button, so both map to TRACK here.
        let _ = parsed;
        raw.push(RawItem {
            kind: SPOTIFY_SEARCH_TRACK,
            uri: meta.uri,
            name: meta.title,
            subtitle: meta.artist,
            duration_ms: meta.duration_ms,
        });
    }
    Ok(raw)
}

#[no_mangle]
pub unsafe extern "C" fn spotify_playlist_tracks(
    player: *mut SpotifyPlayer,
    playlist: *const c_char,
    limit: std::os::raw::c_int,
    offset: std::os::raw::c_int,
    items: *mut SpotifySearchItem,
    cap: std::os::raw::c_int,
    total_out: *mut std::os::raw::c_int,
) -> std::os::raw::c_int {
    let handle = match check_library_args(player, items, cap, total_out) {
        Ok(h) => h,
        Err((code, msg)) => return fail(code, msg),
    };
    if playlist.is_null() {
        return fail(SPOTIFY_ERR_NULL_ARG, "null playlist".to_owned());
    }
    // SAFETY: null-checked; caller passes valid UTF-8 per header.
    let id_text = match unsafe { CStr::from_ptr(playlist) }.to_str() {
        Ok(s) => s.trim().to_owned(),
        Err(_) => return fail(SPOTIFY_ERR_BAD_URI, "playlist is not valid UTF-8".to_owned()),
    };
    let id = id_text
        .strip_prefix("spotify:playlist:")
        .unwrap_or(&id_text)
        .to_owned();
    if id.is_empty() {
        return fail(SPOTIFY_ERR_BAD_URI, "empty playlist id".to_owned());
    }
    // Playlist ITEMS are not fetchable via Web API for new apps (robust
    // 403s even for own playlists). Resolve through librespot's own
    // context machinery instead (same source Spirc plays from), then
    // attach names with concurrent metadata fetches.
    let limit_c = (limit.clamp(1, 50) as usize).min(cap as usize);
    let offset_c = offset.max(0) as usize;
    let mut raw = match handle.rt.block_on(context_track_items(
        handle,
        &format!("spotify:playlist:{id}"),
        limit_c,
        offset_c,
    )) {
        Ok(r) => r,
        Err(msg) => return fail(SPOTIFY_ERR_INTERNAL, msg),
    };
    let count = fill_items(items, cap as usize, &mut raw);
    // Total track count isn't exposed by first-page context resolution.
    // SAFETY: null-checked above; caller provides storage.
    unsafe {
        *total_out = -1;
    }
    count as std::os::raw::c_int
}

fn check_library_args<'a>(
    player: *mut SpotifyPlayer,
    items: *mut SpotifySearchItem,
    cap: std::os::raw::c_int,
    total_out: *mut std::os::raw::c_int,
) -> Result<&'a SpotifyPlayer, (std::os::raw::c_int, String)> {
    let handle = handle_ref(player)?;
    if let Err(e) = check_connected(handle) {
        return Err(e);
    }
    if items.is_null() || cap <= 0 || total_out.is_null() {
        return Err((SPOTIFY_ERR_NULL_ARG, "null items/total buffer".to_owned()));
    }
    Ok(handle)
}

fn write_total(total_out: *mut std::os::raw::c_int, total: u64) {
    // SAFETY: null-checked by callers; caller provides storage.
    unsafe {
        *total_out = total.min(i32::MAX as u64) as std::os::raw::c_int;
    }
}

#[no_mangle]
pub unsafe extern "C" fn spotify_albums(
    player: *mut SpotifyPlayer,
    limit: std::os::raw::c_int,
    offset: std::os::raw::c_int,
    items: *mut SpotifySearchItem,
    cap: std::os::raw::c_int,
    total_out: *mut std::os::raw::c_int,
) -> std::os::raw::c_int {
    let handle = match check_library_args(player, items, cap, total_out) {
        Ok(h) => h,
        Err((code, msg)) => return fail(code, msg),
    };
    let limit_c = limit.clamp(1, 50) as i64;
    let offset_c = offset.max(0) as i64;
    let value = handle.rt.block_on(web_get(
        handle,
        "/v1/me/albums",
        &[
            ("limit", limit_c.to_string()),
            ("offset", offset_c.to_string()),
        ],
    ));
    let value = match value {
        Ok(v) => v,
        Err(SearchError::Unauthorized) => {
            return fail(SPOTIFY_ERR_AUTH, "library unauthorized even after refresh".to_owned())
        }
        Err(SearchError::Auth(msg)) => return fail(SPOTIFY_ERR_AUTH, msg),
        Err(SearchError::Other(msg)) => return fail(SPOTIFY_ERR_INTERNAL, msg),
    };
    let total = value
        .get("total")
        .and_then(|t| t.as_u64())
        .unwrap_or(0);
    let mut raw: Vec<RawItem> = Vec::new();
    if let Some(entries) = value.get("items").and_then(|i| i.as_array()) {
        for entry in entries {
            if let Some(album) = entry.get("album") {
                raw.push(RawItem {
                    kind: SPOTIFY_SEARCH_ALBUM,
                    uri: str_field(album, "uri"),
                    name: str_field(album, "name"),
                    subtitle: artists_of(album),
                    duration_ms: 0,
                });
            }
        }
    }
    let count = fill_items(items, cap as usize, &mut raw);
    write_total(total_out, total);
    count as std::os::raw::c_int
}

fn strip_id(text: &str, prefix: &str) -> Result<String, (std::os::raw::c_int, String)> {
    let id = text.strip_prefix(prefix).unwrap_or(text).to_owned();
    if id.is_empty() {
        return Err((SPOTIFY_ERR_BAD_URI, "empty id".to_owned()));
    }
    Ok(id)
}

#[no_mangle]
pub unsafe extern "C" fn spotify_album_tracks(
    player: *mut SpotifyPlayer,
    album: *const c_char,
    limit: std::os::raw::c_int,
    offset: std::os::raw::c_int,
    items: *mut SpotifySearchItem,
    cap: std::os::raw::c_int,
    total_out: *mut std::os::raw::c_int,
) -> std::os::raw::c_int {
    let handle = match check_library_args(player, items, cap, total_out) {
        Ok(h) => h,
        Err((code, msg)) => return fail(code, msg),
    };
    if album.is_null() {
        return fail(SPOTIFY_ERR_NULL_ARG, "null album".to_owned());
    }
    // SAFETY: null-checked; caller passes valid UTF-8 per header.
    let id_text = match unsafe { CStr::from_ptr(album) }.to_str() {
        Ok(s) => s.trim().to_owned(),
        Err(_) => return fail(SPOTIFY_ERR_BAD_URI, "album is not valid UTF-8".to_owned()),
    };
    let id = match strip_id(&id_text, "spotify:album:") {
        Ok(id) => id,
        Err((code, msg)) => return fail(code, msg),
    };
    let limit_c = limit.clamp(1, 50) as i64;
    let offset_c = offset.max(0) as i64;
    let value = handle.rt.block_on(web_get(
        handle,
        &format!("/v1/albums/{id}/tracks"),
        &[
            ("limit", limit_c.to_string()),
            ("offset", offset_c.to_string()),
        ],
    ));
    let value = match value {
        Ok(v) => v,
        Err(SearchError::Unauthorized) => {
            return fail(SPOTIFY_ERR_AUTH, "library unauthorized even after refresh".to_owned())
        }
        Err(SearchError::Auth(msg)) => return fail(SPOTIFY_ERR_AUTH, msg),
        Err(SearchError::Other(msg)) => return fail(SPOTIFY_ERR_INTERNAL, msg),
    };
    let total = value
        .get("total")
        .and_then(|t| t.as_u64())
        .unwrap_or(0);
    let mut raw: Vec<RawItem> = Vec::new();
    if let Some(entries) = value.get("items").and_then(|i| i.as_array()) {
        for track in entries {
            map_track(&mut raw, track);
        }
    }
    let count = fill_items(items, cap as usize, &mut raw);
    write_total(total_out, total);
    count as std::os::raw::c_int
}

#[no_mangle]
pub unsafe extern "C" fn spotify_followed_artists(
    player: *mut SpotifyPlayer,
    limit: std::os::raw::c_int,
    offset: std::os::raw::c_int,
    items: *mut SpotifySearchItem,
    cap: std::os::raw::c_int,
    total_out: *mut std::os::raw::c_int,
) -> std::os::raw::c_int {
    let handle = match check_library_args(player, items, cap, total_out) {
        Ok(h) => h,
        Err((code, msg)) => return fail(code, msg),
    };
    // Followed-artists paging is cursor-based (`after`), so numeric offsets
    // walk fixed 50-pages forward. Offsets stay small in practice.
    let limit_c = (limit.clamp(1, 50) as usize).min(cap as usize);
    let mut skip = offset.max(0) as usize;
    let mut after = String::new();
    let mut last_after = String::from("\0");
    let (mut raw, mut total) = (Vec::new(), 0u64);
    for _ in 0..32 {
        let mut params = vec![
            ("type", "artist".to_owned()),
            ("limit", "50".to_owned()),
        ];
        if !after.is_empty() {
            params.push(("after", after.clone()));
        }
        let value = match handle.rt.block_on(web_get(handle, "/v1/me/following", &params)) {
            Ok(v) => v,
            Err(SearchError::Unauthorized) => {
                return fail(SPOTIFY_ERR_AUTH, "library unauthorized even after refresh".to_owned())
            }
            Err(SearchError::Auth(msg)) => return fail(SPOTIFY_ERR_AUTH, msg),
            Err(SearchError::Other(msg)) => return fail(SPOTIFY_ERR_INTERNAL, msg),
        };
        let artists = match value.get("artists") {
            Some(a) => a,
            None => break,
        };
        total = artists
            .get("total")
            .and_then(|t| t.as_u64())
            .unwrap_or(0);
        let entries: Vec<&Value> = artists
            .get("items")
            .and_then(|i| i.as_array())
            .map(|arr| arr.iter().collect())
            .unwrap_or_default();
        if entries.is_empty() {
            break;
        }
        if skip >= entries.len() {
            skip -= entries.len();
            let next = artists
                .get("cursors")
                .and_then(|c| c.get("after"))
                .and_then(|a| a.as_str())
                .map(str::to_owned)
                .filter(|s| !s.is_empty())
                .or_else(|| entries.last().map(|e| str_field(e, "id")));
            match next {
                Some(cursor) if cursor != last_after => {
                    last_after = after.clone();
                    after = cursor;
                    continue;
                }
                _ => break,
            }
        }
        for entry in entries.iter().skip(skip).take(limit_c) {
            map_artist(&mut raw, entry);
        }
        break;
    }
    let count = fill_items(items, cap as usize, &mut raw);
    write_total(total_out, total);
    count as std::os::raw::c_int
}

#[no_mangle]
pub unsafe extern "C" fn spotify_artist_tracks(
    player: *mut SpotifyPlayer,
    artist: *const c_char,
    limit: std::os::raw::c_int,
    offset: std::os::raw::c_int,
    items: *mut SpotifySearchItem,
    cap: std::os::raw::c_int,
    total_out: *mut std::os::raw::c_int,
) -> std::os::raw::c_int {
    let handle = match check_library_args(player, items, cap, total_out) {
        Ok(h) => h,
        Err((code, msg)) => return fail(code, msg),
    };
    if artist.is_null() {
        return fail(SPOTIFY_ERR_NULL_ARG, "null artist".to_owned());
    }
    // SAFETY: null-checked; caller passes valid UTF-8 per header.
    let id_text = match unsafe { CStr::from_ptr(artist) }.to_str() {
        Ok(s) => s.trim().to_owned(),
        Err(_) => return fail(SPOTIFY_ERR_BAD_URI, "artist is not valid UTF-8".to_owned()),
    };
    let id = match strip_id(&id_text, "spotify:artist:") {
        Ok(id) => id,
        Err((code, msg)) => return fail(code, msg),
    };
    let limit_c = (limit.clamp(1, 50) as usize).min(cap as usize);
    let offset_c = offset.max(0) as usize;
    let mut raw = match handle.rt.block_on(context_track_items(
        handle,
        &format!("spotify:artist:{id}"),
        limit_c,
        offset_c,
    )) {
        Ok(r) => r,
        Err(msg) => return fail(SPOTIFY_ERR_INTERNAL, msg),
    };
    let count = fill_items(items, cap as usize, &mut raw);
    // SAFETY: null-checked above; caller provides storage.
    unsafe {
        *total_out = -1;
    }
    count as std::os::raw::c_int
}

#[no_mangle]
pub unsafe extern "C" fn spotify_get_volume(
    player: *mut SpotifyPlayer,
    out: *mut c_float,
) -> std::os::raw::c_int {
    let handle = match handle_ref(player) {
        Ok(h) => h,
        Err((code, msg)) => return fail(code, msg),
    };
    if out.is_null() {
        return fail(SPOTIFY_ERR_NULL_ARG, "null volume out-pointer".to_owned());
    }
    let raw = handle.mixer.volume();
    // SAFETY: null-checked above; caller provides storage.
    unsafe {
        *out = raw as c_float / u16::MAX as c_float;
    }
    SPOTIFY_OK
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
