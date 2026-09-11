// App shell + entry point. Build (no CMake change — direct link):
//   g++ -std=c++17 cpp/ui/app.cpp cpp/ui/queue_window.cpp
//       cpp/ui/search_window.cpp cpp/ui/playlists_window.cpp
//       cpp/ui/content_window.cpp cpp/ui/settings_window.cpp
//       cpp/ui/window_placer.cpp cpp/core/player.cpp
//       third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp
//       third_party/imgui/imgui_tables.cpp third_party/imgui/imgui_widgets.cpp
//       third_party/imgui/backends/imgui_impl_win32.cpp
//       third_party/imgui/backends/imgui_impl_dx11.cpp
//       -Iinclude -Icpp -Ithird_party/imgui -Ithird_party/imgui/backends
//       target/release/liblibrespot_bridge.a -o build/gui.exe
//       -lws2_32 -luserenv -lbcrypt -lole32 -loleaut32 -lpropsys -lntdll
//       -ld3d11 -ld3dcompiler -ldwmapi -lgdi32 -luser32 -lkernel32 -limm32
#include "ui/app.h"

#include <windows.h>

#include <chrono>
#include <cstdio>
#include <d3d11.h>
#include <tchar.h>
#include <vector>

#include "imgui.h"
#include "imgui_impl_dx11.h"
#include "imgui_impl_win32.h"

// ---- DirectX 11 boilerplate (standard ImGui example pattern) ----

static ID3D11Device* g_pd3dDevice = nullptr;
static ID3D11DeviceContext* g_pd3dDeviceContext = nullptr;
static IDXGISwapChain* g_pSwapChain = nullptr;
static ID3D11RenderTargetView* g_mainRenderTargetView = nullptr;

static void CleanupRenderTarget() {
    if (g_mainRenderTargetView) {
        g_mainRenderTargetView->Release();
        g_mainRenderTargetView = nullptr;
    }
}

static void CreateRenderTarget() {
    ID3D11Texture2D* pBackBuffer = nullptr;
    g_pSwapChain->GetBuffer(0, IID_PPV_ARGS(&pBackBuffer));
    if (pBackBuffer) {
        g_pd3dDevice->CreateRenderTargetView(pBackBuffer, nullptr, &g_mainRenderTargetView);
        pBackBuffer->Release();
    }
}

static bool CreateDeviceD3D(HWND hWnd) {
    DXGI_SWAP_CHAIN_DESC sd{};
    sd.BufferCount = 2;
    sd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    sd.BufferDesc.RefreshRate.Numerator = 60;
    sd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.OutputWindow = hWnd;
    sd.SampleDesc.Count = 1;
    sd.Windowed = TRUE;
    sd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

    const D3D_FEATURE_LEVEL kLevels[2] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0};
    D3D_FEATURE_LEVEL featureLevel = D3D_FEATURE_LEVEL_11_0;
    HRESULT res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_HARDWARE, nullptr, 0,
                                               kLevels, 2, D3D11_SDK_VERSION, &sd, &g_pSwapChain,
                                               &g_pd3dDevice, &featureLevel, &g_pd3dDeviceContext);
    if (res == DXGI_ERROR_UNSUPPORTED) {
        res = D3D11CreateDeviceAndSwapChain(nullptr, D3D_DRIVER_TYPE_WARP, nullptr, 0, kLevels, 2,
                                           D3D11_SDK_VERSION, &sd, &g_pSwapChain, &g_pd3dDevice,
                                           &featureLevel, &g_pd3dDeviceContext);
    }
    if (res != S_OK) {
        return false;
    }
    CreateRenderTarget();
    return true;
}

static void CleanupDeviceD3D() {
    CleanupRenderTarget();
    if (g_pSwapChain) {
        g_pSwapChain->Release();
        g_pSwapChain = nullptr;
    }
    if (g_pd3dDeviceContext) {
        g_pd3dDeviceContext->Release();
        g_pd3dDeviceContext = nullptr;
    }
    if (g_pd3dDevice) {
        g_pd3dDevice->Release();
        g_pd3dDevice = nullptr;
    }
}

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam,
                                                             LPARAM lParam);

