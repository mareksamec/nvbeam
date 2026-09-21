COMMENT =	fast native Neovim GUI client built with Qt6

GH_ACCOUNT =	mareksamec
GH_PROJECT =	neobeam
GH_TAGNAME =	v0.1.0

CATEGORIES =	editors x11

HOMEPAGE =	https://github.com/mareksamec/neobeam
MAINTAINER =	Marek Samec <marek@samec.me>

# MIT
PERMIT_PACKAGE =	Yes

# CMake fetches toml11 (header-only) via FetchContent at configure time;
# ports builds have no network access, so ship it as a second distfile and
# point FetchContent at the extracted tree below.
TOML11_V =	4.2.0
DISTFILES.toml11 =	toml11-{v}${TOML11_V}.tar.gz
SITES.toml11 =	https://github.com/ToruNiina/toml11/archive/refs/tags/

# Run `make port-lib-depends-check` after the first build and paste the
# suggested WANTLIB here.
WANTLIB +=	Qt6Core Qt6Gui Qt6Widgets c m ${COMPILER_LIBCXX}

MODULES =	devel/cmake

LIB_DEPENDS =	x11/qt6/qtbase

# rsvg-convert rasterizes the app icon at build time (cmake/AppIcons.cmake)
BUILD_DEPENDS =	x11/gnome/librsvg

RUN_DEPENDS =	editors/neovim \
		devel/desktop-file-utils

# qtermwidget in ports is Qt5-based; the embedded terminal needs qtermwidget6.
# KF6SyntaxHighlighting is not in ports either, so the review diffs keep their
# own added/removed coloring instead of per-language code coloring.
CONFIGURE_ARGS =	-DNEOBEAM_TERMINAL=OFF \
		-DNEOBEAM_SYNTAX=OFF \
		-DFETCHCONTENT_FULLY_DISCONNECTED=ON \
		-DFETCHCONTENT_SOURCE_DIR_TOML11=${WRKDIR}/toml11-${TOML11_V}

.include <bsd.port.mk>
