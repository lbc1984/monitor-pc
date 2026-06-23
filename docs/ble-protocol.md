# Giao thức BLE

## GATT
| Thành phần | Giá trị |
|------------|---------|
| Device name | `ESP32-Stats-Monitor` |
| Service UUID | `9f1f8f00-1d7d-4b4f-9da8-f2f89d9ef001` |
| Characteristic UUID | `9f1f8f01-1d7d-4b4f-9da8-f2f89d9ef001` |
| Properties | `WRITE` \| `WRITE_NR` (write no response) |

Định nghĩa ở cả 2 phía: [../include/config.h](../include/config.h) (firmware) và hằng số
`SERVICE_UUID`/`CHAR_UUID` trong [../pc_ble_sender.py](../pc_ble_sender.py). **Đổi UUID phải
sửa cả hai.**

Server NimBLE ([../src/ble_server.cpp](../src/ble_server.cpp)) advertise service UUID + tên.
Khi disconnect tự gọi lại `startAdvertising()` để PC reconnect.

## Payload JSON
PC ghi JSON compact (không khoảng trắng) vào characteristic. Firmware parse bằng ArduinoJson,
merge từng field — **field thiếu giữ nguyên giá trị cũ** (toán tử `doc["x"] | tmp.x`). Nhờ vậy
có thể tách payload thành 2 phần luân phiên.

### Part A — metrics (số liệu thay đổi nhanh)
```json
{"cpu":23.4,"gpu_used":41.2,"gpu_temp":62.5,"ram_used":12.7,"ram_total":31.8,"net_up":1.5,"net_down":3.2}
```

### Part B — identity (thông tin ít đổi)
```json
{"cpu_name":"Intel...","gpu_name":"NVIDIA...","wifi_name":"MyWiFi","datetime":"21/06/2026 22:14:30"}
```
> Lưu ý: `datetime` được gửi trong **Part B**, không nằm trong Part A.

## Luân phiên & fallback
- `pc_ble_sender.py` toggle cờ `send_identity_part` mỗi lần gửi → xen kẽ Part A / Part B,
  mỗi `SEND_INTERVAL = 0.2s`.
- Nếu payload > `MAX_BLE_PAYLOAD_BYTES = 220`, builder rút gọn dần:
  - Part A: bỏ `net_down` (`metrics-min`)
  - Part B: bỏ tên CPU/GPU, chỉ còn wifi+datetime (`identity-wifi`), cuối cùng chỉ `wifi_name`
    (`identity-min`)

Chi tiết builder: [python-sender.md](python-sender.md).
