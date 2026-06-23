# Python sender (`pc_ble_sender.py`)

Script chạy trên Windows, thu thập thông số máy và gửi sang ESP32 qua BLE. Nguồn:
[../pc_ble_sender.py](../pc_ble_sender.py).

## Dependencies
- `bleak` — BLE client (scan, connect, write)
- `psutil` — CPU / RAM / Network

```
pip install bleak psutil
```

## Hằng số chính
| Hằng số | Giá trị | Ý nghĩa |
|---------|---------|---------|
| `DEVICE_NAME` | `ESP32-Stats-Monitor` | Tên BLE cần tìm |
| `SERVICE_UUID` / `CHAR_UUID` | (khớp firmware) | UUID GATT |
| `SEND_INTERVAL` | 0.2 | Chu kỳ gửi (giây) |
| `SCAN_TIMEOUT` | 6.0 | Timeout quét BLE |
| `MAX_BLE_PAYLOAD_BYTES` | 220 | Ngưỡng kích hoạt fallback rút gọn payload |
| `DATETIME_FORMAT` | `%d/%m/%Y %H:%M:%S` | Định dạng datetime |
| `DEVICE_ADDRESS` | env `ESP32_BLE_ADDRESS` | Override địa chỉ BLE (bỏ qua quét) |

## Thu thập metric (mỗi chu kỳ)
| Hàm | Nguồn |
|-----|-------|
| `get_cpu_percent()` | `psutil.cpu_percent(interval=None)` |
| `get_ram_used_total_gb()` | `psutil.virtual_memory()` → (used, total) GB |
| `get_gpu_metrics()` | `nvidia-smi --query-gpu=utilization.gpu,temperature.gpu` |
| `get_net_speed_kbps()` | delta `psutil.net_io_counters()` (lưu state qua function attribute) |
| `get_current_datetime_str()` | `datetime.now()` |

## Query identity (chạy 1 lần lúc start)
| Hàm | Lệnh |
|-----|------|
| `_check_nvidia()` | `nvidia-smi --query-gpu=count` (phát hiện có GPU NVIDIA) |
| `_get_cpu_model()` | `wmic cpu get name` |
| `_get_gpu_model()` | `nvidia-smi --query-gpu=name` (chỉ khi có NVIDIA) |
| `_get_wifi_ssid()` | `netsh wlan show interfaces` (kiểm tra state "connected" trước khi lấy SSID) |

Kết quả lưu vào biến module `_CPU_NAME` / `_GPU_NAME` / `_WIFI_NAME`, dùng lại trong Part B.

## Vòng đời kết nối
1. `find_device_address()` — quét BLE (hoặc dùng `ESP32_BLE_ADDRESS` nếu set), khớp theo
   `local_name` hoặc service UUID; in danh sách thiết bị lân cận để debug nếu không thấy.
2. `send_loop(address)` — `BleakClient` connect, vòng lặp `build_payload_part()` →
   `write_gatt_char(..., response=False)`, toggle Part A/B, `sleep(SEND_INTERVAL)`.
3. `main()` — phát hiện GPU/CPU/WiFi 1 lần, đăng ký SIGINT/SIGTERM, vòng ngoài tự reconnect:
   retry 2s nếu không thấy thiết bị, 1.5s sau khi mất kết nối.

Xây dựng payload & fallback: [ble-protocol.md](ble-protocol.md).
