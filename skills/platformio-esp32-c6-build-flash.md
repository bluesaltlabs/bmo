# Skill: PlatformIO ESP32-C6 Build & Flash

## Purpose

Build and upload Phase 1 firmware to Waveshare ESP32-C6 1.47 boards with PlatformIO.

## Commands

```bash
pio run -e waveshare_esp32_c6_lcd_147_4mb
pio run -e waveshare_esp32_c6_lcd_147_4mb -t upload
pio device monitor -b 115200
```

## Alternate target (8MB variants)

```bash
pio run -e waveshare_esp32_c6_touch_lcd_147_8mb
pio run -e waveshare_esp32_c6_touch_lcd_147_8mb -t upload
```

## Expected boot output

- Startup banner: `BMO Phase 1 face engine booting...`
- Help text listing available serial commands.
