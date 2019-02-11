
CFLAGS = -g3 -ggdb -Wall -std=gnu99 -fwrapv
CFLAGS += -DTESTING
CFLAGS += -I../src

all: pcm_to_pdm pdm_to_pcm adpcm_to_pcm

pcm_to_pdm: pcm_to_pdm.c ../src/audio.c
	$(CC) $(CFLAGS) -o $@ $^

pdm_to_pcm: pdm_to_pcm.c ../src/audio.c
	$(CC) $(CFLAGS) -o $@ $^

adpcm_to_pcm:	adpcm_to_pcm.c ../src/adpcm.c
	$(CC) $(CFLAGS) -o $@ $^
