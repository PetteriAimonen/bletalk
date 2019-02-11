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
        uint8_t inbuf[BLOCKSIZE * AUDIO_PDM_BYTES_PER_PCM_SAMPLE];
        int readcount = fread(inbuf, 1, sizeof(inbuf), stdin);
        if (readcount != sizeof(inbuf) && ferror(stdin))
        {
            perror("stdin");
            return 1;
        }

        audio_pdm_convert(inbuf, readcount);

        int samplecount = readcount / AUDIO_PDM_BYTES_PER_PCM_SAMPLE;
        const int16_t *pcmbuf = audio_get_readptr(samplecount);
        if (fwrite(pcmbuf, sizeof(int16_t), samplecount, stdout) != samplecount)
        {
            perror("stdout");
            return 2;
        }
    }

    return 0;
}

