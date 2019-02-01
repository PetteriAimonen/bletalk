#include "bluetooth.h"
#include "hal-config.h"

#include <bg_types.h>
#include <native_gecko.h>
#include <infrastructure.h>

#ifndef MAX_CONNECTIONS
#define MAX_CONNECTIONS 4
#endif
uint8_t g_bluetooth_heap[DEFAULT_BLUETOOTH_HEAP(MAX_CONNECTIONS)];

static const gecko_configuration_t g_gecko_config = {
  .config_flags = 0,
  .sleep.flags = SLEEP_FLAGS_DEEP_SLEEP_ENABLE,
  .bluetooth.max_connections = MAX_CONNECTIONS,
  .bluetooth.heap = g_bluetooth_heap,
  .bluetooth.heap_size = sizeof(g_bluetooth_heap),
  .bluetooth.sleep_clock_accuracy = 100,
  .gattdb = NULL,
  .ota.flags = 0,
  .ota.device_name_len = 11,
  .ota.device_name_ptr = "BLETALK_OTA",
  .pa.config_enable = 1,
  .pa.input = GECKO_RADIO_PA_INPUT_DCDC,
};

void bluetooth_init()
{
    gecko_init(&g_gecko_config);
}

void bluetooth_poll()
{
    static uint8_t adv_data[24] = {
		  23, 0x21, /* UUID: e2bbea4e-539a-40d3-bcd8-eeabddc779a6 */
		  0xa6, 0x79, 0xc7, 0xdd, 0xab, 0xee, 0xd8, 0xbc, 0xd3, 0x40, 0x9a, 0x53, 0x4e, 0xea, 0xbb, 0xe2,
		  'T', 'E', 'S', 'T',
		  1, 2, 3, 4
    };

    struct gecko_cmd_packet* evt;
    evt = gecko_wait_event();
    
    switch (BGLIB_MSG_ID(evt->header))
    {
        case gecko_evt_system_boot_id:
            gecko_cmd_le_gap_bt5_set_adv_data(0, 0, sizeof(adv_data), adv_data);
            gecko_cmd_le_gap_set_advertise_timing(0, 160, 160, 0, 0);
            gecko_cmd_le_gap_start_advertising(0, le_gap_user_data, le_gap_non_connectable);
            break;
    }
}

