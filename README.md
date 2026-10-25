# BMO

Local-first desk robot project inspired by *Adventure Time* BMO.

## What exists so far

- Phase 1 firmware (`src/main.cpp`): animated face on the Waveshare ESP32-C6 1.47" LCD, flashed and running on the dev board.
- PlatformIO project (`platformio.ini`) using the pioarduino platform (required for Arduino on ESP32-C6).
- Agent and skill docs: `AGENTS.md`, `agents/`, `skills/`.
- Design record: [DECISIONS.md](DECISIONS.md). Roadmap: [CONTEXT.md](CONTEXT.md).
- Not yet built: audio, wake word, Raspberry Pi link, networking (later phases).
- Known open items: flicker fix awaiting visual confirmation; serial `STATUS` not yet verified at runtime.

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
├── CONTEXT.md
├── DECISIONS.md
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
pio run -e waveshare_esp32_c6_lcd_147_4mb -t upload --upload-port /dev/cu.usbmodemXXXX
pio device monitor -b 115200
```

If your board variant is 8MB, use `waveshare_esp32_c6_touch_lcd_147_8mb`.

## Controlling expressions

Send newline-terminated ASCII commands. Expressions: `IDLE HAPPY THINKING LISTENING SPEAKING CONFUSED SLEEPING`.
`STATE HAPPY` and the shorthand `HAPPY` are equivalent. Every command gets a reply (`OK ...` / `ERR ...`) on the link that sent it.

**From your computer (USB):** see [Accessing the terminal interface](#accessing-the-terminal-interface), then type `STATE HAPPY` + Enter.

**From the second ESP32 (audio board) over UART:** wire a cross-connected UART link to the display board's `Serial1`:

| Display board | Audio ESP32 |
|---|---|
| GPIO17 (RX) | its TX pin |
| GPIO16 (TX) | its RX pin |
| GND | GND |

115200 8N1, 3.3 V logic. Pins/baud are overridable via `BMO_PIN_LINK_RX`, `BMO_PIN_LINK_TX`, `BMO_LINK_BAUD`. On the audio ESP32 (Arduino):

```cpp
Serial1.begin(115200, SERIAL_8N1, /*rx=*/RX_PIN, /*tx=*/TX_PIN);
Serial1.println("LISTENING");   // wake word heard
Serial1.println("THINKING");    // processing
Serial1.println("SPEAKING");    // playing reply
Serial1.println("IDLE");
```

Verify GPIO16/17 are free on your header before wiring.

## Serial commands

Commands are newline-delimited, case-insensitive, max 95 characters.

| Command | Effect | Reply |
|---|---|---|
| `HELP` | Print the command list | command list |
| `STATUS` | Print current state | `{"state":"HAPPY","talk":false,"debug":false}` |
| `NEXT` | Advance to the next expression | `OK STATE <name>` |
| `BLINK` | Trigger a blink | `OK BLINK` |
| `STATE <name>` | Set expression | `OK STATE <name>` |
| `<name>` | Shorthand for `STATE <name>` | `OK STATE <name>` |
| `TALK ON` / `OFF` / `TOGGLE` | Talking mouth animation | `OK TALK ON\|OFF` |
| `DEBUG ON` / `OFF` / `TOGGLE` | Auto-cycle expressions every 4 s | `OK DEBUG ON\|OFF` |

`<name>` is one of `IDLE`, `HAPPY`, `THINKING`, `LISTENING`, `SPEAKING`, `CONFUSED`, `SLEEPING`.

Errors: `ERR unknown state`, `ERR unknown command`, `ERR TALK expects ON/OFF/TOGGLE`, `ERR DEBUG expects ON/OFF/TOGGLE`, `ERR command too long`.

BOOT button (GPIO9): short press = `NEXT`, double press = `BLINK`, hold ≥1.2 s = `TALK TOGGLE`, hold ≥3 s = `DEBUG TOGGLE`. Each press is logged as `BTN ...`.

## Accessing the terminal interface

The board's USB-C port appears as a serial device (USB-Serial/JTAG, 115200 baud). The same console carries commands, replies, boot messages and button logs.

1. Plug in the board and find the port: `ls /dev/cu.usbmodem*` (macOS) or `ls /dev/ttyACM*` (Linux).
2. Set up PlatformIO once: `python3 -m venv .venv && .venv/bin/pip install platformio`.
3. Open the monitor:
   ```bash
   .venv/bin/pio device monitor -p /dev/cu.usbmodemXXXX -b 115200 --echo --eol LF
   ```
4. Type a command (e.g. `HAPPY`) and press Enter. Exit with Ctrl+C.

Alternatives: `screen /dev/cu.usbmodemXXXX 115200` (exit: Ctrl+A, K, Y), or a one-shot `echo "HAPPY" > /dev/cu.usbmodemXXXX`.

Notes:
- Only one program can hold the port; close the monitor before flashing.
- Opening the port can reset the board (DTR/RTS toggle); wait about a second for the boot banner.
- Do not hold BOOT while connecting; that enters download mode and the face won't show. Hold BOOT + tap RESET only to flash when upload fails.
- If the port is missing, replug the cable (use a data cable).
