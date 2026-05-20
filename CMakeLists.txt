cmake_minimum_required(VERSION 3.20)
project(neobeam VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

add_executable(neobeam
    src/main.cpp
    src/mainwindow.cpp
    src/nvimprocess.cpp
    src/gridwidget.cpp
    src/config.cpp
    src/uiwidgets.cpp
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
