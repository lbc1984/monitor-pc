/**
 * main.cpp – ESP32-S3 PC Stats Monitor
 *
 * BLE GATT Server receives JSON telemetry from pc_ble_sender.py every 200 ms.
 * Renders a dark-themed dashboard on the 240×320 ILI9341 display.
 *
 * Layout (Portrait 240×320):
 *   [0  – 27]  Header bar  : title + BLE status dot
 *   [28 – 137] CPU section : name, big %, progress bar, freq
 *   [138– 247] GPU section : name, big %, progress bar, temperature
 *   [248– 271] RAM row     : used / total GB + mini bar
 *   [272– 295] NET row     : upload / download KB/s
 *   [296– 319] Footer      : local clock (hh:mm:ss)
 */

#include <Arduino.h>
#include <NimBLEDevice.h>
#include <ArduinoJson.h>
#include "lgfx.hpp"
#include <Adafruit_NeoPixel.h>

/* ═══════════════════════════════════════════════════════════
   BLE UUIDs
   ═══════════════════════════════════════════════════════════ */
#define SERVICE_UUID "9f1f8f00-1d7d-4b4f-9da8-f2f89d9ef001"
#define CHAR_UUID    "9f1f8f01-1d7d-4b4f-9da8-f2f89d9ef001"
#define DEVICE_NAME  "ESP32-Stats-Monitor"

/* ═══════════════════════════════════════════════════════════
   RGB LED (WS2812 / NeoPixel)
   ═══════════════════════════════════════════════════════════ */
#define LED_RGB_PIN    42
#define NUM_PIXELS     1
Adafruit_NeoPixel led(NUM_PIXELS, LED_RGB_PIN, NEO_GRB + NEO_KHZ800);

static void setLedColor(uint8_t r, uint8_t g, uint8_t b) {
    led.setPixelColor(0, led.Color(r, g, b));
    led.show();
}

/* ═══════════════════════════════════════════════════════════
   Display geometry
   ═══════════════════════════════════════════════════════════ */
static constexpr int LCD_W = 240;
static constexpr int LCD_H = 320;

// Row Y-coordinates
static constexpr int HDR_Y  = 0;   // header   28 px
static constexpr int CPU_Y  = 28;  // CPU sect 110 px
static constexpr int GPU_Y  = 138; // GPU sect 110 px
static constexpr int RAM_Y  = 248; // RAM row  24 px
static constexpr int NET_Y  = 272; // NET row  24 px
static constexpr int FTR_Y  = 296; // footer   24 px

static constexpr int HDR_H  = 28;
static constexpr int SEC_H  = 110; // CPU & GPU section height
static constexpr int ROW_H  = 24;

/* ═══════════════════════════════════════════════════════════
   Colour palette  (RGB565)
   ═══════════════════════════════════════════════════════════ */
static constexpr uint32_t C_BG       = 0x0000u; // pure black
static constexpr uint32_t C_HDR      = 0x0000u; // pure black header
static constexpr uint32_t C_CPU_BG   = 0x0000u; // pure black CPU panel
static constexpr uint32_t C_GPU_BG   = 0x0000u; // pure black GPU panel
static constexpr uint32_t C_ROW_BG   = 0x0000u; // pure black rows

static constexpr uint32_t C_TITLE    = 0xEFFFu; // near-white with blue tint
static constexpr uint32_t C_LABEL    = 0xAD75u; // cool grey (slight blue tint)
static constexpr uint32_t C_DIM      = 0x6B6Du; // dim grey-blue
static constexpr uint32_t C_SEP      = 0x18C3u; // deep blue separator

// Device color order is GRB, so these values are mapped to match on-screen color names.
static constexpr uint16_t C_RED     = 0x07E0;
static constexpr uint16_t C_GREEN   = 0xF800;
static constexpr uint16_t C_BLUE    = 0x001F;
static constexpr uint16_t C_YELLOW  = 0xFFE0;
static constexpr uint16_t C_CYAN    = 0x07FF;
static constexpr uint16_t C_MAGENTA = 0xF81F;
static constexpr uint32_t C_BLE_ON   = C_GREEN;
static constexpr uint32_t C_BLE_OFF  = C_RED;

