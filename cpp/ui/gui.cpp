// GUI implementation + entry point. Build (no CMake change — direct link):
//   g++ -std=c++17 cpp/ui/gui.cpp cpp/core/player.cpp
//       third_party/imgui/imgui.cpp third_party/imgui/imgui_draw.cpp
//       third_party/imgui/imgui_tables.cpp third_party/imgui/imgui_widgets.cpp
//       third_party/imgui/backends/imgui_impl_win32.cpp
//       third_party/imgui/backends/imgui_impl_dx11.cpp
//       -Iinclude -Icpp -Ithird_party/imgui -Ithird_party/imgui/backends
//       target/release/liblibrespot_bridge.a -o build/gui.exe
//       -lws2_32 -luserenv -lbcrypt -lole32 -loleaut32 -lpropsys -lntdll
//       -ld3d11 -ld3dcompiler -ldwmapi -lgdi32 -luser32 -lkernel32 -limm32
#include "ui/gui.h"

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

void Gui::onTrackChanged(const std::string& uri) {
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

void Gui::pollMetadata() {
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

bool Gui::loadArtTexture(const std::string& path) {
    releaseArtTexture();
    ID3D11ShaderResourceView* srv = nullptr;
    if (!BmpToTexture(dev_, path, &srv)) {
        return false;
    }
    artTex_ = srv;
    return true;
}

void Gui::releaseArtTexture() {
    if (artTex_) {
        artTex_->Release();
        artTex_ = nullptr;
    }
    artUri_.clear();
}

bool Gui::playSelected() {
    const int i = queueSel_;
    if (i < 0 || static_cast<std::size_t>(i) >= player_.queue().size()) {
        return false;
    }
    if (!player_.queue().select(static_cast<std::size_t>(i))) {
        return false;
    }
    return player_.playCurrent();
}

void Gui::pollSearch() {
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

bool Gui::playSearchResult() {
    if (searchSel_ < 0 || static_cast<std::size_t>(searchSel_) >= searchResults_.size()) {
        return false;
    }
    const SearchResult& item = searchResults_[static_cast<std::size_t>(searchSel_)];
    if (item.kind != SEARCH_TRACK) {
        error_ = "only tracks can be played yet";
        return false;
    }
    if (!player_.loadUri(item.uri)) {
        error_ = player_.lastError();
        return false;
    }
    error_.clear();
    return true;
}

void Gui::frame() {
    // Events drive state; artwork texture follows READY events.
    PlayerEvent event;
    while (player_.pollEvent(event)) {
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

    const PlaybackState& s = player_.state();
    // Fill the whole OS window: no ImGui window-inside-a-window.
    ImGuiViewport* viewport = ImGui::GetMainViewport();
    ImGui::SetNextWindowPos(viewport->WorkPos);
    ImGui::SetNextWindowSize(viewport->WorkSize);
    ImGui::Begin("spotilite", nullptr,
                 ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
                     ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoCollapse);

    // 1+2. URI input + queue-as-results (real search is Phase 8).
    ImGui::InputText("URI", uriBuf_, sizeof(uriBuf_));
    ImGui::SameLine();
    if (ImGui::Button("Play")) {
        if (!player_.loadUri(uriBuf_)) {
            error_ = player_.lastError();
        } else {
            error_.clear();
            uriBuf_[0] = '\0';
        }
    }
    std::vector<const char*> rows;
    for (std::size_t i = 0; i < player_.queue().size(); ++i) {
        rows.push_back(player_.queue().at(i).c_str());
    }
    if (queueSel_ >= static_cast<int>(rows.size())) {
        queueSel_ = static_cast<int>(rows.size()) - 1;
    }
    if (queueSel_ < 0 && !rows.empty()) {
        queueSel_ = 0;
    }
    ImGui::ListBox("##queue", &queueSel_, rows.data(), static_cast<int>(rows.size()), 5);
    ImGui::SameLine();
    if (ImGui::Button("Play selected") && !playSelected()) {
        error_ = player_.lastError();
    }
    ImGui::SameLine();
    if (ImGui::Button("Add URI")) {
        if (uriBuf_[0] == '\0') {
            error_ = "type a URI above first";
        } else {
            player_.enqueue(uriBuf_);
            uriBuf_[0] = '\0';
            error_.clear();
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Del")) {
        const std::size_t row =
            queueSel_ < 0 ? 0 : static_cast<std::size_t>(queueSel_);
        if (!player_.queue().removeAt(row)) {
            error_ = "nothing to delete";
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Up")) {
        const int from = queueSel_;
        if (from > 0 &&
            player_.queue().move(static_cast<std::size_t>(from),
                                 static_cast<std::size_t>(from - 1))) {
            queueSel_ = from - 1;
        }
    }
    ImGui::SameLine();
    if (ImGui::Button("Down")) {
        const int from = queueSel_;
        if (from >= 0 &&
            static_cast<std::size_t>(from + 1) < player_.queue().size() &&
            player_.queue().move(static_cast<std::size_t>(from),
                                 static_cast<std::size_t>(from + 1))) {
            queueSel_ = from + 1;
        }
    }

    // 8. Web API search + results (tracks playable, other rows display).
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
    ImGui::ListBox("##results", &searchSel_, found.data(), static_cast<int>(found.size()), 6);
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
    if (ImGui::Button(s.playing ? "Pause" : "Play")) {
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

int Gui::run() {
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
                               660, 720, nullptr, nullptr, wc.hInstance, nullptr);
    if (!CreateDeviceD3D(hwnd)) {
        CleanupDeviceD3D();
        ::UnregisterClassW(wc.lpszClassName, wc.hInstance);
        return 1;
    }
    ::ShowWindow(hwnd, SW_SHOWDEFAULT);
    ::UpdateWindow(hwnd);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // no imgui.ini droppings
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
        spotilite::Gui gui;
        return gui.run();
    } catch (const std::exception& e) {
        ::MessageBoxA(nullptr, e.what(), "spotilite: fatal", MB_OK | MB_ICONERROR);
        return 1;
    }
}
