// Reading and handling of button events and also basic power on/off management
#pragma once
#include <stdint.h>

void buttons_init();

uint8_t buttons_read();

void buttons_poll();
