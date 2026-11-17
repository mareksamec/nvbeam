#!/usr/bin/env bash
#
# Rasterize the nvbeam.svg master into the PNG size ladder in png/.
#
# The PNGs are committed so a build needs no SVG rasterizer. CMakeLists.txt
# embeds every png/nvbeam_<size>.png it finds into the executable (window icon)
# and installs it into the hicolor icon theme. Rerun this after editing the SVG
# and commit the result.
#
# Usage:
#   resources/icons/generate-pngs.sh [size ...]
#
# Without arguments it renders the default sizes below. Requires rsvg-convert
# (Arch: librsvg, Debian: librsvg2-bin, Fedora: librsvg2-tools).
set -euo pipefail

icons_dir="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
svg="$icons_dir/nvbeam.svg"
out_dir="$icons_dir/png"

default_sizes=(16 24 32 48 64 128 256)
sizes=("${@:-${default_sizes[@]}}")

if ! command -v rsvg-convert >/dev/null; then
    echo "error: rsvg-convert not found (install librsvg)" >&2
    exit 1
fi

mkdir -p "$out_dir"
for size in "${sizes[@]}"; do
    rsvg-convert -w "$size" -h "$size" "$svg" -o "$out_dir/nvbeam_${size}.png"
    echo "$out_dir/nvbeam_${size}.png"
done
