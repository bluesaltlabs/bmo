# Piper speech

Piper is BMO's preferred host/Pi speech backend. The original formant engine
optimized for speaking directly on an ESP32 without the Pi. It is useful as a
backup, but its size constraints limited speech quality. Piper now provides the
main voice, and `lib/bmo_voice/` remains the standalone backup.

See the [voice development record](DEVELOPMENT.md) for this phase's experiments,
listening results, cleanup and remaining work. The [training guide](training/README.md)
describes gathering recordings, assigning emotion tags, training and Pi deployment.

The current Mac setup has Piper 1.8.0 and `en_US-amy-medium` installed. This is a
stock U.S. English voice at 22,050 Hz. Its natural speech is the starting point;
it does not reproduce BMO's actor or accent. No pitch shift, bit crushing or
whole-voice low-pass is applied to Piper audio.

Written `BMO` is now pronounced **BEEMO**, rather than spelling the initials.
`speech/text.py` changes synthesis input only, preserving displayed text and
explicit `[[...]]` phoneme overrides. Training transcripts use the same rule.

## Use on this Mac

```sh
voice_demo/run.sh
voice_demo/run.sh "Hello! I am BMO."
voice_demo/run.sh -i
voice_demo/run.sh --speed 1.05 --volume 0.8 "Good morning!"
voice_demo/run.sh --style lively "Yay! We did it!"
voice_demo/run.sh --style gentle "It is okay. I am right here with you."
voice_demo/run.sh --wav .tmp/voice/my_line.wav "This writes audio without playing it."
voice_demo/run.sh --rate 16000 --wav .tmp/voice/esp32_line.wav "I am still waking up."
voice_demo/run.sh --raw .tmp/voice/esp32_line.pcm "I am still waking up."
voice_demo/run.sh --engine formant "This is the standalone backup."
```

`-i` loads the model once and reuses it across lines. `--stdin` reads a complete
text input from a pipe. The CPU model is loaded once per process; a single-line
command pays the load cost on each invocation. WAV/playback defaults to the
model's native rate. Raw output defaults to **16 kHz, mono, signed 16-bit
little-endian PCM**, with no header. `--rate` selects another rate between
8 and 48 kHz using polyphase resampling, preserving pitch and duration.

## Expression and emotion

Amy is a stock single-speaker voice. The installed model has no emotion input;
Piper's `noise_scale` and `noise_w_scale` are generation/timing variability
controls, not happiness or sadness controls. Emotion depends primarily on the
performance learned by the voice model, as well as the words and punctuation.

Listening feedback: **Amy is much closer to BMO than Jenny, and all tested
voice/pacing combinations still sound emotionally flat.** Keep Amy as the voice
target. The presets below are timing utilities; they have not solved expression.
Further voice auditions or stronger timing randomness are not the next step.

Three optional delivery presets are available:

| Style | Nominal pacing | Piper duration-noise setting | Minimum gap between sentences |
|---|---|---|---|
| `neutral` (default) | Model default | Model default | Native model output |
| `lively` | 5% faster | Model default × 1.15 | 80 ms |
| `gentle` | 7% slower | Model default × 0.85 | 200 ms |

These adjust Piper's synthesis controls and add only missing silence between
its sentence chunks. They do not rewrite words, trim quiet consonants, pitch
shift, filter, or create learned emotional acting. Existing longer model
pauses are preserved. `--speed` combines with the preset's tempo multiplier.
`BMO_SPEECH_STYLE=lively` selects a default for a shell/service; normal startup
keeps the previous neutral Amy delivery.

