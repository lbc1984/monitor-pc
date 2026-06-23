# Build & chạy

## Firmware ESP32 (PlatformIO)
Cấu hình: [../platformio.ini](../platformio.ini)

| Mục | Giá trị |
|-----|---------|
| Env | `esp32s3` |
| Platform | `espressif32` |
| Board | `esp32-s3-devkitc-1` |
| Framework | `arduino` |
| Monitor speed | 115200 |

Build flags: `-DARDUINO_USB_CDC_ON_BOOT=1`, `-DARDUINO_USB_MODE=1`, `-Iinclude`,
`-DCORE_DEBUG_LEVEL=0`.

Dependencies (`lib_deps`):
- `lovyan03/LovyanGFX@^1.2.0`
- `h2zero/NimBLE-Arduino@^1.4.3`
- `bblanchon/ArduinoJson@^7.3.1`
- `adafruit/Adafruit NeoPixel@^1.12.3`

### Lệnh
```bash
pio run                       # build
pio run -t upload             # build + nạp firmware
pio device monitor -b 115200  # xem log serial
```

## Python sender (Windows)
```bash
pip install bleak psutil
python pc_ble_sender.py
```

- Mặc định tự quét tìm `ESP32-Stats-Monitor`.
- Có thể cố định địa chỉ BLE để bỏ qua quét:
  ```bash
  set ESP32_BLE_ADDRESS=XX:XX:XX:XX:XX:XX   # cmd
  $env:ESP32_BLE_ADDRESS="XX:XX:XX:XX:XX:XX"  # PowerShell
  ```
- GPU metrics chỉ có khi máy có NVIDIA (`nvidia-smi`). WiFi/CPU name dùng lệnh Windows
  (`netsh`, `wmic`).

Chi tiết script: [python-sender.md](python-sender.md).
