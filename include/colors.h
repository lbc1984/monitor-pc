#pragma once

#include <Arduino.h>

/* Colour palette (RGB565) */
static constexpr uint32_t C_BG     = 0x0000u;
static constexpr uint32_t C_HDR    = 0x0000u;
static constexpr uint32_t C_CPU_BG = 0x0000u;
static constexpr uint32_t C_GPU_BG = 0x0000u;
static constexpr uint32_t C_ROW_BG = 0x0000u;

static constexpr uint32_t C_TITLE = 0xEFFFu;
static constexpr uint32_t C_LABEL = 0xAD75u;
static constexpr uint32_t C_DIM   = 0x6B6Du;
static constexpr uint32_t C_SEP   = 0x18C3u;

// Device color order is GRB, so these values are mapped to match on-screen color names.
static constexpr uint16_t C_RED     = 0x07E0;
static constexpr uint16_t C_GREEN   = 0xF800;
static constexpr uint16_t C_BLUE    = 0x001F;
static constexpr uint16_t C_YELLOW  = 0xFFE0;
static constexpr uint16_t C_CYAN    = 0x07FF;
static constexpr uint16_t C_MAGENTA = 0xF81F;

static constexpr uint32_t C_BLE_ON  = C_GREEN;
static constexpr uint32_t C_BLE_OFF = C_RED;

uint32_t barColor(float pct);
uint32_t tempColor(float t);
