#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/.."
source speech/training/environment.sh
if [ ! -x .venv-piper-train/bin/python ]; then echo "Run voice_demo/setup_training.sh first." >&2; exit 1; fi
exec .venv-piper-train/bin/python -m speech.training.export "$@"
