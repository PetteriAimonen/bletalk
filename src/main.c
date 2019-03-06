#include "board.h"
#include "bluetooth.h"
#include "audio.h"
#include "binlog.h"

int main()
{
    board_init();
    binlog_init();
    audio_init();
    bluetooth_init();
    
    while (1)
    {
        bluetooth_poll(10);
    }
}
