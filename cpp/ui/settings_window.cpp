// Settings window (separate OS window). Edits save to spotilite.toml
// immediately and apply on restart; validation errors are shown inline.
#include "ui/app.h"

#include <cstdio>
#include <string>

#include "core/config.h"
#include "imgui.h"

namespace spotilite {

void App::openSettings() {
    settingsOpen_ = true;
    focusSettings_ = true;
    placeSettings_ = true;
    std::string value;
    if (Config::get("device_name", value)) {
        std::snprintf(cfgDevice_, sizeof(cfgDevice_), "%s", value.c_str());
    }
    if (Config::get("bitrate", value)) {
        cfgBitrateIdx_ = value == "96" ? 0 : (value == "320" ? 2 : 1);
    }
    if (Config::get("normalisation", value)) {
        cfgNorm_ = value == "true" || value == "1";
    }
    if (Config::get("volume", value)) {
        cfgVolPct_ = static_cast<int>(std::stod(value) * 100.0 + 0.5);
    }
    if (Config::get("cache_size_mb", value)) {
        cfgCacheMb_ = std::stoi(value);
    }
    cfgError_.clear();
}

void App::drawSettingsWindow() {
    if (focusSettings_) {
        ImGui::SetNextWindowFocus();
        focusSettings_ = false;
    }
    if (placeSettings_) {
        placeMe("settings", ImVec2(360, 0), 0);
        placeSettings_ = false;
    }
    if (!ImGui::Begin("Settings", &settingsOpen_)) {
        ImGui::End();
        return;
    }
    if (ImGui::InputText("Device name", cfgDevice_, sizeof(cfgDevice_),
                         ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (!Config::set("device_name", cfgDevice_)) {
            cfgError_ = Config::lastError;
        }
    }
    const char* bitrates = "96\0160\0320\0";
    if (ImGui::Combo("Bitrate", &cfgBitrateIdx_, bitrates)) {
        const char* want[3] = {"96", "160", "320"};
        if (!Config::set("bitrate", want[cfgBitrateIdx_])) {
            cfgError_ = Config::lastError;
        }
    }
    if (ImGui::Checkbox("Normalisation", &cfgNorm_)) {
        if (!Config::set("normalisation", cfgNorm_ ? "true" : "false")) {
            cfgError_ = Config::lastError;
        }
    }
    if (ImGui::SliderInt("Volume %", &cfgVolPct_, 0, 100)) {
        // Live drag position only; save on release below.
    }
    if (ImGui::IsItemDeactivatedAfterEdit()) {
        char buf[16];
        std::snprintf(buf, sizeof(buf), "%.2f", cfgVolPct_ / 100.0);
        if (!Config::set("volume", buf)) {
            cfgError_ = Config::lastError;
        }
    }
    if (ImGui::InputInt("Cache MB", &cfgCacheMb_, 64, 256,
                        ImGuiInputTextFlags_EnterReturnsTrue)) {
        if (!Config::set("cache_size_mb", std::to_string(cfgCacheMb_))) {
            cfgError_ = Config::lastError;
        }
    }
    if (!cfgError_.empty()) {
        ImGui::Text("%s", cfgError_.c_str());
    }
    ImGui::Text("Saved instantly. Restart the app to apply.");
    ImGui::End();
}

}  // namespace spotilite
