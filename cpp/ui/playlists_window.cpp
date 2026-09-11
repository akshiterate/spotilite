// Playlists window (separate OS window): Liked Songs entry + own
// playlists list with paging. Opening a row spawns a content window;
// this list reuses the shared library fetch.
#include "ui/app.h"

#include <string>
#include <vector>

#include "imgui.h"

namespace spotilite {

void App::drawPlaylistsWindow() {
    if (focusPlaylists_) {
        ImGui::SetNextWindowFocus();
        focusPlaylists_ = false;
    }
    if (placePlaylists_) {
        placeMe("playlists", ImVec2(420, 0), 0);
        placePlaylists_ = false;
    }
    if (!ImGui::Begin("Playlists", &playlistsOpen_)) {
        ImGui::End();
        return;
    }
    if (ImGui::Button("♥ Liked Songs")) {
        openContent("liked", "Liked Songs", true);
    }
    pollLibrary();
    std::vector<std::string> libText;
    for (const auto& r : libResults_) {
        libText.push_back(r.subtitle.empty() ? r.name : r.name + " - " + r.subtitle);
    }
    std::vector<const char*> libRows;
    for (const auto& t : libText) {
        libRows.push_back(t.c_str());
    }
    if (libSel_ >= static_cast<int>(libRows.size())) {
        libSel_ = static_cast<int>(libRows.size()) - 1;
    }
    if (libSel_ < 0 && !libRows.empty()) {
        libSel_ = 0;
    }
    ImGui::ListBox("##playlists", &libSel_, libRows.data(), static_cast<int>(libRows.size()),
                   10);
    if (ImGui::Button("<##pl") && libPage_ > 0) {
        fetchLibrary(LibMode::PLAYLISTS, libPage_ - 1, "", "");
    }
    ImGui::SameLine();
    if (ImGui::Button(">##pl") && (libTotal_ < 0 || (libPage_ + 1) * 20 < libTotal_)) {
        fetchLibrary(LibMode::PLAYLISTS, libPage_ + 1, "", "");
    }
    ImGui::SameLine();
    if (libTotal_ >= 0) {
        ImGui::Text("page %d of %d", libPage_ + 1, libTotal_ / 20 + 1);
    }
    ImGui::SameLine();
    if (ImGui::Button("Open")) {
        if (libSel_ < 0 || static_cast<std::size_t>(libSel_) >= libResults_.size()) {
            error_ = "nothing selected";
        } else {
            const SearchResult& item =
                libResults_[static_cast<std::size_t>(libSel_)];
            openContent(item.uri, item.name, false);
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Refresh")) {
        fetchLibrary(LibMode::PLAYLISTS, libPage_, "", "");
    }
    ImGui::End();
}

}  // namespace spotilite
