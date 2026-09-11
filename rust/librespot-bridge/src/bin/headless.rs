//! Phase 1: minimal headless Spotify Connect receiver (playback proof).
//!
//! Credentials come from the local cache or, on first run, from zeroconf
//! discovery: the device advertises as `spotilite` and the official
//! Spotify app provisions credentials when the user selects it. Playback is
//! driven entirely from the Spotify app. Ctrl+C quits.
//!
//! All state (credentials, device id, audio cache) lives under
//! `%LOCALAPPDATA%\spotilite\cache`, outside the repo, so no secrets can be
//! committed (plans.md 1.15).

use std::path::PathBuf;

use futures_util::StreamExt as _;
use librespot::{
    connect::{ConnectConfig, Spirc},
    core::{
        cache::Cache,
        config::{DeviceType, SessionConfig},
        session::Session,
        Error,
    },
    discovery::Discovery,
    playback::{
        audio_backend,
        config::{AudioFormat, PlayerConfig},
        mixer::{self, MixerConfig},
        player::Player,
    },
};

const DEVICE_NAME: &str = "spotilite";
const DEVICE_ID_FILE: &str = "device-id";

fn cache_dir() -> PathBuf {
    let base = std::env::var_os("LOCALAPPDATA")
        .map(PathBuf::from)
        .unwrap_or_else(std::env::temp_dir);
    base.join("spotilite").join("cache")
}

/// Stable device id across restarts (credential blobs are device-bound).
fn device_id(dir: &PathBuf) -> Result<String, Error> {
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

#[tokio::main]
async fn main() -> Result<(), Error> {
    env_logger::builder()
        .filter_module("librespot", log::LevelFilter::Info)
        .init();

    println!("spotilite headless receiver (Phase 1 playback proof)");

    let dir = cache_dir();
    let id = device_id(&dir)?;
    let files_dir = dir.join("files");
    let cache = Cache::new(Some(&dir), Some(&dir), Some(&files_dir), None)?;

    let mut session_config = SessionConfig::default();
    session_config.device_id = id.clone();

    let credentials = match cache.credentials() {
        Some(credentials) => {
            println!("Using cached credentials.");
            credentials
        }
        None => {
            println!("Advertising as \"{DEVICE_NAME}\" ...");
            println!("In the Spotify app: Connect to a device -> {DEVICE_NAME}.");
            let mut discovery = Discovery::builder(id, session_config.client_id.clone())
                .name(DEVICE_NAME)
                .device_type(DeviceType::Computer)
                .launch()?;
            let credentials = discovery.next().await.ok_or_else(|| {
                Error::unavailable("discovery ended before credentials arrived")
            })?;
            println!("Credentials received from the Spotify app.");
            credentials
        }
    };

    let session = Session::new(session_config, Some(cache));

    let mixer = mixer::find(None)
        .ok_or_else(|| Error::unavailable("no mixer backend"))?(MixerConfig::default())?;
    let sink_builder =
        audio_backend::find(None).ok_or_else(|| Error::unavailable("no audio backend"))?;
    let audio_format = AudioFormat::default();

    let player = Player::new(
        PlayerConfig::default(),
        session.clone(),
        mixer.get_soft_volume(),
        move || sink_builder(None, audio_format),
    );

    // ConnectConfig::default().name is "librespot" — show our own name.
    // No auto-activate: launching must not hijack playback that is already
    // playing elsewhere; the user transfers playback explicitly.
    let mut connect_config = ConnectConfig::default();
    connect_config.name = DEVICE_NAME.to_owned();

    let (_spirc, spirc_task) = Spirc::new(
        connect_config,
        session.clone(),
        credentials,
        player,
        mixer,
    )
    .await?;

    println!("Connected as {}.", session.username());
    println!("Play from the Spotify app. Press Ctrl+C to quit.");

    tokio::select! {
        () = spirc_task => println!("Connect session ended."),
        _ = tokio::signal::ctrl_c() => println!("Shutting down."),
    }

    Ok(())
}
