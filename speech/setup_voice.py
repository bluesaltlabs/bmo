"""Explicit one-time network setup. Runtime synthesis never calls this module."""
import argparse
from datetime import datetime, timezone
import hashlib
from importlib.metadata import version
import json
import os
from pathlib import Path
import shutil
import tempfile
from urllib.request import urlopen

from . import DEFAULT_VOICE, MODEL_DIR, ROOT
from .backend import voice_path


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--voice", default=os.environ.get("BMO_PIPER_VOICE", DEFAULT_VOICE))
    parser.add_argument("--force", action="store_true", help="replace the downloaded model/config")
    args = parser.parse_args()
    model = voice_path(args.voice)
    config = Path(str(model) + ".json")
    MODEL_DIR.mkdir(parents=True, exist_ok=True)
    if model.is_file() and model.stat().st_size > 0 and config.is_file() and not args.force:
        json.loads(config.read_text())
        print(f"Using existing local voice: {model}")
        return
    from piper.download_voices import download_voice
    language, name, quality = args.voice.split("-", 2)
    source = f"https://huggingface.co/rhasspy/piper-voices/resolve/main/{language.split('_')[0]}/{language}/{name}/{quality}"
    scratch = ROOT / ".tmp"
    scratch.mkdir(exist_ok=True)
    # Interrupted downloads stay in scratch, never at the runtime model path.
    with tempfile.TemporaryDirectory(prefix="piper-download-", dir=scratch) as temporary:
        stage = Path(temporary)
        download_voice(args.voice, stage)
        downloaded_config = json.loads((stage / config.name).read_text())
        with urlopen(f"{source}/MODEL_CARD", timeout=60) as response:
            (stage / f"{args.voice}.MODEL_CARD").write_bytes(response.read())
        files = {path.name: {"sha256": hashlib.sha256(path.read_bytes()).hexdigest(), "bytes": path.stat().st_size}
                 for path in stage.iterdir()}
        manifest = {"voice": args.voice, "downloaded_at": datetime.now(timezone.utc).isoformat(),
                    "piper_version": version("piper-tts"), "source": source,
                    "sample_rate": downloaded_config["audio"]["sample_rate"], "files": files}
        (stage / f"{args.voice}.manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        for path in stage.iterdir():
            shutil.move(str(path), str(MODEL_DIR / path.name))
    print(f"Installed local voice: {model}")


if __name__ == "__main__":
    main()
