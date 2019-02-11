#include <stdint.h>

// Write to internal ringbuffer
int audio_max_writecount();
void audio_write(const int16_t *samples, int sample_count);

// Read from internal ringbuffer
int audio_max_readcount();
void audio_read(int16_t *samples, int sample_count);

// Set speaker/mic gain, 256 = 1x gain
void audio_set_mic_gain(int gain);
void audio_set_spk_gain(int gain);

#define AUDIO_PDM_BYTES_PER_PCM_SAMPLE 60

#ifdef TESTING
void audio_pdm_convert(uint8_t* pdm_buf, int size);
#endif

#ifndef TESTING
void audio_init();
#endif
