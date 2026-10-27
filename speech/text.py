"""Speech-only pronunciation rules shared by inference and training metadata."""
import re

_BMO = re.compile(r"\bbmo\b", re.IGNORECASE)
_PHONEMES = re.compile(r"(\[\[.*?\]\])", re.DOTALL)


def spoken_text(text: str) -> str:
    """Say BMO as BEEMO; keep display text and explicit phoneme overrides intact."""
    return "".join(part if part.startswith("[[") else _BMO.sub("Beemo", part)
                   for part in _PHONEMES.split(text))
