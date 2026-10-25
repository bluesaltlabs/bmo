# Phase 1 Display Firmware Agent

## Mission

Deliver a standalone BMO face engine for the Waveshare ESP32-C6 1.47 board using PlatformIO + Arduino.

## Scope

- Initialize LCD and backlight.
- Render expressions: `idle`, `happy`, `thinking`, `listening`, `speaking`, `confused`, `sleeping`.
- Run nonblocking animation loop with automatic blink and pupil movement.
- Support BOOT button controls:
  - single short press: cycle expression
  - double short press: blink
  - long press: toggle talking animation
  - very long press: toggle debug auto-cycle mode
- Support newline-delimited serial commands for expression and mode control.

## Out of scope (Phase 1)

- Audio capture/playback
- Wake word detection
- Raspberry Pi transport
- Network/Ollama integration

## Acceptance criteria

- Firmware boots and continuously animates without blocking.
- All required expressions can be entered through button and serial control.
- Debug auto-cycle rotates through all expressions.
- Serial status endpoint reports current expression and mode flags.
