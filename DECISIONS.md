# Phase 1 Decisions

Scope: display-only animated face engine on the Waveshare ESP32-C6 1.47" LCD board.

| # | Decision | Rationale |
|---|----------|-----------|
| 1 | PlatformIO + Arduino framework | User choice; simple CLI build/flash, reproducible. |
| 2 | pioarduino platform instead of stock `espressif32` | Stock platform reports "board doesn't support arduino framework" for ESP32-C6. pioarduino provides Arduino core 3.x with C6 support. |
| 3 | Arduino_GFX (`moononournation/GFX Library for Arduino`) with ST7789 driver | Supports the 172x320 panel with column offset 34; Canvas support for framebuffering. |
| 4 | Input = BOOT button (GPIO9) + serial commands only | The display has no touch. Gestures: short=cycle, double=blink, long (≥1.2 s)=talk toggle, very long (≥3 s)=debug auto-cycle. |
| 5 | Serial protocol: newline-delimited text (`HELP`, `STATUS` JSON, `NEXT`, `BLINK`, `STATE`, `TALK`, `DEBUG`) | Easy to script and test; later phases can drive the face over the same port. |
| 6 | Landscape (`BMO_LCD_ROTATION=1`, 320x172) with IPS inversion on | Initial vertical/purple output was wrong; landscape fits the face. |
| 7 | Visuals follow `bmo-faces.jpg` and the user palette: light-mint screen (`#d2fce4`, sampled) fills the whole LCD, `#171717` eyes/outlines, sampled greens for mouths, `#ffb600` debug and `#ff66cc` talk indicators in the top-right corner. Body, bezel, bar and button are not drawn because the physical case provides them (user request). `#63bda4` and `#0099cb` are unused for now | The earlier blue palette did not match BMO (and first rendered reddish-purple). |
| 7b | Expression set grew from 7 to 9 (added `ANGRY`, `SAD`); face redrawn from scratch each frame | One expression per reference face; full-frame redraw is already needed for the canvas flush. Mapping: IDLE=line smile, HAPPY=arc eyes+U smile, THINKING=one dot eye, one spiral, tilted mouth, LISTENING=dot eyes+small open mouth, SPEAKING=big eyes+animated open mouth, CONFUSED=tilted eyes+worm mouth, SLEEPING=closed eyes+dome mouth, ANGRY=gritted teeth, SAD=lidded eyes+flat frown. |
| 8 | Off-screen `Arduino_Canvas` framebuffer, one flush per frame (~110 KB RAM) | Direct clear-then-redraw over SPI caused visible flicker. |
| 9 | 45 ms frame interval, non-blocking loop | Smooth animation while staying responsive to button and serial. |
| 10 | Pins and panel geometry as `-D` build flags in `platformio.ini` | Overridable per board variant without code edits. |
| 11 | Two envs: `waveshare_esp32_c6_lcd_147_4mb` (default) and `waveshare_esp32_c6_touch_lcd_147_8mb` | Cover board variants. Note the chip reports 8 MB flash even under the 4 MB env. |
| 12 | `AGENTS.md` registry with `agents/` and `skills/` markdown files | Current convention for agent guidance and reusable procedures. |
| 13 | Out of scope: audio, wake word, Raspberry Pi, networking | Reserved for later phases per `CONTEXT.md`. |

## Operational notes

- Always upload with an explicit port (e.g. `--upload-port /dev/cu.usbmodem2124301`); auto-detect picked a Bluetooth port.
- Toggling DTR/RTS when opening the serial port can put the chip in download mode; recover with RESET, or hold BOOT while tapping RESET.

## Verification status

- Resolved: flicker fix confirmed visually by the user (canvas framebuffer rendering).
- Resolved: runtime serial verification (`STATUS` reply) captured during Phase 2 testing.
- Open: color order was never explicitly verified against the panel; the BMO-reference redraw (colors and 9 faces) is flashed but not yet visually confirmed.

## Phase 2 decisions

