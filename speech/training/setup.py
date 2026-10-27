"""Explicit one-time setup of native alignment code and the Amy seed checkpoint."""
import hashlib
import json
import logging
from pathlib import Path
import shutil
import subprocess
import sys
from urllib.request import urlopen

from speech import ROOT

CORE_URL = "https://raw.githubusercontent.com/OHF-Voice/piper1-gpl/v1.8.0/src/piper/train/vits/monotonic_align/core.pyx"
CHECKPOINT_URL = "https://huggingface.co/datasets/rhasspy/piper-checkpoints/resolve/main/en/en_US/amy/medium/epoch%3D6679-step%3D1554200.ckpt"


def download(url: str, path: Path):
    if path.is_file() and path.stat().st_size > 0:
        return
    path.parent.mkdir(parents=True, exist_ok=True)
    part = path.with_name(path.name + ".part")
    with urlopen(url, timeout=120) as response, part.open("wb") as output:
        shutil.copyfileobj(response, output, length=1024 * 1024)
    part.replace(path)


def main():
    import piper
    alignment = Path(piper.__file__).parent / "train/vits/monotonic_align/monotonic_align"
    alignment.mkdir(parents=True, exist_ok=True)
    if not list(alignment.glob("core*.so")):
        source = ROOT / ".tmp/piper-alignment/core.pyx"
        download(CORE_URL, source)
        subprocess.run([str(Path(sys.executable).with_name("cythonize")), "-i", str(source)], cwd=source.parent, check=True)
        for library in source.parent.glob("core*.so"):
            shutil.copy2(library, alignment / library.name)
    checkpoint = ROOT / "models/checkpoints/amy-medium.ckpt"
    download(CHECKPOINT_URL, checkpoint)
    digest = hashlib.sha256()
    with checkpoint.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    checkpoint.with_suffix(".manifest.json").write_text(json.dumps({"source": CHECKPOINT_URL,
        "sha256": digest.hexdigest(), "bytes": checkpoint.stat().st_size, "piper_version": "1.8.0"}, indent=2) + "\n")
    # Check the actual single-speaker -> multi-condition warmstart without
    # running a training step or presenting random new conditioning as emotion.
    from piper.train.vits.lightning import VitsModel
    logging.basicConfig(level=logging.INFO)
    model = VitsModel(num_speakers=4, gin_channels=512, mos_metric=None)
    model._warmstart_from_ckpt(str(checkpoint))
    print(f"Training dependencies/alignment and Amy warmstart verified: {checkpoint}")
    print("No training was started. Curated recordings are required next.")


if __name__ == "__main__":
    main()
