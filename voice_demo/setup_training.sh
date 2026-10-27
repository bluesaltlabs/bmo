#!/usr/bin/env bash
# Separate training tools; the Pi/runtime venv remains small and unchanged.
set -euo pipefail
cd "$(dirname "$0")/.."
source speech/training/environment.sh
if [ ! -x .venv-piper-train/bin/python ]; then
  TRAIN_PYTHON="${BMO_TRAIN_PYTHON:-}"
  if [ -z "$TRAIN_PYTHON" ]; then
    for candidate in python3.12 python3.11 python3.13; do
      if command -v "$candidate" >/dev/null 2>&1; then TRAIN_PYTHON="$candidate"; break; fi
    done
  fi
  if [ -z "$TRAIN_PYTHON" ]; then echo "Set BMO_TRAIN_PYTHON to a Python 3.11–3.13 executable." >&2; exit 1; fi
  "$TRAIN_PYTHON" -m venv .venv-piper-train
fi
.venv-piper-train/bin/python -m pip install -r speech/training/requirements.txt
exec .venv-piper-train/bin/python -m speech.training.setup
