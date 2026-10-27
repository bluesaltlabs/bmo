"""Validate curated recordings and prepare a normal multi-condition Piper dataset.

Speaker IDs carry emotion labels for the same voice. No training or downloading
is performed by this command. Song/vocalization assets stay outside speech data.
"""
import argparse
from collections import Counter, defaultdict
import csv
import hashlib
import json
from pathlib import Path
import re

import numpy as np
from scipy.io import wavfile

from speech import ROOT
from speech.backend import Audio, voice_path
from speech.text import spoken_text

FIELDS = ("id", "audio", "text", "emotion", "split", "source_group", "kind", "reviewed", "source")


def read_audio(path: Path) -> Audio:
    rate, samples = wavfile.read(path)
    if samples.dtype == np.uint8:
        samples = (samples.astype(np.float32) - 128) / 128
    elif samples.dtype.kind == "i":
        samples = samples.astype(np.float32) / (2 ** (8 * samples.dtype.itemsize - 1))
    else:
        samples = samples.astype(np.float32)
    if samples.ndim == 2:
        samples = samples.mean(axis=1)
    return Audio(samples, rate)


def audit(manifest: Path, minimum_train: int = 20, minimum_holdout: int = 3):
    errors, warnings, rows = [], [], []
    counts = defaultdict(Counter)
    durations = defaultdict(float)
    seen_ids, hashes, groups = set(), {}, {}
    excluded = Counter()
    with manifest.open(newline="", encoding="utf-8") as source:
        reader = csv.DictReader(source, delimiter="|")
        if not set(FIELDS).issubset(reader.fieldnames or []):
            raise ValueError("Corpus header must be: " + "|".join(FIELDS))
        for line, raw in enumerate(reader, 2):
            row = {key: (raw.get(key) or "").strip() for key in FIELDS}
            prefix = f"line {line} ({row['id'] or 'no id'}): "
            if not re.fullmatch(r"[A-Za-z0-9_-]+", row["id"]) or row["id"] in seen_ids:
                errors.append(prefix + "invalid or duplicate clip ID")
                continue
            seen_ids.add(row["id"])
            if row["kind"] in ("song", "vocalization"):
                excluded[row["kind"]] += 1
                continue
            if row["kind"] != "speech":
                errors.append(prefix + "kind must be speech, song, or vocalization")
                continue
            if row["reviewed"].lower() != "yes":
                errors.append(prefix + "needs human review of transcript, voice, emotion and background audio")
                continue
            if (not row["text"] or not row["source_group"] or not row["source"]
                    or row["split"] not in ("train", "holdout")
                    or not re.fullmatch(r"[a-z][a-z0-9_]*", row["emotion"])
                    or row["emotion"] in ("singing", "song")):
                errors.append(prefix + "missing/invalid text, emotion, source, source_group or split")
                continue
            path = (manifest.parent / row["audio"]).resolve()
            try:
                audio = read_audio(path)
            except (OSError, ValueError) as error:
                errors.append(prefix + f"unreadable WAV: {error}")
                continue
            seconds = len(audio.samples) / audio.sample_rate
            if not 0.5 <= seconds <= 15:
                errors.append(prefix + "speech clip must be 0.5–15 seconds; split longer recordings")
                continue
            if (not np.isfinite(audio.samples).all() or abs(audio.samples).max() > 1.001
                    or np.sqrt(np.mean(audio.samples ** 2)) < 0.0001):
                errors.append(prefix + "non-finite, out-of-range, or silent audio")
                continue
            if float(np.mean(abs(audio.samples) >= 0.999)) > 0.01:
                errors.append(prefix + "more than 1% rail samples; review clipping")
                continue
            digest = hashlib.sha256(path.read_bytes()).hexdigest()
            if digest in hashes:
                errors.append(prefix + f"duplicate recording of {hashes[digest]}")
                continue
            hashes[digest] = row["id"]
            prior = groups.setdefault(row["source_group"], row["split"])
            if prior != row["split"]:
                errors.append(prefix + "source_group crosses train/holdout; keep each source group in one split")
                continue
            row.update(path=path, seconds=seconds, sha256=digest)
            rows.append(row)
            counts[row["emotion"]][row["split"]] += 1
            durations[row["emotion"]] += seconds
    if not rows:
        errors.append("No reviewed speech recordings supplied. The reference monologue is not a curated training corpus.")
    if "neutral" not in counts or len(counts) < 2:
        errors.append("Need neutral plus at least one other recorded emotion; labels alone cannot create expression.")
    for emotion, count in sorted(counts.items()):
        if count["train"] < minimum_train or count["holdout"] < minimum_holdout:
            errors.append(f"{emotion}: pilot gate needs at least {minimum_train} train and {minimum_holdout} holdout clips")
    if sum(durations.values()) < 1800:
        warnings.append("Less than 30 minutes of curated speech: treat this as a limited pilot, not evidence of voice quality.")
    report = {"ready_for_pilot": not errors, "speech_clips": len(rows), "speech_minutes": sum(durations.values()) / 60,
              "emotions": {key: dict(value) for key, value in counts.items()}, "excluded_assets": dict(excluded),
              "errors": errors, "warnings": warnings}
    return report, rows


