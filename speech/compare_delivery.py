"""Create native-rate listening comparisons without changing the default voice."""
import html
import json
from pathlib import Path
import time

from . import ROOT
from .backend import PiperEngine, voice_path

PHRASES = (
    "Hello! I am BMO. Do you want to play a game with me?",
    "Yay! You did it! I knew you could do it.",
    "Oh no. That does not look right. Let us try again.",
    "Hmm. What happens if we do it the other way?",
    "It is okay. I am right here with you.",
)
CANDIDATES = (
    ("amy", "Amy", "en_US-amy-medium"),
    ("jenny", "Jenny (Dioco)", "en_GB-jenny_dioco-medium"),
)


def main():
    output = ROOT / ".tmp/voice/delivery"
    output.mkdir(parents=True, exist_ok=True)
    engines = {key: PiperEngine(voice_path(voice)) for key, _, voice in CANDIDATES}
    rows, records = [], []
    for index, phrase in enumerate(PHRASES):
        cells = [f"<th>{html.escape(phrase)}</th>"]
        for key, label, voice in CANDIDATES:
            for style in ("neutral", "lively"):
                started = time.monotonic()
                audio = engines[key].synthesize(phrase, style=style)
                name = f"{index:02d}_{key}_{style}.wav"
                audio.write_wav(output / name)
                records.append({"text": phrase, "voice": voice, "label": label, "style": style,
                                "file": name, "sample_rate": audio.sample_rate,
                                "duration_s": round(len(audio.samples) / audio.sample_rate, 3),
                                "render_s": round(time.monotonic() - started, 3)})
                cells.append(f'<td><audio controls preload="none" src="{name}"></audio></td>')
        rows.append("<tr>" + "".join(cells) + "</tr>")
    header = "<tr><th>Same words in every version</th>" + "".join(
        f"<th>{html.escape(label)} — {style}</th>"
        for _, label, _ in CANDIDATES for style in ("neutral", "lively")) + "</tr>"
    page = '''<!doctype html><meta charset="utf-8"><title>BMO: Piper delivery comparison</title>
<style>body{font:17px system-ui;max-width:1400px;margin:30px auto;padding:20px;background:#e8fff2;color:#172e28}table{border-collapse:collapse;width:100%}td,th{padding:14px;border-bottom:1px solid #a4c9b7;text-align:left}audio{width:240px}.scroll{overflow:auto}th{min-width:180px}</style>
<h1>Piper voice and delivery comparison</h1>
<p>Compare the same words at each model's native 22,050 Hz. Neutral preserves the original delivery. Lively uses slightly faster pacing and more variation in phoneme timing, plus a minimum sentence gap.</p>
<p>These presets change delivery; they do not add trained happy, sad or curious conditioning. The voice model still determines most intonation and emotion. Amy remains the default; use this comparison to choose a voice and delivery.</p>
<p><a href="https://github.com/dioco-group/jenny-tts-dataset">Jenny (Dioco)</a> is a separate voice with an Irish speaker and varied source material. It is an audition candidate, not a BMO voice model.</p>
<div class="scroll"><table>'''
    page += header + "".join(rows) + "</table></div>"
    (output / "index.html").write_text(page)
    (output / "renders.json").write_text(json.dumps(records, indent=2) + "\n")
    print(f"Rendered {len(records)} comparisons: {output / 'index.html'}")


if __name__ == "__main__":
    main()
