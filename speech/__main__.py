"""Local Piper demo on macOS or Linux. Run through voice_demo/run.sh."""
import argparse
import os
from pathlib import Path
import shutil
import subprocess
import sys
import time

from . import DEFAULT_VOICE, ROOT
from .backend import PiperEngine, voice_path
from .delivery import DELIVERY

DEMO = (
    "Hello! I am BMO.",
    "Do you want to play a game with me?",
    "Oh no! Something is not working. Can you help me fix it?",
    "Good morning! You have 3 new messages and 12 things to do today.",
    "Beep boop. Battery is low, please feed me electricity.",
)


def play(path: Path) -> None:
    player = "afplay" if sys.platform == "darwin" else "aplay"
    if not shutil.which(player):
        raise RuntimeError(f"{player} is unavailable. Use --wav output.wav to save audio.")
    subprocess.run([player, str(path)], check=True)


def main() -> int:
    parser = argparse.ArgumentParser(description="Offline Piper speech (CPU, local model).")
    parser.add_argument("text", nargs="*")
    parser.add_argument("-i", "--interactive", action="store_true")
    parser.add_argument("--stdin", action="store_true", help="read text from standard input")
    output = parser.add_mutually_exclusive_group()
    output.add_argument("--wav", type=Path, help="write a WAV without playback")
    output.add_argument("--raw", type=Path, help="write mono signed 16-bit little-endian PCM (16 kHz by default)")
    model = parser.add_mutually_exclusive_group()
    model.add_argument("--voice", help=f"local voice ID (default: {DEFAULT_VOICE}; BMO_PIPER_VOICE overrides)")
    model.add_argument("--model", type=Path, help="local ONNX model; its .onnx.json must be alongside it")
    parser.add_argument("--speed", type=float, default=1)
    parser.add_argument("--volume", type=float, default=0.85)
    parser.add_argument("--style", choices=DELIVERY, default=os.environ.get("BMO_SPEECH_STYLE", "neutral"),
                        help="delivery pacing preset; these are not trained emotion labels")
    conditioning = parser.add_mutually_exclusive_group()
    conditioning.add_argument("--speaker", type=int, help="speaker ID for a multi-speaker model")
    conditioning.add_argument("--emotion", help="trained emotion name; requires a model with emotion:<name> speaker IDs")
    parser.add_argument("--list-emotions", action="store_true", help="show the selected model's trained emotion names")
    parser.add_argument("--rate", type=int, help="output sample rate; WAV/playback defaults to the model's native rate")
    args = parser.parse_args()
    if sum((bool(args.text), args.stdin, args.interactive)) > 1:
        parser.error("choose text arguments, --stdin, or -i")
    try:
        path = args.model or voice_path(args.voice or os.environ.get("BMO_PIPER_VOICE", DEFAULT_VOICE))
        started = time.monotonic()
        engine = PiperEngine(path)
        voice_label = "Jenny (Dioco)" if path.stem == "en_GB-jenny_dioco-medium" else path.stem
        print(f"Piper: {voice_label}, {args.style} delivery, CPU, {engine.sample_rate} Hz (loaded in {time.monotonic() - started:.2f}s)", file=sys.stderr)
        if args.list_emotions:
            print("Trained emotions: " + (", ".join(engine.emotions) or "none (this voice has no emotion conditioning)"))
            return 0
        destination = args.wav or args.raw or ROOT / ".tmp/voice/piper_say.wav"
        rate = args.rate if args.rate is not None else (16000 if args.raw else None)

        def say(text: str) -> None:
            started = time.monotonic()
            audio = engine.synthesize(text, speed=args.speed, volume=args.volume, speaker=args.speaker, rate=rate,
                                      style=args.style, emotion=args.emotion)
            if args.raw:
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.write_bytes(audio.pcm16())
            else:
                audio.write_wav(destination)
            seconds = len(audio.samples) / audio.sample_rate
            print(f"BMO (Piper): {text}")
            print(f"wrote {destination} ({seconds:.2f}s, {audio.sample_rate} Hz; synthesized in {time.monotonic() - started:.2f}s)")
            if not (args.wav or args.raw):
                play(destination)

        if args.interactive:
            print("Type a line and press enter (empty line quits). Model stays loaded.")
            while True:
                try:
                    line = input("> ")
                except EOFError:
                    break
                if not line.strip():
                    break
                say(line)
        elif args.stdin:
            say(sys.stdin.read())
        elif args.text:
            say(" ".join(args.text))
        elif args.wav or args.raw:
            say(" ".join(DEMO))
        else:
            for text in DEMO:
                say(text)
        return 0
    except (OSError, ValueError, RuntimeError, ImportError, subprocess.CalledProcessError) as error:
        print(f"Piper error: {error}", file=sys.stderr)
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
