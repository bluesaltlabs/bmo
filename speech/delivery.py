"""Lightweight pacing presets, not trained emotional conditioning."""
from dataclasses import dataclass

import numpy as np


@dataclass(frozen=True)
class Delivery:
    tempo: float = 1
    timing_variation: float = 1
    minimum_pause_s: float = 0


DELIVERY = {
    "neutral": Delivery(),
    "lively": Delivery(tempo=1.05, timing_variation=1.15, minimum_pause_s=0.08),
    "gentle": Delivery(tempo=0.93, timing_variation=0.85, minimum_pause_s=0.20),
}


def join_sentences(chunks: list[np.ndarray], rate: int, pause_s: float) -> np.ndarray:
    """Add only missing inter-sentence silence; never trim or filter speech.

    Piper already splits sentences. Using its chunks avoids breaking decimals,
    abbreviations, quoted dialogue, or phoneme markup with a second text parser.
    """
    if not chunks:
        return np.empty(0, dtype=np.float32)
    if pause_s == 0:
        return np.concatenate(chunks)
    parts = []
    previous = None
    for chunk in chunks:
        if previous is not None:
            before = np.flatnonzero(np.abs(previous) > 0.0001)
            after = np.flatnonzero(np.abs(chunk) > 0.0001)
            tail = len(previous) - int(before[-1]) - 1 if len(before) else len(previous)
            head = int(after[0]) if len(after) else len(chunk)
            missing = max(0, round(pause_s * rate) - tail - head)
            if missing:
                parts.append(np.zeros(missing, dtype=np.float32))
        parts.append(chunk)
        previous = chunk
    return np.concatenate(parts)
