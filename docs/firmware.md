# Firmware ESP32

## Struct `Stats` ([../include/stats.h](../include/stats.h))
| Field | Kiểu | Mặc định | Ý nghĩa |
|-------|------|----------|---------|
| `cpu` | float | 0 | % sử dụng CPU |
| `gpu_used` | float | 0 | % sử dụng GPU |
| `gpu_temp` | float | 0 | Nhiệt độ GPU (°C) |
| `ram_used` | float | 0 | RAM đã dùng (GB) |
| `ram_total` | float | 0 | RAM tổng (GB) |
| `net_up` | float | 0 | Upload (KB/s) |
| `net_down` | float | 0 | Download (KB/s) |
| `cpu_name` | char[48] | "CPU" | Tên CPU |
| `gpu_name` | char[48] | "GPU" | Tên GPU |
| `wifi_name` | char[48] | "WiFi" | SSID WiFi |
| `datetime_str` | char[32] | "00:00:00" | Chuỗi ngày giờ từ PC |
| `valid` | bool | false | Đã nhận data hợp lệ |

## State dùng chung ([../src/stats.cpp](../src/stats.cpp))
- `g_stats` — bản telemetry chính
- `g_new_data` (`volatile bool`) — cờ báo có data mới chờ vẽ
- `g_mux` (`portMUX_TYPE`) — spinlock đồng bộ BLE callback ↔ `loop()`
- `g_ble_connected` (`bool`) — trạng thái kết nối BLE

Quy tắc đồng bộ: xem [architecture.md](architecture.md).

## Layout màn hình (portrait 240×320)
Hằng số trong [../include/config.h](../include/config.h):

| Vùng | Y | Cao | Nội dung |
|------|---|-----|----------|
| Header | 0 | 28 | "PC MONITOR" + chấm trạng thái BLE |
| CPU | 28 | 110 | Tên CPU, % lớn (Font7), progress bar, nhãn % |
| GPU | 138 | 110 | Tên GPU, % lớn, nhiệt độ + icon, progress bar |
| RAM | 248 | 24 | Used/Total GB + mini-bar |
| Network | 272 | 24 | SSID, Upload (^), Download (v) |
| Footer | 296 | 24 | Đồng hồ datetime căn giữa |

## Sprite (chống nháy) ([../src/display.cpp](../src/display.cpp))
Mỗi section vẽ vào sprite riêng rồi `pushSprite()` — tránh flicker:

| Sprite | Kích thước | Section |
|--------|-----------|---------|
| `spr_hdr` | 240 × 28 | Header |
| `spr_cpu` | 240 × 110 | CPU |
| `spr_gpu` | 240 × 110 | GPU |
| `spr_ram` | 240 × 24 | RAM |
| `spr_net` | 240 × 24 | Network |
| `spr_ftr` | 240 × 24 | Footer |

Tất cả tạo trong `initDisplay()` với `setColorDepth(16)`. Xem lưu ý màu/swap trong
[hardware.md](hardware.md).

### Font dùng
- `FreeSansBold9pt7b` — tiêu đề header, dấu "%", nhiệt độ
- `Font7` — số % lớn (CPU/GPU)
- `Font0` — nhãn, tên thiết bị, RAM/Net/Footer

### Helper vẽ
- `truncatedText()` — cắt chuỗi + thêm "..." khi vượt độ rộng tối đa
- `drawBar()` — progress bar bo góc có viền (CPU/GPU, cao 20px)
- `drawMiniBar()` — bar nhỏ cho RAM (x=155, w=78, h=14)

### Network row
- SSID ở x=6 (cắt tối đa 60px), `^` upload (cyan) ở x=70, `v` download (magenta) ở x=155
- Tốc độ ≥ 1024 KB/s tự đổi đơn vị sang MB/s

### Footer
Đọc `datetime_str` trong critical section; nếu rỗng/"00:00:00" hiển thị
`--/--/---- --:--:--`.

### Splash screen (`drawSplash()`)
"PC MONITOR" + "Waiting for BLE..." + logo BLE (3 vòng tròn đồng tâm bán kính 20/13/6 + chấm
đặc r=3), màu `C_BLE_OFF`.

## Màu sắc ([../include/colors.h](../include/colors.h))
Nền các vùng = `0x0000` (đen). Palette (BGR-aware, hex đã swap):
`C_RED=0x07E0`, `C_GREEN=0xF800`, `C_BLUE=0x001F`, `C_YELLOW=0xFFE0`, `C_CYAN=0x07FF`,
`C_MAGENTA=0xF81F`, `C_TITLE=0xEFFF`, `C_LABEL=0xAD75`, `C_DIM=0x6B6D`, `C_SEP=0x18C3`.

Helper ngưỡng ([../src/colors.cpp](../src/colors.cpp)):
- `barColor(pct)` — ≥80% đỏ, ≥50% vàng, <50% xanh lá
- `tempColor(t)` — ≥85°C đỏ, ≥70°C vàng, <70°C xanh lá

## Danh sách hàm theo module
**display** ([../include/display.h](../include/display.h)): `initDisplay()`, `clearDisplay()`,
`drawSplash()`, `drawHeader(bool)`, `drawCpuSection(Stats)`, `drawGpuSection(Stats)`,
`drawRamRow(Stats)`, `drawNetRow(Stats)`, `drawFooter()`

**led_status** ([../include/led_status.h](../include/led_status.h)): `initStatusLed()`,
`setLedColor(r,g,b)`, `setLedWaiting()`, `setLedConnected()`, `setLedDisconnected()`

**ble_server** ([../include/ble_server.h](../include/ble_server.h)): `initBleServer()`;
callback nội bộ `StatsCharCallback` (parse JSON), `ServerCallbacks` (connect/disconnect)

**colors**: `barColor(float)`, `tempColor(float)`

## Khi thêm field mới vào `Stats`
Cập nhật đồng bộ: struct ([stats.h](../include/stats.h)) → parse JSON
([ble_server.cpp](../src/ble_server.cpp)) → hàm vẽ ([display.cpp](../src/display.cpp)) →
builder Python ([../pc_ble_sender.py](../pc_ble_sender.py)).
