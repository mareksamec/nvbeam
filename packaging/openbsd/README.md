# OpenBSD port skeleton

Skeleton for an `editors/neobeam` port. Not buildable in-place — OpenBSD
ports build inside the ports tree:

```sh
cp -R packaging/openbsd /usr/ports/editors/neobeam
cd /usr/ports/editors/neobeam
make makesum          # fetch distfiles, generate distinfo (not committed here)
make build
make port-lib-depends-check   # paste suggested WANTLIB into the Makefile
make update-plist     # regenerate pkg/PLIST if install rules changed
make package
```

Notes:

- The repo is currently private, so `make makesum` cannot fetch the source
  from GitHub. Until it is public, place the release tarball (from
  `git archive`, prefix `neobeam-<version>/`) into `/usr/ports/distfiles/`
  manually.
- toml11 is normally pulled by CMake FetchContent at configure time, which is
  not allowed in ports builds. The Makefile ships it as a second distfile and
  points `FETCHCONTENT_SOURCE_DIR_TOML11` at the extracted tree.
- Built with `-DNEOBEAM_TERMINAL=OFF`: the qtermwidget port is Qt5-based and
  neobeam's terminal panel needs qtermwidget6.
- Bump `GH_TAGNAME` on each release and re-run `make makesum`.
