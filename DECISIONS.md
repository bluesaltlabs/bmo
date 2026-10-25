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
| 7 | Blue-first palette | BMO is teal/blue; first palette rendered reddish-purple on the panel. |
| 8 | Off-screen `Arduino_Canvas` framebuffer, one flush per frame (~110 KB RAM) | Direct clear-then-redraw over SPI caused visible flicker. |
| 9 | 45 ms frame interval, non-blocking loop | Smooth animation while staying responsive to button and serial. |
| 10 | Pins and panel geometry as `-D` build flags in `platformio.ini` | Overridable per board variant without code edits. |
| 11 | Two envs: `waveshare_esp32_c6_lcd_147_4mb` (default) and `waveshare_esp32_c6_touch_lcd_147_8mb` | Cover board variants. Note the chip reports 8 MB flash even under the 4 MB env. |
| 12 | `AGENTS.md` registry with `agents/` and `skills/` markdown files | Current convention for agent guidance and reusable procedures. |
| 13 | Out of scope: audio, wake word, Raspberry Pi, networking | Reserved for later phases per `CONTEXT.md`. |

## Operational notes

- Always upload with an explicit port (e.g. `--upload-port /dev/cu.usbmodem2124301`); auto-detect picked a Bluetooth port.
- Toggling DTR/RTS when opening the serial port can put the chip in download mode; recover with RESET, or hold BOOT while tapping RESET.

## Open items

- Visual confirmation that flicker is fully gone.
- Runtime serial verification (`STATUS` reply) not yet captured.
- Color order was never explicitly verified against the panel.
