# Phần cứng

Board: **ESP32-S3 DevKit-C-1** (Arduino framework). Nguồn chuẩn của mọi giá trị dưới đây là
[../include/lgfx.h](../include/lgfx.h), [../include/config.h](../include/config.h) và
[../src/led_status.cpp](../src/led_status.cpp).

## Màn hình ILI9341
- **Độ phân giải**: 240 × 320 pixel, 16-bit RGB565 (5-6-5)
- **Controller**: ILI9341 (LovyanGFX `Panel_ILI9341`)
- **Rotation**: 0 (portrait), brightness mặc định 100

### SPI bus
| Tham số | Giá trị |
|---------|---------|
| Host | SPI3_HOST |
| Mode | 0 |
| Freq write | 40 MHz |
| Freq read | 16 MHz |
| DMA channel | `SPI_DMA_CH_AUTO` |
| Bus shared | true |

### Sơ đồ chân
| Chức năng | GPIO |
|-----------|------|
| SCLK | 12 |
| MOSI | 11 |
| MISO | 13 |
| DC | 46 |
| CS | 10 |
| RST | -1 (không dùng) |
| BUSY | -1 (không dùng) |
| BL (PWM) | 45 |

### Panel flags (đã kiểm chứng)
| Flag | Giá trị | Lý do |
|------|---------|-------|
| `cfg.invert` | `true` | Khớp phân cực panel, tránh màu bạc/nhạt |
| `cfg.rgb_order` | `true` | Panel BGR → giá trị hex bị swap trong code |
| `cfg.dlen_16bit` | `false` | Tránh lỗi màn hình trắng |
| `cfg.dummy_read_pixel` | `8` | Cần khi đọc pixel từ panel |

### Backlight (PWM)
- Chân 45, tần số 44.1 kHz, PWM channel 7, `cfg.invert = true`

## LED RGB trạng thái (WS2812 / NeoPixel)
| Tham số | Giá trị |
|---------|---------|
| Chân | GPIO 42 (`LED_RGB_PIN`) |
| Số LED | 1 (`NUM_PIXELS`) |
| Loại | WS2812, `NEO_GRB + NEO_KHZ800` |
| Thư viện | Adafruit NeoPixel |
| Brightness | **100** (0–255) — đặt trong `initStatusLed()` |

Hành vi màu: xem máy trạng thái LED trong [architecture.md](architecture.md).

## Lưu ý xử lý sự cố màu
- Panel có thứ tự BGR (`cfg.rgb_order = true`) → tên màu bị swap trong code:
  `C_RED = 0x07E0`, `C_GREEN = 0xF800`, `C_BLUE = 0x001F`.
- **Sprite bắt buộc gọi `setSwapBytes(true)`** sau `createSprite()` — thiếu sẽ sai màu (đây
  là nguyên nhân lỗi màu phổ biến nhất). Lưu ý: trong code hiện tại `setSwapBytes(true)` được
  gọi ở mức `lcd` trong `initDisplay()`.
- **Không gọi double swap** không cần thiết — tránh đảo byte 2 lần.
- Giữ `cfg.dlen_16bit = false`; đổi sang `true` gây màn hình trắng.
- Nếu ảnh bị đảo phân cực (âm bản), thử bật/tắt `cfg.invert`.

## Tham khảo phần cứng
- LovyanGFX: https://github.com/lovyan03/LovyanGFX
- LCDWiki board: https://www.lcdwiki.com/2.8inch_ESP32-S3_Display
- ILI9341 init (LCDWiki): https://www.lcdwiki.com/res/ES3C28P/ILI9341V_Init.txt
- ESP32-S3 DevKit-C-1: https://docs.espressif.com/projects/esp-idf/en/stable/esp32s3/hw-reference/en/user_guide-esp32-s3-devkitc-1.html
