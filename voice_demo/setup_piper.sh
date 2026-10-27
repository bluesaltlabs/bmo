#!/usr/bin/env bash
# One-time Piper installation/download, isolated from the PlatformIO venv.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p .tmp/piper-cache models/piper
export TMPDIR="$PWD/.tmp"
export PIP_CACHE_DIR="$PWD/.tmp/piper-cache"
export PIP_DISABLE_PIP_VERSION_CHECK=1
export ORT_DISABLE_TELEMETRY=1
if [ ! -x .venv-piper/bin/python ]; then
  PIPER_PYTHON="${BMO_PIPER_PYTHON:-}"
  if [ -z "$PIPER_PYTHON" ]; then
    for candidate in python3.13 python3.12 python3.11 python3; do
      if command -v "$candidate" >/dev/null 2>&1; then
        PIPER_PYTHON="$candidate"
        break
      fi
    done
  fi
  if [ -z "$PIPER_PYTHON" ]; then
    echo "Install Python 3.11–3.13, or set BMO_PIPER_PYTHON to its executable." >&2
    exit 1
  fi
  "$PIPER_PYTHON" -c 'import sys,struct; sys.exit(0 if (3,11) <= sys.version_info[:2] <= (3,13) and struct.calcsize("P") == 8 else "Piper setup needs 64-bit Python 3.11–3.13 (64-bit Raspberry Pi OS on the Pi). Set BMO_PIPER_PYTHON to a supported interpreter.")'
  "$PIPER_PYTHON" -m venv .venv-piper
fi
.venv-piper/bin/python -m pip install -r speech/requirements.txt
exec .venv-piper/bin/python -m speech.setup_voice "$@"
