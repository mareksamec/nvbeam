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

`nvbeam-bin` is bumped by the `publish-aur-bin` job in
`.github/workflows/release.yml` after each stable release (not for `-rc`
tags or pre-releases). It takes the `PKGBUILD` from this directory, sets
`pkgver` to the tag, pins the release asset's checksum, test-builds the
package, regenerates `.SRCINFO` and pushes both to the AUR. The copy here
keeps the version of the last manual update; edit it only to change the
recipe itself.

One-time setup for the job:

1. Generate a key used only for this: `ssh-keygen -t ed25519 -N '' -f aur_ci`.
2. Add `aur_ci.pub` to your AUR account under *My Account* → *SSH Public Key*
   (one key per line, next to your own).
3. Store the private key as the `AUR_SSH_PRIVATE_KEY` repository secret:
   `gh secret set AUR_SSH_PRIVATE_KEY < aur_ci`, then delete both files.

To bump by hand instead (e.g. to fix the recipe without a release):

```sh
cd packaging/aur/nvbeam-bin
sed -i 's/^pkgver=.*/pkgver=0.2.11/; s/^pkgrel=.*/pkgrel=1/' PKGBUILD
updpkgsums                         # pacman-contrib; downloads the asset
makepkg --printsrcinfo > .SRCINFO
```

Then copy both files into the AUR clone and push. When only the recipe
changes, bump `pkgrel` instead of `pkgver`.

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
- `qtermwidget` is a hard runtime dependency. To build `nvbeam-git` without
  the embedded terminal, set `-DNVBEAM_TERMINAL=OFF` in `build()` and drop
  `qtermwidget` from `depends`.
- `nvbeam-bin` links against the Qt6, KF6 and qtermwidget sonames of the
  Arch system the release was built on. A soname bump in one of them (e.g.
  `libqtermwidget6.so.3`) breaks the installed binary until the next release.
