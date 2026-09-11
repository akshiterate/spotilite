// Playlist/Liked content windows (separate OS windows, multi-instance by
// id). Each fetches its own rows on open; closing one never affects
// playback or the others.
#include "ui/app.h"

#include <chrono>
#include <string>
#include <vector>

#include "imgui.h"

namespace spotilite {

void App::openContent(const std::string& key, const std::string& title, bool isLiked) {
    for (auto& win : contentWins_) {
        if (win.key == key) {
            win.open = true;
            win.focusMe = true;
            return;
        }
    }
    ContentWin win;
    win.key = key;
    win.title = title;
    win.isLiked = isLiked;
    win.open = true;
    win.placeMe = true;
    win.active = true;
    win.future = std::async(std::launch::async, [this, key, isLiked] {
        std::pair<std::vector<SearchResult>, std::string> out;
        int total = 0;
        bool ok = false;
        if (isLiked) {
            // Liked has real totals: page until complete (cap 500).
            ok = true;
            int offset = 0;
            while (ok && static_cast<int>(out.first.size()) < 500) {
                std::vector<SearchResult> page;
                int pageTotal = 0;
                ok = player_.likedTracks(50, offset, page, pageTotal);
                if (!ok || page.empty()) {
                    break;
                }
                total = pageTotal;
                offset += static_cast<int>(page.size());
                out.first.insert(out.first.end(), page.begin(), page.end());
                if (offset >= total) {
                    break;
                }
            }
        } else {
            // Playlists/albums/artists: page until an empty page (cap 500).
            // Playlist/artist totals are unknown (-1); albums report real
            // totals, which just ends the loop sooner.
            ok = true;
            int offset = 0;
            int knownTotal = -1;
            while (ok && static_cast<int>(out.first.size()) < 500 &&
                   (knownTotal < 0 || offset < knownTotal)) {
                std::vector<SearchResult> page;
                int pageTotal = -1;
                if (key.rfind("spotify:album:", 0) == 0) {
                    ok = player_.albumTracks(key, 50, offset, page, pageTotal);
                } else if (key.rfind("spotify:artist:", 0) == 0) {
                    ok = player_.artistTracks(key, 50, offset, page, pageTotal);
                } else {
                    ok = player_.playlistTracks(key, 50, offset, page, pageTotal);
                }
                if (!ok || page.empty()) {
                    break;
                }
                knownTotal = pageTotal;
                total = pageTotal;
                offset += static_cast<int>(page.size());
                out.first.insert(out.first.end(), page.begin(), page.end());
            }
        }
        if (!ok) {
            out.second = player_.lastError();
        }
        return out;
    });
    contentWins_.push_back(std::move(win));
}

void App::drawContentWindows() {
    int cascade = 0;
    for (auto it = contentWins_.begin(); it != contentWins_.end();) {
        if (!it->open) {
            it = contentWins_.erase(it);
            continue;
        }
        if (it->focusMe) {
            ImGui::SetNextWindowFocus();
            it->focusMe = false;
        }
        if (it->placeMe) {
            placeMe("content", ImVec2(560, 0), cascade);
            it->placeMe = false;
        }
        ++cascade;
        if (it->sizeMe) {
            ImGui::SetNextWindowSize(ImVec2(560, 0), ImGuiCond_Always);
            it->sizeMe = false;
        }
        if (it->active) {
            if (it->future.wait_for(std::chrono::seconds(0)) == std::future_status::ready) {
                it->active = false;
                auto [rows, error] = it->future.get();
                if (!error.empty() && rows.empty()) {
                    it->error = error;
                } else {
                    it->rows = std::move(rows);
                    it->sizeMe = true;
                }
            } else {
                ImGui::Begin(it->title.c_str(), &it->open);
                ImGui::Text("Loading...");
                ImGui::End();
                ++it;
                continue;
            }
        }
        if (!ImGui::Begin(it->title.c_str(), &it->open)) {
            ImGui::End();
            ++it;
            continue;
        }
        std::vector<std::string> labels;
        for (const auto& r : it->rows) {
            labels.push_back(r.subtitle.empty() ? r.name : r.name + " - " + r.subtitle);
        }
        std::vector<const char*> list;
        for (const auto& label : labels) {
            list.push_back(label.c_str());
        }
        if (it->sel >= static_cast<int>(list.size())) {
            it->sel = static_cast<int>(list.size()) - 1;
        }
        if (it->sel < 0 && !list.empty()) {
            it->sel = 0;
        }
        ImGui::ListBox("##content", &it->sel, list.data(), static_cast<int>(list.size()), 12);
        if (ImGui::Button("Play Selected")) {
            if (it->sel < 0 ||
                static_cast<std::size_t>(it->sel) >= it->rows.size()) {
                it->error = "nothing selected";
            } else if (!player_.playFirst(it->rows[static_cast<std::size_t>(it->sel)].uri)) {
                it->error = player_.lastError();
            } else {
                it->error.clear();
            }
        }
        ImGui::SameLine();
        if (ImGui::Button(it->isLiked ? "Add All to Queue" : "Add Playlist to Queue")) {
            for (const auto& r : it->rows) {
                player_.enqueue(r.uri);
            }
        }
        ImGui::SameLine();
        if (ImGui::Button("Add Selected")) {
            if (it->sel < 0 ||
                static_cast<std::size_t>(it->sel) >= it->rows.size()) {
                it->error = "nothing selected";
            } else {
                player_.enqueue(it->rows[static_cast<std::size_t>(it->sel)].uri);
            }
        }
        if (!it->error.empty()) {
            ImGui::Text("%s", it->error.c_str());
        }
        ImGui::End();
        ++it;
    }
}

}  // namespace spotilite
