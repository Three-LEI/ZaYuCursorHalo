cmake_minimum_required(VERSION 3.21)
cmake_policy(SET CMP0091 NEW)

project(ZaYuCursorHalo LANGUAGES CXX RC)

set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded")

add_executable(ZaYuCursorHalo WIN32
    src/main.cpp
    src/mouse_hook.cpp
    src/overlay.cpp
    src/overlay_renderer.cpp
    src/frame_pacer.cpp
    src/frame_stats.cpp
    src/hotkey.cpp
    src/key_overlay.cpp
    src/keyboard_hook.cpp
    src/settings.cpp
    src/settings_window.cpp
    src/timer_widget.cpp
    src/trail_overlay.cpp
    src/click_feedback.cpp
    src/sound_decoder.cpp
    src/tray_panel.cpp
    src/ui_theme.cpp
    src/util.cpp
    src/app.rc
    res/tray_active.ico
    res/tray_paused.ico
    res/dseg7-modern.ttf
)

target_compile_features(ZaYuCursorHalo PRIVATE cxx_std_17)

target_compile_options(ZaYuCursorHalo PRIVATE
    /utf-8
    /W4
    /permissive-
    /EHsc
    /O2
    /Gy
    /Gw
)

target_compile_definitions(ZaYuCursorHalo PRIVATE
    UNICODE
    _UNICODE
    WINVER=0x0601
    _WIN32_WINNT=0x0601
    NTDDI_VERSION=0x06010000
    NOMINMAX
    WIN32_LEAN_AND_MEAN
)

target_link_libraries(ZaYuCursorHalo PRIVATE
    d2d1
    dwrite
    windowscodecs
    gdiplus
    dwmapi
    winmm
    comdlg32
    shell32
    ole32
    user32
    gdi32
    advapi32
    imm32
)

# A custom manifest is embedded from app.rc. Disable the duplicate manifest
# that MSBuild would otherwise generate.
target_link_options(ZaYuCursorHalo PRIVATE
    /MANIFEST:NO
    /SUBSYSTEM:WINDOWS,6.01
    /DYNAMICBASE
    /NXCOMPAT
    /OPT:REF
    /OPT:ICF
)

set_target_properties(ZaYuCursorHalo PROPERTIES
    OUTPUT_NAME "ZaYuCursorHalo"
)
