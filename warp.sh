#!/bin/bash
# TOTVS WARP — 16-bit style launcher. Builds the raylib menu on first run (and
# rebuilds when src/ or CMakeLists.txt changed), then execs it. Works through
# a symlink (e.g. ~/.local/bin/warp -> this script).
set -eu

DIR="$(cd "$(dirname "$(readlink -f "${BASH_SOURCE[0]}")")" && pwd)"
BIN="$DIR/build/warp-menu"

if [[ ! -x "$BIN" ]]; then
	echo "Building warp-menu (first run, fetches raylib)..."
	cmake -S "$DIR" -B "$DIR/build" -DCMAKE_BUILD_TYPE=Release
	cmake --build "$DIR/build" -j"$(nproc)"
elif [[ -n "$(find "$DIR/src" "$DIR/CMakeLists.txt" -newer "$BIN" -print -quit)" ]]; then
	echo "Sources changed, rebuilding warp-menu..."
	cmake --build "$DIR/build" -j"$(nproc)" >/dev/null
fi

exec "$BIN"
