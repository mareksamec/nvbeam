# Unit tests for nvbeam_core: the non-UI half of the app (config and project
# files, git output parsing, color schemes, session state). Nothing here starts
# a window or an nvim process, so the whole suite runs headless.
find_package(Qt6 REQUIRED COMPONENTS Test)

# Each file is one QTest class and one ctest case.
set(NVBEAM_TEST_SOURCES
    tst_config.cpp
    tst_fuzzy.cpp
    tst_gitstatus.cpp
    tst_colorscheme.cpp
    tst_session.cpp
    tst_pinnedprojects.cpp
    tst_terminalkeys.cpp
)

foreach(src IN LISTS NVBEAM_TEST_SOURCES)
    get_filename_component(name "${src}" NAME_WE)
    add_executable(${name} ${src})
    target_link_libraries(${name} PRIVATE nvbeam_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
    file(MAKE_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/home-${name}")
    # offscreen: the QGuiApplication a QFont needs must not require a display.
    # The home redirect keeps the tests that exercise QStandardPaths locations
    # (session.ini, projects.toml) out of the real config directory; it pairs
    # with QStandardPaths::setTestModeEnabled() in those tests.
    set_tests_properties(${name} PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;HOME=${CMAKE_CURRENT_BINARY_DIR}/home-${name}")
endforeach()
