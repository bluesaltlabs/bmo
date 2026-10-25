# BMO

Local-first desk robot project inspired by *Adventure Time* BMO.

## Phase 1 (implemented)

Phase 1 targets only the Waveshare ESP32-C6 1.47 LCD board and delivers a standalone face engine with:

- default **landscape orientation**
- blue-first BMO color palette
- expression states: `idle`, `happy`, `thinking`, `listening`, `speaking`, `confused`, `sleeping`
- nonblocking animation loop with auto-blink and moving pupils
- off-screen RGB565 framebuffer; complete frames are flushed to the LCD to avoid visible clear-then-redraw flashes (about 110 KB RAM at landscape resolution)
- BOOT-button input mapping:
  - single short press: cycle expressions
  - double short press: blink
  - long press: toggle talking animation
  - very long press: toggle debug auto-cycle
- serial command control for state/mode changes

## Project layout

```text
.
├── AGENTS.md
├── agents/
├── skills/
├── platformio.ini
└── src/
    └── main.cpp
```

## Hardware assumptions

This firmware is configured for **Waveshare ESP32-C6-LCD-1.47** pinout:

- MOSI `GPIO6`
- SCLK `GPIO7`
- LCD_CS `GPIO14`
- LCD_DC `GPIO15`
- LCD_RST `GPIO21`
- LCD_BL `GPIO22`
- BOOT button `GPIO9`

These are set as build flags in `platformio.ini` and can be overridden.

Default display tuning flags:

- `BMO_LCD_ROTATION=1` (landscape)
- `BMO_LCD_IPS=1`

## Build and upload

```bash
pio run -e waveshare_esp32_c6_lcd_147_4mb
pio run -e waveshare_esp32_c6_lcd_147_4mb -t upload
pio device monitor -b 115200
```

If your board variant is 8MB, use `waveshare_esp32_c6_touch_lcd_147_8mb`.

## Serial commands

Commands are newline-delimited:

```text
HELP
STATUS
NEXT
BLINK
STATE IDLE|HAPPY|THINKING|LISTENING|SPEAKING|CONFUSED|SLEEPING
TALK ON|OFF|TOGGLE
DEBUG ON|OFF|TOGGLE
```
