// TUI implementation + entry point. Build (no CMake change — direct link):
//   g++ -std=c++17 cpp/app/tui.cpp cpp/core/player.cpp -Iinclude -Icpp
//       target/release/liblibrespot_bridge.a -o build/tui.exe
//       -lws2_32 -luserenv -lbcrypt -lole32 -loleaut32 -lpropsys -lntdll -liphlpapi
#include "app/tui.h"

#include <chrono>
#include <iostream>
#include <sstream>
#include <thread>

namespace spotilite {
namespace {

std::string formatTime(uint32_t ms) {
    const uint32_t s = ms / 1000;
    char buf[16];
    std::snprintf(buf, sizeof(buf), "%02u:%02u", s / 60, s % 60);
    return buf;
}

}  // namespace

void Tui::render() {
    player_.drainEvents();
    const PlaybackState& s = player_.state();
    std::cout << "spotilite\n\n"
              << "Track: " << (s.currentUri.empty() ? "-" : s.currentUri) << "\n"
              << "Artist: - (Phase 5)\n"
              << "Album: - (Phase 5)\n\n"
              << formatTime(s.positionMs) << " elapsed\n\n"
              << "[" << (s.playing ? "playing" : (s.currentUri.empty() ? "stopped" : "paused")) << "]"
              << "  vol " << static_cast<int>(s.volume * 100.0f + 0.5f) << "%"
              << "  queue " << player_.queue().size() << " (#" << player_.queue().index() << ")\n";
}

bool Tui::exec(const std::string& line) {
    std::istringstream in(line);
    std::string cmd;
    in >> cmd;
    if (cmd.empty()) {
        return true;
    }
    if (cmd == "q") {
        return false;
    } else if (cmd == "h") {
        std::cout << "play <uri> | p play-pause | n next | b previous | "
                     "add <uri> | v <0-100> | s status | h help | q quit\n";
    } else if (cmd == "s") {
        // Status: drain + render below.
    } else if (cmd == "play") {
        std::string uri;
        in >> uri;
        if (uri.empty()) {
            std::cout << "usage: play <uri>\n";
        } else if (!player_.loadUri(uri)) {
            std::cout << "load failed: " << player_.lastError() << "\n";
        }
    } else if (cmd == "p") {
        if (player_.state().currentUri.empty()) {
            std::cout << "nothing loaded; play <uri> first\n";
        } else if (player_.state().playing) {
            if (!player_.pause()) {
                std::cout << "pause failed: " << player_.lastError() << "\n";
            }
        } else if (!player_.resume()) {
            std::cout << "resume failed: " << player_.lastError() << "\n";
        }
    } else if (cmd == "n") {
        if (!player_.next()) {
            std::cout << "next failed: " << player_.lastError() << "\n";
        }
    } else if (cmd == "b") {
        if (!player_.previous()) {
            std::cout << "previous failed: " << player_.lastError() << "\n";
        }
    } else if (cmd == "add") {
        std::string uri;
        in >> uri;
        if (uri.empty()) {
            std::cout << "usage: add <uri>\n";
        } else {
            player_.enqueue(uri);
        }
    } else if (cmd == "v") {
        double pct = -1.0;
        if (!(in >> pct)) {
            std::cout << "usage: v <0-100>\n";
        } else if (!player_.setVolume(static_cast<float>(pct / 100.0))) {
            std::cout << "volume failed: " << player_.lastError() << "\n";
        }
    } else {
        std::cout << "unknown command (h for help)\n";
    }
    // Playback commands are async: let the resulting events land before the
    // render drains them, so the display reflects the command just issued.
    if (cmd == "play" || cmd == "p" || cmd == "n" || cmd == "b" || cmd == "v") {
        std::this_thread::sleep_for(std::chrono::milliseconds(300));
    }
    render();
    return true;
}

int Tui::run() {
    if (!player_.connect()) {
        std::cerr << "connect failed: " << player_.lastError() << "\n";
        return 1;
    }
    render();
    std::string line;
    while (true) {
        std::cout << "> " << std::flush;
        if (!std::getline(std::cin, line) || !exec(line)) {
            break;
        }
    }
    std::cout << "Bye.\n";
    return 0;
}

}  // namespace spotilite

int main() {
    try {
        spotilite::Tui tui;
        return tui.run();
    } catch (const std::exception& e) {
        std::cerr << "tui failed: " << e.what() << "\n";
        return 1;
    }
}
