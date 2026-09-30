#!/usr/bin/env bash
#
# Build the Arch package locally from the current checkout.
#
# Mirrors what .github/workflows/release.yml does: stamps pkgver into a copy of
# PKGBUILD, creates the source tarball with `git archive`, and runs makepkg in
# a temporary directory so the repo's PKGBUILD keeps its 0.0.0 placeholder.
# The finished *.pkg.tar.zst is copied next to this script.
#
# Usage:
#   packaging/arch/build-local.sh [options] [-- extra makepkg args]
#
# Options:
#   -v, --version VER   Package version (default: project() version in CMakeLists.txt)
#   -r, --ref REF       Git ref to package (default: HEAD)
#   -d, --dirty         Include uncommitted changes to tracked files
#   -i, --install       Install the built package (makepkg -i)
#   -k, --keep          Keep the temporary build directory
#   -h, --help          Show this help
set -euo pipefail

script_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
repo_root="$(git -C "$script_dir" rev-parse --show-toplevel)"

# Defaults, overridable by the options above.
version=""
ref="HEAD"
dirty=false
install=false
keep=false
makepkg_extra=()

usage() {
    sed -n '3,19p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -v|--version) version="$2"; shift 2 ;;
        -r|--ref)     ref="$2"; shift 2 ;;
        -d|--dirty)   dirty=true; shift ;;
        -i|--install) install=true; shift ;;
        -k|--keep)    keep=true; shift ;;
        -h|--help)    usage; exit 0 ;;
        --)           shift; makepkg_extra=("$@"); break ;;
        *)            echo "error: unknown option: $1" >&2; usage >&2; exit 1 ;;
    esac
done

# Read the version from `project(nvbeam VERSION x.y.z ...)` when not given.
if [[ -z "$version" ]]; then
    version="$(sed -n 's/^project(nvbeam VERSION \([^ )]*\).*/\1/p' "$repo_root/CMakeLists.txt")"
    if [[ -z "$version" ]]; then
        echo "error: could not read version from CMakeLists.txt, pass --version" >&2
        exit 1
    fi
fi
# makepkg forbids hyphens in pkgver ("0.3.0-rc1" -> "0.3.0_rc1").
pkgver="${version//-/_}"

# `git stash create` snapshots tracked changes as a commit without touching
# the working tree or the stash list. It prints nothing when the tree is clean.
if $dirty; then
    snapshot="$(git -C "$repo_root" stash create)"
    ref="${snapshot:-$ref}"
elif ! git -C "$repo_root" diff --quiet HEAD; then
    echo "warning: uncommitted changes are NOT packaged (use --dirty to include them)" >&2
fi

work_dir="$(mktemp -d -t nvbeam-pkg.XXXXXX)"
cleanup() {
    if $keep; then
        echo "Build directory kept: $work_dir"
    else
        rm -rf "$work_dir"
    fi
}
trap cleanup EXIT

echo "Packaging nvbeam ${pkgver} from ${ref} in ${work_dir}"

cp "$script_dir/PKGBUILD" "$work_dir/"
sed -i "s/^pkgver=.*/pkgver=${pkgver}/" "$work_dir/PKGBUILD"

git -C "$repo_root" archive --format=tar.gz \
    --prefix="nvbeam-${pkgver}/" \
    -o "$work_dir/nvbeam-${pkgver}.tar.gz" "$ref"

makepkg_args=(--syncdeps --cleanbuild)
$install && makepkg_args+=(--install)

(cd "$work_dir" && makepkg "${makepkg_args[@]}" "${makepkg_extra[@]}")

cp "$work_dir"/*.pkg.tar.zst "$script_dir/"
echo "Package written to:"
ls -1 "$script_dir"/*.pkg.tar.zst
