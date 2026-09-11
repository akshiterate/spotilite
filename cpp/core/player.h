// C++ player core (plans.md Phase 3). RAII owner of the C ABI handle with
// a queue, mirrored playback state, and polled events. Frontends use this;
// nothing outside cpp/core/ talks to Rust directly.
#pragma once

#include <cstdint>
#include <string>

#include "queue.h"
#include "spotify_bridge.h"

namespace spotilite {

struct PlaybackState {
    bool connected = false;
    bool playing = false;
    std::string currentUri;
    float volume = 0.5f;
    uint32_t positionMs = 0;
};

struct PlayerEvent {
    int type = SPOTIFY_EVENT_NONE;
    uint32_t positionMs = 0;
    float volume = 0.0f;
    std::string uri;
};

struct TrackMetadata {
    std::string title;
    std::string artist;
    std::string album;
    uint32_t durationMs = 0;
    std::string uri;
    std::string trackId;
};

class Player {
public:
    // Throws std::runtime_error if the Rust player cannot be created.
    Player();
    ~Player();
    Player(const Player&) = delete;
    Player& operator=(const Player&) = delete;

    // All commands return false on failure; details in lastError().
    bool connect();
    bool loadUri(const std::string& uri);
    bool play();
    bool pause();
    bool resume();
    bool seek(uint32_t positionMs);
    bool setVolume(float volume);  // 0.0..1.0, clamped by the bridge
    bool metadata(TrackMetadata& out);  // fetch for last loaded URI
    bool requestArtwork(const std::string& uri);  // background fetch, idempotent
    bool artworkReady(const std::string& uri, int size);  // cached? size: 128/256
    std::string artworkPath(const std::string& uri, int size);  // "" on failure
    bool next();                   // queue forward + load; false at the end
    bool previous();               // queue back + load; false at the start
    void enqueue(const std::string& uri);

    // Drain one event into `out` and apply it to state(); true if an event
    // was delivered. drainEvents() repeats until the queue is empty.
    bool pollEvent(PlayerEvent& out);
    int drainEvents();

    Queue& queue() { return queue_; }
    const Queue& queue() const { return queue_; }
    const PlaybackState& state() const { return state_; }
    const std::string& lastError() const { return lastError_; }

private:
    bool callOk(int rc);
    void applyEvent(const PlayerEvent& event);

    SpotifyPlayer* handle_ = nullptr;
    PlaybackState state_;
    Queue queue_;
    std::string lastError_;
};

}  // namespace spotilite
