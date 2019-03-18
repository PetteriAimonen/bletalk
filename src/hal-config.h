#ifndef HAL_CONFIG_H
#define HAL_CONFIG_H

#include <bgm113a256v2.h>

#define HAL_EXTFLASH_FREQUENCY                        (1000000)

#define HAL_PA_ENABLE                                 (1)
#define HAL_PA_RAMP                                   (10)
#define HAL_PA_2P4_LOWPOWER                           (0)
#define HAL_PA_POWER                                  (252)
#define HAL_PA_CURVE_HEADER                            "pa_curves_efr32.h"
#define HAL_PA_VOLTAGE                                (1800)

#define HAL_PTI_ENABLE                                (1)
#define HAL_PTI_MODE                                  (HAL_PTI_MODE_UART)
#define HAL_PTI_BAUD_RATE                             (1600000)

#define TICK_FREQ   32768
#define MS2TICKS(x) (x * TICK_FREQ / 1000)

#endif

