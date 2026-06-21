# Display Configuration

## Overview
This project uses **LovyanGFX** as the graphics driver to control a color LCD display on the ESP32-S3.

---

## Hardware Configuration

### Display Panel
- **Resolution**: 240 × 320 pixels
- **Color Depth**: 16-bit RGB565 (5-6-5 format)
- **Controller**: ILI9341 (LovyanGFX `Panel_ILI9341`)

### Current Configuration
- **Active Controller**: ILI9341
- **Panel Invert**: Enabled (`cfg.invert = true`)
- **Color Order**: BGR (`cfg.rgb_order = false`)
- **Command/Data Length**: 8-bit command/data mode (`cfg.dlen_16bit = false`)

These values are verified as the stable configuration for this board.

---

## SPI Communication

### SPI Bus Settings
| Parameter | Value | Notes |
|-----------|-------|-------|
| Host | SPI3_HOST | Primary SPI interface |
| Mode | 0 | Standard SPI mode |
| Write Frequency | 40 MHz | Display write speed |
| Read Frequency | 16 MHz | Display read speed |
| 3-Wire Mode | Disabled | Uses separate MOSI/MISO lines |
| DMA Channel | Auto | Automatic DMA allocation |

---

## Pin Configuration

### SPI Pins
| Function | GPIO | Notes |
|----------|------|-------|
| SCLK (Clock) | 12 | Serial clock for SPI |
| MOSI (Data Out) | 11 | Master Out, Slave In |
| MISO (Data In) | 13 | Master In, Slave Out |
| DC (Data/Command) | 46 | Data = 1, Command = 0 |

### Display Control Pins
| Function | GPIO | Notes |
|----------|------|-------|
| CS (Chip Select) | 10 | Active low |
| RST (Reset) | -1 | Not used (disabled) |
| BL (Backlight PWM) | 45 | Brightness control |
| BUSY | -1 | Not used |

### Backlight Configuration
- **Pin**: GPIO 45
- **Type**: PWM-controlled
- **Frequency**: 44.1 kHz
- **PWM Channel**: 7
- **Invert**: Disabled (active high)

---

## Build Configuration

### PlatformIO Settings
```ini
[env:esp32s3]
platform = espressif32
board = esp32-s3-devkitc-1
framework = arduino

monitor_speed = 115200

build_flags =
    -DARDUINO_USB_CDC_ON_BOOT=1
    -DARDUINO_USB_MODE=1
    -Iinclude

lib_deps =
    lovyan03/LovyanGFX
    h2zero/NimBLE-Arduino
```

### Build Flags
| Flag | Purpose |
|------|---------|
| `ARDUINO_USB_CDC_ON_BOOT=1` | Enable USB CDC debug output on boot |
| `ARDUINO_USB_MODE=1` | USB mode configuration |
| `-Iinclude` | Include path for custom headers |

---

## Display Initialization

The display is initialized through the custom `LGFX` class defined in [include/lgfx.hpp](include/lgfx.hpp), which:

1. Configures SPI bus with specified pins and speeds
2. Initializes panel dimensions and memory mapping
3. Sets up PWM backlight control
4. Connects panel and light peripherals

### Verified Panel Flags (from `include/lgfx.hpp`)

| Flag | Value | Why |
|------|-------|-----|
| `cfg.invert` | `true` | Matches this panel's expected display polarity and avoids washed/odd colors |
| `cfg.rgb_order` | `false` | Uses BGR order (matches LCDWiki ILI9341 init `MADCTL=0x08`) |
| `cfg.dlen_16bit` | `false` | Prevents white-screen issue seen with `true` on this hardware |

### Color Troubleshooting Notes

If colors look wrong again:

1. Keep `cfg.dlen_16bit = false` (changing this to `true` caused white-screen output).
2. Verify `cfg.rgb_order` against panel behavior (red/blue swapped means wrong order).
3. If image polarity looks inverted/strange, toggle `cfg.invert`.

---

## References
- **LovyanGFX**: https://github.com/lovyan03/LovyanGFX
- **LCDWiki Board Page**: https://www.lcdwiki.com/2.8inch_ESP32-S3_Display
- **ILI9341 Init (LCDWiki)**: https://www.lcdwiki.com/res/ES3C28P/ILI9341V_Init.txt
- **ESP32-S3 DevKit-C-1**: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/hw-reference/en/user_guide-esp32-s3-devkitc-1.html

---

## App UI Color Mapping (GRB)

For this project, application-level color constants are maintained with GRB-aware naming in the dashboard code:

- `C_RED = 0x07E0`
- `C_GREEN = 0xF800`
- `C_BLUE = 0x001F`

Status colors in the UI use:

- `C_BLE_ON = C_GREEN`
- `C_BLE_OFF = C_RED`

These constants are defined in [src/main.cpp](src/main.cpp).