/* ═══════════════════════════════════════════════════════════
   Shared state (written from BLE callback, read from loop)
   ═══════════════════════════════════════════════════════════ */
struct Stats {
    float cpu      = 0.f;
    float gpu_used = 0.f;
    float gpu_temp = 0.f;
    float ram_used = 0.f;
    float ram_total= 0.f;
    float net_up   = 0.f;
    float net_down = 0.f;
    char  cpu_name[48] = "CPU";
    char  gpu_name[48] = "GPU";
    char  wifi_name[48] = "WiFi";
    char  datetime_str[32] = "00:00:00";
    bool  valid    = false;
};

static volatile bool g_new_data = false;
static Stats g_stats;
static portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
static bool g_ble_connected = false;

/* ═══════════════════════════════════════════════════════════
   Display + Sprites
   ═══════════════════════════════════════════════════════════ */
static LGFX           lcd;
static LGFX_Sprite    spr_hdr(&lcd);   // header sprite  240×28
static LGFX_Sprite    spr_cpu(&lcd);   // CPU sprite     240×110
static LGFX_Sprite    spr_gpu(&lcd);   // GPU sprite     240×110
static LGFX_Sprite    spr_ram(&lcd);   // RAM row        240×24
static LGFX_Sprite    spr_net(&lcd);   // NET row        240×24
static LGFX_Sprite    spr_ftr(&lcd);   // footer         240×24

/* ═══════════════════════════════════════════════════════════
   Helpers
   ═══════════════════════════════════════════════════════════ */
static uint32_t barColor(float pct) {
    if (pct >= 80.f) return C_RED;      // 81-100%
    if (pct >= 50.f) return C_YELLOW;   // 51-80%
    return C_GREEN;                      // 0-50%
}

static uint32_t tempColor(float t) {
    if (t >= 85.f) return C_RED;
    if (t >= 70.f) return C_YELLOW;
    return C_GREEN;
}

