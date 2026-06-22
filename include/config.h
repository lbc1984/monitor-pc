#pragma once

/* BLE UUIDs */
#define SERVICE_UUID "9f1f8f00-1d7d-4b4f-9da8-f2f89d9ef001"
#define CHAR_UUID    "9f1f8f01-1d7d-4b4f-9da8-f2f89d9ef001"
#define DEVICE_NAME  "ESP32-Stats-Monitor"

/* RGB LED (WS2812 / NeoPixel) */
#define LED_RGB_PIN 42
#define NUM_PIXELS  1

/* Display geometry */
static constexpr int LCD_W = 240;
static constexpr int LCD_H = 320;

/* Row Y-coordinates */
static constexpr int HDR_Y = 0;   // header   28 px
static constexpr int CPU_Y = 28;  // CPU sect 110 px
static constexpr int GPU_Y = 138; // GPU sect 110 px
static constexpr int RAM_Y = 248; // RAM row  24 px
static constexpr int NET_Y = 272; // NET row  24 px
static constexpr int FTR_Y = 296; // footer   24 px

static constexpr int HDR_H = 28;
static constexpr int SEC_H = 110; // CPU & GPU section height
static constexpr int ROW_H = 24;
