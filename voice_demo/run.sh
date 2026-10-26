#!/usr/bin/env bash
# Build (if needed) and run the BMO voice demo on this computer. Arguments are passed to bmo_say.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p .tmp/voice
LIB=lib/bmo_voice/src
SOURCE=voice_demo/bmo_say.cpp
BIN=.tmp/voice/bmo_say
if [ ! -x "$BIN" ] || [ "$SOURCE" -nt "$BIN" ] || [ -n "$(find "$LIB" -type f -newer "$BIN" -print -quit 2>/dev/null)" ]; then
  echo "Building BMO voice demo from the shared engine..."
  c++ -std=c++17 -O2 -Wall -Wextra -I"$LIB" "$SOURCE" "$LIB"/bmo_voice.cpp "$LIB"/bmo_g2p.cpp -o "$BIN"
fi
exec "$BIN" "$@"
