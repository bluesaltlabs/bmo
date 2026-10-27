#!/usr/bin/env bash
# Explicit standalone/formant backup, built from the shared ESP32 engine.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p .tmp/voice .tmp/compiler-cache
export TMPDIR="$PWD/.tmp"
export CLANG_MODULE_CACHE_PATH="$PWD/.tmp/compiler-cache"
LIB=lib/bmo_voice/src
SOURCE=voice_demo/bmo_say.cpp
BIN=.tmp/voice/bmo_say
if [ ! -x "$BIN" ] || [ "$SOURCE" -nt "$BIN" ] || [ -n "$(find "$LIB" -type f -newer "$BIN" -print -quit 2>/dev/null)" ]; then
  echo "Building the standalone formant backup..."
  c++ -std=c++17 -O2 -Wall -Wextra -I"$LIB" "$SOURCE" "$LIB"/bmo_voice.cpp "$LIB"/bmo_g2p.cpp -o "$BIN"
fi
exec "$BIN" "$@"
