// Player implementation: thin stateful wrapper over the C ABI.
#include "player.h"

#include <stdexcept>

namespace spotilite {

Player::Player() {
    handle_ = spotify_create();
    if (handle_ == nullptr) {
        throw std::runtime_error(spotify_last_error(nullptr));
    }
}

Player::~Player() { spotify_destroy(handle_); }

bool Player::callOk(int rc) {
    if (rc == SPOTIFY_OK) {
        return true;
    }
    lastError_ = spotify_last_error(handle_);
    return false;
}

bool Player::connect() {
    if (!callOk(spotify_connect(handle_))) {
        return false;
    }
    state_.connected = true;
    return true;
}

bool Player::loadUri(const std::string& uri) {
    if (!callOk(spotify_load_uri(handle_, uri.c_str()))) {
        return false;
    }
    queue_.clear();
    queue_.add(uri);
    state_.currentUri = uri;
    state_.playing = true;  // optimistic; events confirm/correct it
    state_.positionMs = 0;
    return true;
}

bool Player::play() { return callOk(spotify_play(handle_)); }

// Optimistic: a successful pause means "not playing" even before the async
// Paused event arrives, so toggles built on state() can't invert. Stale
// Playing events are flushed first so they can't re-set the flag.
bool Player::pause() {
    if (!callOk(spotify_pause(handle_))) {
        return false;
    }
    drainEvents();
    state_.playing = false;
    return true;
}
bool Player::resume() { return callOk(spotify_resume(handle_)); }

bool Player::seek(uint32_t positionMs) {
    return callOk(spotify_seek(handle_, positionMs));
}

bool Player::setVolume(float volume) {
    if (!callOk(spotify_set_volume(handle_, volume))) {
        return false;
    }
    if (volume < 0.0f) {
        volume = 0.0f;
    } else if (volume > 1.0f) {
        volume = 1.0f;
    }
    state_.volume = volume;
    return true;
}

bool Player::metadata(TrackMetadata& out) {
    SpotifyMetadata raw{};
    if (!callOk(spotify_current_metadata(handle_, &raw))) {
        return false;
    }
    out.title = raw.title;
    out.artist = raw.artist;
    out.album = raw.album;
    out.durationMs = raw.duration_ms;
    out.uri = raw.uri;
    out.trackId = raw.track_id;
    return true;
}

bool Player::requestArtwork(const std::string& uri) {
    return callOk(spotify_request_artwork(handle_, uri.c_str()));
}

bool Player::artworkReady(const std::string& uri, int size) {
    int ready = 0;
    if (!callOk(spotify_artwork_state(handle_, uri.c_str(), size, &ready))) {
        return false;
    }
    return ready != 0;
}

std::string Player::artworkPath(const std::string& uri, int size) {
    char buf[SPOTIFY_ART_PATH_MAX];
    if (!callOk(spotify_artwork_path(handle_, uri.c_str(), size, buf, sizeof(buf)))) {
        return "";
    }
    return buf;
}

bool Player::next() {
    if (!queue_.next()) {
        lastError_ = "at end of queue";
        return false;
    }
    return loadUri(queue_.current());
}

bool Player::previous() {
    if (!queue_.previous()) {
        lastError_ = "at start of queue";
        return false;
    }
    return loadUri(queue_.current());
}

void Player::enqueue(const std::string& uri) { queue_.add(uri); }

void Player::applyEvent(const PlayerEvent& event) {
    switch (event.type) {
        case SPOTIFY_EVENT_TRACK_STARTED:
        case SPOTIFY_EVENT_PLAYING:
            state_.playing = true;
            if (!event.uri.empty()) {
                state_.currentUri = event.uri;
            }
            state_.positionMs = event.positionMs;
            break;
        case SPOTIFY_EVENT_PAUSED:
            state_.playing = false;
            state_.positionMs = event.positionMs;
            break;
        case SPOTIFY_EVENT_TRACK_ENDED:
            state_.playing = false;
            break;
        case SPOTIFY_EVENT_SEEKED:
        case SPOTIFY_EVENT_POSITION:
            state_.positionMs = event.positionMs;
            if (!event.uri.empty()) {
                state_.currentUri = event.uri;
            }
            break;
        case SPOTIFY_EVENT_VOLUME_CHANGED:
            state_.volume = static_cast<float>(event.volume) / 65535.0f;
            break;
        default:
            break;
    }
}

bool Player::pollEvent(PlayerEvent& out) {
    SpotifyEvent raw{};
    const int rc = spotify_poll_event(handle_, &raw);
    if (rc < 0) {
        lastError_ = spotify_last_error(handle_);
        return false;
    }
    if (rc == 0) {
        return false;
    }
    out.type = raw.type;
    out.positionMs = raw.position_ms;
    out.volume = static_cast<float>(raw.volume) / 65535.0f;
    out.uri = raw.uri;
    applyEvent(out);
    return true;
}

int Player::drainEvents() {
    int count = 0;
    PlayerEvent event;
    while (pollEvent(event)) {
        ++count;
    }
    return count;
}

}  // namespace spotilite
