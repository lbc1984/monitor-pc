#pragma once

#include <Arduino.h>

void initStatusLed();
void setLedColor(uint8_t r, uint8_t g, uint8_t b);
void setLedWaiting();
void setLedConnected();
void setLedDisconnected();
