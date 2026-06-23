# Kiến trúc & luồng dữ liệu

## Tổng quan hệ thống
Hệ thống gồm 2 thành phần chạy độc lập, giao tiếp qua BLE:

```
┌────────────────────────┐        BLE GATT Write         ┌──────────────────────────┐
│  PC (Windows)          │  ───────────────────────────► │  ESP32-S3                 │
│  pc_ble_sender.py      │   JSON payload mỗi 200 ms      │  firmware (Arduino/C++)   │
│  - psutil / nvidia-smi │                                │  - NimBLE GATT server     │
│  - wmic / netsh        │                                │  - LovyanGFX → ILI9341    │
└────────────────────────┘                                └──────────────────────────┘
```

## Luồng dữ liệu (PC → màn hình)
1. **PC thu thập metric** — `pc_ble_sender.py` đọc CPU/RAM (psutil), GPU (nvidia-smi),
   Network (delta `net_io_counters`), tên thiết bị (wmic/nvidia-smi/netsh), datetime.
2. **Gửi BLE** — ghi JSON vào characteristic (`write_gatt_char`, không cần response) mỗi
   `SEND_INTERVAL = 0.2s`, luân phiên payload Part A (metrics) / Part B (identity).
   Xem [ble-protocol.md](ble-protocol.md).
3. **ESP32 nhận & parse** — `StatsCharCallback::onWrite` (trong [../src/ble_server.cpp](../src/ble_server.cpp))
   deserialize JSON bằng ArduinoJson, merge từng field vào bản tạm `tmp` (giữ giá trị cũ nếu
   field thiếu), rồi copy vào `g_stats` và bật cờ `g_new_data` — tất cả trong critical section
   `g_mux`.
4. **loop() vẽ lại** — `loop()` trong [../src/main.cpp](../src/main.cpp) đọc `g_stats` ra biến
   cục bộ (trong `g_mux`) rồi gọi các hàm vẽ từng section qua sprite.

## Bản đồ module
| File | Vai trò |
|------|---------|
| [../include/config.h](../include/config.h) | UUID BLE, tên thiết bị, chân LED, hằng số geometry/layout |
| [../include/stats.h](../include/stats.h) + [../src/stats.cpp](../src/stats.cpp) | Struct `Stats` và state telemetry dùng chung |
| [../include/colors.h](../include/colors.h) + [../src/colors.cpp](../src/colors.cpp) | Bảng màu RGB565 + helper màu theo ngưỡng |
| [../include/led_status.h](../include/led_status.h) + [../src/led_status.cpp](../src/led_status.cpp) | Điều khiển LED trạng thái WS2812 |
| [../include/display.h](../include/display.h) + [../src/display.cpp](../src/display.cpp) | Khởi tạo LovyanGFX + vẽ dashboard |
| [../include/ble_server.h](../include/ble_server.h) + [../src/ble_server.cpp](../src/ble_server.cpp) | NimBLE server, callback, parse JSON |
| [../include/lgfx.h](../include/lgfx.h) | Cấu hình phần cứng driver LovyanGFX ILI9341 |
| [../src/main.cpp](../src/main.cpp) | Chỉ điều phối setup/loop |

## Logic vòng lặp (`loop()`)
1. Đọc trạng thái `g_ble_connected`. Nếu vừa **mất kết nối** → vẽ lại splash.
2. Nếu **vừa connect** (`splash_shown`) → `clearDisplay()` rồi vẽ toàn bộ các section với
   data hiện có, đặt lại mốc footer.
3. Nếu **chưa connect** → `delay(500)` và bỏ qua.
4. Nếu **đang connect & có `g_new_data`** → đọc ra bản cục bộ, vẽ lại Header/CPU/GPU/RAM/Net.
5. **Footer tự refresh mỗi 1000 ms** bất kể có data mới hay không (đồng hồ luôn cập nhật).

## State dùng chung (ISR-safe)
BLE callback chạy trên task riêng, `loop()` chạy task khác → mọi truy cập `g_stats` /
`g_new_data` phải nằm trong `portENTER_CRITICAL(&g_mux)` / `portEXIT_CRITICAL(&g_mux)`.
Chi tiết struct: [firmware.md](firmware.md).

## Máy trạng thái LED RGB
| Sự kiện | Màu | Hàm |
|---------|-----|-----|
| Khởi động / chờ BLE | Xanh dương (0,0,255) | `setLedWaiting()` |
| BLE connected | Xanh lá (0,255,0) | `setLedConnected()` |
| BLE disconnected | Đỏ (255,0,0) | `setLedDisconnected()` |

`onDisconnect` còn gọi lại `startAdvertising()` để PC có thể reconnect. Chi tiết phần cứng
LED: [hardware.md](hardware.md).
