#pragma once

#include <Arduino.h>

struct Stats {
    float cpu       = 0.f;
    float gpu_used  = 0.f;
    float gpu_temp  = 0.f;
    float ram_used  = 0.f;
    float ram_total = 0.f;
    float net_up    = 0.f;
    float net_down  = 0.f;
    char  cpu_name[48]     = "CPU";
    char  gpu_name[48]     = "GPU";
    char  wifi_name[48]    = "WiFi";
    char  datetime_str[32] = "00:00:00";
    bool  valid = false;
};

extern volatile bool g_new_data;
extern Stats g_stats;
extern portMUX_TYPE g_mux;
extern bool g_ble_connected;
