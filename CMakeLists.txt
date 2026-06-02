cmake_minimum_required(VERSION 3.20)
project(neobeam VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(NEOBEAM_TERMINAL "Build with embedded terminal (requires qtermwidget6)" ON)

include(FetchContent)
FetchContent_Declare(
    toml11
    GIT_REPOSITORY https://github.com/ToruNiina/toml11.git
    GIT_TAG        v4.2.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(toml11)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

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
    src/uiwidgets.cpp
    src/terminalpanel.cpp
    src/minimap.cpp
    src/imagerenderer.cpp
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

# --- Install & packaging ---
install(TARGETS neobeam RUNTIME DESTINATION bin)

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