| # | Decision | Rationale |
|---|----------|-----------|
| 14 | Newline-delimited ASCII protocol over UART (115200 8N1): `FACE <name>` / `ACK FACE <NAME>`, `PING` / `PONG` | Human-readable and testable from any terminal. |
| 15 | Display and audio boards cross-wired on GPIO16/17 plus GND (display: `Serial1`, audio: `Serial2`) | Free pins on both boards; USB consoles stay available for debugging. |
| 16 | Any valid link command counts as a heartbeat; 3 s of silence returns the face to IDLE and stops talk animation | Safe fallback when the audio board disconnects. |
| 17 | Audio board sends one outstanding command at a time, retrying 3 times at 250 ms | Simple reliability without sequence numbers. |
| 18 | Audio board is a separate PlatformIO env (`audio_esp32_wroom`, source in `audio_src/`) | Keeps the two firmwares independent. |

## Phase 2 verification

- Definition of done met: pressing the audio board's BOOT button changes the face on the display (confirmed visually by the user).
- Tested over both USB consoles: ACKs, heartbeat, malformed and repeated commands, 3 s timeout to IDLE, and recovery after resetting either board.
- Fixed during testing: unsigned timestamp underflow in the audio loop that caused false retries and `LINK LOST/UP`.
- Known quirks: C6 boot log noise on GPIO16/17 (UART0 pins); retries continue after an `ERR` reply.
- Wiring physically verified; early boot-loop symptoms occurred before the final wiring was in place.

## Phase 3+ link and Pi-awareness decisions (planned)

| # | Decision | Rationale |
|---|----------|-----------|
| 19 | Pi connects to each ESP32 by its own direct UART; USB only for flashing and debugging | Runtime independence: no USB enumeration, DTR/RTS resets or boot-order dependence, and the ESP32s can tell the Pi is absent. |
| 20 | Audio ESP32 does not relay Pi traffic; the audio-to-display UART remains a local fast path | Avoids a single point of failure and extra forwarding load on the audio chip. |
| 21 | One 5 V rail powers the Pi and both ESP32s; ESP32s are not powered from Pi USB | ESP32s boot in about 1 s versus 20-40 s for the Pi, so they can answer while it boots. Avoid USB back-power conflicts. |
| 22 | `PI_READY` GPIO (pulldown on ESP32) plus `PING`/`PONG` heartbeat feed a per-board state machine: `OFF`, `BOOTING`, `READY`, `DEGRADED`, `LOST` | Hardware line works even if Pi software hangs; the heartbeat confirms the service is alive. |
| 23 | Standalone mode: audio ESP32 runs wake/command words on-chip (ESP-SR), maps them to faces, and plays flash-stored clips when the Pi is not `READY` | Required to work in a limited way without the Pi. No TTS or networking needed. |
| 24 | Face arbitration: a fresh Pi command wins; local commands apply briefly or when the Pi is not `READY` | Prevents the Pi and the audio chip fighting over the face. |
| 25 | Protocol additions: `PI <state>`, `WAKE <word>`; binary framing only when audio streaming starts | Keeps the ASCII protocol backwards-compatible. |

Open: exact Pi UART choice (UART0 vs a second overlay UART), WROOM GPIOs for the Pi link and `PI_READY` (avoid 0, 2, 12, 15 and I2S pins), and flash partition size for audio clips.

## Voice engine decisions

| # | Decision | Rationale |
|---|----------|-----------|
| 26 | On-device formant synthesizer (`lib/bmo_voice/`), not neural TTS, for the audio ESP32's own speech | Needs no model storage, network or Pi, so it works standalone; neural TTS (Piper) stays an option on the Pi. |
| 27 | One C++ library with no heap/IO, built for both ESP32 and host (`voice_demo/run.sh`) | Voice can be tuned on the Mac with identical code before hardware exists. |
| 28 | BMO character from parameters: ~340 Hz pitch, formants x1.28, expressive intonation and subtle vibrato; bit-crush and sample-hold are optional effects, off by default | Keeps the characterful pitch/formants while avoiding unnecessary static. Defaults are being iteratively tuned by ear against the user's single-speaker reference. |
| 29 | Rule-based letter-to-sound plus a small exception dictionary; numbers are spelled out | Small footprint; mispronunciations are fixed by adding dictionary entries. |
| 30 | `Voice::mouth()` (0..255) exposed for lip-sync | Drives the display's SPEAKING face over the existing `FACE` link later. |

Open: compare by ear against the user-provided BMO-only monologue (https://www.youtube.com/watch?v=qswatNKP3YY); the measurement is only a starting point. DAC/amp and I2S pins (placeholders 26/25/22) are undecided.
