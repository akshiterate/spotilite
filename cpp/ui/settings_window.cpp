// Settings window (separate OS window). Placeholder until Phase 11:
// no settings correspond to real features yet.
#include "ui/app.h"

#include "imgui.h"

namespace spotilite {

void App::drawSettingsWindow() {
    if (focusSettings_) {
        ImGui::SetNextWindowFocus();
        focusSettings_ = false;
    }
    if (placeSettings_) {
        placeMe("settings", ImVec2(320, 0), 0);
        placeSettings_ = false;
    }
    if (!ImGui::Begin("Settings", &settingsOpen_)) {
        ImGui::End();
        return;
    }
    ImGui::Text("Edit %LOCALAPPDATA%\\spotilite\\spotilite.toml,");
    ImGui::Text("then restart. Device name, bitrate, normalisation,");
    ImGui::Text("startup volume and audio cache size apply on launch.");
    ImGui::End();
}

}  // namespace spotilite