Jenny (Dioco), `en_GB-jenny_dioco-medium`, is installed on this Mac as a second
voice to audition. The dataset's speaker is Irish and its material spans books,
headlines, dialogue and other sources; see the [Jenny (Dioco) dataset](https://github.com/dioco-group/jenny-tts-dataset).
Its different delivery is a candidate to compare, not a claim that it reproduces
BMO or provides emotional conditioning.

```sh
# On another machine, install the optional comparison voice once:
bash voice_demo/setup_piper.sh --voice en_GB-jenny_dioco-medium
voice_demo/run.sh --voice en_GB-jenny_dioco-medium --style lively "Yay! You did it!"
.venv-piper/bin/python -m speech.compare_delivery
```

Open `.tmp/voice/delivery/index.html` for Amy/Jenny (Dioco), each neutral/lively,
using identical words across greetings, celebration, concern, questions and
reassurance. The files preserve native 22,050 Hz output. The comparison records
render metadata, not an emotion score. It does not change the default voice.

The chosen next step is an expressive Piper voice that runs locally on the Pi.
Training may run on the Mac; runtime must not depend on it. The
[training guide](training/README.md) covers recordings, preparation, Amy
warmstart, ONNX export and Pi benchmarking. The tools/checkpoint are installed,
but no curated corpus or trained expressive voice exists yet.

`--emotion excited` selects a genuinely trained condition, using
`emotion:excited` in the model's speaker map. `--list-emotions` lists the actual
conditions; stock Amy reports none and rejects `--emotion`. `--speaker` and
`--emotion` are mutually exclusive. These controls do not invent emotions for
an unconditioned model. Songs remain separate recorded assets initially;
speech TTS does not guarantee melody control.

The prepared `.tmp/voice/amy_reference.wav` and its adjacent JSON contain a clean
Amy sample and transcript for future voice-preservation tests. This reference
establishes the target identity, not emotional training data. See
[the expression plan](EXPRESSION.md) for the chosen architecture and acceptance.

The generated examples are in `.tmp/voice/piper_demo.wav` (all five demo
phrases, native rate) and `.tmp/voice/piper_waking_up.wav` (16 kHz). These are
local generated artifacts, not committed firmware assets.

## Set up another machine or the Pi

Use **64-bit Raspberry Pi OS** and 64-bit Python 3.11–3.13. Put this repository
on the Pi, then run from its root:

```sh
bash voice_demo/setup_piper.sh
voice_demo/run.sh --wav .tmp/voice/pi_check.wav "Hello! I am BMO."
voice_demo/run.sh "Hello! I am BMO."
```

The setup script creates `.venv-piper/`, installs the pinned Piper release and
SciPy, and downloads the model, matching JSON, model card and a provenance
manifest into `models/piper/`. Venvs, caches, download scratch and generated
audio stay inside the repository and are gitignored. PlatformIO's `.venv/` is
separate. `BMO_PIPER_PYTHON=/path/to/python3.11` selects an interpreter when
creating a fresh Piper venv.

Setup requires network access once; normal English synthesis uses the installed
files offline. ONNX telemetry is disabled **before importing ONNX Runtime**,
including the initialization uploader/device identifier, using
[its documented runtime switch](https://github.com/microsoft/onnxruntime/blob/main/docs/Privacy.md).
The synth does not download missing voice models or silently switch to the
formant engine. It reports the missing path and setup command.

Do not copy the Mac's `.venv-piper/` to the Pi: Python wheels are platform
specific. The `.onnx`/JSON/model-card files in `models/piper/` are portable and
can be copied to the Pi to avoid another voice download. A fresh Pi may need
its OS `python3-venv` package. Direct Linux playback uses `aplay` (the
`alsa-utils` package), targeting the Pi's configured ALSA output.

No Pi deployment has been performed: its SSH host/user was not available in
this session. The backend and setup work are ready to copy and run there.

A trained Piper voice can run locally on the Pi with the Mac switched off.
Deploy the exported ONNX model and matching JSON, rather than the training
checkpoint or Mac venv. Piper 1.8.0 provides
[Linux ARM64 runtime packages](https://pypi.org/project/piper-tts/1.8.0/).
The specific trained model still needs a Pi benchmark and robot audio integration.

## Voice selection

[Listen to Piper's voice samples](https://rhasspy.github.io/piper-samples/) to
choose another starting voice. Explicit setup installs additional voices:

```sh
bash voice_demo/setup_piper.sh --voice en_US-lessac-medium
voice_demo/run.sh --voice en_US-lessac-medium "Hello! I am BMO."
```

`BMO_PIPER_VOICE=en_US-lessac-medium` makes that installed voice the default in
a shell/service environment. `--model /path/to/custom.onnx` uses another local
model with its matching `custom.onnx.json` alongside it. `--speaker` selects
an ID for multi-speaker models. `--force` on setup explicitly downloads a fresh
copy; interrupted downloads never replace the installed model.

Matching BMO closely requires a suitable Piper voice model. Changing engines
alone does not create that voice, and feeding the reference clip to Piper at
runtime is not a supported cloning feature. The model path is configurable for
a later trained/adapted voice. See [Piper's voice training documentation](https://github.com/OHF-Voice/piper1-gpl/blob/main/docs/TRAINING.md).

## Pi integration and remaining robot work

The reusable API keeps one model loaded, with no shell command or HTTP service
needed for each reply:

```python
from pathlib import Path
from speech.backend import PiperEngine

engine = PiperEngine()
audio = engine.synthesize("Hello! I am BMO.", rate=16000)
audio.write_wav(Path(".tmp/voice/reply.wav"))
pcm = audio.pcm16()
```

Use `style="lively"` or `style="gentle"` on `engine.synthesize` to select the
same delivery controls as the CLI.

The intended audio path is text on the Pi → Piper → 16 kHz PCM → the Pi's direct
UART to the audio ESP32 → I2S DAC/amplifier → speaker. The existing firmware
currently accepts text to synthesize locally, not binary PCM. UART audio
framing, buffering, rate control and hardware playback still need to be
implemented and tested during audio bring-up. No UART wiring or display
protocol was changed here.

Pi-down replies can use pre-rendered Piper clips stored in ESP32 flash; the
formant engine remains available independently. The generated waking-up clip
is ready for that future asset step. The ESP32 does not run Piper or require
the Mac/network for stored clips.

## Verification

```sh
.venv-piper/bin/python -m unittest speech.check speech.training.check -v
```

Twenty-six checks pass on this Mac with both voices installed: CPU inference with Python
networking blocked, telemetry disabled before import, finite/non-silent output,
8/16/44.1/48 kHz resampling with preserved duration, mono PCM/WAV consistency,
empty and volume-zero speech, invalid settings, missing-model behavior, default
CLI/PCM export, no-argument demo on macOS Bash 3.2, interactive model reuse and
the explicit formant backup, all delivery presets, the optional Jenny voice,
and sentence joining that preserves quiet endings and existing pauses. The
Jenny-specific test is skipped when that optional voice is not installed.
They also verify BEEMO normalization using the real phonemizer, trained-emotion
selection/rejection, and dataset handling of review status, duplicates, source
leakage, holdout, resampling and song exclusion. Synthetic WAV fixtures exercise
preparation only; they are not BMO recordings or voice-quality evidence.

One measured Mac run rendered the full 17.6-second demo in about 0.49 seconds,
after about 0.91 seconds loading. Pi performance and robot speaker playback
have not been measured. The audio player is stubbed during automated tests;
generated files can be auditioned with the normal demo.

Implementation follows the [official Piper Python API](https://github.com/OHF-Voice/piper1-gpl/blob/main/docs/API_PYTHON.md).
