#include <stdint.h>

// Get a pointer that can be used to write PCM samples to the internal ring buffer.
// Samples should be written at 12kHz samplerate, if timing is off the ring buffer
// will just wrap around and play garbage.
int audio_max_writecount();
int16_t *audio_get_writeptr(int sample_count);

// Get a pointer that can be used to read PCM samples from the microphone, at 12kHz.
int audio_max_readcount();
const int16_t *audio_get_readptr(int sample_count);

#ifdef TESTING
#define AUDIO_PDM_BYTES_PER_PCM_SAMPLE 20
void audio_pdm_convert(uint8_t* pdm_buf, int size);
#endif

#ifndef TESTING
void audio_init();
#endif
