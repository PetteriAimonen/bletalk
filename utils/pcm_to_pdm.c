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
        int16_t *buf = audio_get_writeptr(BLOCKSIZE);
        int readcount = fread(buf, sizeof(int16_t), BLOCKSIZE, stdin);
        if (readcount != BLOCKSIZE && ferror(stdin))
        {
            perror("stdin");
            return 1;
        }

        fprintf(stderr, "Writeptr %p, readcount %d\n", buf, readcount);

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
