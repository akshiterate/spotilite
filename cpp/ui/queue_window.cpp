// Queue window (separate OS window): currently playing on top, numbered
// upcoming queue below. Play Selected discards everything before the
// selection and plays it; Delete/ shuffle / Move-to-N edit the order;
// finishing a track auto-advances (app layer) with numbers following.
#include "ui/app.h"

#include <cstdlib>
#include <string>
#include <vector>

#include "imgui.h"

namespace spotilite {

void App::drawQueueWindow() {
    if (focusQueue_) {
        ImGui::SetNextWindowFocus();
        focusQueue_ = false;
    }
    if (placeQueue_) {
        placeMe("queue", ImVec2(360, 430), 0);
        placeQueue_ = false;
    }
    if (!ImGui::Begin("Queue", &queueOpen_)) {
        ImGui::End();
        return;
    }
    Queue& queue = player_.queue();
    const std::string& cur = player_.state().currentUri;
    ImGui::Text("Now: %s", cur.empty() ? "-" : cur.c_str());

    const std::size_t base = queue.empty() ? 0 : queue.index() + 1;
    const std::size_t upcoming = queue.size() > base ? queue.size() - base : 0;
    std::vector<std::string> labels;
    for (std::size_t i = 0; i < upcoming; ++i) {
        labels.push_back(std::to_string(i + 1) + ". " + queue.at(base + i));
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
    if (ImGui::Button("Move Selected")) {
        const int want = std::atoi(queueMoveBuf_);
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
    ImGui::End();
}

}  // namespace spotilite
