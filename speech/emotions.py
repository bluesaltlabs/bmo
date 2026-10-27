"""Resolve genuinely trained emotion IDs using Piper's speaker conditioning."""
PREFIX = "emotion:"


def emotion_ids(speaker_ids: dict[str, int]) -> dict[str, int]:
    return {name[len(PREFIX):]: value for name, value in speaker_ids.items() if name.startswith(PREFIX)}


def emotion_speaker(speaker_ids: dict[str, int], count: int, emotion: str) -> int:
    available = emotion_ids(speaker_ids)
    if emotion not in available:
        labels = ", ".join(available) or "none"
        raise ValueError(f"This voice has no trained '{emotion}' emotion. Available emotions: {labels}. "
                         "Use a trained BMO emotion model; pacing presets do not supply emotion conditioning.")
    speaker = available[emotion]
    if not isinstance(speaker, int) or not 0 <= speaker < count:
        raise ValueError(f"Invalid emotion speaker ID for {emotion}: {speaker}")
    return speaker
