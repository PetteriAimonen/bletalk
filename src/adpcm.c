#include "adpcm.h"
#include <stdint.h>

static const int8_t tbl_stepsize_change[16] = {
    -1, -1, -1, -1, 2, 4, 6, 8,
    -1, -1, -1, -1, 2, 4, 6, 8,
};

static const uint16_t tbl_stepsize[49] = {
    16, 17, 19, 21, 23, 25, 28, 31, 34, 37, 41,
    45, 50, 55, 60, 66, 73, 80, 88, 97, 107, 118, 130, 143, 157, 173,
    190, 209, 230, 253, 279, 307, 337, 371, 408, 449, 494, 544, 598, 658,
    724, 796, 876, 963, 1060, 1166, 1282, 1408, 1552
};

static inline void adpcm_sub_decode(uint8_t nibble, int32_t *prev_val, int *stepidx)
{
    if (nibble & 0x08)
    {
        *prev_val -= (nibble & 0x07) * tbl_stepsize[*stepidx];
        if (*prev_val < INT16_MIN) *prev_val = INT16_MIN;
    }
    else
    {
        *prev_val += (nibble & 0x07) * tbl_stepsize[*stepidx];
        if (*prev_val > INT16_MAX) *prev_val = INT16_MAX;
    }

    *stepidx += tbl_stepsize_change[nibble];
    if (*stepidx < 0) *stepidx = 0;
    if (*stepidx > 48) *stepidx = 48;
}

static inline uint8_t adpcm_sub_encode(int new_value, int32_t *prev_val, int *stepidx)
{
    uint8_t nibble = 0;
    int diff = new_value - *prev_val;

    if (diff < 0)
    {
        nibble |= 0x08;
        diff = -diff;
    }

    int step = tbl_stepsize[*stepidx];
    int quantized = (diff + step / 2) / step;
    if (quantized > 7) quantized = 7;

    nibble |= quantized;

    adpcm_sub_decode(nibble, prev_val, stepidx);

    return nibble;
}

void adpcm_encode(adpcm_state_t* state, const int16_t* in_buf, uint8_t *out_buf, int samplecount)
{
    int32_t prev_val = state->value;
    int stepidx = state->stepidx;

    for (int i = 0; i < samplecount; i += 2)
    {
        uint8_t byte;

        byte = adpcm_sub_encode(in_buf[i], &prev_val, &stepidx) << 4;
        byte |= adpcm_sub_encode(in_buf[i+1], &prev_val, &stepidx);

        out_buf[i/2] = byte;
    }

    state->value = prev_val;
    state->stepidx = stepidx;
}

void adpcm_decode(adpcm_state_t* state, const uint8_t* in_buf, int16_t *out_buf, int samplecount)
{
    int32_t prev_val = state->value;
    int stepidx = state->stepidx;

    for (int i = 0; i < samplecount; i += 2)
    {
        uint8_t byte = in_buf[i / 2];

        adpcm_sub_decode(byte >> 4, &prev_val, &stepidx);
        out_buf[i] = prev_val;

        adpcm_sub_decode(byte & 0x0F, &prev_val, &stepidx);
        out_buf[i + 1] = prev_val;
    }

    state->value = prev_val;
    state->stepidx = stepidx;
}


