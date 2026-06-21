# PC Stats Monitor - AI Context

## Tổng quan dự án
Dự án ESP32-S3 PC Stats Monitor: hiển thị thông số PC (CPU, GPU, RAM, Network) lên màn hình ILI9341 240×320 qua BLE.
Gồm 2 thành phần: firmware ESP32 (C++) và Python sender chạy trên Windows.

---

## FIRMWARE ESP32 (`src/main.cpp` + `include/lgfx.hpp`)

### Platform & Board
- PlatformIO, board `esp32-s3-devkitc-1`, Arduino framework
- Dependencies: LovyanGFX ^1.2.0, NimBLE-Arduino ^1.4.3, ArduinoJson ^7.3.1

### Display (ILI9341)
- 240×320 portrait, 16-bit RGB565
- SPI3_HOST: SCLK=12, MOSI=11, MISO=13, DC=46, CS=10, BL=45 (PWM)
- Dùng 6 sprites để vẽ từng section riêng tránh flicker:
  - `spr_hdr` (240×28) - header
  - `spr_cpu` (240×110) - CPU section
  - `spr_gpu` (240×110) - GPU section
  - `spr_ram` (240×24) - RAM row
  - `spr_net` (240×24) - Network row
  - `spr_ftr` (240×24) - Footer clock

### BLE GATT Server
- Service UUID: `9f1f8f00-1d7d-4b4f-9da8-f2f89d9ef001`
- Characteristic UUID: `9f1f8f01-1d7d-4b4f-9da8-f2f89d9ef001`
- Device name: `ESP32-Stats-Monitor`
- Properties: WRITE | WRITE_NR (no response)
- Callbacks: `StatsCharCallback` (parse JSON), `ServerCallbacks` (track connect/disconnect)

### Shared State (ISR-safe)
- `Stats` struct chứa tất cả fields: cpu, gpu_used, gpu_temp, ram_used, ram_total, net_up, net_down, cpu_name, gpu_name, wifi_name, datetime_str, valid
- Dùng `portMUX_TYPE g_mux` + `portENTER_CRITICAL`/`portEXIT_CRITICAL` để đồng bộ BLE callback và loop()
- Flag `g_new_data` báo hiệu có dữ liệu mới

### Layout màn hình (Portrait 240×320)
| Vùng    | Y     | Cao | Nội dung                            |
|---------|-------|-----|-------------------------------------|
| Header  | 0     | 28  | "PC MONITOR" + BLE status dot      |
| CPU     | 28    | 110 | Tên CPU, % lớn, progress bar, tần số |
| GPU     | 138   | 110 | Tên GPU, % lớn, progress bar, nhiệt  |
| RAM     | 248   | 24  | Used/Total GB + mini bar            |
| Network | 272   | 24  | Upload/Download KB/s + WiFi name    |
| Footer  | 296   | 24  | Đồng hồ (hh:mm:ss từ datetime PC)  |

### Color Palette (RGB565, GRB order!)
- C_BG/C_HDR/C_CPU_BG/C_GPU_BG/C_ROW_BG = 0x0000 (black)
- Màu sắc swap GRB: C_RED=0x07E0 (thực tế là xanh lá), C_GREEN=0xF800, C_BLUE=0x001F
- C_TITLE=0xEFFFu, C_LABEL=0xAD75u, C_DIM=0x6B6Du, C_SEP=0x18C3u
- barColor(): >=80% đỏ, >=50% vàng, <50% xanh lá
- tempColor(): >=85°C đỏ, >=70°C vàng, <70°C xanh lá

### Splash Screen
- Hiển thị "PC MONITOR" + "Waiting for BLE..." + BLE logo (vòng tròn đồng tâm) khi chưa kết nối

### Loop logic
1. Kiểm tra BLE connected state → nếu thay đổi: clear screen / show splash
2. Nếu vừa connect: fill black, vẽ tất cả sections với data hiện tại
3. Nếu có `g_new_data`: đọc vào local copy, vẽ lại Header/CPU/GPU/RAM/Net
4. Footer tự refresh mỗi 1 giây (bất kể có data mới hay không)

---

## PYTHON SENDER (`pc_ble_sender.py`)

### Dependencies
- bleak (BLE client), psutil (system stats)

### Cách hoạt động
- Quét BLE tìm device name "ESP32-Stats-Monitor" hoặc service UUID
- Kết nối, gửi telemetry mỗi 200ms (`SEND_INTERVAL = 0.2`)
- Luân phiên 2 loại payload (send_identity_part toggle):
  - **Part A (metrics)**: cpu, gpu_used, gpu_temp, ram_used, ram_total, net_up, net_down
  - **Part B (identity)**: cpu_name, gpu_name, wifi_name, datetime
- Fallback nếu payload > 220 bytes (`MAX_BLE_PAYLOAD_BYTES`)
- Tự động reconnect khi mất kết nối

### Metrics collection
- **CPU**: `psutil.cpu_percent(interval=None)`
- **RAM**: `psutil.virtual_memory()` → used/total GB
- **GPU**: `nvidia-smi --query-gpu=utilization.gpu,temperature.gpu` (chỉ khi có NVIDIA)
- **Network**: `psutil.net_io_counters()` tính delta KB/s
- **CPU name**: `wmic cpu get name` (chạy 1 lần khi start)
- **GPU name**: `nvidia-smi --query-gpu=name` (chạy 1 lần)
- **WiFi SSID**: `netsh wlan show interfaces` (chạy 1 lần)
- **Datetime**: `datetime.now().strftime("%d/%m/%Y %H:%M:%S")`

### BLE Write
- Dùng BleakClient, write_gatt_char với response=False
- Có thể override address qua env var `ESP32_BLE_ADDRESS`

---

## Cấu trúc JSON Payload

### Part A (metrics)
```json
{"cpu":23.4,"gpu_used":41.2,"gpu_temp":62.5,"ram_used":12.7,"ram_total":31.8,"net_up":1.5,"net_down":3.2}
```

### Part B (identity)
```json
{"cpu_name":"Intel...","gpu_name":"NVIDIA...","wifi_name":"MyWiFi","datetime":"21/06/2026 22:14:30"}
```

---

## Lưu ý khi sửa code
- Khi thay đổi cấu trúc JSON payload: cần update cả firmware (parse) và Python sender (build)
- Khi thay đổi BLE UUIDs: cần update cả `SERVICE_UUID`/`CHAR_UUID` ở 2 bên
- Khi thay đổi display geometry: cập nhật constants `LCD_W`, `LCD_H`, các `*_Y`, `*_H`
- Mọi shared state giữa BLE callback và loop() phải dùng critical section
- Nếu thêm field mới vào `Stats`: cập nhật struct, BLE parse, draw functions, splash init