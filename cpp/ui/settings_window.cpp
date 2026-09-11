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
    if (!ImGui::Begin("Settings", &settingsOpen_)) {
        ImGui::End();
        return;
    }
    ImGui::Text("No settings yet.");
    ImGui::Text("Configuration arrives in Phase 11.");
    ImGui::End();
}

}  // namespace spotilite
