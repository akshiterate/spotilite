// Machine-local configuration access (Phase 11 follow-up). Thin wrapper
// over the config file ABI; validation lives in Rust.
#pragma once

#include <string>

#include "spotify_bridge.h"

namespace spotilite {

struct Config {
    static std::string lastError;

    static bool get(const std::string& key, std::string& out) {
        char buf[256];
        if (spotify_config_get(key.c_str(), buf, sizeof(buf)) != SPOTIFY_OK) {
            lastError = spotify_last_error(nullptr);
            return false;
        }
        out = buf;
        return true;
    }

    static bool set(const std::string& key, const std::string& value) {
        if (spotify_config_set(key.c_str(), value.c_str()) != SPOTIFY_OK) {
            lastError = spotify_last_error(nullptr);
            return false;
        }
        return true;
    }
};

inline std::string Config::lastError;

}  // namespace spotilite
