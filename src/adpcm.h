// ADPCM format encoding and decoding.
// Matches Dialogic / VOX ADPCM format:
// https://en.wikipedia.org/wiki/Dialogic_ADPCM
// https://www.mp3-tech.org/programmer/docs/adpcm.pdf

#pragma once
#include <stdint.h>

typedef struct {
    uint8_t stepidx;
    int16_t value;
} adpcm_state_t;

// Encode samplecount input samples into samplecount/2 output bytes.
// samplecount must be multiple of two.
void adpcm_encode(adpcm_state_t* state, const int16_t* in_buf, uint8_t *out_buf, int samplecount);

// Decode samplecount/2 input bytes to samplecount output samples.
// samplecount must be multiple of two.
void adpcm_decode(adpcm_state_t* state, const uint8_t* in_buf, int16_t *out_buf, int samplecount);
