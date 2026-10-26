# AGENTS

This project keeps agent guidance in-repo so implementation stays aligned with the hardware roadmap.

## Phase focus

**Completed:** Phase 1 (display face engine on Waveshare ESP32-C6 1.47 LCD) and Phase 2 (second ESP32-WROOM audio board linked by UART with ACK, heartbeat and timeout).

**Current implementation phase:** Phase 3 (audio hardware), not started. See `DECISIONS.md` for what was decided so far.

- Voice engine work lives in `lib/bmo_voice/` (shared ESP32/host code), with the host demo in `voice_demo/` and firmware bring-up in `voice_src/`.

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

- Never use the system-wide temp directory (`/tmp`, `$TMPDIR`) for venvs, caches, build output, or scratch files. Keep them inside the repo (e.g. `.venv/`, `.pio/`, `.tmp/`), and make sure they are gitignored.
- Run PlatformIO from the repo-local venv: `.venv/bin/pio`.
- Always pass an explicit `--upload-port` / `-p` (e.g. `/dev/cu.usbmodem*`); auto-detect may pick a Bluetooth port.
