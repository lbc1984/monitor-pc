#include "led_status.h"

#include <Adafruit_NeoPixel.h>
#include "config.h"

static Adafruit_NeoPixel led(NUM_PIXELS, LED_RGB_PIN, NEO_GRB + NEO_KHZ800);

void initStatusLed() {
    led.begin();
    led.setBrightness(100);
    setLedWaiting();
}

void setLedColor(uint8_t r, uint8_t g, uint8_t b) {
    led.setPixelColor(0, led.Color(r, g, b));
    led.show();
}

void setLedWaiting() {
    setLedColor(0, 0, 255);
}

void setLedConnected() {
    setLedColor(0, 255, 0);
}

void setLedDisconnected() {
    setLedColor(255, 0, 0);
}
