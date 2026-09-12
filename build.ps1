# spotilite build script (Windows, PowerShell 5.1+).
# Single source of truth for all binaries - run from the repo root:
#   powershell -ExecutionPolicy Bypass -File build.ps1
# Prereqs on PATH: cmake, cargo, g++ (MinGW-w64), strip.
# Static linking: exes run on machines without MinGW installed.

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $MyInvocation.MyCommand.Path
Set-Location -LiteralPath $Root

$cmakeBin = "C:\Users\asac\AppData\Local\spotilite-tools\cmake\cmake-4.4.3-windows-x86_64\bin"
$cargoBin = Join-Path $Env:USERPROFILE ".cargo\bin"
$Env:PATH = "$cmakeBin;$cargoBin;$Env:PATH"

$sysLibs = @("-lws2_32", "-luserenv", "-lbcrypt", "-lole32", "-loleaut32", "-lpropsys", "-lntdll", "-liphlpapi")
$guiLibs = $sysLibs + @("-ld3d11", "-ld3dcompiler", "-ldwmapi", "-lgdi32", "-luser32", "-lkernel32", "-limm32")
$imguiSrc = @(
    "third_party/imgui/imgui.cpp",
    "third_party/imgui/imgui_draw.cpp",
    "third_party/imgui/imgui_tables.cpp",
    "third_party/imgui/imgui_widgets.cpp",
    "third_party/imgui/backends/imgui_impl_win32.cpp",
    "third_party/imgui/backends/imgui_impl_dx11.cpp"
)
$imguiInc = @("-Ithird_party/imgui", "-Ithird_party/imgui/backends")
$staticLib = "target/release/liblibrespot_bridge.a"

function Invoke-Link($name, $sources, $libs, $out) {
    Write-Output "--- linking $name ---"
    $args = @("-std=c++17", "-static") + $sources +
        @("-Iinclude", "-Icpp") + $imguiInc + @($staticLib, "-o", $out) + $libs
    & g++ @args
    if ($LASTEXITCODE -ne 0) { throw "link failed: $name" }
    & strip $out
    if ($LASTEXITCODE -ne 0) { throw "strip failed: $name" }
}

Write-Output "--- cmake (C++ exe + orchestrated cargo build) ---"
& cmake -S . -B build -G "MinGW Makefiles" -DCMAKE_BUILD_TYPE=Release
if ($LASTEXITCODE -ne 0) { throw "cmake configure failed" }
& cmake --build build --config Release
if ($LASTEXITCODE -ne 0) { throw "cmake build failed" }

Invoke-Link "core_test" @("cpp/core/core_test.cpp", "cpp/core/player.cpp") $sysLibs "build/core_test.exe"
Invoke-Link "tui" @("cpp/app/tui.cpp", "cpp/core/player.cpp") $sysLibs "build/tui.exe"
Invoke-Link "bridge_test" @("cpp/bridge_test.cpp") $sysLibs "build/bridge_test.exe"
Invoke-Link "gui" (@("cpp/ui/app.cpp", "cpp/ui/queue_window.cpp", "cpp/ui/search_window.cpp",
    "cpp/ui/playlists_window.cpp", "cpp/ui/content_window.cpp", "cpp/ui/settings_window.cpp",
    "cpp/ui/window_placer.cpp", "cpp/core/player.cpp") + $imguiSrc) $guiLibs "build/gui.exe"

Write-Output "--- done ---"
Get-ChildItem -LiteralPath "build" -Filter "*.exe" |
    Select-Object Name, @{N = "MB"; E = { [math]::Round($_.Length / 1MB, 1) } }
