#include <stdint.h>

#define AUDIO_SAMPLERATE 8000

// Mix audio to internal ringbuffer
uint32_t audio_write_pos(); // Oldest position that can still be written
void audio_write(uint32_t write_pos, const int16_t *samples, int sample_count);

// Read from internal ringbuffer
uint32_t audio_read_pos(); // Newest position that can be read
void audio_read(uint32_t read_pos, int16_t *samples, int sample_count);

// Start streaming audio.
void audio_init();
