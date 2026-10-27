# BMO

A local-first desk robot inspired by *Adventure Time* BMO, pronounced **BEEMO**.
Two ESP32 boards handle the face and audio hardware; a Raspberry Pi is the
planned local speech and conversation host. The voice must work without a
running Mac or cloud service.

## Current status

| Component | Status |
|---|---|
| Display — Phase 1 | Animated face firmware runs on the Waveshare ESP32-C6 1.47" LCD. Nine expressions, BOOT controls and serial commands are implemented. |
| Board link — Phase 2 | ESP32-WROOM sends face commands to the display over UART. ACKs, heartbeat, timeout and reset recovery were verified on hardware. |
| Audio — Phase 3 preparation | Host speech software and a formant I2S bring-up firmware target exist. Microphone/speaker hardware and I2S playback have not been brought up. |
| Piper voice | Local CPU synthesis works on the Mac. Stock Amy is the preferred starting voice; written BMO is spoken as BEEMO. |
| Expressive voice training | Training tools and Amy's checkpoint are prepared. Curated recordings are still needed; no expressive BMO model has been trained. |
| Pi integration | Runtime setup and a benchmark command exist. Pi deployment, measured performance, UART PCM streaming and the robot voice service remain unfinished. |
| Later features | Wake word, Pi-awareness/fallback behavior, notifications and optional advanced conversation are planned. |

The project is not yet a complete speaking robot. Completed hardware checks and
open items are recorded in [DECISIONS.md](DECISIONS.md). The original
[design and roadmap](CONTEXT.md) contains earlier proposals; later decisions
supersede its USB runtime-link suggestions and old progress checklist.

## Try the voice

Run from the repository root. First-time setup needs 64-bit Python 3.11–3.13
and network access to install Piper and download the model:

```sh
bash voice_demo/setup_piper.sh
voice_demo/run.sh
voice_demo/run.sh "Hello! I am BMO."
voice_demo/run.sh -i
voice_demo/run.sh --wav .tmp/voice/hello.wav "Hello! I am BMO."
```

Normal synthesis uses local model files and runs offline. The demo plays audio
on the computer running it; it does not yet stream to the robot's speaker.
Interactive mode keeps the model loaded between lines.

The default voice is `en_US-amy-medium`. It is stock Amy, not a trained BMO
voice. The available `--style` presets change pacing; they did not solve the
flat emotional delivery heard in testing. `--emotion` requires a model trained
with the requested condition.

The compact ESP32 formant voice remains available as a backup; its host demo
needs a C++ compiler:

```sh
voice_demo/run.sh --engine formant "Hello! I am BMO."
```

See [speech setup and controls](speech/README.md) for custom models, PCM export,
Pi setup and verification.

## Establish BMO's voice

The target is a childlike, emotional, playful voice with natural pitch and
rhythm changes. Amy is the starting checkpoint; expressive recordings must
teach the performance.

1. Gather clean recordings of one consistent voice and keep their source information.
2. Segment speech, transcribe it exactly, and label the audible performance:
   `neutral`, `excited`, `concerned` or `playful`. Keep songs as separate assets.
3. Reserve whole source scenes or recording sessions for evaluation, then audit
   and prepare the corpus.
4. Train, compare held-out samples, export the selected checkpoint and benchmark
   it on the Pi.

The [recording, tagging and training guide](speech/training/README.md) includes
examples and commands. The [voice development record](speech/DEVELOPMENT.md)
covers the formant experiments, Piper auditions, listening feedback and cleanup.

**The exported Piper voice can run locally on the Raspberry Pi with the Mac
switched off.** Train on the Mac or another training machine, then deploy the
`.onnx` model and matching `.onnx.json` with the CPU runtime on 64-bit Raspberry
Pi OS. Performance still needs measuring on the actual Pi.

## Firmware and hardware

The display and Phase 2 link can be used independently of the Pi. The audio
board's Phase 2 link firmware and Phase 3 formant bring-up firmware are separate
targets; the latter does not receive Piper PCM.

See the [firmware reference](docs/firmware.md) for setup, build/upload commands,
board pinouts, UART wiring, serial commands and BOOT controls. USB is used for
flashing and debugging. The planned runtime topology gives the Pi a direct
UART to each ESP32, while retaining the existing audio-to-display link.

## Project layout

| Path | Purpose |
|---|---|
| [src/](src/) | Display firmware |
| [audio_src/](audio_src/) | Phase 2 UART link sender |
| [voice_src/](voice_src/) | Formant voice/I2S bring-up firmware |
| [lib/bmo_voice/](lib/bmo_voice/) | Shared ESP32/host formant synthesizer |
| [speech/](speech/) | Piper runtime, emotion selection, training tools and benchmark |
| [voice_demo/](voice_demo/) | Host demo, setup/training/export scripts and formant evaluation |
| [platformio.ini](platformio.ini) | Firmware environments and pin configuration |
| [resources/](resources/) | Face reference and hardware pinout images |
| [docs/firmware.md](docs/firmware.md) | Firmware operation and wiring reference |
| `models/`, `data/voice/` | Local models, training checkpoint, curated recordings and runs; gitignored |
| [AGENTS.md](AGENTS.md) | Agent guidance, environment conventions and links to agent/skill files |

## Next milestones

- Curate the expressive speech corpus and train the first voice candidate.
- Bring up microphone capture and speaker playback on the audio board.
- Deploy Piper on the Pi and measure synthesis latency and memory.
- Implement UART audio transport, buffering, face timing and Pi-down behavior.
- Add wake-word handling and the Pi's local command/conversation service.
