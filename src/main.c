#include "board.h"
#include "bluetooth.h"

int main()
{
    board_init();
    bluetooth_init();
    
    while (1)
    {
        bluetooth_poll();
    }
}
