// Minimal terminal frontend (plans.md Phase 4). Uses the C++ core only:
// no playback logic and no Rust symbols here. Line-based commands keep it
// portable (no console platform APIs before Phase 12).
#pragma once

#include <string>

#include "core/player.h"

namespace spotilite {

class Tui {
public:
    // Throws std::runtime_error if the core player cannot be created.
    Tui() = default;

    // Connect, then run the command loop until 'q'. Returns exit code.
    int run();

private:
    void render();
    // Returns false to quit.
    bool exec(const std::string& line);

    Player player_;
};

}  // namespace spotilite
