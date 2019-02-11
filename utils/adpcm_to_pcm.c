#ifndef TESTING
#define TESTING
#endif

#include <stdio.h>
#include <adpcm.h>

int main()
{
    int hdrlen = 5;
    uint8_t buffer[256];
    int16_t pcm_buffer[512];

    while (!feof(stdin))
    {
        if (fread(buffer, hdrlen, 1, stdin) != 1)
        {
            if (ferror(stdin))
            {
                perror("stdin");
                return 1;
            }
            else
            {
                break;
            }
        }

        int bytes = buffer[0];
        adpcm_state_t state = {};
        state.stepidx = buffer[1] & 0x3F;
        state.value = ((buffer[2] << 2) | (buffer[1] >> 6)) * 16;
        fprintf(stderr, "Len %3d/%3d, idx %3d\n", bytes, buffer[4], buffer[3]);

        bytes -= 4;
        fread(buffer, bytes, 1, stdin);

        adpcm_decode(&state, buffer, pcm_buffer, bytes * 2);

        fwrite(pcm_buffer, 2, bytes * 2, stdout);
    }

    return 0;
}