/** Truncate string to fit maxPx pixels using the sprite's current font. */
static void truncatedText(LGFX_Sprite &s, const char *str, int x, int y,
                          int maxPx, uint32_t color) {
    char buf[64];
    strncpy(buf, str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    int len = strlen(buf);
    s.setTextColor(color);
    while (len > 0 && s.textWidth(buf) > maxPx) {
        buf[--len] = '\0';
        if (len > 2) { buf[len-1] = '.'; buf[len-2] = '.'; buf[len-3] = '.'; }
    }
    s.drawString(buf, x, y);
}

/** Draw a rounded progress bar. */
static void drawBar(LGFX_Sprite &s, int x, int y, int w, int h,
                    float pct, uint32_t col, uint32_t bg = 0x2104u) {
    int filled = (int)(pct / 100.f * (float)(w - 2));
    if (filled < 0) filled = 0;
    if (filled > w - 2) filled = w - 2;

    s.fillRoundRect(x, y, w, h, h / 2, bg);
    if (filled > 0)
        s.fillRoundRect(x + 1, y + 1, filled, h - 2, (h - 2) / 2, col);
    s.drawRoundRect(x, y, w, h, h / 2, C_DIM);
}

/** Draw a small horizontal mini-bar (RAM). */
static void drawMiniBar(LGFX_Sprite &s, int x, int y, int w, int h,
                        float pct, uint32_t col) {
    int filled = (int)(pct / 100.f * (float)(w - 2));
    if (filled < 0) filled = 0;
    if (filled > w - 2) filled = w - 2;

    s.fillRoundRect(x, y, w, h, h / 2, 0x2104u);
    if (filled > 0)
        s.fillRoundRect(x + 1, y + 1, filled, h - 2, (h - 2) / 2, col);
    s.drawRoundRect(x, y, w, h, h / 2, C_DIM);
}

/* ═══════════════════════════════════════════════════════════
   Draw functions – each writes into its sprite then pushes
   ═══════════════════════════════════════════════════════════ */

static void drawHeader(bool ble_on) {
    spr_hdr.fillScreen(C_HDR);

    // Title
    spr_hdr.setTextColor(C_TITLE);
    spr_hdr.setTextSize(1);
    spr_hdr.setFont(&fonts::FreeSansBold9pt7b);
    spr_hdr.drawString("PC MONITOR", 8, 7);

    // BLE dot + label
    uint32_t dot_col = ble_on ? C_BLE_ON : C_BLE_OFF;
    spr_hdr.fillCircle(LCD_W - 14, 14, 5, dot_col);
    spr_hdr.setFont(&fonts::Font0);
    spr_hdr.setTextColor(C_DIM);
    spr_hdr.drawString(ble_on ? "BLE" : "---", LCD_W - 50, 9);

    // Bottom separator
    spr_hdr.drawFastHLine(0, HDR_H - 1, LCD_W, C_SEP);

    spr_hdr.pushSprite(0, HDR_Y);
}

static void drawCpuSection(const Stats &st) {
    spr_cpu.fillScreen(C_CPU_BG);

    // ── CPU name (truncated, dim) ──
    spr_cpu.setFont(&fonts::Font0);
    truncatedText(spr_cpu, st.cpu_name, 8, 5, LCD_W - 44, C_YELLOW);

    // ── Big percentage number – color changes with load ──
    spr_cpu.setFont(&fonts::Font7);          // 7-segment large font
    spr_cpu.setTextSize(1);
    char pct_str[8];
    snprintf(pct_str, sizeof(pct_str), "%4.1f", st.cpu);
    uint32_t col = barColor(st.cpu);
    spr_cpu.setTextColor(col);
    spr_cpu.drawString(pct_str, 6, 18);
    int pct_w = spr_cpu.textWidth(pct_str);

    // Percent sign
    spr_cpu.setFont(&fonts::FreeSansBold9pt7b);
    spr_cpu.setTextColor(col);
    int pct_sign_x = 6 + pct_w + 4;
    if (pct_sign_x > LCD_W - 20) pct_sign_x = LCD_W - 20;
    spr_cpu.drawString("%", pct_sign_x, 36);

    // ── Progress bar (full width) ──
    drawBar(spr_cpu, 6, 76, LCD_W - 12, 20, st.cpu, col);

    // ── Percentage text on bar ──
    spr_cpu.setFont(&fonts::Font0);
    spr_cpu.setTextColor(col);
    char bar_label[16];
    snprintf(bar_label, sizeof(bar_label), "%.1f%%", st.cpu);
    int tw = spr_cpu.textWidth(bar_label);
    spr_cpu.drawString(bar_label, (LCD_W - tw) / 2, 81);

    // ── Bottom separator ──
    spr_cpu.drawFastHLine(0, SEC_H - 1, LCD_W, C_SEP);

    spr_cpu.pushSprite(0, CPU_Y);
}

static void drawGpuSection(const Stats &st) {
    spr_gpu.fillScreen(C_GPU_BG);

    // ── GPU name ──
    truncatedText(spr_gpu, st.gpu_name, 8, 5, LCD_W - 20, C_YELLOW);

    // ── Big percentage number – color changes with load ──
    spr_gpu.setFont(&fonts::Font7);
    char pct_str[8];
    snprintf(pct_str, sizeof(pct_str), "%4.1f", st.gpu_used);
    uint32_t col = barColor(st.gpu_used);
    spr_gpu.setTextColor(col);
    spr_gpu.drawString(pct_str, 6, 18);
    int pct_w = spr_gpu.textWidth(pct_str);

    // Percent sign
    spr_gpu.setFont(&fonts::FreeSansBold9pt7b);
    spr_gpu.setTextColor(col);
    int pct_sign_x = 6 + pct_w + 4;
    if (pct_sign_x > LCD_W - 20) pct_sign_x = LCD_W - 20;
    spr_gpu.drawString("%", pct_sign_x, 36);

    // ── Temperature ──
    uint32_t tc = tempColor(st.gpu_temp);
    spr_gpu.setFont(&fonts::FreeSansBold9pt7b);
    spr_gpu.setTextColor(tc);
    char temp_str[16];
    snprintf(temp_str, sizeof(temp_str),"%.0f"" ""\xB0"" C", st.gpu_temp); // °C
    spr_gpu.drawString(temp_str, 168, 24);

    // Thermometer icon (simple rectangle)
    spr_gpu.fillRect(185 + 34, 22, 5, 14, tc);
    spr_gpu.fillCircle(185 + 36, 40, 5, tc);

    // ── Progress bar ──
    drawBar(spr_gpu, 6, 76, LCD_W - 12, 20, st.gpu_used, col);

    // Percentage text on bar
    spr_gpu.setFont(&fonts::Font0);
    spr_gpu.setTextColor(col);
    char bar_label[16];
    snprintf(bar_label, sizeof(bar_label), "%.1f%%", st.gpu_used);
    int tw = spr_gpu.textWidth(bar_label);
    spr_gpu.drawString(bar_label, (LCD_W - tw) / 2, 81);

    // ── Separator ──
    spr_gpu.drawFastHLine(0, SEC_H - 1, LCD_W, C_SEP);

    spr_gpu.pushSprite(0, GPU_Y);
}

static void drawRamRow(const Stats &st) {
    spr_ram.fillScreen(C_ROW_BG);

    spr_ram.setFont(&fonts::Font0);
    spr_ram.setTextColor(C_CYAN);
    spr_ram.drawString("RAM", 6, 7);

    char buf[32];
    snprintf(buf, sizeof(buf), "%.1f/%.1f GB", st.ram_used, st.ram_total);
    spr_ram.setTextColor(C_LABEL);
    spr_ram.drawString(buf, 34, 7);

    float ram_pct = (st.ram_total > 0.f) ? (st.ram_used / st.ram_total * 100.f) : 0.f;
    uint32_t rc = barColor(ram_pct);
    drawMiniBar(spr_ram, 155, 5, 78, 14, ram_pct, rc);

    spr_ram.drawFastHLine(0, ROW_H - 1, LCD_W, C_SEP);
    spr_ram.pushSprite(0, RAM_Y);
}

static void drawNetRow(const Stats &st) {
    spr_net.fillScreen(C_ROW_BG);
    spr_net.setFont(&fonts::Font0);

    // WiFi SSID (left) - bright cyan for visibility
    spr_net.setTextColor(C_YELLOW);
    truncatedText(spr_net, st.wifi_name, 6, 7, 60, C_YELLOW);

    // Upload (middle-left)
    char buf[24];
    spr_net.setTextColor(C_CYAN);
    spr_net.drawString("^", 70, 7); // up arrow
    if (st.net_up >= 1024.f)
        snprintf(buf, sizeof(buf), "%.1f MB/s", st.net_up / 1024.f);
    else
        snprintf(buf, sizeof(buf), "%.0f KB/s", st.net_up);
    spr_net.setTextColor(C_LABEL);
    spr_net.drawString(buf, 80, 7);

    // Download (middle-right)
    spr_net.setTextColor(C_MAGENTA);
    spr_net.drawString("v", 155, 7); // down arrow
    if (st.net_down >= 1024.f)
        snprintf(buf, sizeof(buf), "%.1f MB/s", st.net_down / 1024.f);
    else
        snprintf(buf, sizeof(buf), "%.0f KB/s", st.net_down);
    spr_net.setTextColor(C_LABEL);
    spr_net.drawString(buf, 165, 7);

    spr_net.drawFastHLine(0, ROW_H - 1, LCD_W, C_SEP);
    spr_net.pushSprite(0, NET_Y);
}

static void drawFooter() {
    spr_ftr.fillScreen(C_HDR);

    char datetime_buf[32];
    portENTER_CRITICAL(&g_mux);
    strncpy(datetime_buf, g_stats.datetime_str, sizeof(datetime_buf) - 1);
    datetime_buf[sizeof(datetime_buf) - 1] = '\0';
    portEXIT_CRITICAL(&g_mux);

    if (datetime_buf[0] == '\0' || strcmp(datetime_buf, "00:00:00") == 0) {
        strncpy(datetime_buf, "--/--/---- --:--:--", sizeof(datetime_buf) - 1);
        datetime_buf[sizeof(datetime_buf) - 1] = '\0';
    }

    spr_ftr.setFont(&fonts::Font0);
    spr_ftr.setTextColor(C_TITLE);
    int tw = spr_ftr.textWidth(datetime_buf);
    int x = (LCD_W - tw) / 2;
    if (x < 0) x = 0;
    spr_ftr.drawString(datetime_buf, x, 8);

    spr_ftr.pushSprite(0, FTR_Y);
}

/* ═══════════════════════════════════════════════════════════
   Splash screen shown while waiting for BLE connection
   ═══════════════════════════════════════════════════════════ */
static void drawSplash() {
    lcd.fillScreen(C_BG);

    lcd.setFont(&fonts::FreeSansBold9pt7b);
    lcd.setTextColor(C_TITLE);
    lcd.setTextDatum(textdatum_t::middle_center);
    lcd.drawString("PC MONITOR", LCD_W / 2, 100);

    lcd.setFont(&fonts::Font0);
    lcd.setTextColor(C_DIM);
    lcd.drawString("Waiting for BLE...", LCD_W / 2, 130);

    // Animated BLE logo placeholder (3 concentric arcs - static)
    int cx = LCD_W / 2, cy = 200;
    lcd.drawCircle(cx, cy, 20, C_BLE_OFF);
    lcd.drawCircle(cx, cy, 13, C_BLE_OFF);
    lcd.drawCircle(cx, cy, 6,  C_BLE_OFF);
    lcd.fillCircle(cx, cy, 3,  C_BLE_OFF);

    lcd.setTextDatum(textdatum_t::top_left); // reset
}

/* ══════════════════════════════════════════════════════════════
   BLE Callbacks
   ══════════════════════════════════════════════════════════════ */
class StatsCharCallback : public NimBLECharacteristicCallbacks {
    void onWrite(NimBLECharacteristic *pChar) override {
        std::string val = pChar->getValue();
        if (val.empty()) return;

        JsonDocument doc;
        DeserializationError err = deserializeJson(doc, (const char *)val.c_str(), val.size());
        if (err) return;

        Stats tmp;
        portENTER_CRITICAL(&g_mux);
        tmp = g_stats;
        portEXIT_CRITICAL(&g_mux);

        tmp.cpu       = doc["cpu"]      | tmp.cpu;
        tmp.gpu_used  = doc["gpu_used"] | tmp.gpu_used;
        tmp.gpu_temp  = doc["gpu_temp"] | tmp.gpu_temp;
        tmp.ram_used  = doc["ram_used"] | tmp.ram_used;
        tmp.ram_total = doc["ram_total"]| tmp.ram_total;
        tmp.net_up    = doc["net_up"]   | tmp.net_up;
        tmp.net_down  = doc["net_down"] | tmp.net_down;
        tmp.valid     = true;

        // Safe string parsing
        if (!doc["cpu_name"].isNull()) {
            const char *s = doc["cpu_name"];
            if (s) strncpy(tmp.cpu_name, s, sizeof(tmp.cpu_name) - 1);
        }
        if (!doc["gpu_name"].isNull()) {
            const char *s = doc["gpu_name"];
            if (s) strncpy(tmp.gpu_name, s, sizeof(tmp.gpu_name) - 1);
        }
        if (!doc["wifi_name"].isNull()) {
            const char *s = doc["wifi_name"];
            if (s) strncpy(tmp.wifi_name, s, sizeof(tmp.wifi_name) - 1);
        }
        if (!doc["datetime"].isNull()) {
            const char *s = doc["datetime"];
            if (s) strncpy(tmp.datetime_str, s, sizeof(tmp.datetime_str) - 1);
        }

        portENTER_CRITICAL(&g_mux);
        g_stats   = tmp;
        g_new_data = true;
        portEXIT_CRITICAL(&g_mux);
    }
};

class ServerCallbacks : public NimBLEServerCallbacks {
    void onConnect(NimBLEServer *) override {
        g_ble_connected = true;
        setLedColor(0, 255, 0);   // xanh lá: BLE connected
    }
    void onDisconnect(NimBLEServer *pSrv) override {
        g_ble_connected = false;
        setLedColor(255, 0, 0);   // đỏ: BLE disconnected
        pSrv->startAdvertising();
    }
};

/* ══════════════════════════════════════════════════════════════
   setup()
   ══════════════════════════════════════════════════════════════ */
void setup() {
    Serial.begin(115200);

    /* ── Display init ── */
    lcd.init();
    lcd.setRotation(0);      // portrait
    lcd.setBrightness(100);
    lcd.setColorDepth(16);
	lcd.setSwapBytes(true);

    /* ── Allocate sprites ── */
    spr_hdr.setColorDepth(16);  spr_hdr.createSprite(LCD_W, HDR_H);
    spr_cpu.setColorDepth(16);  spr_cpu.createSprite(LCD_W, SEC_H);
    spr_gpu.setColorDepth(16);  spr_gpu.createSprite(LCD_W, SEC_H);
    spr_ram.setColorDepth(16);  spr_ram.createSprite(LCD_W, ROW_H);
    spr_net.setColorDepth(16);  spr_net.createSprite(LCD_W, ROW_H);
    spr_ftr.setColorDepth(16);  spr_ftr.createSprite(LCD_W, ROW_H);

    drawSplash();

    /* ── RGB LED ── */
    led.begin();
    led.setBrightness(50);
    setLedColor(0, 0, 255);   // xanh dương: chờ BLE connect

    /* ── BLE ── */
    NimBLEDevice::init(DEVICE_NAME);
    NimBLEServer *pServer = NimBLEDevice::createServer();
    pServer->setCallbacks(new ServerCallbacks());

    NimBLEService        *pSvc  = pServer->createService(SERVICE_UUID);
    NimBLECharacteristic *pChar = pSvc->createCharacteristic(
        CHAR_UUID,
        NIMBLE_PROPERTY::WRITE | NIMBLE_PROPERTY::WRITE_NR
    );
    pChar->setCallbacks(new StatsCharCallback());
    pSvc->start();

    NimBLEAdvertising *pAdv = NimBLEDevice::getAdvertising();
    pAdv->addServiceUUID(SERVICE_UUID);
    pAdv->setName(DEVICE_NAME);
    pAdv->start();

    Serial.println("BLE advertising started");
}

/* ══════════════════════════════════════════════════════════════
   loop()
   ══════════════════════════════════════════════════════════════ */
static uint32_t last_footer_ms = 0;
static bool     was_connected   = false;
static bool     splash_shown    = true;

void loop() {
    bool connected = g_ble_connected;

    /* Redraw full layout when BLE state changes */
    if (connected != was_connected) {
        was_connected = connected;
        if (!connected) {
            drawSplash();
            splash_shown = true;
        }
    }

    /* If we just got a connection, do a one-time full-screen clear */
    if (connected && splash_shown) {
        lcd.fillScreen(C_BG);
        splash_shown = false;
        // Draw all sections once with zeros so screen is populated
        Stats empty;
        empty.valid = true;

        portENTER_CRITICAL(&g_mux);
        Stats local = g_stats;
        bool  have  = g_new_data;
        if (have) g_new_data = false;
        portEXIT_CRITICAL(&g_mux);


        drawHeader(connected);
        drawCpuSection(local);
        drawGpuSection(local);
        drawRamRow(local);
        drawNetRow(local);
        drawFooter();
        last_footer_ms = millis();
        return;
    }

    if (!connected) {
        /* Pulse BLE dot on splash screen */
        delay(500);
        return;
    }

    /* ── Consume new data ── */
    bool have_data = false;
    Stats local;
    portENTER_CRITICAL(&g_mux);
    if (g_new_data) {
        local     = g_stats;
        have_data = true;
        g_new_data = false;
    }
    portEXIT_CRITICAL(&g_mux);

    if (have_data) {
        drawHeader(connected);
        drawCpuSection(local);
        drawGpuSection(local);
        drawRamRow(local);
        drawNetRow(local);
    }

    /* Footer clock ticks every second regardless */
    uint32_t now_ms = millis();
    if (now_ms - last_footer_ms >= 1000) {
        last_footer_ms = now_ms;
        drawFooter();
    }

    delay(10);
}
