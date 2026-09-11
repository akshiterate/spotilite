// Search window (separate OS window): query input, results list, play or
// queue the selected result. Closing it never affects playback.
#include "ui/app.h"

#include <string>
#include <vector>

#include "imgui.h"

namespace spotilite {

void App::drawSearchWindow() {
    if (focusSearch_) {
        ImGui::SetNextWindowFocus();
        focusSearch_ = false;
    }
    if (placeSearch_) {
        placeMe("search", ImVec2(560, 0), 0);
        placeSearch_ = false;
    }
    if (!ImGui::Begin("Search", &searchOpen_)) {
        ImGui::End();
        return;
    }
    ImGui::InputText("Search", searchBuf_, sizeof(searchBuf_));
    ImGui::SameLine();
    if (ImGui::Button("Find") && searchBuf_[0] != '\0') {
        pendingSearch_.active = true;
        const std::string query = searchBuf_;
        pendingSearch_.future = std::async(std::launch::async, [this, query] {
            std::pair<std::vector<SearchResult>, std::string> out;
            if (!player_.search(query, SEARCH_ANY, 20, 0, out.first)) {
                out.second = player_.lastError();
            }
            return out;
        });
    }
    pollSearch();
    std::vector<std::string> foundText;
    for (const auto& r : searchResults_) {
        foundText.push_back(r.subtitle.empty() ? r.name : r.name + " - " + r.subtitle);
    }
    std::vector<const char*> found;
    for (const auto& t : foundText) {
        found.push_back(t.c_str());
    }
    if (searchSel_ >= static_cast<int>(found.size())) {
        searchSel_ = static_cast<int>(found.size()) - 1;
    }
    if (searchSel_ < 0 && !found.empty()) {
        searchSel_ = 0;
    }
    ImGui::ListBox("##results", &searchSel_, found.data(), static_cast<int>(found.size()),
                   10);
    ImGui::SameLine();
    if (ImGui::Button("Play result")) {
        playSearchResult();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add to queue")) {
        if (searchSel_ < 0 ||
            static_cast<std::size_t>(searchSel_) >= searchResults_.size()) {
            error_ = "nothing selected";
        } else {
            player_.enqueue(searchResults_[static_cast<std::size_t>(searchSel_)].uri);
            error_.clear();
        }
    }
    if (!error_.empty()) {
        ImGui::Text("%s", error_.c_str());
    }
    ImGui::End();
}

}  // namespace spotilite
