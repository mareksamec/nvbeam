cmake_minimum_required(VERSION 3.20)
project(neobeam VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)  # compile .qrc resources (generated app icons)

list(APPEND CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake")

option(NEOBEAM_TERMINAL "Build with embedded terminal (requires qtermwidget6)" ON)
option(NEOBEAM_SYNTAX "Color code in review diffs (requires KF6SyntaxHighlighting)" ON)

include(FetchContent)
FetchContent_Declare(
    toml11
    GIT_REPOSITORY https://github.com/ToruNiina/toml11.git
    GIT_TAG        v4.2.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(toml11)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# Code coloring inside the change review panel's diffs. Optional: without it
# the panel still colors added/removed/hunk lines itself.
if(NEOBEAM_SYNTAX)
    find_package(KF6SyntaxHighlighting QUIET)
    if(NOT KF6SyntaxHighlighting_FOUND)
        message(STATUS "KF6SyntaxHighlighting not found - diffs without code coloring")
        set(NEOBEAM_SYNTAX OFF)
    endif()
endif()

if(NEOBEAM_TERMINAL)
    find_package(qtermwidget6 QUIET)
    if(NOT qtermwidget6_FOUND)
        message(STATUS "qtermwidget6 not found — terminal panel disabled")
        set(NEOBEAM_TERMINAL OFF)
    endif()
endif()

add_executable(neobeam
    src/main.cpp
    src/mainwindow.cpp
    src/nvimprocess.cpp
    src/gridwidget.cpp
    src/config.cpp
    src/session.cpp
    src/switcher.cpp
    src/branchswitcher.cpp
    src/uiwidgets.cpp
    src/gitstatus.cpp
    src/colorscheme.cpp
    src/changereview.cpp
    src/terminalpanel.cpp
    src/minimap.cpp
    src/imagerenderer.cpp
    src/logging.cpp
)

target_include_directories(neobeam PRIVATE
    src
)

target_link_libraries(neobeam PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
    toml11::toml11
)

if(NEOBEAM_TERMINAL)
    target_compile_definitions(neobeam PRIVATE NEOBEAM_HAS_TERMINAL)
    target_link_libraries(neobeam PRIVATE qtermwidget6)
endif()

if(NEOBEAM_SYNTAX)
    target_compile_definitions(neobeam PRIVATE NEOBEAM_HAS_SYNTAX)
    target_link_libraries(neobeam PRIVATE KF6::SyntaxHighlighting)
endif()

# --- Application icon ---
# Rasterize a PNG size ladder from the SVG master at build time and embed it
# into the executable as a Qt resource (no runtime Qt6::Svg dependency). The
# same PNGs are installed into the Linux hicolor icon theme below.
include(AppIcons)
set(NEOBEAM_ICON_SIZES_REQUESTED 16 24 32 48 64 128 256)
neobeam_generate_icons(
    TARGET  neobeam
    SVG     "${CMAKE_SOURCE_DIR}/resources/icons/neobeam.svg"
    SIZES   ${NEOBEAM_ICON_SIZES_REQUESTED}
    OUT_DIR "${CMAKE_BINARY_DIR}/generated-icons"
)
# Let the source know which icon sizes were actually embedded so it can build
# the QIcon from the matching resource paths.
if(NEOBEAM_ICON_SIZES)
    string(REPLACE ";" "," _icon_sizes_csv "${NEOBEAM_ICON_SIZES}")
    target_compile_definitions(neobeam PRIVATE
        NEOBEAM_ICON_SIZES=${_icon_sizes_csv})
endif()

# --- Install & packaging ---
install(TARGETS neobeam RUNTIME DESTINATION bin)

# Runtime Lua scripts (minimap, bufferline, statusline feed data via
# nvim_exec_lua). These are required at runtime, not just build-time — without
# them the minimap/bufferline/statusline silently no-op. loadLua() in
# mainwindow.cpp resolves them relative to the executable, so for an installed
# /usr/bin/neobeam this maps to /usr/share/neobeam/lua.
install(DIRECTORY lua/ DESTINATION share/neobeam/lua FILES_MATCHING PATTERN "*.lua")

# Terminal color schemes shipped with neobeam (Neoblue, the default, plus
# OneDark and Dracula).
# TerminalPanel registers this directory with QTermWidget at startup; without
# it the terminal falls back to QTermWidget's built-in default scheme.
install(DIRECTORY resources/color-schemes/ DESTINATION share/neobeam/color-schemes
        FILES_MATCHING PATTERN "*.colorscheme")

# Linux desktop integration: launcher entry + themed icons. The .desktop file's
# "Icon=neobeam" resolves to the PNGs installed under hicolor/<size>x<size>/apps.
if(UNIX AND NOT APPLE)
    install(FILES packaging/neobeam.desktop
            DESTINATION share/applications)

    # Install each generated PNG into its matching hicolor size directory.
    # Falls back gracefully: if no rasterizer was available NEOBEAM_ICON_SIZES
    # is empty and nothing icon-related is installed.
    foreach(size IN LISTS NEOBEAM_ICON_SIZES)
        install(FILES "${CMAKE_BINARY_DIR}/generated-icons/neobeam_${size}.png"
                DESTINATION "share/icons/hicolor/${size}x${size}/apps"
                RENAME neobeam.png)
    endforeach()

    # Also ship the SVG master into the scalable theme dir for environments
    # that prefer vector icons (rendered by the desktop, not by our app).
    install(FILES resources/icons/neobeam.svg
            DESTINATION share/icons/hicolor/scalable/apps)
endif()

# Platform icon embedding for later (roadmap: Windows, macOS):
# if(WIN32)
#     target_sources(neobeam PRIVATE packaging/neobeam.rc)  # references .ico
# elseif(APPLE)
#     set(MACOSX_BUNDLE_ICON_FILE neobeam.icns)
#     set_source_files_properties(resources/icons/neobeam.icns PROPERTIES
#         MACOSX_PACKAGE_LOCATION Resources)
# endif()

set(CPACK_PACKAGE_NAME "neobeam")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Fast native Neovim GUI client built with Qt6")
set(CPACK_PACKAGE_CONTACT "marek@samec.me")
set(CPACK_PACKAGE_LICENSE "MIT")

# DEB
set(CPACK_DEBIAN_PACKAGE_DEPENDS "libqt6core6, libqt6gui6, libqt6widgets6, neovim (>= 0.10)")
set(CPACK_DEBIAN_PACKAGE_SECTION "editors")

# RPM
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase >= 6.0, neovim >= 0.10")
set(CPACK_RPM_PACKAGE_LICENSE "MIT")

include(CPack)
