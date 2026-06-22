#include "display.h"

#include <Arduino.h>
#include <cstring>
#include "colors.h"
#include "config.h"
#include "lgfx.h"

static LGFX        lcd;
static LGFX_Sprite spr_hdr(&lcd);
static LGFX_Sprite spr_cpu(&lcd);
static LGFX_Sprite spr_gpu(&lcd);
static LGFX_Sprite spr_ram(&lcd);
static LGFX_Sprite spr_net(&lcd);
static LGFX_Sprite spr_ftr(&lcd);

static void truncatedText(LGFX_Sprite &s, const char *str, int x, int y,
                          int maxPx, uint32_t color) {
    char buf[64];
    strncpy(buf, str, sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = '\0';
    int len = strlen(buf);
    s.setTextColor(color);
    while (len > 0 && s.textWidth(buf) > maxPx) {
        buf[--len] = '\0';
        if (len > 2) { buf[len - 1] = '.'; buf[len - 2] = '.'; buf[len - 3] = '.'; }
    }
    s.drawString(buf, x, y);
}

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

void initDisplay() {
    lcd.init();
    lcd.setRotation(0);
    lcd.setBrightness(100);
    lcd.setColorDepth(16);
    lcd.setSwapBytes(true);

    spr_hdr.setColorDepth(16); spr_hdr.createSprite(LCD_W, HDR_H);
    spr_cpu.setColorDepth(16); spr_cpu.createSprite(LCD_W, SEC_H);
    spr_gpu.setColorDepth(16); spr_gpu.createSprite(LCD_W, SEC_H);
    spr_ram.setColorDepth(16); spr_ram.createSprite(LCD_W, ROW_H);
    spr_net.setColorDepth(16); spr_net.createSprite(LCD_W, ROW_H);
    spr_ftr.setColorDepth(16); spr_ftr.createSprite(LCD_W, ROW_H);
}

void clearDisplay() {
    lcd.fillScreen(C_BG);
}

void drawHeader(bool ble_on) {
    spr_hdr.fillScreen(C_HDR);
    spr_hdr.setTextColor(C_TITLE);
    spr_hdr.setTextSize(1);
    spr_hdr.setFont(&fonts::FreeSansBold9pt7b);
    spr_hdr.drawString("PC MONITOR", 8, 7);

    uint32_t dot_col = ble_on ? C_BLE_ON : C_BLE_OFF;
    spr_hdr.fillCircle(LCD_W - 14, 14, 5, dot_col);
    spr_hdr.setFont(&fonts::Font0);
    spr_hdr.setTextColor(C_DIM);
    spr_hdr.drawString(ble_on ? "BLE" : "---", LCD_W - 50, 9);
    spr_hdr.drawFastHLine(0, HDR_H - 1, LCD_W, C_SEP);
    spr_hdr.pushSprite(0, HDR_Y);
}

void drawCpuSection(const Stats &st) {
    spr_cpu.fillScreen(C_CPU_BG);
    spr_cpu.setFont(&fonts::Font0);
    truncatedText(spr_cpu, st.cpu_name, 8, 5, LCD_W - 44, C_YELLOW);

    spr_cpu.setFont(&fonts::Font7);
    spr_cpu.setTextSize(1);
    char pct_str[8];
    snprintf(pct_str, sizeof(pct_str), "%4.1f", st.cpu);
    uint32_t col = barColor(st.cpu);
    spr_cpu.setTextColor(col);
    spr_cpu.drawString(pct_str, 6, 18);
    int pct_w = spr_cpu.textWidth(pct_str);

    spr_cpu.setFont(&fonts::FreeSansBold9pt7b);
    spr_cpu.setTextColor(col);
    int pct_sign_x = 6 + pct_w + 4;
    if (pct_sign_x > LCD_W - 20) pct_sign_x = LCD_W - 20;
    spr_cpu.drawString("%", pct_sign_x, 36);

    drawBar(spr_cpu, 6, 76, LCD_W - 12, 20, st.cpu, col);

    spr_cpu.setFont(&fonts::Font0);
    spr_cpu.setTextColor(col);
    char bar_label[16];
    snprintf(bar_label, sizeof(bar_label), "%.1f%%", st.cpu);
    int tw = spr_cpu.textWidth(bar_label);
    spr_cpu.drawString(bar_label, (LCD_W - tw) / 2, 81);

    spr_cpu.drawFastHLine(0, SEC_H - 1, LCD_W, C_SEP);
    spr_cpu.pushSprite(0, CPU_Y);
}

void drawGpuSection(const Stats &st) {
    spr_gpu.fillScreen(C_GPU_BG);
    truncatedText(spr_gpu, st.gpu_name, 8, 5, LCD_W - 20, C_YELLOW);

    spr_gpu.setFont(&fonts::Font7);
    char pct_str[8];
    snprintf(pct_str, sizeof(pct_str), "%4.1f", st.gpu_used);
    uint32_t col = barColor(st.gpu_used);
    spr_gpu.setTextColor(col);
    spr_gpu.drawString(pct_str, 6, 18);
    int pct_w = spr_gpu.textWidth(pct_str);

    spr_gpu.setFont(&fonts::FreeSansBold9pt7b);
    spr_gpu.setTextColor(col);
    int pct_sign_x = 6 + pct_w + 4;
    if (pct_sign_x > LCD_W - 20) pct_sign_x = LCD_W - 20;
    spr_gpu.drawString("%", pct_sign_x, 36);

    uint32_t tc = tempColor(st.gpu_temp);
    spr_gpu.setFont(&fonts::FreeSansBold9pt7b);
    spr_gpu.setTextColor(tc);
    char temp_str[16];
    snprintf(temp_str, sizeof(temp_str), "%.0f" " " "\xB0" " C", st.gpu_temp);
    spr_gpu.drawString(temp_str, 168, 24);

    spr_gpu.fillRect(185 + 34, 22, 5, 14, tc);
    spr_gpu.fillCircle(185 + 36, 40, 5, tc);

    drawBar(spr_gpu, 6, 76, LCD_W - 12, 20, st.gpu_used, col);

    spr_gpu.setFont(&fonts::Font0);
    spr_gpu.setTextColor(col);
    char bar_label[16];
    snprintf(bar_label, sizeof(bar_label), "%.1f%%", st.gpu_used);
    int tw = spr_gpu.textWidth(bar_label);
    spr_gpu.drawString(bar_label, (LCD_W - tw) / 2, 81);

    spr_gpu.drawFastHLine(0, SEC_H - 1, LCD_W, C_SEP);
    spr_gpu.pushSprite(0, GPU_Y);
}

void drawRamRow(const Stats &st) {
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

void drawNetRow(const Stats &st) {
    spr_net.fillScreen(C_ROW_BG);
    spr_net.setFont(&fonts::Font0);

    spr_net.setTextColor(C_YELLOW);
    truncatedText(spr_net, st.wifi_name, 6, 7, 60, C_YELLOW);

    char buf[24];
    spr_net.setTextColor(C_CYAN);
    spr_net.drawString("^", 70, 7);
    if (st.net_up >= 1024.f)
        snprintf(buf, sizeof(buf), "%.1f MB/s", st.net_up / 1024.f);
    else
        snprintf(buf, sizeof(buf), "%.0f KB/s", st.net_up);
    spr_net.setTextColor(C_LABEL);
    spr_net.drawString(buf, 80, 7);

    spr_net.setTextColor(C_MAGENTA);
    spr_net.drawString("v", 155, 7);
    if (st.net_down >= 1024.f)
        snprintf(buf, sizeof(buf), "%.1f MB/s", st.net_down / 1024.f);
    else
        snprintf(buf, sizeof(buf), "%.0f KB/s", st.net_down);
    spr_net.setTextColor(C_LABEL);
    spr_net.drawString(buf, 165, 7);

    spr_net.drawFastHLine(0, ROW_H - 1, LCD_W, C_SEP);
    spr_net.pushSprite(0, NET_Y);
}

void drawFooter() {
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

void drawSplash() {
    lcd.fillScreen(C_BG);
    lcd.setFont(&fonts::FreeSansBold9pt7b);
    lcd.setTextColor(C_TITLE);
    lcd.setTextDatum(textdatum_t::middle_center);
    lcd.drawString("PC MONITOR", LCD_W / 2, 100);

    lcd.setFont(&fonts::Font0);
    lcd.setTextColor(C_DIM);
    lcd.drawString("Waiting for BLE...", LCD_W / 2, 130);

    int cx = LCD_W / 2, cy = 200;
    lcd.drawCircle(cx, cy, 20, C_BLE_OFF);
    lcd.drawCircle(cx, cy, 13, C_BLE_OFF);
    lcd.drawCircle(cx, cy, 6, C_BLE_OFF);
    lcd.fillCircle(cx, cy, 3, C_BLE_OFF);

    lcd.setTextDatum(textdatum_t::top_left);
}
