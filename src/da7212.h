#pragma once
#include <stdint.h>
#include <stdbool.h>

bool da7212_init();

bool da7212_set_powersave(bool powersave);

bool da7212_beep(int freq, int length_ms);
