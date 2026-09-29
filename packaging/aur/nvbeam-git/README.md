# nvbeam-git — AUR package

This directory contains the AUR `PKGBUILD` for the `nvbeam-git` package
(builds the latest `main` of this repository).

## Publishing to the AUR

1. Create an AUR account at <https://aur.archlinux.org/> and upload your SSH
   public key under *My Account → Edit*.
2. Clone the empty AUR repo:

   ```sh
   git clone ssh://aur@aur.archlinux.org/nvbeam-git.git
   ```

3. Copy `PKGBUILD` and `.SRCINFO` from this directory into the cloned AUR repo,
   set the `Maintainer:` line in `PKGBUILD` to your real name + email, then:

   ```sh
   makepkg --printsrcinfo > .SRCINFO   # regenerate to be safe
   git add PKGBUILD .SRCINFO
   git commit -m "Initial import: nvbeam-git"
   git push
   ```

## Local test build

From an Arch system (or `archlinux:latest` container):

```sh
cd packaging/aur/nvbeam-git
makepkg -fs --noconfirm        # builds and installs deps via pacman
namcap PKGBUILD                 # lint the recipe
namcap nvbeam-git-*.pkg.tar.zst   # lint the built package
```

## Notes

- `toml11 v4.2.0` is vendored via a second `source=()` entry; `prepare()`
  rewrites the upstream `FetchContent_Declare(toml11 ...)` to point at the
  extracted directory, so no network access is required during `build()`.
- `qtermwidget` is a hard runtime dependency. To build without the embedded
  terminal, change `-DNVBEAM_TERMINAL=ON` to `OFF` in `build()` and drop
  `qtermwidget` from `depends`.
- When you cut your first tagged release (e.g. `v0.1.0`), `pkgver()` will
  automatically pick it up and emit something like `0.1.0.r0.cf5f03b`.
- For a future versioned package (`nvbeam`, no `-git`), copy this PKGBUILD
  and replace the git `source` with a release tarball URL pinned by sha256.
