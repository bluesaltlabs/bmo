# AGENTS

This project keeps agent guidance in-repo so implementation stays aligned with the hardware roadmap.

## Phase focus

**Completed:** Phase 1 (display face engine on Waveshare ESP32-C6 1.47 LCD) and Phase 2 (second ESP32-WROOM audio board linked by UART with ACK, heartbeat and timeout).

**Current work:** Phase 3 preparation. Host speech and training tools are implemented; audio hardware bring-up has not started. The Pi voice service, UART PCM transport, Pi-awareness state machines and wake-word behavior are not implemented. See [DECISIONS.md](DECISIONS.md) for current decisions and [speech/DEVELOPMENT.md](speech/DEVELOPMENT.md) for the voice-development handoff.

- Preferred speech is local CPU Piper in `speech/`, with the demo/setup in `voice_demo/`. Piper runs on the Mac/Pi, with local model files and no runtime network requirement.
- Expressive speech must run locally on the Pi. Training may happen on the Mac or another training machine; the deployed voice must not depend on a running Mac or cloud service. Amy is the preferred starting voice; timing presets alone did not meet the user's emotion requirement. See `speech/training/README.md`.
- Pronounce the character's name **BEEMO**. Preserve written `BMO`; `speech/text.py` normalizes it for synthesis and training transcripts.
- `lib/bmo_voice/` is the shared ESP32/host formant backup, selected with `voice_demo/run.sh --engine formant`; firmware bring-up stays in `voice_src/`. Pi PCM transport and audio hardware remain separate bring-up work.

## Agent registry

1. `agents/phase1-display-firmware-agent.md`  
   Owns embedded firmware for expression rendering, serial command handling, and BOOT-button interaction.
2. `agents/project-orchestrator-agent.md`  
   Owns phase sequencing, acceptance criteria tracking, and interface contracts across future phases.

## Skills registry

1. `skills/platformio-esp32-c6-build-flash.md`  
   Build/flash/monitor workflow for PlatformIO + Arduino.
2. `skills/phase1-validation-checklist.md`  
   Phase 1 behavior checks, soak test steps, and expected output.

## Coordination rules

- Preserve a local-first architecture and phase boundaries from `CONTEXT.md`.
- Use `DECISIONS.md` and this file for current implementation constraints. `CONTEXT.md` is the original design: its USB runtime-link suggestions and old progress checklist have been superseded. Do not infer completed work from planned behavior.
- Keep display firmware independent of audio, Pi, or network code; the audio board talks to it only through the UART protocol (`FACE`, `PING`).
- Topology: the Pi connects to each ESP32 over its own direct UART (3.3 V, common ground). The audio ESP32 is not a relay. The existing audio-to-display UART stays as a local fast path for wake-word reactions.
- USB is for flashing and debugging only, never the runtime link. All boards run from one shared 5 V rail, and the ESP32s must not be powered from the Pi's USB ports.
- Each ESP32 must boot and work without the Pi and track Pi state itself: `OFF`, `BOOTING`, `READY`, `DEGRADED`, `LOST`. Inputs are a `PI_READY` GPIO (ESP32-side pulldown) plus a `PING`/`PONG` heartbeat.
- Without the Pi, the audio ESP32 handles wake and command words on-chip (ESP-SR), maps them to faces, and answers Pi-dependent requests with stored flash clips ("still waking up") and a face. It never needs TTS or networking for that.
- Face arbitration on the display: a fresh Pi command wins; local commands apply only briefly or when the Pi is not `READY`.
- Use explicit serial commands and deterministic state transitions for debugging and future integration.

## Documentation and verification

- Keep `README.md` focused on current project status, demo usage, next milestones and links. Put agent/environment rules here, firmware operation and wiring in [docs/firmware.md](docs/firmware.md), speech controls in [speech/README.md](speech/README.md), and rationale/history in `DECISIONS.md` and `speech/DEVELOPMENT.md`.
- Preserve display behavior and its face-only rendering from `resources/bmo-faces.jpg`; do not redraw the physical body or bezel. Pin assignments and panel configuration are in `platformio.ini` and the firmware reference.
- Treat the Phase 2 UART sender and the Phase 3 formant/I2S target as separate firmware. The formant target does not implement the sender or receive Piper PCM; its I2S pins are placeholders, not a verified wiring plan.
- Validate changes with the relevant host checks or firmware build. Keep build/inference success, listening acceptance and hardware verification distinct. Do not claim emotion from pacing presets, BMO identity from pitch statistics, or Pi performance from Mac timings.

## Environment preferences

- Use this repository's `.tmp/` for project temporary files, scratch work, downloads in progress, and generated audio/reference files. Do not use global temporary locations such as `/tmp`, `/private/tmp`, a home/global `.tmp/`, or the system-default `TMPDIR` for these files.
- Before running tools that create temporary files, create `.tmp/` and set `TMPDIR` to its absolute repository-local path. From the repo root: `mkdir -p .tmp` followed by `export TMPDIR="$PWD/.tmp"`. Configure tool-specific temporary/cache paths locally when supported; Python `tempfile` calls should use the repo's `.tmp/` via `dir=`.
- Keep venvs, caches, and build output inside the repo as well (e.g. `.venv/`, `.venv-piper/`, `.tmp/`, `.pio/`). Ensure all generated directories are gitignored; `.tmp/` is already ignored.
- Keep curated speech data and valuable training runs in gitignored `data/voice/`, training seeds in `models/checkpoints/`, and deployed models in `models/piper/`. Use new run/output paths to preserve candidates. These files need separate backups; git does not retain them.
- Do not blanket-delete `.tmp/`: it still contains the original BMO reference, provenance and historical listening comparisons. Review artifacts before cleanup and preserve unique inputs and useful evidence.
- Run PlatformIO from the repo-local venv: `.venv/bin/pio`.
- Set `PLATFORMIO_CORE_DIR="$PWD/.tmp/platformio"` from the repo root to keep PlatformIO's packages and caches local too. See the firmware reference for first-time setup.
- Always pass an explicit `--upload-port` / `-p` (e.g. `/dev/cu.usbmodem*`); auto-detect may pick a Bluetooth port.
