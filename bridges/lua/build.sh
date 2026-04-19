#!/bin/sh
# Copyright 2025-2026 Vivian Voss. Licensed under the BSD 3-Clause License.
# SPDX-License-Identifier: BSD-3-Clause
# Scope: Build the TRIC+ Lua loadable module (tric.so / tric.dylib).

set -eu

LUA_PREFIX="${LUA_PREFIX:-/opt/homebrew/opt/lua}"
LUA_INCLUDE="$LUA_PREFIX/include/lua"
LUA_LIB="$LUA_PREFIX/lib"
BRIDGES_C="$(cd "$(dirname "$0")/../c" && pwd)"
HERE="$(cd "$(dirname "$0")" && pwd)"

case "$(uname -s)" in
    Darwin) EXT=dylib; SHARED_FLAGS="-bundle -undefined dynamic_lookup" ;;
    *)      EXT=so;    SHARED_FLAGS="-shared" ;;
esac

cc -std=c11 -Wall -Wextra -Wpedantic -Werror -O2 -fPIC \
    -I"$LUA_INCLUDE" -I"$BRIDGES_C" \
    $SHARED_FLAGS \
    -o "$HERE/tric.$EXT" \
    "$HERE/tric.c" "$BRIDGES_C/tric.c" \
    -L"$LUA_LIB"

echo "built $HERE/tric.$EXT"