def prepare(manifest: Path, output: Path, checkpoint: Path, *, minimum_train: int = 20, minimum_holdout: int = 3):
    if not output.resolve().is_relative_to(ROOT):
        raise ValueError("Keep prepared data, caches and training output inside the repository")
    report, rows = audit(manifest, minimum_train, minimum_holdout)
    if not report["ready_for_pilot"]:
        raise ValueError("Corpus is not ready: " + "; ".join(report["errors"]))
    if not checkpoint.is_file() or checkpoint.suffix != ".ckpt":
        raise ValueError("Provide an actual Piper training .ckpt; the Amy .onnx inference model is not a training checkpoint")
    if output.exists() and any(output.iterdir()):
        raise ValueError("Output directory is not empty; choose a new run directory to preserve existing work")
    output = output.resolve()
    audio_dir = output / "audio"
    audio_dir.mkdir(parents=True, exist_ok=True)
    labels = ["neutral"] + sorted({row["emotion"] for row in rows} - {"neutral"})
    # Piper assigns speaker IDs by first appearance in training CSV. Emit in
    # this order and use the actual exported JSON mapping during inference.
    rows.sort(key=lambda row: (labels.index(row["emotion"]), row["id"]))
    for split in ("train", "holdout"):
        with (output / f"{split}.csv").open("w", newline="", encoding="utf-8") as target:
            writer = csv.writer(target, delimiter="|")
            for row in rows:
                if row["split"] != split:
                    continue
                read_audio(row["path"]).resampled(22050).write_wav(audio_dir / f"{row['id']}.wav")
                writer.writerow((f"{row['id']}.wav", f"emotion:{row['emotion']}", spoken_text(row["text"])))
    seed_config = json.loads(Path(str(voice_path()) + ".json").read_text())
    (output / "phonemes.json").write_text(json.dumps(seed_config["phoneme_id_map"], ensure_ascii=False))
    config = {
        "seed_everything": 42,
        "model": {"sample_rate": 22050, "num_speakers": len(labels), "gin_channels": 512,
                  "warmstart_ckpt": str(checkpoint.resolve()), "mos_metric": None},
        "data": {"voice_name": "bmo_expressive", "csv_path": str(output / "train.csv"),
                 "audio_dir": str(audio_dir), "espeak_voice": "en-us", "cache_dir": str(output / "cache"),
                 "config_path": str(output / "voice.onnx.json"), "phonemes_path": str(output / "phonemes.json"),
                 "num_symbols": seed_config["num_symbols"], "batch_size": 16, "num_workers": 0,
                 "validation_split": 0.1, "trim_silence": False},
        "trainer": {"accelerator": "cpu", "devices": 1, "precision": "32-true", "max_steps": 5000,
                    "default_root_dir": str(output / "runs"), "logger": False},
    }
    (output / "train_config.json").write_text(json.dumps(config, indent=2) + "\n")
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    records = [{key: str(value) if isinstance(value, Path) else value for key, value in row.items()} for row in rows]
    (output / "curation.json").write_text(json.dumps(records, indent=2) + "\n")
    return report


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--manifest", type=Path, default=Path(__file__).with_name("corpus.csv"))
    parser.add_argument("--out", type=Path, default=ROOT / ".tmp/voice/training/prepared")
    parser.add_argument("--checkpoint", type=Path)
    parser.add_argument("--check", action="store_true", help="audit only; do not convert audio or create a training run")
    args = parser.parse_args()
    try:
        if args.check:
            report, _ = audit(args.manifest)
        else:
            if args.checkpoint is None:
                parser.error("--checkpoint is required when preparing a training run")
            report = prepare(args.manifest, args.out, args.checkpoint)
        print(json.dumps(report, indent=2))
        return 0 if report["ready_for_pilot"] else 2
    except (OSError, ValueError) as error:
        parser.exit(2, f"Dataset preparation error: {error}\n")


if __name__ == "__main__":
    raise SystemExit(main())
