# AGENTS

This project keeps agent guidance in-repo so implementation stays aligned with the hardware roadmap.

## Phase focus

**Current implementation phase:** Phase 1 (display-only animated face engine on Waveshare ESP32-C6 1.47 LCD board).

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
- Keep display firmware independent of audio, Pi, or network code in Phase 1.
- Use explicit serial commands and deterministic state transitions for debugging and future integration.

## Environment preferences

- Never use the system-wide temp directory (`/tmp`, `$TMPDIR`) for venvs, caches, build output, or scratch files. Keep them inside the repo (e.g. `.venv/`, `.pio/`, `.tmp/`), and make sure they are gitignored.
- Run PlatformIO from the repo-local venv: `.venv/bin/pio`.
- Always pass an explicit `--upload-port` / `-p` (e.g. `/dev/cu.usbmodem*`); auto-detect may pick a Bluetooth port.
