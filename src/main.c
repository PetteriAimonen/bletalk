#include "board.h"
#include "bluetooth.h"
#include "audio.h"
#include "da7212.h"
#include "buttons.h"
#include "adc.h"
#include "binlog.h"

int main()
{
    board_init();
    binlog_init();
    audio_init();
    bluetooth_init();

    da7212_init();
    da7212_set_powersave(false);
    buttons_init();

    binlog("Temperature %d mC", adc_get_temperature_mC(), 0);

    while (1)
    {
        bluetooth_poll(10);
        adc_poll();
        buttons_poll();
    }
}
