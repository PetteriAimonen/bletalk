#ifndef TESTING
#define TESTING
#endif

#include <stdio.h>
#include <audio.h>

#define BLOCKSIZE 256

int main()
{
    while (!feof(stdin))
    {
        int16_t pcm_buf[BLOCKSIZE];
        int readcount = fread(pcm_buf, sizeof(int16_t), BLOCKSIZE, stdin);
        if (readcount != BLOCKSIZE && ferror(stdin))
        {
            perror("stdin");
            return 1;
        }

        audio_write(pcm_buf, readcount);

        uint8_t outbuf[BLOCKSIZE * AUDIO_PDM_BYTES_PER_PCM_SAMPLE];
        int pdm_bytes = readcount * AUDIO_PDM_BYTES_PER_PCM_SAMPLE;
        audio_pdm_convert(outbuf, pdm_bytes);
        if (fwrite(outbuf, 1, pdm_bytes, stdout) != pdm_bytes)
        {
            perror("stdout");
            return 2;
        }
    }

    return 0;
}
