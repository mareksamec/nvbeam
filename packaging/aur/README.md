# AUR packages

| Package      | Builds                                                                 |
|--------------|------------------------------------------------------------------------|
| `nvbeam-git` | Latest `main`, compiled from source.                                   |
| `nvbeam-bin` | A tagged release, repackaged from the `nvbeam-*.pkg.tar.zst` asset the release workflow attaches to the GitHub release. Nothing is compiled. |

Both provide and conflict with `nvbeam`, so only one can be installed.

Each directory holds the `PKGBUILD` and `.SRCINFO` that go into the matching
AUR git repository. Regenerate `.SRCINFO` after every `PKGBUILD` change:

```sh
makepkg --printsrcinfo > .SRCINFO
```

## First publish

1. Create an AUR account at <https://aur.archlinux.org/> and add your SSH
   public key under *My Account*.
2. For each package, clone the (still empty) AUR repo, copy the files in and
   push:

   ```sh
   git clone ssh://aur@aur.archlinux.org/nvbeam-bin.git ~/aur/nvbeam-bin
   cp packaging/aur/nvbeam-bin/{PKGBUILD,.SRCINFO} ~/aur/nvbeam-bin/
   cd ~/aur/nvbeam-bin
   git add PKGBUILD .SRCINFO
   git commit -m "Initial import: nvbeam-bin 0.2.10"
   git push
   ```

   Same for `nvbeam-git`. The AUR only accepts pushes to the `master` branch.

## Updating

`nvbeam-git` needs no update per release: `pkgver()` derives the version
from `git describe` on every build. Push a new commit only when the recipe
itself changes (dependencies, build flags) and bump `pkgrel` then.

`nvbeam-bin` must be bumped after each stable release, once the release
workflow has uploaded the Arch package:

```sh
cd packaging/aur/nvbeam-bin
sed -i 's/^pkgver=.*/pkgver=0.2.11/; s/^pkgrel=.*/pkgrel=1/' PKGBUILD
updpkgsums                         # pacman-contrib; downloads the asset
makepkg --printsrcinfo > .SRCINFO
```

Commit that here, then copy both files into the AUR clone and push.

## Local test build

```sh
cd packaging/aur/nvbeam-git        # or nvbeam-bin
makepkg -fs                         # -s installs missing deps via pacman
namcap PKGBUILD *.pkg.tar.zst       # lint (namcap package)
```

For a build in a clean chroot, as AUR users with `devtools` get it, use
`extra-x86_64-build` instead of `makepkg`.

## Notes

- `toml11 v4.2.0` is vendored as a second `source=()` entry in `nvbeam-git`
  and passed to CMake via `FETCHCONTENT_SOURCE_DIR_TOML11`, so `build()`
  needs no network access.
- `librsvg` is a make dependency because `rsvg-convert` rasterizes the app
  icon. Without it the build still succeeds but the window has no icon and
  no PNG icons are installed.
- `qtermwidget` is a hard runtime dependency. To build `nvbeam-git` without
  the embedded terminal, set `-DNVBEAM_TERMINAL=OFF` in `build()` and drop
  `qtermwidget` from `depends`.
- `nvbeam-bin` links against the Qt6, KF6 and qtermwidget sonames of the
  Arch system the release was built on. A soname bump in one of them (e.g.
  `libqtermwidget6.so.3`) breaks the installed binary until the next release.
