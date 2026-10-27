#!/usr/bin/env bash
# Run only an explicitly prepared dataset/config; never start from empty data.
set -euo pipefail
cd "$(dirname "$0")/.."
source speech/training/environment.sh
if [ "$#" -lt 1 ]; then echo "Usage: voice_demo/train_piper.sh prepared/train_config.json [Piper/Lightning overrides]" >&2; exit 2; fi
if [ ! -x .venv-piper-train/bin/python ]; then echo "Run voice_demo/setup_training.sh first." >&2; exit 1; fi
TRAIN_CONFIG="$1"
shift
.venv-piper-train/bin/python -c 'import json,sys; from pathlib import Path; p=Path(sys.argv[1]); config=json.loads(p.read_text()); report=json.loads((p.parent/"report.json").read_text()); assert report["ready_for_pilot"], "Corpus has not passed preparation"; assert Path(config["model"]["warmstart_ckpt"]).is_file(), "Training checkpoint is missing"' "$TRAIN_CONFIG"
exec .venv-piper-train/bin/python -m piper.train fit --config "$TRAIN_CONFIG" "$@"
