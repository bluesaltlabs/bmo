# Train an expressive Piper voice for the Pi

The runtime target is a standard medium Piper ONNX voice on the Pi's CPU.
Train on the Mac or another suitable machine, then copy the model and its JSON
to the Pi. The Pi does not need PyTorch, the training checkpoint or a running
Mac. Amy is the preferred starting voice and remains the fallback.

**Status:** this Mac has the separate training environment, compiled alignment
extension and Amy training checkpoint. Warmstart into a four-condition model,
config parsing and stock Amy CPU export have been verified. No training fit
has run. There are zero reviewed speech recordings in `corpus.csv`; no expressive
BMO voice exists yet. The temporary stock-Amy test export was removed after
validation. See the [development record](../DEVELOPMENT.md) for this phase's
results and cleanup.

## 1. Training tools

From the repository root:

```sh
bash voice_demo/setup_piper.sh
bash voice_demo/setup_training.sh
```

Setup uses a separate `.venv-piper-train/`, pinned Piper 1.8.0 training tools,
and the roughly 846 MB [Amy training checkpoint](https://huggingface.co/datasets/rhasspy/piper-checkpoints/tree/main/en/en_US/amy/medium).
An inference `.onnx` cannot substitute for this `.ckpt`. A C compiler is needed
for Piper's native monotonic-alignment extension; on macOS use Xcode Command
Line Tools. All temporary files, caches and training output stay in the repo.
`BMO_TRAIN_PYTHON=/path/to/python3.12` selects the setup interpreter.

The workflow follows [Piper 1.8 training](https://github.com/OHF-Voice/piper1-gpl/blob/v1.8.0/docs/TRAINING.md).
Speaker IDs carry performance conditions for a single voice, following the
pattern in [Piper's Thorsten emotional model](https://huggingface.co/rhasspy/piper-voices/raw/main/de/de_DE/thorsten_emotional/medium/de_DE-thorsten_emotional-medium.onnx.json).
New conditioning layers need training; successfully copying Amy's shared
weights does not create emotional acting.

## 2. Curate real performances

Use clean recordings of one consistent voice: checked BMO dialogue, or an
actor performing the desired childlike, playful voice. Start with neutral,
excited, concerned and playful performances. Each condition needs varied
sentences and deliveries. Exact transcripts must match the audio; label the
performance actually heard. Amy-generated neutral speech is not emotion data.

`sources.json` records the existing reference and retrieval status. The
one-minute monologue is reference-only until individually segmented and
reviewed. Avoid music, other speakers, clipping, reverberation and aggressive
denoising artifacts. Preserve expressive timing. Split speech into 0.5–15 second
utterances. Keep songs and nonverbal vocalizations as separate clip assets.

`prompts.csv` contains original scripts for acted recordings and later
held-out synthesis. These are **not transcripts of the show**. Record training
prompts with each desired performance, rather than attaching those words to
unrelated audio. Reserve the holdout prompts and recording sessions from
training. The pilot scripts are a starting set, not a complete phonetic corpus.

Keep source recordings and the working manifest in gitignored `data/voice/`:

```sh
mkdir -p data/voice/audio
cp speech/training/corpus.csv data/voice/corpus.csv
```

Run the copy command only when starting a new manifest; do not overwrite one
you have already labelled. A useful layout is:

```text
data/voice/
  raw/          original source files, unchanged
  audio/        segmented speech WAVs
  assets/       songs and nonverbal clips, outside speech training
  corpus.csv    reviewed utterance metadata
  sources.md    original filenames/URLs, scene times and recording notes
  runs/         prepared datasets, configs and valuable training checkpoints
```

Preserve the original recording rate and keep lossless sources where available;
preparation performs the one required resampling step. Cut on natural phrase
boundaries, keep a little space at both ends, and listen for chopped consonants.
Avoid overlapping speech, background music and effects. Do not repeatedly encode
MP3 or process away the expressive pitch and rhythm. If recording an actor,
keep the voice, microphone position and room consistent across conditions.

For example, with FFmpeg installed, this extracts one short utterance from a
local source. Replace the filenames, start time and duration with your own:

```sh
mkdir -p .tmp data/voice/raw data/voice/assets data/voice/runs
export TMPDIR="$PWD/.tmp"
ffmpeg -n -ss 00:01:12.500 -i data/voice/raw/scene001.mkv -t 4.200 \
  -vn -ac 1 -c:a pcm_s16le data/voice/audio/scene001_0001.wav
```

This makes mono PCM16 audio and refuses to overwrite an existing clip. It does
not remove music or separate speakers. Log the source and time range so every
utterance can be traced back to its original.

### Choose and apply tags

Use one dominant performance label per speech clip. Start with a small,
consistent set; add conditions only when you can supply enough examples.

| Manifest emotion | What to listen for |
|---|---|
| `neutral` | Ordinary conversational delivery in the target character's voice |
| `excited` | Audible delight or enthusiasm, energetic emphasis and pitch movement |
| `concerned` | Audible worry, uncertainty or gentle concern |
| `playful` | Mischievous, theatrical or whimsical spoken delivery |

Tag the **acting heard**, not the emotion implied by the words. A flat recording
of “Yay!” is not an excited performance. Use `playful` for a clearly defined
whimsical performance you can label consistently. Split a mixed-emotion clip
at a natural boundary or leave it unreviewed until you can classify it.
Use `kind=song` or `kind=vocalization` for those separate assets; `singing` is
not a speech emotion in this workflow. Sing-song spoken delivery can be
`playful` when the words remain ordinary speech.

Transcribe every spoken word, including contractions and repetitions. Listen
again while reading the transcript, check the emotion and background, then set
`reviewed=yes`. Leave uncertain clips as `no`; they will not pass the audit.
Write plain labels such as `excited` in the manifest. Preparation adds the
`emotion:` prefix required by the model automatically. Extra columns can hold
your notes, but the preparer uses only the documented fields below.

### Fill the manifest and reserve evaluation audio

The manifest is UTF-8, pipe-delimited, with this header:

```text
id|audio|text|emotion|split|source_group|kind|reviewed|source
```

| Field | Meaning |
|---|---|
| `id` | Unique letters/numbers/underscore/hyphen clip ID |
| `audio` | WAV path relative to the manifest |
| `text` | Exact spoken words; written BMO becomes Beemo during preparation |
| `emotion` | Lowercase label such as neutral, excited, concerned, playful |
| `split` | train or holdout |
| `source_group` | Original scene/recording session; never crosses splits |
| `kind` | speech, song or vocalization; only speech enters TTS training |
| `reviewed` | yes only after checking voice, transcript, emotion and background |
| `source` | Source URL, recording session or original-file provenance |

Illustrative rows, not existing recordings (review is deliberately `no`):

```text
scene001_0001|audio/scene001_0001.wav|Hello! I am BMO.|neutral|train|episode001|speech|no|episode001 scene001 00:01:12.500-00:01:16.700
session02_p08|audio/session02_p08.wav|You did it! I knew you could do it.|excited|train|session02|speech|no|original acted recording session02
session03_p21|audio/session03_p21.wav|BMO has a surprise for you. Are you ready?|excited|holdout|session03|speech|no|separate held-out recording session03
```

Decide the split **by source group before extracting clips**. Reserve roughly
10–20% of sessions/scenes for holdout, while ensuring every emotion has held-out
examples. Keep an entire episode together where practical. Adjacent cuts from
the same scene must not appear in both splits under different group names.
Also reserve the holdout recording prompts from training if you use `prompts.csv`.

Audit without preparing or training:

```sh
.venv-piper/bin/python -m speech.training.prepare --check --manifest data/voice/corpus.csv
```

The initial pilot gate requires neutral plus another emotion, at least 20
training and 3 held-out clips **per emotion**, reviewed audio and disjoint
source groups. These counts are an engineering gate, not a promise of quality.
Less than 30 minutes warns that the result is a limited pilot. Real adaptation
may require substantially more varied audio; judge the held-out results.
The audit rejects duplicates, unreadable/silent/clipped recordings and missing
review, but cannot automatically establish voice similarity or acting quality.

For four conditions, the gate is 92 clips total: 80 training and 12 holdout.
Use this small pilot to check the pipeline first. A practical collection target
for the first serious adaptation is 30–60 minutes of clean, varied speech spread
across the conditions; this is a planning target, not a measured requirement or
quality guarantee. Expand the weaker conditions after listening to the pilot.

## 3. Prepare and train

Once the audit passes:

```sh
.venv-piper/bin/python -m speech.training.prepare \
  --manifest data/voice/corpus.csv \
  --checkpoint models/checkpoints/amy-medium.ckpt \
  --out data/voice/runs/run01
bash voice_demo/train_piper.sh data/voice/runs/run01/train_config.json
```

Preparation resamples to mono 22,050 Hz PCM16 without pitch shifting, preserves
timing, and writes Piper metadata, phoneme IDs, provenance and a training
config. It refuses to overwrite an existing run. Speaker labels are
`emotion:neutral`, `emotion:excited`, etc.; neutral is emitted first.

Use `data/voice/runs/` for valuable runs rather than the preparer's default
scratch output under `.tmp/`. Prepared audio, recordings and checkpoints are
gitignored, so back them up separately. Use a new run directory after changing
audio, transcripts, labels or splits to avoid reusing stale preprocessing caches.

The config uses Amy's phoneme map and `model.warmstart_ckpt` to copy compatible
weights. Do not resume Amy's optimizer using `--ckpt_path` after changing the
speaker count. New speaker-conditioning parameters must learn from the corpus.
`holdout.csv` stays out of fitting; Piper's internal validation split uses only
training-source audio and does not replace external holdout evaluation.

The starter config uses CPU float32, batch size 16 and a 5,000-step pilot cap.
It is a conservative, parse-checked configuration, not a measured training
speed or convergence guarantee. CPU training may be slow. Training on a suitable
GPU is also possible; profile and verify that accelerator separately. Overrides
pass through to Piper/Lightning, for example `--data.batch_size 8`. Use
`--print_config` to inspect the resolved config without fitting. Choose the
checkpoint by held-out listening rather than the step count alone.

Before a long run, inspect the parsed configuration without starting fitting:

```sh
bash voice_demo/train_piper.sh data/voice/runs/run01/train_config.json --print_config
```

The current Mac CPU configuration has been parse-checked, not timed in a real
training run. The Pi is the deployment target, not the intended training machine.

## 4. Export and listen

Use a checkpoint from this run and the JSON written by its data module:

```sh
bash voice_demo/export_piper.sh \
  --checkpoint data/voice/runs/run01/runs/REPLACE_WITH_ACTUAL_CHECKPOINT.ckpt \
  --config data/voice/runs/run01/voice.onnx.json \
  --out models/piper/en_US-bmo_expressive-medium.onnx
voice_demo/run.sh --model models/piper/en_US-bmo_expressive-medium.onnx --list-emotions
voice_demo/run.sh --model models/piper/en_US-bmo_expressive-medium.onnx \
  --emotion excited "BMO has a surprise for you. Are you ready?"
```

Export checks the speaker count and runs CPU inference for each named condition
before installing the model and JSON. It refuses to overwrite existing models.
This is structural validation, not a listening score. Compare identical held-out
sentences in every condition with stock Amy and real BMO references. Judge
identity, intelligibility, natural variation and actual emotion separately.
Keep Amy as default until the new model passes listening and Pi checks.

Replace the checkpoint placeholder with the actual file printed by training;
do not export the original Amy seed as if it were a trained BMO voice. Use a new
output filename for later model candidates so earlier auditions remain available.
At runtime, the dialogue code must select `emotion="excited"` (or another
trained label); default speaker 0 is neutral. Learning and selecting conditions
are separate steps. Automatic emotion selection is future robot-service work.

## 5. Validate the actual Pi

Copy the repository changes and exported `.onnx`/`.onnx.json` to the Pi. Set up
only the runtime there.

Use 64-bit Raspberry Pi OS and Python 3.11–3.13 for this project's setup.
Transfer the exported model and its matching JSON together; do not transfer the
Mac venv or install the training environment on the Pi. Piper 1.8.0 has
[Linux ARM64 packages](https://pypi.org/project/piper-tts/1.8.0/). Installed
English synthesis uses the Pi's CPU and needs no Mac or network service.

From the repository root on the Pi, run:

```sh
bash voice_demo/setup_piper.sh
.venv-piper/bin/python -m speech.benchmark \
  --model models/piper/en_US-bmo_expressive-medium.onnx \
  --require-pi --max-rtf 1.0
```

The report records board, load time, peak process memory and render/audio ratios
for each condition. `--require-pi` rejects Mac/PC results. Ratio below 1 means
warm synthesis is faster than playback; check shorter utterance latency and
concurrent robot workloads too. No Pi performance has been measured yet.
Audio ESP32 UART PCM transport and speaker/DAC testing remain separate bring-up.
Songs can play as stored clips on the Pi (or later flash assets on the ESP32);
this speech workflow does not promise arbitrary melody generation.
