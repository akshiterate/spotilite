// Preferred-layout placer: secondary windows orbit the main player.
// Queue left, Search above, Playlists right, contents right-cascaded,
// Settings below. Clamped into the work area; overlap when nothing fits.
#include "ui/window_placer.h"

#include <cstring>

#include "ui/app.h"

namespace spotilite {
namespace {

ImVec2 clampInto(ImVec2 pos, ImVec2 size, RECT workArea) {
    if (pos.x < (float)workArea.left) {
        pos.x = (float)workArea.left;
    }
    if (pos.y < (float)workArea.top) {
        pos.y = (float)workArea.top;
    }
    if (pos.x + size.x > (float)workArea.right) {
        pos.x = (float)workArea.right - size.x;
    }
    if (pos.y + size.y > (float)workArea.bottom) {
        pos.y = (float)workArea.bottom - size.y;
    }
    return pos;
}

}  // namespace

ImVec2 PlaceWindow(const char* role, ImVec2 size, RECT mainRect, RECT workArea,
                   int cascade) {
    const float mainW = (float)(mainRect.right - mainRect.left);
    const float mainH = (float)(mainRect.bottom - mainRect.top);
    ImVec2 pos{(float)mainRect.left, (float)mainRect.top};
    if (std::strcmp(role, "queue") == 0) {
        pos.x = (float)mainRect.left - size.x - 12.0f;
    } else if (std::strcmp(role, "search") == 0) {
        pos.x = (float)mainRect.left + (mainW - size.x) * 0.5f;
        pos.y = (float)mainRect.top - size.y - 12.0f;
    } else if (std::strcmp(role, "playlists") == 0) {
        pos.x = (float)mainRect.right + 12.0f;
    } else if (std::strcmp(role, "content") == 0) {
        pos.x = (float)mainRect.right + 12.0f + (float)(cascade * 28);
        pos.y = (float)mainRect.top + 40.0f + (float)(cascade * 28);
    } else {  // settings
        pos.x = (float)mainRect.left + (mainW - size.x) * 0.5f;
        pos.y = (float)mainRect.bottom + 12.0f;
    }
    // Never cover the main playback controls when space allows: prefer the
    // computed spot, but clamping (below) may still overlap on tiny screens.
    (void)mainH;
    return clampInto(pos, size, workArea);
}

void App::placeMe(const char* role, ImVec2 size, int cascade) {
    RECT work{};
    ::SystemParametersInfoA(SPI_GETWORKAREA, 0, &work, 0);
    const ImVec2 pos = PlaceWindow(role, size, mainRect_, work, cascade);
    ImGui::SetNextWindowPos(pos, ImGuiCond_Always);
    ImGui::SetNextWindowSize(size, ImGuiCond_Always);
}

}  // namespace spotilite
