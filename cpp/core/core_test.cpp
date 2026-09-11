// Scripted core test (plans.md Phase 3 acceptance): drives playback
// through the C++ core only — no Rust symbols here. Usage:
//   core_test.exe [spotify:track:...]
// Without a URI it exercises connect, error paths and the queue; with one
// it also plays/pauses/resumes/seeks with event polling. Exit 0 on success.
#include <chrono>
#include <iostream>
#include <string>
#include <thread>

#include "core/player.h"

namespace {

int failures = 0;

void check(bool ok, const std::string& what, const std::string& detail = "") {
    if (ok) {
        std::cout << "ok: " << what << "\n";
    } else {
        ++failures;
        std::cout << "FAIL: " << what;
        if (!detail.empty()) {
            std::cout << " (" << detail << ")";
        }
        std::cout << "\n";
    }
}

const char* eventName(int type) {
    switch (type) {
        case SPOTIFY_EVENT_TRACK_STARTED:
            return "track_started";
        case SPOTIFY_EVENT_PLAYING:
            return "playing";
        case SPOTIFY_EVENT_PAUSED:
            return "paused";
        case SPOTIFY_EVENT_TRACK_ENDED:
            return "track_ended";
        case SPOTIFY_EVENT_SEEKED:
            return "seeked";
        case SPOTIFY_EVENT_VOLUME_CHANGED:
            return "volume";
        case SPOTIFY_EVENT_POSITION:
            return "position";
        default:
            return "other";
    }
}

// Poll for up to `seconds`, printing events as they arrive.
void watch(spotilite::Player& player, int seconds, const char* label) {
    std::cout << "-- watching events (" << label << ") --\n";
    const auto end = std::chrono::steady_clock::now() + std::chrono::seconds(seconds);
    while (std::chrono::steady_clock::now() < end) {
        spotilite::PlayerEvent event;
        while (player.pollEvent(event)) {
            std::cout << "event: " << eventName(event.type) << " pos=" << event.positionMs;
            if (!event.uri.empty()) {
                std::cout << " uri=" << event.uri;
            }
            std::cout << "\n";
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

}  // namespace

int main(int argc, char** argv) {
    std::cout << "spotilite core test\n";

    spotilite::Player player;
    std::cout << "ok: core player created\n";

    check(player.connect(), "connect", player.lastError());
    check(player.state().connected, "state.connected");

    // Error path: garbage URI must fail without touching state.
    check(!player.loadUri("garbage"), "garbage uri rejected", player.lastError());

    if (argc > 1) {
        const std::string uri = argv[1];
        check(player.loadUri(uri), "load uri", player.lastError());
        watch(player, 8, "playing");
        check(player.state().playing, "state.playing after load");

        check(player.pause(), "pause", player.lastError());
        watch(player, 2, "paused");

        check(player.resume(), "resume", player.lastError());
        watch(player, 4, "resumed");

        check(player.seek(30000), "seek 30s", player.lastError());
        watch(player, 3, "after seek");

        check(player.setVolume(0.4f), "volume 0.4", player.lastError());
        watch(player, 2, "after volume");
    }

    // Queue logic (no playback needed). Start from empty: loadUri() above
    // (if any) leaves one entry behind.
    player.queue().clear();
    player.enqueue("spotify:track:aaa");
    player.enqueue("spotify:track:bbb");
    player.enqueue("spotify:track:ccc");
    check(player.queue().size() == 3, "queue size 3");
    check(player.queue().current() == "spotify:track:aaa", "queue head");
    check(player.queue().next() && player.queue().current() == "spotify:track:bbb",
          "queue next");
    check(player.queue().previous() && player.queue().current() == "spotify:track:aaa",
          "queue previous");
    check(player.queue().select(2) && player.queue().current() == "spotify:track:ccc",
          "queue select");
    check(!player.queue().next(), "queue next at end fails");

    // Player-level next/previous wire queue navigation into bridge loads.
    // Dummy URIs fail at the bridge, which proves the call chain reaches it.
    player.queue().select(1);
    check(!player.previous(), "player previous reaches bridge", player.lastError());
    check(player.queue().current() == "spotify:track:aaa", "previous advanced queue");
    check(!player.next(), "player next reaches bridge", player.lastError());

    const auto& s = player.state();
    std::cout << "state: connected=" << s.connected << " playing=" << s.playing
              << " uri=" << s.currentUri << " pos=" << s.positionMs
              << " vol=" << s.volume << "\n";

    if (failures == 0) {
        std::cout << "CORE TEST DONE: all passed\n";
        return 0;
    }
    std::cout << "CORE TEST DONE: " << failures << " failures\n";
    return 1;
}
