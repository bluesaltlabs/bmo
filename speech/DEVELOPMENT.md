# BMO voice development record — 2026-10-26

This voice-development pass prepared the speech software for Phase 3. It did
not complete audio hardware bring-up or the Pi voice service. The implementation
and next steps are recorded here so work can resume without replaying auditions.

## What happened

| Stage | Work and listening result | Decision |
|---|---|---|
| ESP32 formant voice | Evaluated the small C++ synthesizer against a real BMO monologue. Tuned pronunciation, register, stress and phrase contours. | Useful standalone ESP32 backup; acoustic pitch similarity did not establish BMO's identity or acting. |
| Hiss and clarity | The user heard hiss and loss of quality after filtering. Reduced frication at its source, separated noise envelopes and bypassed the whole-voice low-pass. | Retain the improved formant engine, but use neural speech for the preferred voice. |
| Local Piper | Installed Piper 1.8.0 with CPU synthesis, persistent model loading, native-rate playback and optional 16 kHz PCM export. | `voice_demo/run.sh` now defaults to Piper; `--engine formant` selects the backup. |
| Amy versus Jenny | The user preferred Amy's voice as closer to BMO. All stock voice and pacing comparisons sounded emotionally flat. | Keep Amy as the starting identity. Faster/slower speech and duration noise do not solve emotional performance. |
| Pronunciation | Piper spelled BMO as initials. Added speech-only normalization and checked it against the real phonemizer. | Written BMO is pronounced **BEEMO**; displayed text stays unchanged. |
| Expressive voice architecture | The user wants a childlike, emotional, playful voice with substantial variation, and requires Pi runtime. | Train an expressive Piper voice elsewhere, export ONNX, synthesize locally on the Pi. A live Mac speech service is not the chosen route. |
| Training preparation | Installed a separate training environment, built native alignment support and downloaded Amy's training checkpoint. Added corpus audit/preparation, training/export wrappers, emotion selection and a Pi benchmark. | The tools are ready for a curated dataset; no emotional voice has been trained. |

The detailed formant measurements and limitations are in
[the original evaluation](../voice_demo/EVALUATION.md). The
[expression plan](EXPRESSION.md) records listening acceptance criteria.

## Working state at handoff

- `speech/` is the preferred local CPU speech backend. Stock
  `en_US-amy-medium` remains the default and has no trained emotions.
- `lib/bmo_voice/` remains the shared host/ESP32 formant backup.
- `.venv-piper/` is the runtime environment. `.venv-piper-train/` contains the
  heavier training tools, independently of PlatformIO's `.venv/`.
- `models/piper/` contains installed Amy and optional Jenny model/config pairs,
  model cards and provenance. `models/checkpoints/amy-medium.ckpt` is the
  training seed, not a deployed voice.
- `speech/training/corpus.csv` is an empty manifest template.
  `prompts.csv` contains original recording/evaluation scripts.
  `sources.json` records the existing reference and unavailable source attempt.
- There are **zero curated training utterances**, no completed training fit and
  no trained expressive BMO model. The roughly one-minute show reference has
  not been transcribed and labelled per utterance for training.

Checks completed during this pass: 26 runtime/preparation checks, actual Amy
checkpoint warmstart into a four-condition model, training-config parsing
without fitting, and stock Amy export followed by CPU inference. Mac timings
are engineering baselines only. These checks do not demonstrate voice identity,
emotion, convergence or Raspberry Pi performance.

The normal demo still works:

```sh
voice_demo/run.sh "Hello! I am BMO."
voice_demo/run.sh -i
voice_demo/run.sh --list-emotions
```

Amy reports no trained emotions. After training, `--emotion excited` selects a
condition actually present in the exported model. The future dialogue service
must choose the condition; automatic emotion selection is not implemented.

## Cleanup

Removed about **487 MiB** of disposable development files: package download
caches, native-extension build scratch, empty compiler scratch directories,
the duplicate stock-Amy ONNX export used for validation, temporary schema logs,
old scratch WAVs, compiled test/debug files and Python bytecode caches. The
exact local deletion record is `.tmp/voice/cleanup-2026-10-26.json`.

Retained source and checks, all installed environments/models, the Amy training
checkpoint, the original reference and provenance, historical A/B audio/pages,
the Amy reference, BEEMO sample and useful demo clips. The historical pages are:

- `.tmp/voice/evaluation/index.html` — formant pitch/prosody comparison.
- `.tmp/voice/noise_evaluation/index.html` — formant hiss/clarity comparison.
- `.tmp/voice/delivery/index.html` — Amy/Jenny pacing audition.

All generated files and caches remain repository-local and gitignored. Some
retained reference evidence is under `.tmp/`; do not treat every file there as
disposable. Future curated recordings and valuable training runs should live in
`data/voice/`, with exported models in `models/piper/`. These directories are
also gitignored: keep a separate backup of recordings and chosen checkpoints.

## Next steps

1. **Gather performances.** Collect clean same-voice dialogue or record a
   consistent acted voice. Start with neutral, excited, concerned and playful.
   Keep originals, source information and timestamps. Separate songs and
   nonverbal vocalizations from the speech corpus.
2. **Segment and label.** Make short WAV utterances, transcribe exactly what is
   said, tag the performance heard, and listen to every clip before marking it
   reviewed. Keep whole source scenes/sessions in training or holdout.
3. **Audit and prepare.** Fill the manifest and pass the corpus audit. The
   20 training + 3 holdout clips per condition gate is a small pilot gate, not
   enough evidence to promise the final voice.
4. **Train and audition.** Warmstart from Amy, train the new conditioning and
   compare checkpoints on identical held-out sentences. Choose by listening
   for identity, intelligibility and acting, not training loss alone.
5. **Export and deploy.** Export ONNX plus the matching JSON, install the Pi
   runtime, copy those two files and benchmark every condition on the Pi.
6. **Integrate the robot.** Add Pi-side condition selection and PCM transport
   over the direct Pi-to-audio-ESP32 UART, buffering and I2S playback. Test the
   actual amplifier/speaker, face timing and behavior while the Pi boots or is
   unavailable. Audio hardware has not yet been brought up.

The [training guide](training/README.md) gives tagging examples, corpus layout,
commands and acceptance checks for these steps.

## Can the trained voice run without the Mac?

**Yes.** Training and inference are separate. Piper 1.8.0 provides
[Linux ARM64 runtime packages](https://pypi.org/project/piper-tts/1.8.0/), and
[its training workflow exports](https://github.com/OHF-Voice/piper1-gpl/blob/v1.8.0/docs/TRAINING.md#exporting)
a portable `.onnx` model with a matching `.onnx.json` configuration. Run those
with the project's CPU backend on 64-bit Raspberry Pi OS. The Pi does not need
the Mac, PyTorch, the training checkpoint or network access for installed
English speech synthesis.

The specific trained model still needs a speed/memory benchmark on the actual
Pi. This establishes an independent voice path; the remaining robot service,
audio transport and hardware integration must also be completed before the
whole robot is ready. Optional future Mac-hosted conversation features are a
separate choice and are not required by the voice architecture.
