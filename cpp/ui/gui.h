// First native GUI (plans.md Phase 7): Dear ImGui over Win32 + DirectX 11.
// Uses the C++ core only: no playback logic and no Rust symbols here.
// Deliberately sparse: URI input, queue list, current track, controls,
// progress, volume, small artwork. Search arrives in Phase 8.
#pragma once

#include <d3d11.h>

#include <future>
#include <string>

#include "core/player.h"

namespace spotilite {

class Gui {
public:
    Gui() = default;
    // Creates the window, runs the frame loop, returns the exit code.
    int run();

private:
    void frame();
    void onTrackChanged(const std::string& uri);
    void pollMetadata();
    bool loadArtTexture(const std::string& path);
    void releaseArtTexture();
    bool playSelected();

    Player player_;
    char uriBuf_[256] = "";
    int queueSel_ = 0;
    std::string lastUri_;
    std::string error_;

    struct PendingMeta {
        bool active = false;
        std::string uri;
        std::future<TrackMetadata> future;
    };
    PendingMeta pendingMeta_;
    TrackMetadata meta_;
    bool metaHave_ = false;

    ID3D11ShaderResourceView* artTex_ = nullptr;
    std::string artUri_;

    ID3D11Device* dev_ = nullptr;
    ID3D11DeviceContext* ctx_ = nullptr;
};

}  // namespace spotilite
