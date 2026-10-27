"""CPU Piper synthesis from local models, with optional PCM rate conversion."""
from dataclasses import dataclass
import math
from pathlib import Path
import re
import wave

import numpy as np
from scipy.signal import resample_poly

from . import DEFAULT_VOICE, MODEL_DIR
from .delivery import DELIVERY, join_sentences
from .text import spoken_text
from .emotions import emotion_ids, emotion_speaker


def voice_path(name: str = DEFAULT_VOICE) -> Path:
    if not re.fullmatch(r"[a-z]{2,3}_[A-Z]{2}-[A-Za-z0-9_]+-(?:x_low|low|medium|high)", name):
        raise ValueError("Use a Piper voice ID such as en_US-amy-medium, or --model for a local ONNX file")
    return MODEL_DIR / f"{name}.onnx"


@dataclass
class Audio:
    samples: np.ndarray
    sample_rate: int

    def resampled(self, rate: int | None = None) -> "Audio":
        if rate is None or rate == self.sample_rate:
            return self
        if not 8000 <= rate <= 48000:
            raise ValueError("sample rate must be between 8000 and 48000 Hz")
        divisor = math.gcd(rate, self.sample_rate)
        samples = resample_poly(self.samples, rate // divisor, self.sample_rate // divisor) if len(self.samples) else self.samples.copy()
        return Audio(np.clip(samples, -1, 1).astype(np.float32), rate)

    def pcm16(self) -> bytes:
        return np.round(np.clip(self.samples, -1, 1) * 32767).astype("<i2").tobytes()

    def write_wav(self, path: Path) -> None:
        path = Path(path)
        path.parent.mkdir(parents=True, exist_ok=True)
        with wave.open(str(path), "wb") as output:
            output.setnchannels(1)
            output.setsampwidth(2)
            output.setframerate(self.sample_rate)
            output.writeframes(self.pcm16())


class PiperEngine:
    """Keep one model loaded across utterances. No voice downloading in this API."""

    def __init__(self, model: Path | None = None):
        self.model = Path(model) if model is not None else voice_path()
        for path in (self.model, Path(str(self.model) + ".json")):
            if not path.is_file():
                raise FileNotFoundError(f"Missing local voice file: {path}. Run voice_demo/setup_piper.sh first.")
        import onnxruntime
        onnxruntime.disable_telemetry_events()
        from piper import PiperVoice
        self.voice = PiperVoice.load(self.model, use_cuda=False, download_dir=MODEL_DIR)
        self.sample_rate = self.voice.config.sample_rate
        self.emotions = emotion_ids(self.voice.config.speaker_id_map)

    def synthesize(self, text: str, *, speed: float = 1, volume: float = 0.85,
                   speaker: int | None = None, rate: int | None = None,
                   style: str = "neutral", emotion: str | None = None) -> Audio:
        if emotion is not None:
            if speaker is not None:
                raise ValueError("choose emotion or speaker, not both")
            speaker = emotion_speaker(self.voice.config.speaker_id_map, self.voice.config.num_speakers, emotion)
        if style not in DELIVERY:
            raise ValueError(f"style must be one of: {', '.join(DELIVERY)}")
        delivery = DELIVERY[style]
        if not math.isfinite(speed) or not 0.5 <= speed <= 2:
            raise ValueError("speed must be between 0.5 and 2")
        if not math.isfinite(volume) or not 0 <= volume <= 1:
            raise ValueError("volume must be between 0 and 1")
        if rate is not None and not 8000 <= rate <= 48000:
            raise ValueError("sample rate must be between 8000 and 48000 Hz")
        if speaker is not None and not 0 <= speaker < self.voice.config.num_speakers:
            raise ValueError(f"speaker must be in 0..{self.voice.config.num_speakers - 1}")
        if not text.strip():
            return Audio(np.empty(0, dtype=np.float32), rate or self.sample_rate)
        from piper import SynthesisConfig
        config = SynthesisConfig(length_scale=self.voice.config.length_scale / (speed * delivery.tempo),
                                 noise_w_scale=self.voice.config.noise_w_scale * delivery.timing_variation,
                                 volume=volume, speaker_id=speaker)
        chunks = [chunk.audio_float_array for chunk in self.voice.synthesize(spoken_text(text), syn_config=config)]
        samples = join_sentences(chunks, self.sample_rate, delivery.minimum_pause_s)
        if not np.isfinite(samples).all():
            raise RuntimeError("Piper produced non-finite audio")
        return Audio(samples, self.sample_rate).resampled(rate)
