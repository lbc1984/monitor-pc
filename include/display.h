#pragma once

#include "stats.h"

void initDisplay();
void clearDisplay();
void drawSplash();
void drawHeader(bool ble_on);
void drawCpuSection(const Stats &st);
void drawGpuSection(const Stats &st);
void drawRamRow(const Stats &st);
void drawNetRow(const Stats &st);
void drawFooter();
