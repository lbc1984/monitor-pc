#include "stats.h"

volatile bool g_new_data = false;
Stats g_stats;
portMUX_TYPE g_mux = portMUX_INITIALIZER_UNLOCKED;
bool g_ble_connected = false;
