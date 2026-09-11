// Secondary-window placement (UI rework phase 3): positions computed from
// the main window rect + screen work area. Overlap is an accepted fallback.
#pragma once

#include <windows.h>

#include "imgui.h"

namespace spotilite {

// Preferred roles: "queue" | "search" | "playlists" | "content" | "settings".
// cascade staggers same-role windows (playlist contents).
ImVec2 PlaceWindow(const char* role, ImVec2 size, RECT mainRect, RECT workArea,
                   int cascade);

}  // namespace spotilite
