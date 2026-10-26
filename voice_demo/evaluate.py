#!/usr/bin/env python3
"""Render a repeatable local A/B comparison. Requires repo-venv numpy/scipy.

Acoustic diagnostics are not a voice identity or perceptual similarity score.
"""
import argparse
import html
import hashlib
import json
import math
from pathlib import Path
import subprocess

import numpy as np
from scipy.io import wavfile
from scipy.signal import resample_poly, find_peaks

ROOT = Path(__file__).resolve().parents[1]
PHRASES = [
    "If I was grown, I would know everything.",
    "Hello! I am BMO.",
    "Who wants to play video games?",
    "I am still waking up. Please wait.",
    "Oh no! Something is not working. Can you help me fix it?",
    "Good morning! You have 3 new messages and 12 things to do today.",
]


def read_audio(path):
    rate, samples = wavfile.read(path)
    if samples.dtype.kind in "iu":
        info = np.iinfo(samples.dtype)
        samples = (samples.astype(float) - (128 if info.min == 0 else 0)) / (info.max + 1)
    else:
        samples = samples.astype(float)
    if samples.ndim == 2:
        samples = samples.mean(axis=1)
    divisor = math.gcd(rate, 16000)
    return resample_poly(samples, 16000 // divisor, rate // divisor)


def pitch_track(samples):
    """YIN difference/CMND, 40 ms comparison, 10 ms hop, 100..700 Hz.

    First local minimum below 0.15, parabolic lag interpolation, energy gate.
    Music and octave errors remain possible. de Cheveigne & Kawahara,
    JASA 111, 1917-1930 (2002), doi:10.1121/1.1458024.
    """
    width, hop, max_lag = 640, 160, 160
    energy = np.array([np.sqrt(np.mean(samples[i:i + width] ** 2))
                       for i in range(0, len(samples) - width, hop)])
    gate = max(1e-4, np.max(energy, initial=0) * 0.08)
    pitches = []
    for i in range(0, len(samples) - width - max_lag, hop):
        y = samples[i:i + width + max_lag]
        if np.sqrt(np.mean(y[:width] ** 2)) < gate:
            continue
        difference = np.array([np.sum((y[:width] - y[k:k + width]) ** 2)
                               for k in range(1, max_lag + 1)])
        cmnd = difference * np.arange(1, max_lag + 1) / np.maximum(np.cumsum(difference), 1e-15)
        minima, _ = find_peaks(-cmnd)
        candidates = minima[(minima + 1 >= 16000 / 700) & (cmnd[minima] < 0.15)]
        if not len(candidates):
            continue
        j = int(candidates[0])
        denominator = cmnd[j - 1] - 2 * cmnd[j] + cmnd[j + 1]
        offset = 0.5 * (cmnd[j - 1] - cmnd[j + 1]) / denominator if denominator else 0
        pitches.append((i / 16000, 16000 / (j + 1 + offset)))
    return np.array(pitches).reshape(-1, 2)


def metrics(samples):
    pitch = pitch_track(samples)
    rms = float(np.sqrt(np.mean(samples ** 2))) if len(samples) else 0
    return {
        "duration_s": round(len(samples) / 16000, 3),
        "pitch_p10_p50_p90_hz": np.round(np.percentile(pitch[:, 1], [10, 50, 90]), 1).tolist() if len(pitch) else [],
        "accepted_pitch_frames": len(pitch),
        "rms_dbfs": round(20 * math.log10(max(rms, 1e-12)), 2),
        "peak": round(float(np.max(np.abs(samples), initial=0)), 5),
        "rail_samples": int(np.sum(np.abs(samples) >= 32767 / 32768)),
    }


def audition_copy(path, samples):
    # Equal active-RMS target, with peak headroom. Metrics use original samples.
    active = samples[np.abs(samples) > max(np.max(np.abs(samples), initial=0) * 0.05, 1e-4)]
    rms = np.sqrt(np.mean(active ** 2)) if len(active) else 1
    gain = min(0.12 / max(rms, 1e-9), 0.95 / max(np.max(np.abs(samples), initial=0), 1e-9))
    wavfile.write(path, 16000, np.round(samples * gain * 32767).astype(np.int16))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--reference", type=Path, required=True)
    parser.add_argument("--before", type=Path, required=True)
    parser.add_argument("--after", type=Path, required=True)
    parser.add_argument("--out", type=Path, default=ROOT / ".tmp/voice/evaluation")
    parser.add_argument("--phrase", action="append", help="comparison sentence; repeat to replace the default corpus")
    args = parser.parse_args()
    output = args.out.resolve()
    if not output.is_relative_to(ROOT / ".tmp"):
        parser.error("keep generated evaluation files inside the repo .tmp directory")
    output.mkdir(parents=True, exist_ok=True)
    reference = read_audio(args.reference)
    provenance = {"path": str(args.reference), "sha256": hashlib.sha256(args.reference.read_bytes()).hexdigest()}
    metadata = args.reference.with_suffix(".info.json")
    if metadata.exists():
        info = json.loads(metadata.read_text())
        provenance.update({key: info.get(key) for key in ("title", "uploader", "webpage_url", "id")})
    result = {"reference": {**provenance, **metrics(reference)}, "phrases": []}
    reference_blocks, comparison_blocks = [], []
    for start, end in [(0, 20), (20, 40), (40, 60)]:
        clip = reference[start * 16000:end * 16000]
        if not len(clip):
            continue
        name = f"reference_{start}_{end}.wav"
        audition_copy(output / name, clip)
        reference_blocks.append(f'<p>Reference {start}–{end} s</p><audio controls src="{name}"></audio>')
    for index, phrase in enumerate(args.phrase or PHRASES):
        row = {"text": phrase}
        players = []
        for label, binary in [("before", args.before), ("after", args.after)]:
            path = output / f"{index:02d}_{label}.wav"
            subprocess.run([str(binary.resolve()), "--wav", str(path), phrase], cwd=ROOT, check=True, capture_output=True)
            samples = read_audio(path)
            row[label] = metrics(samples)
            audition = f"{index:02d}_{label}_listen.wav"
            audition_copy(output / audition, samples)
            players.append(f'<td>{label}<br><audio controls src="{audition}"></audio></td>')
        result["phrases"].append(row)
        comparison_blocks.append(f'<tr><td>{html.escape(phrase)}</td>{"".join(players)}</tr>')
    (output / "metrics.json").write_text(json.dumps(result, indent=2) + "\n")
    page = '''<!doctype html><meta charset="utf-8"><title>BMO voice comparison</title>
<style>body{font:17px system-ui;max-width:1100px;margin:35px auto;padding:20px;background:#e8fff2;color:#172e28}td{padding:15px;border-bottom:1px solid #9ac7b4}audio{width:300px}pre{white-space:pre-wrap}</style>
<h1>BMO voice comparison</h1><p>Actual show audio, followed by identical text through the original and revised engines. Playback uses an equal active-RMS target with peak headroom; metrics use original files.</p>
<p>Pitch estimates describe acoustic range, not voice identity. Music, sound effects and octave errors can affect reference estimates. A listening judgment is still required.</p>'''
    if provenance.get("webpage_url", "").startswith("https://www.youtube.com/watch?"):
        page += '<p>Reference: <a href="' + html.escape(provenance["webpage_url"], quote=True) + '">' + html.escape(provenance.get("title") or "source") + '</a></p>'
    page += "".join(reference_blocks) + '<table>' + "".join(comparison_blocks) + '</table>'
    page += '<h2>Diagnostics</h2><pre>' + html.escape(json.dumps(result, indent=2)) + '</pre>'
    (output / "index.html").write_text(page)
    print(json.dumps(result, indent=2))
    print(f"Listening comparison: {output / 'index.html'}")


if __name__ == "__main__":
    main()
