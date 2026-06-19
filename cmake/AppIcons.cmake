# AppIcons.cmake
#
# Generates a ladder of PNG application icons from an SVG master at build time,
# embeds them into the target as a Qt resource (for the in-app window/taskbar
# icon), and exposes the generated files for installation into the Linux
# hicolor icon theme.
#
# This avoids a runtime dependency on Qt6::Svg: the SVG is rasterized once at
# build time with an external tool (rsvg-convert preferred, ImageMagick as a
# fallback). If no rasterizer is available the function is a graceful no-op —
# the build still succeeds, just without a bundled icon.
#
# Usage:
#   neobeam_generate_icons(
#       TARGET   neobeam
#       SVG      ${CMAKE_SOURCE_DIR}/resources/icons/neobeam.svg
#       SIZES    16 24 32 48 64 128 256
#       OUT_DIR  ${CMAKE_BINARY_DIR}/generated-icons
#   )
#
# On success it sets two variables in the caller's scope:
#   NEOBEAM_ICON_PNGS   absolute paths of the generated PNG files
#   NEOBEAM_ICON_SIZES  the sizes that were actually generated (parallel list)

function(neobeam_generate_icons)
    set(options "")
    set(oneValueArgs TARGET SVG OUT_DIR)
    set(multiValueArgs SIZES)
    cmake_parse_arguments(ARG "${options}" "${oneValueArgs}" "${multiValueArgs}" ${ARGN})

    # Locate a rasterizer. rsvg-convert gives the most consistent output; the
    # ImageMagick CLIs are a widely available fallback.
    find_program(RSVG_CONVERT rsvg-convert)
    find_program(MAGICK_TOOL NAMES magick convert)

    if(NOT RSVG_CONVERT AND NOT MAGICK_TOOL)
        message(STATUS
            "No SVG rasterizer (rsvg-convert / ImageMagick) found — "
            "application icon will not be generated.")
        set(NEOBEAM_ICON_PNGS "" PARENT_SCOPE)
        set(NEOBEAM_ICON_SIZES "" PARENT_SCOPE)
        return()
    endif()

    file(MAKE_DIRECTORY "${ARG_OUT_DIR}")

    set(_pngs "")
    set(_sizes "")
    foreach(size IN LISTS ARG_SIZES)
        set(_out "${ARG_OUT_DIR}/neobeam_${size}.png")

        if(RSVG_CONVERT)
            # rsvg-convert renders at exact pixel dimensions with transparency.
            add_custom_command(
                OUTPUT "${_out}"
                COMMAND "${RSVG_CONVERT}"
                        -w ${size} -h ${size}
                        "${ARG_SVG}" -o "${_out}"
                DEPENDS "${ARG_SVG}"
                COMMENT "Rasterizing app icon ${size}x${size} (rsvg-convert)"
                VERBATIM)
        else()
            # ImageMagick fallback. -background none preserves transparency.
            add_custom_command(
                OUTPUT "${_out}"
                COMMAND "${MAGICK_TOOL}"
                        -background none
                        "${ARG_SVG}"
                        -resize ${size}x${size}
                        "${_out}"
                DEPENDS "${ARG_SVG}"
                COMMENT "Rasterizing app icon ${size}x${size} (ImageMagick)"
                VERBATIM)
        endif()

        # Generated outputs must be flagged so the resource compiler accepts
        # them as build-time dependencies rather than expecting them on disk.
        # The alias gives each PNG a stable, predictable resource path that
        # main.cpp can load (icons/neobeam_<size>.png).
        set_source_files_properties("${_out}" PROPERTIES
            GENERATED TRUE
            QT_RESOURCE_ALIAS "icons/neobeam_${size}.png")

        list(APPEND _pngs "${_out}")
        list(APPEND _sizes "${size}")
    endforeach()

    # Compile the rasterized PNGs into the target as a Qt resource. The target
    # signature of qt6_add_resources wires the custom-command outputs as proper
    # build dependencies, so the icons are generated before RCC runs.
    qt6_add_resources(${ARG_TARGET} "neobeam_app_icons"
                      PREFIX "/"
                      FILES ${_pngs})

    set(NEOBEAM_ICON_PNGS "${_pngs}" PARENT_SCOPE)
    set(NEOBEAM_ICON_SIZES "${_sizes}" PARENT_SCOPE)
endfunction()
