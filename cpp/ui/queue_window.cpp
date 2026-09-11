// Queue window (separate OS window): currently playing on top, numbered
// upcoming queue below. Play Selected discards everything before the
// selection and plays it; Delete/ shuffle / Move-to-N edit the order;
// finishing a track auto-advances (app layer) with numbers following.
#include "ui/app.h"

#include <algorithm>
#include <cstdlib>
#include <string>
#include <vector>

#include "imgui.h"

namespace spotilite {

void App::ensureNames() {
    if (pendingNames_.active) {
        if (pendingNames_.future.wait_for(std::chrono::seconds(0)) !=
            std::future_status::ready) {
            return;
        }
        pendingNames_.active = false;
        if (nameCache_.size() > 200) {
            nameCache_.clear();
        }
        for (auto& [uri, meta] : pendingNames_.future.get()) {
            nameCache_[uri] = meta;
        }
        return;
    }
    // Queue URIs (plus current) missing from the cache, background-filled.
    std::vector<std::string> missing;
    auto want = [&](const std::string& uri) {
        if (!uri.empty() && nameCache_.find(uri) == nameCache_.end() &&
            std::find(missing.begin(), missing.end(), uri) == missing.end() &&
            missing.size() < 50) {
            missing.push_back(uri);
        }
    };
    want(player_.state().currentUri);
    for (std::size_t i = 0; i < player_.queue().size(); ++i) {
        want(player_.queue().at(i));
    }
    if (missing.empty()) {
        return;
    }
    pendingNames_.active = true;
    pendingNames_.future = std::async(std::launch::async, [this, missing] {
        std::map<std::string, TrackMetadata> out;
        for (const auto& uri : missing) {
            TrackMetadata meta;
            if (player_.metadataFor(uri, meta)) {
                out[uri] = meta;
            }
        }
        return out;
    });
}

std::string App::trackLabel(const std::string& uri) {
    auto it = nameCache_.find(uri);
    if (it != nameCache_.end() && !it->second.title.empty()) {
        if (it->second.artist.empty()) {
            return it->second.title;
        }
        return it->second.title + " - " + it->second.artist;
    }
    return uri;
}

void App::drawQueueWindow() {
    if (focusQueue_) {
        ImGui::SetNextWindowFocus();
        focusQueue_ = false;
    }
    if (placeQueue_) {
        placeMe("queue", ImVec2(360, 0), 0);
        placeQueue_ = false;
    }
    if (!ImGui::Begin("Queue", &queueOpen_)) {
        ImGui::End();
        return;
    }
    Queue& queue = player_.queue();
    ensureNames();
    ImGui::Text("Now: %s", player_.state().currentUri.empty()
                                ? "-"
                                : trackLabel(player_.state().currentUri).c_str());

    const std::size_t base = queue.empty() ? 0 : queue.index() + 1;
    const std::size_t upcoming = queue.size() > base ? queue.size() - base : 0;
    std::vector<std::string> labels;
    for (std::size_t i = 0; i < upcoming; ++i) {
        labels.push_back(std::to_string(i + 1) + ". " + trackLabel(queue.at(base + i)));
    }
    std::vector<const char*> rows;
    for (const auto& label : labels) {
        rows.push_back(label.c_str());
    }
    if (queueUpSel_ >= static_cast<int>(rows.size())) {
        queueUpSel_ = static_cast<int>(rows.size()) - 1;
    }
    if (queueUpSel_ < 0 && !rows.empty()) {
        queueUpSel_ = 0;
    }
    ImGui::ListBox("##upcoming", &queueUpSel_, rows.data(), static_cast<int>(rows.size()),
                   8);
    const std::size_t globalSel = base + (queueUpSel_ < 0 ? 0 : static_cast<std::size_t>(queueUpSel_));

    if (ImGui::Button("Play Selected")) {
        if (globalSel >= queue.size()) {
            error_ = "nothing selected";
        } else if (!player_.playFrom(globalSel)) {
            error_ = player_.lastError();
        } else {
            error_.clear();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Delete")) {
        if (globalSel >= queue.size() || !queue.removeAt(globalSel)) {
            error_ = "nothing to delete";
        } else {
            error_.clear();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Shuffle")) {
        queue.shuffleUpcoming();
    }
    ImGui::InputText("##movepos", queueMoveBuf_, sizeof(queueMoveBuf_));
    ImGui::SameLine();
    if (ImGui::Button("Move Selected")) {        const int want = std::atoi(queueMoveBuf_);
        if (want < 1 || static_cast<std::size_t>(want) > upcoming ||
            globalSel >= queue.size()) {
            error_ = "enter a position 1..N shown above";
        } else {
            const std::size_t target = base + static_cast<std::size_t>(want - 1);
            if (queue.move(globalSel, target)) {
                queueUpSel_ = want - 1;
                error_.clear();
            } else {
                error_ = "move failed";
            }
        }
    }
    if (!error_.empty()) {
        ImGui::Text("%s", error_.c_str());
    }
    ImGui::End();
}

}  // namespace spotilite