static LRESULT WINAPI WndProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wParam, lParam)) {
        return true;
    }
    switch (msg) {
        case WM_SIZE:
            if (g_pd3dDevice && wParam != SIZE_MINIMIZED) {
                CleanupRenderTarget();
                g_pSwapChain->ResizeBuffers(0, (UINT)LOWORD(lParam), (UINT)HIWORD(lParam),
                                           DXGI_FORMAT_UNKNOWN, 0);
                CreateRenderTarget();
            }
            return 0;
        case WM_SYSCOMMAND:
            if ((wParam & 0xfff0) == SC_KEYMENU) {
                return 0;
            }
            break;
        case WM_DESTROY:
            ::PostQuitMessage(0);
            return 0;
    }
    return ::DefWindowProcA(hWnd, msg, wParam, lParam);
}

namespace spotilite {
namespace {

// 24-bit BMP from our disk cache to an RGBA DX11 texture (128px artwork).
bool BmpToTexture(ID3D11Device* dev, const std::string& path,
                  ID3D11ShaderResourceView** outSrv) {
    FILE* f = nullptr;
    if (fopen_s(&f, path.c_str(), "rb") != 0 || !f) {
        return false;
    }
    auto read32 = [f](uint32_t& v) {
        return fread(&v, 1, 4, f) == 4;
    };
    uint32_t dataOff = 0, w = 0, h = 0;
    uint16_t planes = 0, bpp = 0;
    uint32_t compression = 0;
    fseek(f, 10, SEEK_SET);
    bool ok = read32(dataOff);
    fseek(f, 18, SEEK_SET);
    ok = ok && read32(w) && read32(h);
    ok = ok && fread(&planes, 1, 2, f) == 2 && fread(&bpp, 1, 2, f) == 2;
    ok = ok && read32(compression);
    if (!ok || bpp != 24 || compression != 0 || w == 0 || h == 0 || w > 512 || h > 512) {
        fclose(f);
        return false;
    }
    const uint32_t stride = ((w * 3 + 3) / 4) * 4;
    std::vector<unsigned char> bgr(stride * h);
    fseek(f, dataOff, SEEK_SET);
    ok = fread(bgr.data(), 1, bgr.size(), f) == bgr.size();
    fclose(f);
    if (!ok) {
        return false;
    }
    // Bottom-up BGR to top-down RGBA.
    std::vector<unsigned char> rgba(w * h * 4);
    for (uint32_t y = 0; y < h; ++y) {
        for (uint32_t x = 0; x < w; ++x) {
            const unsigned char* src = &bgr[(h - 1 - y) * stride + x * 3];
            unsigned char* dst = &rgba[(y * w + x) * 4];
            dst[0] = src[2];
            dst[1] = src[1];
            dst[2] = src[0];
            dst[3] = 255;
        }
    }
    D3D11_TEXTURE2D_DESC desc{};
    desc.Width = w;
    desc.Height = h;
    desc.MipLevels = 1;
    desc.ArraySize = 1;
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.Usage = D3D11_USAGE_DEFAULT;
    desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
    D3D11_SUBRESOURCE_DATA init{};
    init.pSysMem = rgba.data();
    init.SysMemPitch = w * 4;
    ID3D11Texture2D* tex = nullptr;
    if (FAILED(dev->CreateTexture2D(&desc, &init, &tex))) {
        return false;
    }
    D3D11_SHADER_RESOURCE_VIEW_DESC srvDesc{};
    srvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srvDesc.ViewDimension = D3D11_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MipLevels = 1;
    bool srvOk = SUCCEEDED(dev->CreateShaderResourceView(tex, &srvDesc, outSrv));
    tex->Release();
    return srvOk;
}

const char* formatTime(char* buf, std::size_t cap, uint32_t ms) {
    const uint32_t s = ms / 1000;
    snprintf(buf, cap, "%02u:%02u", s / 60, s % 60);
    return buf;
}

}  // namespace

void App::onTrackChanged(const std::string& uri) {
    releaseArtTexture();
    metaHave_ = false;
    pendingMeta_.active = false;
    if (uri.empty()) {
        return;
    }
    // Metadata fetch off the UI thread; applied only if still current.
    pendingMeta_.active = true;
    pendingMeta_.uri = uri;
    pendingMeta_.future =
        std::async(std::launch::async, [this] {
            TrackMetadata meta;
            if (player_.metadata(meta)) {
                return meta;
            }
            return TrackMetadata{};
        });
    player_.requestArtwork(uri);  // non-blocking; READY event follows
}

void App::pollMetadata() {
    if (!pendingMeta_.active) {
        return;
    }
    if (pendingMeta_.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        return;
    }
    pendingMeta_.active = false;
    TrackMetadata meta = pendingMeta_.future.get();
    if (!meta.uri.empty() && meta.uri == player_.state().currentUri) {
        meta_ = meta;
        metaHave_ = true;
    } else {
        onTrackChanged(player_.state().currentUri);  // track moved on; refetch
    }
}

bool App::loadArtTexture(const std::string& path) {
    releaseArtTexture();
    ID3D11ShaderResourceView* srv = nullptr;
    if (!BmpToTexture(dev_, path, &srv)) {
        return false;
    }
    artTex_ = srv;
    return true;
}

void App::releaseArtTexture() {
    if (artTex_) {
        artTex_->Release();
        artTex_ = nullptr;
    }
    artUri_.clear();
}

void App::pollSearch() {
    if (!pendingSearch_.active) {
        return;
    }
    if (pendingSearch_.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        return;
    }
    pendingSearch_.active = false;
    auto [results, error] = pendingSearch_.future.get();
    if (!error.empty() && results.empty()) {
        error_ = error;
        searchResults_.clear();
        return;
    }
    searchResults_ = std::move(results);
    searchSel_ = 0;
    error_.clear();
}

bool App::playSearchResult() {
    if (searchSel_ < 0 || static_cast<std::size_t>(searchSel_) >= searchResults_.size()) {
        return false;
    }
    const SearchResult& item = searchResults_[static_cast<std::size_t>(searchSel_)];
    if (item.kind != SEARCH_TRACK) {
        error_ = "only tracks can be played yet";
        return false;
    }
    // Top-of-queue play: the rest of the queue survives.
    if (!player_.playFirst(item.uri)) {
        error_ = player_.lastError();
        return false;
    }
    error_.clear();
    return true;
}

void App::fetchLibrary(LibMode mode, int page, const std::string& playlistId,
                      const std::string& playlistName) {
    if (page < 0) {
        return;
    }
    pendingLib_.active = true;
    pendingLib_.mode = mode;
    pendingLib_.page = page;
    pendingLib_.playlistId = playlistId;
    pendingLib_.playlistName = playlistName;
    pendingLib_.future = std::async(std::launch::async, [this, mode, page, playlistId] {
        std::tuple<std::vector<SearchResult>, int, std::string> out;
        std::vector<SearchResult>& rows = std::get<0>(out);
        bool ok = false;
        if (mode == LibMode::LIKED) {
            ok = player_.likedTracks(20, page * 20, rows, std::get<1>(out));
        } else if (mode == LibMode::PLAYLISTS) {
            ok = player_.playlists(20, page * 20, rows, std::get<1>(out));
        } else if (mode == LibMode::PLAYLIST_TRACKS) {
            ok = player_.playlistTracks(playlistId, 20, page * 20, rows, std::get<1>(out));
        } else if (mode == LibMode::ALBUM_TRACKS) {
            ok = player_.albumTracks(playlistId, 20, page * 20, rows, std::get<1>(out));
        } else if (mode == LibMode::ARTISTS) {
            ok = player_.followedArtists(20, page * 20, rows, std::get<1>(out));
        } else {
            ok = player_.artistTracks(playlistId, 20, page * 20, rows, std::get<1>(out));
        }
        if (!ok) {
            std::get<2>(out) = player_.lastError();
        }
        return out;
    });
}

void App::pollLibrary() {
    if (!pendingLib_.active) {
        return;
    }
    if (pendingLib_.future.wait_for(std::chrono::seconds(0)) != std::future_status::ready) {
        return;
    }
    pendingLib_.active = false;
    auto [rows, total, error] = pendingLib_.future.get();
    if (!error.empty() && rows.empty()) {
        error_ = error;
        return;
    }
    libResults_ = std::move(rows);
    libTotal_ = total;
    libMode_ = pendingLib_.mode;
    libPage_ = pendingLib_.page;
    libPlaylistId_ = pendingLib_.playlistId;
    libPlaylistName_ = pendingLib_.playlistName;
    libSel_ = 0;
    error_.clear();
}

bool App::playLibraryResult() {
    if (libSel_ < 0 || static_cast<std::size_t>(libSel_) >= libResults_.size()) {
        return false;
    }
    if (libMode_ == LibMode::PLAYLISTS || libMode_ == LibMode::ALBUMS ||
        libMode_ == LibMode::ARTISTS) {
        // Drill into the playlist/album/artist instead of playing it.
        const SearchResult& item =
            libResults_[static_cast<std::size_t>(libSel_)];
        libBackMode_ = libMode_;
        libBackPage_ = libPage_;
        LibMode tracks = LibMode::PLAYLIST_TRACKS;
        if (libMode_ == LibMode::ALBUMS) {
            tracks = LibMode::ALBUM_TRACKS;
        } else if (libMode_ == LibMode::ARTISTS) {
            tracks = LibMode::ARTIST_TRACKS;
        }
        fetchLibrary(tracks, 0, item.uri, item.name);
        return true;
    }
    const SearchResult& item = libResults_[static_cast<std::size_t>(libSel_)];
    if (!player_.playFirst(item.uri)) {
        error_ = player_.lastError();
        return false;
    }
    error_.clear();
    return true;
}

void App::updateShared() {
    // Events drive state; artwork texture follows READY events.
    PlayerEvent event;
    while (player_.pollEvent(event)) {
        // Queue progression: a finished track advances automatically when
        // it is still current (a manual skip already moved on otherwise).
        if (event.type == SPOTIFY_EVENT_TRACK_ENDED &&
            (event.uri.empty() || event.uri == player_.state().currentUri)) {
            player_.next();  // failure = end of queue, already stopped
        }
        if (event.type == SPOTIFY_EVENT_ARTWORK_READY && !event.uri.empty() &&
            event.uri == player_.state().currentUri) {
            const std::string path = player_.artworkPath(event.uri, SPOTIFY_ART_128);
            if (!path.empty() && loadArtTexture(path)) {
                artUri_ = event.uri;
            }
        }
    }
    const std::string cur = player_.state().currentUri;
    if (cur != lastUri_) {
        lastUri_ = cur;
        onTrackChanged(cur);
    }
    pollMetadata();
}

void App::frame() {
    updateShared();
    if (hwnd_) {
        ::GetWindowRect(hwnd_, &mainRect_);
    }
    if (queueOpen_) {
        if (focusQueue_) {
            ImGui::SetNextWindowFocus();
            focusQueue_ = false;
        }
        drawQueueWindow();
    }
    if (searchOpen_) {
        if (focusSearch_) {
            ImGui::SetNextWindowFocus();
            focusSearch_ = false;
        }
        drawSearchWindow();
    }
    if (playlistsOpen_) {
        if (focusPlaylists_) {
            ImGui::SetNextWindowFocus();
            focusPlaylists_ = false;
        }
        drawPlaylistsWindow();
    }
    if (settingsOpen_) {
        if (focusSettings_) {
            ImGui::SetNextWindowFocus();
            focusSettings_ = false;
        }
        drawSettingsWindow();
    }
    drawContentWindows();

    const PlaybackState& s = player_.state();
    // Fill the whole OS window: no ImGui window-inside-a-window.
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("spotilite", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    // Navigation bar: secondary views are separate OS windows.
    if (ImGui::Button("Queue")) {
        queueOpen_ = true;
        focusQueue_ = true;
        placeQueue_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Search")) {
        searchOpen_ = true;
        focusSearch_ = true;
        placeSearch_ = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Playlists")) {
        playlistsOpen_ = true;
        focusPlaylists_ = true;
        placePlaylists_ = true;
        fetchLibrary(LibMode::PLAYLISTS, 0, "", "");
    }
    ImGui::SameLine();
    if (ImGui::Button("Settings")) {
        settingsOpen_ = true;
        focusSettings_ = true;
    }

    // 3+7. Current track + small artwork.
    if (artTex_) {
        ImGui::Image(reinterpret_cast<ImTextureID>(artTex_), ImVec2(64, 64));
        ImGui::SameLine();
    }
    ImGui::BeginGroup();
    if (metaHave_) {
        ImGui::Text("%s", meta_.title.c_str());
        ImGui::Text("%s - %s", meta_.artist.c_str(), meta_.album.c_str());
    } else {
        ImGui::Text("%s", s.currentUri.empty() ? "-" : s.currentUri.c_str());
    }
    char tbuf[16], dbuf[16];
    const uint32_t durMs = metaHave_ ? meta_.durationMs : 0;
    if (durMs > 0) {
        ImGui::Text("%s / %s", formatTime(tbuf, sizeof(tbuf), s.positionMs),
                    formatTime(dbuf, sizeof(dbuf), durMs));
    } else {
        ImGui::Text("%s elapsed", formatTime(tbuf, sizeof(tbuf), s.positionMs));
    }
    ImGui::EndGroup();

    // 5. Progress (seek on release). While dragged, the live state must
    // not overwrite the thumb, or the slider snaps back and never seeks.
    if (durMs > 0) {
        if (!seekHeld_) {
            seekPosSec_ = static_cast<int>(s.positionMs / 1000);
        }
        const int durSec = static_cast<int>(durMs / 1000);
        ImGui::SliderInt("##progress", &seekPosSec_, 0, durSec > 0 ? durSec : 1);
        seekHeld_ = ImGui::IsItemActive();
        if (ImGui::IsItemDeactivatedAfterEdit()) {
            seekHeld_ = false;
            if (!player_.seek(static_cast<uint32_t>(seekPosSec_) * 1000)) {
                error_ = player_.lastError();
            }
        }
    }

    // 4. Controls.
    if (ImGui::Button("<<")) {
        if (!player_.previous()) {
            error_ = player_.lastError();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(s.playing ? "Pause##toggle" : "Play##toggle")) {
        if (s.playing) {
            if (player_.pause()) {
                error_.clear();
            } else {
                error_ = player_.lastError();
            }
        } else if (s.currentUri.empty()) {
            error_ = "nothing loaded; paste a URI above";
        } else {
            // resume() only revives a paused track: on an ended (or
            // never-started) track it does nothing, so restart instead.
            const bool ended = metaHave_ && meta_.durationMs > 0 &&
                               s.positionMs + 2000 >= meta_.durationMs;
            const bool ok = ended ? player_.loadUri(s.currentUri) : player_.resume();
            if (ok) {
                error_.clear();
            } else {
                error_ = player_.lastError();
            }
        }
    }
    ImGui::SameLine();
    if (ImGui::Button(">>")) {
        if (!player_.next()) {
            error_ = player_.lastError();
        }
    }

    // 6. Volume (live).
    ImGui::SameLine();
    float vol = s.volume;
    ImGui::SetNextItemWidth(120);
    if (ImGui::SliderFloat("Vol", &vol, 0.0f, 1.0f)) {
        if (!player_.setVolume(vol)) {
            error_ = player_.lastError();
        }
    }

    if (!error_.empty()) {
        ImGui::Text("%s", error_.c_str());
    }
    ImGui::End();
}

int App::run() {
    if (!player_.connect()) {
        ::MessageBoxA(nullptr, player_.lastError().c_str(), "spotilite: connect failed",
                      MB_OK | MB_ICONERROR);
        return 1;
    }

    WNDCLASSEXW wc{};
    wc.cbSize = sizeof(wc);
    wc.style = CS_CLASSDC;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = ::GetModuleHandle(nullptr);
    wc.lpszClassName = L"spotilite";
    ::RegisterClassExW(&wc);
    HWND hwnd = ::CreateWindowW(wc.lpszClassName, L"spotilite", WS_OVERLAPPEDWINDOW, 100, 100,
                               660, 900, nullptr, nullptr, wc.hInstance, nullptr);
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    hwnd_ = hwnd;
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // no imgui.ini droppings
    io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;  // separate OS windows
    ImGui::StyleColorsDark();
    ImGui_ImplWin32_Init(hwnd);
    ImGui_ImplDX11_Init(g_pd3dDevice, g_pd3dDeviceContext);
    dev_ = g_pd3dDevice;
    ctx_ = g_pd3dDeviceContext;

    bool done = false;
    while (!done) {
        MSG msg;
        while (::PeekMessage(&msg, nullptr, 0U, 0U, PM_REMOVE)) {
            ::TranslateMessage(&msg);
            ::DispatchMessage(&msg);
            if (msg.message == WM_QUIT) {
                done = true;
            }
        }
        if (done) {
            break;
        }
        ImGui_ImplDX11_NewFrame();
        ImGui_ImplWin32_NewFrame();
        ImGui::NewFrame();
        frame();
        ImGui::Render();
        const float clearColor[4] = {0.10f, 0.10f, 0.10f, 1.00f};
        g_pd3dDeviceContext->OMSetRenderTargets(1, &g_mainRenderTargetView, nullptr);
        g_pd3dDeviceContext->ClearRenderTargetView(g_mainRenderTargetView, clearColor);
        ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
        g_pSwapChain->Present(1, 0);
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable) {
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
        }
    }

    releaseArtTexture();
    ImGui_ImplDX11_Shutdown();
    ImGui_ImplWin32_Shutdown();
    ImGui::DestroyContext();
    CleanupDeviceD3D();
    ::DestroyWindow(hwnd);
    ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
    return 0;
}

}  // namespace spotilite

int main() {
    try {
        spotilite::App app;
        return app.run();
    } catch (const std::exception& e) {
        ::MessageBoxA(nullptr, e.what(), "spotilite: fatal", MB_OK | MB_ICONERROR);
        return 1;
    }
}
