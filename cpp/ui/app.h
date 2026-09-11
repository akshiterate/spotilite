// Spotilite desktop App shell (UI rework phase 2): owns the single Player
// (the source of truth) plus one native OS window per view via Dear ImGui
// viewports (docking branch). Views live in their own files and use only
// this object: no playback logic and no Rust symbols in views.
#pragma once

#include <d3d11.h>

#include <future>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "core/player.h"

namespace spotilite {

class App {
public:
    App() = default;
    // Creates the main window, runs the frame loop, returns the exit code.
    int run();

private:
    enum class LibMode { LIKED, PLAYLISTS, PLAYLIST_TRACKS, ALBUMS, ALBUM_TRACKS, ARTISTS, ARTIST_TRACKS };
    void frame();
    void updateShared();
    void onTrackChanged(const std::string& uri);
    void pollMetadata();
    void pollSearch();
    bool playSearchResult();
    void fetchLibrary(LibMode mode, int page, const std::string& playlistId,
                      const std::string& playlistName);
    void pollLibrary();
    bool playLibraryResult();
    bool loadArtTexture(const std::string& path);
    void releaseArtTexture();
    // Phase-3 library/playlists window reuses these.
    void drawQueueWindow();
    void drawSearchWindow();

    Player player_;
    int queueUpSel_ = 0;
    char queueMoveBuf_[16] = "";
    char searchBuf_[256] = "";
    struct PendingSearch {
        bool active = false;
        std::future<std::pair<std::vector<SearchResult>, std::string>> future;
    };
    PendingSearch pendingSearch_;
    std::vector<SearchResult> searchResults_;
    int searchSel_ = 0;
    struct PendingLibrary {
        bool active = false;
        LibMode mode = LibMode::LIKED;
        int page = 0;
        std::string playlistId;
        std::string playlistName;
        std::future<std::tuple<std::vector<SearchResult>, int, std::string>> future;
    };
    PendingLibrary pendingLib_;
    std::vector<SearchResult> libResults_;
    int libSel_ = 0;
    LibMode libMode_ = LibMode::LIKED;
    int libPage_ = 0;
    int libTotal_ = 0;
    std::string libPlaylistId_;
    std::string libPlaylistName_;
    LibMode libBackMode_ = LibMode::PLAYLISTS;
    int libBackPage_ = 0;
    int seekPosSec_ = 0;
    bool seekHeld_ = false;
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

    bool queueOpen_ = false;
    bool searchOpen_ = false;
    bool focusQueue_ = false;
    bool focusSearch_ = false;
};

}  // namespace spotilite
