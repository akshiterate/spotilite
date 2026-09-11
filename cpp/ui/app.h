// Spotilite desktop App shell (UI rework phase 2): owns the single Player
// (the source of truth) plus one native OS window per view via Dear ImGui
// viewports (docking branch). Views live in their own files and use only
// this object: no playback logic and no Rust symbols in views.
#pragma once

#include <d3d11.h>

#include <future>
#include <map>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "core/player.h"
#include "imgui.h"

namespace spotilite {

class App {
public:
    App() = default;
    // Creates the main window, runs the frame loop, returns the exit code.
    int run();
    // Called from the Win32 message loop on focus loss.
    void closeAllSecondary();
    bool isOwnWindow(HWND hwnd) const;
    // Shared playback actions (buttons, hotkeys, media keys all funnel here).
    void togglePlayPause();
    void queueNext();
    void queuePrev();
    // Low-level hotkey hook callback target: true = key consumed.
    bool handleHotKey(int vk, bool ctrl, bool alt);

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
    void openSettings();
    void drawPlaylistsWindow();
    void drawContentWindows();
    void drawSettingsWindow();
    void openContent(const std::string& key, const std::string& title, bool isLiked);
    void placeMe(const char* role, ImVec2 size, int cascade);

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
    // Refit-once flags: async content can land after auto-size ran.
    bool sizeSearch_ = false;
    bool sizePlaylists_ = false;
    // Track-name cache for queue rows (filled in the background).
    std::map<std::string, TrackMetadata> nameCache_;
    struct PendingNames {
        bool active = false;
        std::future<std::map<std::string, TrackMetadata>> future;
    };
    PendingNames pendingNames_;
    void ensureNames();
    std::string trackLabel(const std::string& uri);
    // Settings editor state (loaded on open, saved per edit).
    char cfgDevice_[128] = "";
    int cfgBitrateIdx_ = 1;  // 0=96, 1=160, 2=320
    bool cfgNorm_ = true;
    int cfgVolPct_ = 50;
    int cfgCacheMb_ = 1024;
    std::string cfgError_;
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
    HHOOK hook_ = nullptr;
    HWND hwnd_ = nullptr;
    RECT mainRect_ = {};

    bool queueOpen_ = false;
    bool searchOpen_ = false;
    bool playlistsOpen_ = false;
    bool settingsOpen_ = false;
    bool focusQueue_ = false;
    bool focusSearch_ = false;
    bool focusPlaylists_ = false;
    bool focusSettings_ = false;
    bool placeQueue_ = false;
    bool placeSearch_ = false;
    bool placePlaylists_ = false;
    bool placeSettings_ = false;
    struct ContentWin {
        std::string key;
        std::string title;
        bool isLiked = false;
        bool open = true;
        bool placeMe = true;
        bool focusMe = false;
        bool sizeMe = false;
        bool active = false;
        std::future<std::pair<std::vector<SearchResult>, std::string>> future;
        std::vector<SearchResult> rows;
        int sel = 0;
        std::string error;
    };
    std::vector<ContentWin> contentWins_;
};

}  // namespace spotilite
