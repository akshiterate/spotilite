// Scripted core test (plans.md Phase 3 acceptance): drives playback
// through the C++ core only — no Rust symbols here. Usage:
//   core_test.exe [spotify:track:...]
// Without a URI it exercises connect, error paths and the queue; with one
// it also plays/pauses/resumes/seeks with event polling. Exit 0 on success.
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <fstream>
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
        case SPOTIFY_EVENT_ARTWORK_READY:
            return "artwork";
        default:
            return "other";
    }
}

// BMP dimensions from the 54-byte header (no image library needed).
bool bmpDims(const std::string& path, int& w, int& h) {
    std::ifstream f(path, std::ios::binary);
    if (!f) {
        return false;
    }
    f.seekg(18);
    int32_t wi = 0, hi = 0;
    f.read(reinterpret_cast<char*>(&wi), 4);
    f.read(reinterpret_cast<char*>(&hi), 4);
    if (!f) {
        return false;
    }
    w = wi;
    h = hi;
    return true;
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

    // Search: without a login (no env, no cached token) expect a clean AUTH
    // error naming SPOTILITE_CLIENT_ID; with a login expect real rows.
    {
        const char* localAppData = std::getenv("LOCALAPPDATA");
        const std::string webCache =
            (localAppData ? localAppData : "") + std::string("\\spotilite\\cache\\webapi.json");
        const bool haveLogin = std::getenv("SPOTILITE_CLIENT_ID") != nullptr ||
                               std::ifstream(webCache).good();
        std::vector<spotilite::SearchResult> results;
        const bool ok = player.search("radiohead", spotilite::SEARCH_ANY, 20, 0, results);
        if (haveLogin) {
            check(ok, "search radiohead", player.lastError());
            if (ok) {
                check(!results.empty(), "search returned rows");
                for (const auto& r : results) {
                    std::cout << "result: [" << r.kind << "] " << r.name;
                    if (!r.subtitle.empty()) {
                        std::cout << " - " << r.subtitle;
                    }
                    std::cout << " <" << r.uri << ">\n";
                }
            }
        } else {
            check(!ok, "search without login fails clean", player.lastError());
        }
    }

    // Liked Songs (Phase 10.1): paged, total reported, same login gating.
    {
        const char* localAppData = std::getenv("LOCALAPPDATA");
        const std::string webCache =
            (localAppData ? localAppData : "") + std::string("\\spotilite\\cache\\webapi.json");
        const bool haveLogin = std::getenv("SPOTILITE_CLIENT_ID") != nullptr ||
                               std::ifstream(webCache).good();
        std::vector<spotilite::SearchResult> liked;
        int total = 0;
        const bool ok = player.likedTracks(5, 0, liked, total);
        if (haveLogin) {
            check(ok, "liked tracks", player.lastError());
            if (ok) {
                std::cout << "liked total: " << total << "\n";
                check(total > 0 && static_cast<int>(liked.size()) <= total,
                      "liked total sane");
                for (const auto& r : liked) {
                    std::cout << "liked: " << r.name;
                    if (!r.subtitle.empty()) {
                        std::cout << " - " << r.subtitle;
                    }
                    std::cout << " <" << r.uri << ">\n";
                }
            }
        } else {
            check(!ok, "liked without login fails clean", player.lastError());
        }
    }

    if (argc > 1) {
        const std::string uri = argv[1];
        check(player.loadUri(uri), "load uri", player.lastError());

        spotilite::TrackMetadata meta;
        check(player.metadata(meta), "fetch metadata", player.lastError());
        std::cout << "metadata:\n"
                  << "  title: " << meta.title << "\n"
                  << "  artist: " << meta.artist << "\n"
                  << "  album: " << meta.album << "\n"
                  << "  duration_ms: " << meta.durationMs << "\n"
                  << "  uri: " << meta.uri << "\n"
                  << "  track_id: " << meta.trackId << "\n";
        check(!meta.title.empty(), "metadata title non-empty");
        check(!meta.artist.empty(), "metadata artist non-empty");
        check(!meta.album.empty(), "metadata album non-empty");
        check(meta.durationMs > 60000 && meta.durationMs < 3600000,
              "metadata duration sane (1min..1h)");
        check(meta.uri == uri, "metadata uri echoes load");

        // Artwork: request, then wait up to 20s for the background fetch
        // while playback continues. BMP dims read straight from headers.
        check(player.requestArtwork(uri), "request artwork", player.lastError());
        bool artReady = false;
        std::string artUri;
        const auto artEnd =
            std::chrono::steady_clock::now() + std::chrono::seconds(20);
        while (!artReady && std::chrono::steady_clock::now() < artEnd) {
            spotilite::PlayerEvent event;
            while (player.pollEvent(event)) {
                if (event.type == SPOTIFY_EVENT_ARTWORK_READY) {
                    artReady = true;
                    artUri = event.uri;
                }
            }
            if (!artReady) {
                std::this_thread::sleep_for(std::chrono::milliseconds(250));
            }
        }
        check(artReady, "artwork ready event", player.lastError());
        check(artUri == uri, "artwork event uri echoes load");
        check(player.artworkReady(uri, 128), "artwork 128 cached");
        check(player.artworkReady(uri, 256), "artwork 256 cached");
        int aw = 0, ah = 0;
        check(bmpDims(player.artworkPath(uri, 128), aw, ah) && aw == 128 && ah == 128,
              "artwork 128x128 bmp");
        check(bmpDims(player.artworkPath(uri, 256), aw, ah) && aw == 256 && ah == 256,
              "artwork 256x256 bmp");
        check(!player.artworkReady("garbage", 128), "artwork garbage rejected",
              player.lastError());
        check(!player.artworkReady(uri, 64), "artwork bad size rejected",
              player.lastError());

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

        // Optional second URI: playback must follow queue navigation.
        if (argc > 2) {
            const std::string uri2 = argv[2];
            player.enqueue(uri2);
            check(player.next(), "queue next loads", player.lastError());
            check(player.state().currentUri == uri2, "playback followed queue");
            watch(player, 4, "second track");
            check(player.previous(), "queue previous loads", player.lastError());
            check(player.state().currentUri == uri, "playback back on first");
        }
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

    // Phase 9: remove + reorder (pure logic).
    check(player.queue().select(0), "queue reselect for remove test");
    check(player.queue().removeAt(1), "queue remove middle");
    check(player.queue().size() == 2, "queue size after remove");
    check(player.queue().current() == "spotify:track:aaa", "queue current stable");
    check(!player.queue().removeAt(9), "queue remove out of range fails");
    player.queue().clear();
    player.enqueue("spotify:track:aaa");
    player.enqueue("spotify:track:bbb");
    player.enqueue("spotify:track:ccc");
    check(player.queue().select(0), "queue reselect head");
    check(player.queue().move(0, 2), "queue move head to tail");
    check(player.queue().at(0) == "spotify:track:bbb" &&
              player.queue().at(2) == "spotify:track:aaa",
          "queue order after move");
    check(player.queue().current() == "spotify:track:aaa", "queue current follows move");
    check(!player.queue().move(9, 0), "queue move out of range fails");
    player.queue().clear();
    player.enqueue("spotify:track:bbb");
    player.queue().insertAt(0, "spotify:track:aaa");
    check(player.queue().at(0) == "spotify:track:aaa" &&
              player.queue().at(1) == "spotify:track:bbb",
          "queue insertAt top");
    check(player.queue().current() == "spotify:track:bbb", "queue index follows insert");
    check(!player.playFirst("spotify:track:ccc"), "playFirst bad uri fails load");
    check(player.queue().size() == 3, "playFirst kept rest");
    check(player.queue().at(0) == "spotify:track:ccc", "playFirst inserted at top");
    check(player.queue().index() == 0, "playFirst selected top");

    // Player-level next/previous wire queue navigation into bridge loads.
    // Dummy URIs fail at the bridge, which proves the call chain reaches it.
    // Rebuild the canonical order first (move tests above reordered it).
    player.queue().clear();
    player.enqueue("spotify:track:aaa");
    player.enqueue("spotify:track:bbb");
    player.enqueue("spotify:track:ccc");
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
