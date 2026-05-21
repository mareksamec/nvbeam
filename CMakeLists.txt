cmake_minimum_required(VERSION 3.20)
project(neobeam VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

option(NEOBEAM_TERMINAL "Build with embedded terminal (requires qtermwidget6)" ON)

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
    third_party/toml11/include
)

target_link_libraries(neobeam PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

if(NEOBEAM_TERMINAL)
    target_compile_definitions(neobeam PRIVATE NEOBEAM_HAS_TERMINAL)
    target_link_libraries(neobeam PRIVATE qtermwidget6)
endif()
