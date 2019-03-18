#pragma once

#include "hal-config.h"
#include <stdint.h>

void board_init();

void delay_ms(int ms);
uint32_t get_ticks();
int get_time_ms();

#define BUTTON_VOLM_PORT    gpioPortB
#define BUTTON_VOLM_PIN     13
#define BUTTON_VOLP_PORT    gpioPortB
#define BUTTON_VOLP_PIN     12
#define BUTTON_VOLP_APORT   adcPosSelAPORT3XCH28

#define VBATT_SENSE_PORT    gpioPortA
#define VBATT_SENSE_PIN     1
#define VBATT_SENSE_APORT   adcPosSelAPORT4XCH9

#define HEADSET_SENSE_PORT  gpioPortD
#define HEADSET_SENSE_PIN   14
#define HEADSET_SENSE_APORT adcPosSelAPORT3XCH6
