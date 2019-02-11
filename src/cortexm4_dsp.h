// Wrappers for Cortex-M4 DSP instructions

#pragma once
#include <stdint.h>

static inline int32_t SMLAD(uint32_t x, uint32_t y, int32_t a)
{
#ifndef __arm__
    int16_t xl = x & 0xFFFF;
    int16_t xh = x >> 16;
    int16_t yl = y & 0xFFFF;
    int16_t yh = y >> 16;
    return a + xl * yl + xh * yh;
#else
    __asm__("smlad %0, %1, %2, %3" : "=r" (a) : "r" (x), "r" (y), "r" (a) );
    return a;
#endif
}
