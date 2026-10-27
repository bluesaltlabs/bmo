#!/usr/bin/env bash
# Piper is the preferred host/Pi voice; --engine formant selects the ESP32 backup.
set -euo pipefail
cd "$(dirname "$0")/.."
mkdir -p .tmp/voice
export TMPDIR="$PWD/.tmp"
export ORT_DISABLE_TELEMETRY=1
ENGINE="${BMO_VOICE_ENGINE:-piper}"
ARGS=()
while [ "$#" -gt 0 ]; do
  case "$1" in
    --engine)
      if [ "$#" -lt 2 ]; then echo "--engine requires piper or formant" >&2; exit 2; fi
      ENGINE="$2"; shift 2 ;;
    --engine=*) ENGINE="${1#--engine=}"; shift ;;
    --) ARGS+=("$@"); break ;;
    *) ARGS+=("$1"); shift ;;
  esac
done
# Bash 3.2 treats an empty array as unset under nounset. Positional arguments
# preserve the zero-argument demo and quoted text on the system macOS shell.
set --
if [ "${#ARGS[@]}" -gt 0 ]; then set -- "${ARGS[@]}"; fi
case "$ENGINE" in
  piper)
    if [ ! -x .venv-piper/bin/python ]; then
      echo "Piper is not installed. Run voice_demo/setup_piper.sh, or select --engine formant." >&2
      exit 1
    fi
    exec .venv-piper/bin/python -m speech "$@" ;;
  formant) exec bash voice_demo/run_formant.sh "$@" ;;
  *) echo "Unknown engine '$ENGINE'; use piper or formant." >&2; exit 2 ;;
esac
