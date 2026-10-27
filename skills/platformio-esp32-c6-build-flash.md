# Skill: PlatformIO ESP32-C6 Build & Flash

## Purpose

Build and upload Phase 1 firmware to Waveshare ESP32-C6 1.47 boards with PlatformIO.

## Commands

Run from the repository root after [first-time firmware setup](../docs/firmware.md#build-and-upload).
Replace the example serial port with the connected board's port.

```bash
mkdir -p .tmp
export TMPDIR="$PWD/.tmp"
export PLATFORMIO_CORE_DIR="$PWD/.tmp/platformio"
.venv/bin/pio run -e waveshare_esp32_c6_lcd_147_4mb
.venv/bin/pio run -e waveshare_esp32_c6_lcd_147_4mb -t upload --upload-port /dev/cu.usbmodemXXXX
.venv/bin/pio device monitor -p /dev/cu.usbmodemXXXX -b 115200 --echo --eol LF
```

## Alternate target (8MB variants)

```bash
.venv/bin/pio run -e waveshare_esp32_c6_touch_lcd_147_8mb
.venv/bin/pio run -e waveshare_esp32_c6_touch_lcd_147_8mb -t upload --upload-port /dev/cu.usbmodemXXXX
```

## Expected boot output

- Startup banner: `BMO Phase 1 face engine booting...`
- Help text listing available serial commands.
