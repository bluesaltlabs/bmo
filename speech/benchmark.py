"""Measure CPU speech throughput on the deployment machine, including the Pi."""
import argparse
import json
import math
from pathlib import Path
import platform
import resource
import time

from . import ROOT
from .backend import PiperEngine

PHRASES = (
    "Hello! I am BMO. Do you want to play a game with me?",
    "Yay! We did it! That was a very good adventure.",
    "Oh no. Something is not working. Let us try it another way.",
)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--model", type=Path)
    parser.add_argument("--out", type=Path, default=ROOT / ".tmp/voice/pi-benchmark.json")
    parser.add_argument("--require-pi", action="store_true", help="refuse to count a Mac/PC run as a Pi benchmark")
    parser.add_argument("--max-rtf", type=float, default=1, help="maximum warm render/audio duration ratio")
    args = parser.parse_args()
    if not math.isfinite(args.max_rtf) or args.max_rtf <= 0:
        parser.error("--max-rtf must be finite and positive")
    device = Path("/proc/device-tree/model")
    board = device.read_text().strip("\x00\n") if device.exists() else platform.platform()
    is_pi = "Raspberry Pi" in board
    if args.require_pi and not is_pi:
        parser.error("This is not a Raspberry Pi. Run this command on the target Pi to validate performance.")
    if not args.out.resolve().is_relative_to(ROOT):
        parser.error("keep benchmark output inside the repository")
    started = time.perf_counter()
    engine = PiperEngine(args.model)
    load_s = time.perf_counter() - started
    engine.synthesize("Beemo is ready.")
    rows = []
    for emotion in (list(engine.emotions) or [None]):
        for text in PHRASES:
            started = time.perf_counter()
            audio = engine.synthesize(text, emotion=emotion, rate=16000)
            render_s = time.perf_counter() - started
            audio_s = len(audio.samples) / audio.sample_rate
            rows.append({"text": text, "emotion": emotion, "render_s": render_s, "audio_s": audio_s,
                         "rtf": render_s / audio_s if audio_s else None})
    peak = resource.getrusage(resource.RUSAGE_SELF).ru_maxrss
    peak_mb = peak / (1024 * 1024) if platform.system() == "Darwin" else peak / 1024
    passed = all(row["rtf"] is not None and row["rtf"] <= args.max_rtf for row in rows)
    result = {"board": board, "is_pi": is_pi, "model": str(engine.model), "load_s": load_s,
              "peak_process_rss_mb": peak_mb, "max_rtf": args.max_rtf, "throughput_passed": passed,
              "pi_performance_validated": is_pi and passed, "utterances": rows}
    args.out.parent.mkdir(parents=True, exist_ok=True)
    args.out.write_text(json.dumps(result, indent=2) + "\n")
    print(json.dumps(result, indent=2))
    return 0 if passed else 1


if __name__ == "__main__":
    raise SystemExit(main())
