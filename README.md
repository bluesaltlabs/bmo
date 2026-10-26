# BMO

Local-first desk robot project inspired by *Adventure Time* BMO.

## What exists so far

- Phase 1 firmware (`src/main.cpp`): animated face on the Waveshare ESP32-C6 1.47" LCD, flashed and running on the dev board.
- Phase 2 firmware (`audio_src/main.cpp`): ESP32-WROOM stand-in sending `FACE` commands over UART with ACK, heartbeat and timeout. Verified on hardware.
- PlatformIO project (`platformio.ini`) using the pioarduino platform (required for Arduino on ESP32-C6).
- Agent and skill docs: `AGENTS.md`, `agents/`, `skills/`.
- Design record: [DECISIONS.md](DECISIONS.md). Roadmap: [CONTEXT.md](CONTEXT.md).
- Not yet built: audio hardware, wake word, Raspberry Pi link, networking (Phase 3 onward).
- Known open items: none for Phase 2. Phase 1 serial `STATUS` is now verified.

## Voice engine (Phase 3 start)

BMO's speech is a small on-device formant synthesizer (`lib/bmo_voice/`): text, then rule-based phonemes, then a glottal source through formant resonators, then 16 kHz mono PCM. It needs no heap, network or voice files, and the same code runs on the ESP32 and on this computer.

Try it on this machine (macOS, no extra installs):

```bash
voice_demo/run.sh                          # plays the demo phrases
voice_demo/run.sh "Hello! I am BMO."       # speak a line
voice_demo/run.sh -i                       # type lines interactively
voice_demo/run.sh --wav out.wav "text"     # write a WAV only
voice_demo/run.sh --pitch 380 --formant 1.3 --crush 8 "text"   # tune the voice
```

Tuning flags: `--pitch --speed --formant --inflect --rough --breath --crush --hold --volume`, `--clean` (unprocessed), `--mouth` (prints mouth-openness trace). Bit-crush and sample-hold effects are off by default to avoid added static; enable them with `--crush` and `--hold`. Defaults live in `VoiceConfig::bmo()` in `lib/bmo_voice/src/bmo_voice.h`.

Firmware bring-up: `.venv/bin/pio run -e voice_esp32_wroom -t upload --upload-port <port>` speaks lines typed on the serial console through an I2S DAC (placeholder pins in `platformio.ini`; builds but not yet run on hardware). `Voice::mouth()` gives 0..255 mouth openness for the face's speaking animation.

## Phase 1 (implemented)

Phase 1 targets only the Waveshare ESP32-C6 1.47 LCD board and delivers a standalone face engine with:

- default **landscape orientation**
- Face-only look matched to `bmo-faces.jpg`: the whole LCD is the light-mint screen (`#d2fce4`) with `#171717` eyes/outlines and green mouths; no body, bezel or button is drawn (the physical case provides those). `#ffb600` debug and `#ff66cc` talk indicator dots appear in the top-right corner
- expression states: `idle`, `happy`, `thinking`, `listening`, `speaking`, `confused`, `sleeping`, `angry`, `sad` (each modeled on a face in `bmo-faces.jpg`)
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

Send newline-terminated ASCII commands. Expressions: `IDLE HAPPY THINKING LISTENING SPEAKING CONFUSED SLEEPING ANGRY SAD`.
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

## Phase 2: two-board link

The audio-board stand-in (`audio_src/main.cpp`, env `audio_esp32_wroom`) talks to the display over UART with a line protocol:

| Direction | Message | Meaning |
|---|---|---|
| audio → display | `FACE <name>` | Set expression; display replies `ACK FACE <NAME>` |
| audio → display | `PING` (every 1 s) | Heartbeat; display replies `PONG` |
| display | (3 s of silence) | Prints `LINK LOST -> IDLE`, face returns to IDLE, talk animation off |
| either | malformed line | `ERR ...` reply, no state change |

The audio board retries an unacknowledged `FACE` up to 3 times (250 ms apart), then gives up. Link wiring (both boards cross-connected):

| Display (C6) | Audio (WROOM) |
|---|---|
| GPIO17 (RX) | GPIO17 (TX2) |
| GPIO16 (TX) | GPIO16 (RX2) |
| GND | GND |

Audio-board controls: BOOT (GPIO0) button sends the next face; the USB console accepts a face name, `DEMO ON|OFF` (auto-cycle every 3 s) and `RAW <text>` (send an unmodified line, for malformed-input tests).

```bash
.venv/bin/pio run -e audio_esp32_wroom -t upload --upload-port /dev/cu.usbserial-XXXX
```

Status: verified on hardware with both boards wired. Tested: face changes with ACK, 1 s heartbeat, malformed lines (`ERR`, state unchanged), repeated commands, the display timing out to IDLE after 3 s of silence, and automatic recovery after resetting either board. Pressing the audio board's BOOT button changing the face on the display was confirmed visually.

Known quirks:
- GPIO16/17 are the C6's UART0 pins, so its ROM boot log is sent down the link when it resets. The audio board ignores it, but a different pin pair would avoid the noise.
- The audio board retries a `FACE` that the display rejects with `ERR` until it gives up after 3 tries.

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

`<name>` is one of `IDLE`, `HAPPY`, `THINKING`, `LISTENING`, `SPEAKING`, `CONFUSED`, `SLEEPING`, `ANGRY`, `SAD`.

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
