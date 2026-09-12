// Phase 2 C++ console test for the Rust bridge (plans.md Phase 2).
//
// Interactive loop; every command prints the result code and, on failure,
// spotify_last_error(). Build (no CMake change — direct link):
//   g++ cpp/bridge_test.cpp -Iinclude target/release/liblibrespot_bridge.a
//       -o build/bridge_test.exe -lws2_32 -luserenv -lbcrypt -lole32
//       -loleaut32 -lpropsys -lntdll -liphlpapi
// (COM/prop-sys libs are needed by cpal/rodio inside the staticlib.)

#include <iostream>
#include <sstream>
#include <string>

#include "spotify_bridge.h"

namespace {

void report(const char* what, int rc, SpotifyPlayer* player) {
    if (rc == SPOTIFY_OK) {
        std::cout << what << ".\n";
    } else {
        std::cout << what << " FAILED (" << rc
                  << "): " << spotify_last_error(player) << "\n";
    }
}

void usage() {
    std::cout << "commands:\n"
                 "  connect            connect with cached credentials\n"
                 "  play <uri>         load a track/episode URI and play it\n"
                 "  pause              pause playback\n"
                 "  resume             resume playback\n"
                 "  seek <ms>          seek to milliseconds\n"
                 "  volume <0-100>     set volume percent\n"
                 "  status             show last error text\n"
                 "  help               this list\n"
                 "  quit               destroy the player and exit\n";
}

}  // namespace

int main() {
    std::cout << "spotilite bridge test\n";

    SpotifyPlayer* player = spotify_create();
    if (player == nullptr) {
        std::cout << "create FAILED: " << spotify_last_error(nullptr) << "\n";
        return 1;
    }
    std::cout << "Player created. Type 'help'.\n";

    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line)) {
            break;
        }
        std::istringstream in(line);
        std::string cmd;
        in >> cmd;
        if (cmd.empty()) {
            continue;
        }
        if (cmd == "quit" || cmd == "exit") {
            break;
        } else if (cmd == "help") {
            usage();
        } else if (cmd == "connect") {
            report("Connected", spotify_connect(player), player);
        } else if (cmd == "play" || cmd == "load") {
            std::string uri;
            in >> uri;
            if (uri.empty()) {
                std::cout << "usage: play <spotify:track:...|spotify:episode:...>\n";
                continue;
            }
            report("Playing", spotify_load_uri(player, uri.c_str()), player);
        } else if (cmd == "pause") {
            report("Paused", spotify_pause(player), player);
        } else if (cmd == "resume") {
            report("Resumed", spotify_resume(player), player);
        } else if (cmd == "seek") {
            unsigned long ms = 0;
            if (!(in >> ms)) {
                std::cout << "usage: seek <ms>\n";
                continue;
            }
            report("Seeked", spotify_seek(player, static_cast<uint32_t>(ms)), player);
        } else if (cmd == "volume") {
            double pct = -1.0;
            if (!(in >> pct)) {
                std::cout << "usage: volume <0-100>\n";
                continue;
            }
            report("Volume set",
                   spotify_set_volume(player, static_cast<float>(pct / 100.0)), player);
        } else if (cmd == "status") {
            std::cout << "last error: " << spotify_last_error(player) << "\n";
        } else {
            std::cout << "unknown command (try 'help')\n";
        }
    }

    spotify_destroy(player);
    std::cout << "Player destroyed. Bye.\n";
    return 0;
}
