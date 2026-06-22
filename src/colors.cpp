#include "colors.h"

uint32_t barColor(float pct) {
    if (pct >= 80.f) return C_RED;
    if (pct >= 50.f) return C_YELLOW;
    return C_GREEN;
}

uint32_t tempColor(float t) {
    if (t >= 85.f) return C_RED;
    if (t >= 70.f) return C_YELLOW;
    return C_GREEN;
}
