# AGENTS

This project keeps agent guidance in-repo so implementation stays aligned with the hardware roadmap.

## Phase focus

**Completed:** Phase 1 (display face engine on Waveshare ESP32-C6 1.47 LCD) and Phase 2 (second ESP32-WROOM audio board linked by UART with ACK, heartbeat and timeout).

**Current implementation phase:** Phase 3 (audio hardware), not started. See `DECISIONS.md` for what was decided so far.

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
- Keep display firmware independent of audio, Pi, or network code; the audio board talks to it only through the UART protocol (`FACE`, `PING`).
- Topology: the Pi connects to each ESP32 over its own direct UART (3.3 V, common ground). The audio ESP32 is not a relay. The existing audio-to-display UART stays as a local fast path for wake-word reactions.
- USB is for flashing and debugging only, never the runtime link. All boards run from one shared 5 V rail, and the ESP32s must not be powered from the Pi's USB ports.
- Each ESP32 must boot and work without the Pi and track Pi state itself: `OFF`, `BOOTING`, `READY`, `DEGRADED`, `LOST`. Inputs are a `PI_READY` GPIO (ESP32-side pulldown) plus a `PING`/`PONG` heartbeat.
- Without the Pi, the audio ESP32 handles wake and command words on-chip (ESP-SR), maps them to faces, and answers Pi-dependent requests with stored flash clips ("still waking up") and a face. It never needs TTS or networking for that.
- Face arbitration on the display: a fresh Pi command wins; local commands apply only briefly or when the Pi is not `READY`.
- Use explicit serial commands and deterministic state transitions for debugging and future integration.

## Environment preferences

- Use this repository's `.tmp/` for project temporary files, scratch work, downloads in progress, and generated audio/reference files. Do not use global temporary locations such as `/tmp`, `/private/tmp`, a home/global `.tmp/`, or the system-default `TMPDIR` for these files.
- Before running tools that create temporary files, create `.tmp/` and set `TMPDIR` to its absolute repository-local path. From the repo root: `mkdir -p .tmp` followed by `export TMPDIR="$PWD/.tmp"`. Configure tool-specific temporary/cache paths locally when supported; Python `tempfile` calls should use the repo's `.tmp/` via `dir=`.
- Keep venvs, caches, and build output inside the repo as well (e.g. `.venv/`, `.venv-piper/`, `.tmp/`, `.pio/`). Ensure all generated directories are gitignored; `.tmp/` is already ignored.
- Run PlatformIO from the repo-local venv: `.venv/bin/pio`.
- Always pass an explicit `--upload-port` / `-p` (e.g. `/dev/cu.usbmodem*`); auto-detect may pick a Bluetooth port.
