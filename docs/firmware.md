# Firmware and hardware reference

This page covers the implemented display and Phase 2 UART link, plus the
separate formant voice bring-up target. See the [project status](../README.md)
and [decision record](../DECISIONS.md) for verification and planned Pi work.

## Build and upload

From the repository root, set up the firmware tools once:

```sh
mkdir -p .tmp
export TMPDIR="$PWD/.tmp"
export PIP_CACHE_DIR="$PWD/.tmp/pip-cache"
export PLATFORMIO_CORE_DIR="$PWD/.tmp/platformio"
python3 -m venv .venv
.venv/bin/python -m pip install platformio
```

Keep the environment exports in effect when building in a new shell. The
project uses PlatformIO with Arduino and the pioarduino platform configured in
`platformio.ini`.

| Environment | Firmware |
|---|---|
| `waveshare_esp32_c6_lcd_147_4mb` | Default Waveshare ESP32-C6 display target |
| `waveshare_esp32_c6_touch_lcd_147_8mb` | Alternate display-board target |
| `audio_esp32_wroom` | Phase 2 UART face-command sender |
| `voice_esp32_wroom` | Separate Phase 3 formant/I2S prototype; hardware untested |

Build the display and audio link firmware:

```sh
.venv/bin/pio run -e waveshare_esp32_c6_lcd_147_4mb
.venv/bin/pio run -e audio_esp32_wroom
```

Replace the example ports below with the connected boards' ports. Find them
with `.venv/bin/pio device list`, or inspect `/dev/cu.usbmodem*` and
`/dev/cu.usbserial-*` on macOS (`/dev/ttyACM*` or `/dev/ttyUSB*` on Linux).

```sh
.venv/bin/pio run -e waveshare_esp32_c6_lcd_147_4mb -t upload --upload-port /dev/cu.usbmodemXXXX
.venv/bin/pio run -e audio_esp32_wroom -t upload --upload-port /dev/cu.usbserial-XXXX
.venv/bin/pio device monitor -p /dev/cu.usbmodemXXXX -b 115200 --echo --eol LF
```

Only one program can hold a serial port; close the monitor before uploading.
Opening the console may reset the display. Wait for the boot banner before
sending commands. If the board enters download mode, release BOOT and tap RESET.
Holding BOOT while tapping RESET is the recovery sequence for a failed upload.

## Display board

Target: Waveshare ESP32-C6-LCD-1.47, with a 172 × 320 ST7789 panel rendered in
landscape (320 × 172). The face fills the display; the enclosure supplies the
body and bezel. Framebuffer rendering provides nonblocking animation, automatic
blinking and pupil movement.

| Signal | GPIO |
|---|---|
| LCD MOSI | 6 |
| LCD SCLK | 7 |
| LCD CS | 14 |
| LCD DC | 15 |
| LCD reset | 21 |
| LCD backlight | 22 |
| BOOT button | 9 |

Pins, panel offsets and geometry are build flags in
[platformio.ini](../platformio.ini). Display defaults are
`BMO_LCD_ROTATION=1` and `BMO_LCD_IPS=1`. Appearance follows
[bmo-faces.jpg](../resources/bmo-faces.jpg); palette and rendering decisions are in
[DECISIONS.md](../DECISIONS.md).

Display BOOT controls:

| Gesture | Action |
|---|---|
| Short press | Next expression |
| Double press | Blink |
| Hold at least 1.2 s | Toggle talking animation |
| Hold at least 3 s | Toggle debug expression cycle |

## Display serial commands

Use newline-terminated, case-insensitive ASCII commands, up to 95 characters.
Replies go to the USB console or link UART that received the command. Empty
lines are ignored. Expressions are `IDLE`, `HAPPY`, `THINKING`, `LISTENING`,
`SPEAKING`, `CONFUSED`, `SLEEPING`, `ANGRY` and `SAD`.

| Command | Effect / reply |
|---|---|
| `HELP` | Print command list |
| `STATUS` | JSON containing `state`, `talk`, `debug` and `link` |
| `NEXT` | Next expression; `OK STATE <name>` |
| `BLINK` | Blink; `OK BLINK` |
| `STATE <name>` or `<name>` | Select expression; `OK STATE <name>` |
| `FACE <name>` | Select expression; `ACK FACE <name>` |
| `PING` | `PONG` |
| `TALK ON`, `TALK OFF`, `TALK TOGGLE` | Set/toggle mouth animation; `OK TALK ON` or `OK TALK OFF` |
| `DEBUG ON`, `DEBUG OFF`, `DEBUG TOGGLE` | Set/toggle the 4 s expression cycle; `OK DEBUG ON` or `OK DEBUG OFF` |

For example, type `STATE HAPPY` and press Enter. A status reply looks like:

```json
{"state":"HAPPY","talk":false,"debug":false,"link":true}
```

Invalid input returns `ERR unknown state`, `ERR unknown face`,
`ERR unknown command`, `ERR TALK expects ON/OFF/TOGGLE`,
`ERR DEBUG expects ON/OFF/TOGGLE` or `ERR command too long`.

## Phase 2 UART link

The display uses `Serial1`; the ESP32-WROOM sender uses `Serial2`. Connect the
boards at 115200 baud, 8N1, with 3.3 V logic and common ground:

| Display C6 | Audio WROOM |
|---|---|
| GPIO17 — RX | GPIO17 — TX |
| GPIO16 — TX | GPIO16 — RX |
| GND | GND |

The defaults are in the source files and audio environment. Override
`BMO_PIN_LINK_RX`, `BMO_PIN_LINK_TX` and `BMO_LINK_BAUD` when needed.

The sender transmits `FACE <name>`, waits for `ACK FACE <name>` and retries up
to three times at 250 ms intervals. It sends `PING` every second. On the
display's link UART, valid `PING` and `FACE` messages refresh link activity;
after 3 s without those messages, the display returns to IDLE and stops talking.
Malformed commands return an error without changing the face.

The WROOM BOOT button (GPIO0) sends the next face. Its USB console accepts:

| Input | Action |
|---|---|
| A face name, e.g. `HAPPY` | Send `FACE HAPPY` |
| `DEMO ON` / `DEMO OFF` | Enable/disable a face change every 3 s |
| `RAW <text>` | Bypass the `FACE` wrapper for protocol checks; console input is uppercased |

Hardware verification covered face changes, ACKs, heartbeat, malformed/repeated
commands, timeout and reset recovery. Known quirks: the C6 ROM boot log shares
GPIO16/17, and the sender retries commands even after an `ERR` reply.

## Formant/I2S prototype

`voice_src/main.cpp` is separate firmware, selected with `voice_esp32_wroom`.
It synthesizes typed USB-console text using `lib/bmo_voice/` and sends 16 kHz,
mono PCM to I2S. It does not include the Phase 2 sender or accept Piper PCM.

```sh
.venv/bin/pio run -e voice_esp32_wroom
```

The configured BCLK/LRC/DOUT pins (26/25/22) are placeholders. This target has
been build-checked, not brought up or auditioned on audio hardware. DAC/amp
selection, microphone capture and speaker playback remain open.

## Planned Pi connection

The runtime design gives the Pi its own direct UART to each ESP32. The
audio-to-display UART remains a local reaction path. USB is for flashing and
debugging; a shared 5 V rail powers the robot, and ESP32 runtime power does not
come from the Pi's USB ports.

Piper will run on the Pi and send PCM to the audio ESP32 for playback. UART
framing, buffering, throughput and face timing still need implementation.
`PI_READY`, per-board Pi state tracking, wake-word handling and stored fallback
clips are planned behavior, not features of the current firmware.
