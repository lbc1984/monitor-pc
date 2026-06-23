# PC Stats Monitor — AI Context

## Tổng quan
Dự án ESP32-S3 hiển thị thông số PC (CPU, GPU, RAM, Network) lên màn hình ILI9341 240×320 qua
BLE. Gồm 2 thành phần:
- **Firmware ESP32** (C++/Arduino, PlatformIO) — nhận telemetry qua BLE, vẽ dashboard.
- **Python sender** (`pc_ble_sender.py`, Windows) — thu thập số liệu máy, gửi qua BLE.

Tài liệu chi tiết được tách theo chủ đề và import bên dưới — đọc đúng file khi cần.

## Lưu ý quan trọng (đọc trước khi sửa display)
- **Sprite phải gọi `setSwapBytes(true)`** sau `createSprite()` — thiếu sẽ sai màu. Hiện code
  đặt swap ở mức `lcd` trong `initDisplay()`; đừng swap byte hai lần ở nơi khác.
- Panel BGR (`cfg.rgb_order = true`) → hex bị swap: `RED=0x07E0`, `GREEN=0xF800`, `BLUE=0x001F`.
- Giữ `cfg.dlen_16bit = false` (đổi sang `true` → màn hình trắng).
- Mọi state dùng chung giữa BLE callback và `loop()` phải nằm trong critical section `g_mux`.

## Quy tắc khi sửa code (xuyên suốt)
- Đổi cấu trúc JSON payload → sửa **cả** firmware parse (`src/ble_server.cpp`) **và** Python
  builder (`pc_ble_sender.py`).
- Đổi BLE UUID → sửa cả `config.h` và `pc_ble_sender.py`.
- Đổi geometry màn hình → cập nhật hằng số trong `include/config.h` (`LCD_W`, `LCD_H`, các `*_Y`/`*_H`).
- Thêm field vào `Stats` → cập nhật struct + parse JSON + hàm vẽ + builder Python.

## Tài liệu chi tiết
@docs/architecture.md
@docs/hardware.md
@docs/firmware.md
@docs/ble-protocol.md
@docs/python-sender.md
@docs/build-and-run.md
