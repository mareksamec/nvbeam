#!/usr/bin/env bash
#
# Reproduce the change review panel freeze (tasks/task-013): a scratch git
# repository whose files keep changing, the way a coding agent writes them.
#
# The script creates the repository, prints its path and then edits it in a
# loop until Ctrl+C:
#   - stress.cpp, tracked: every 50th line is modified once, then lines are
#     appended on every tick, so its diff has many hunks and keeps growing
#   - new.cpp, untracked: grows on every tick
#
# Open the printed directory in nvbeam with the review traces on, expand
# stress.cpp in the review panel (Ctrl+Shift+R) and watch the timings:
#   QT_LOGGING_RULES="nvbeam.review.debug=true" QT_FORCE_STDERR_LOGGING=1 nvbeam <dir>
#
# Usage:
#   tests/manual/review-stress.sh [options]
#
# Options:
#   -l, --lines N      Lines in the tracked file to start with (default: 3000)
#   -a, --append N     Lines appended to each file per tick (default: 20)
#   -i, --interval S   Seconds between ticks (default: 0.1)
#   -d, --dir DIR      Repository directory (default: a new mktemp directory)
#   -h, --help         Show this help
set -euo pipefail

lines=3000
append=20
interval=0.1
dir=""

usage() {
    sed -n '3,25p' "${BASH_SOURCE[0]}" | sed 's/^# \{0,1\}//'
}

while [[ $# -gt 0 ]]; do
    case "$1" in
        -l|--lines)    lines="$2"; shift 2 ;;
        -a|--append)   append="$2"; shift 2 ;;
        -i|--interval) interval="$2"; shift 2 ;;
        -d|--dir)      dir="$2"; shift 2 ;;
        -h|--help)     usage; exit 0 ;;
        *)             echo "error: unknown option: $1" >&2; usage >&2; exit 1 ;;
    esac
done

if [[ -z "$dir" ]]; then
    dir="$(mktemp -d -t nvbeam-review-stress.XXXXXX)"
elif [[ -e "$dir/.git" ]]; then
    echo "error: $dir is already a git repository; pick an empty directory" >&2
    exit 1
fi
mkdir -p "$dir"
cd "$dir"

# C++ so the syntax highlighter has a real language to lex.
code_line() {
    printf 'int value_%d = compute(%d, "line %d"); // filler comment for width\n' "$1" "$1" "$1"
}

git init -q
for ((n = 1; n <= lines; n++)); do code_line "$n"; done > stress.cpp
git add stress.cpp
git -c user.name=stress -c user.email=stress@localhost commit -q -m "base"

# One-off edits spread through the file: many hunks, not one block at the end.
sed -i '0~50s/compute/recompute/' stress.cpp

echo "Repository: $dir"
echo "Editing stress.cpp and new.cpp every ${interval}s, Ctrl+C to stop."

next=$((lines + 1))
while true; do
    for ((n = 0; n < append; n++)); do
        code_line "$next" >> stress.cpp
        code_line "$next" >> new.cpp
        next=$((next + 1))
    done
    sleep "$interval"
done
