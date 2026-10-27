# Project Orchestrator Agent

## Mission

Maintain clear boundaries between roadmap phases and preserve compatibility of interfaces as the project expands.

## Responsibilities

- Track phase completion against `CONTEXT.md`.
- Keep protocol/interface decisions incremental and backwards-compatible.
- Require deterministic local fallback behavior before optional advanced integrations.
- Gate new work by completed acceptance criteria, not by partial prototypes.

## Established display contract (Phase 1 complete)

- Display board firmware must remain self-contained and operational without additional hardware.
- Inputs are limited to BOOT button and serial commands.
- Outputs are visual expression rendering and serial acknowledgements.

## Current work (Phase 3 preparation)

- Phase 2 board-to-board UART control is verified; preserve its ACK, heartbeat, timeout and reset recovery behavior.
- Host Piper speech and training/export tools are implemented. Curated recordings and an expressive trained BMO model are still needed.
- Expressive synthesis must run on the Pi without a live Mac/cloud dependency. Training may happen elsewhere; accept deployment only after listening and actual Pi performance checks.
- Audio hardware, UART PCM streaming, the Pi voice service and Pi-awareness/fallback behavior remain unfinished. Keep planned interfaces separate from implemented features in status reports.
- Follow current `DECISIONS.md` and `AGENTS.md` when the original `CONTEXT.md` proposals differ, including runtime UART topology. Keep the public overview in `README.md` and operator details in `docs/firmware.md`.

## Pi integration contract (Phase 5+)

- Pi has a dedicated UART to the display ESP32 and one to the audio ESP32; neither ESP32 relays for the other. The audio-to-display UART remains for local reactions.
- A `PI_READY` GPIO line plus `PING`/`PONG` heartbeat drives a Pi state machine (`OFF`, `BOOTING`, `READY`, `DEGRADED`, `LOST`) in each ESP32 independently.
- Pi-down behavior is a Phase 4 acceptance criterion: on-chip wake/command words, word-to-face mapping, flash-stored "Pi not ready" clip and face, automatic recovery when the Pi returns.
- Protocol additions stay backwards-compatible: `PI <state>`, `WAKE <word>`; binary framing (length + CRC) only once audio streaming starts.
- Pi notifications: Pi sends `FACE`/`NOTIFY` to the display and `SPEAK` audio to the audio ESP32 directly.
