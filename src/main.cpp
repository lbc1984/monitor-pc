/**
 * main.cpp – ESP32-S3 PC Stats Monitor
 *
 * Main orchestration only. Feature-specific logic is split into modules.
 */

#include <Arduino.h>
#include "ble_server.h"
#include "display.h"
#include "led_status.h"
#include "stats.h"

static uint32_t last_footer_ms = 0;
static bool     was_connected = false;
static bool     splash_shown  = true;

void setup() {
    Serial.begin(115200);

    initDisplay();
    drawSplash();

    initStatusLed();
    initBleServer();
}

void loop() {
    bool connected = g_ble_connected;

    if (connected != was_connected) {
        was_connected = connected;
        if (!connected) {
            drawSplash();
            splash_shown = true;
        }
    }

    if (connected && splash_shown) {
        clearDisplay();
        splash_shown = false;

        portENTER_CRITICAL(&g_mux);
        Stats local = g_stats;
        bool have = g_new_data;
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
        delay(500);
        return;
    }

    bool have_data = false;
    Stats local;
    portENTER_CRITICAL(&g_mux);
    if (g_new_data) {
        local = g_stats;
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

    uint32_t now_ms = millis();
    if (now_ms - last_footer_ms >= 1000) {
        last_footer_ms = now_ms;
        drawFooter();
    }

    delay(10);
}
