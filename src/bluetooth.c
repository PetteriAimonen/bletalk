#include "bluetooth.h"
#include "hal-config.h"
#include "gatt_db.h"
#include "audio.h"
#include "gsm_static.h"

#include <em_core.h>
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
  .gattdb = &bg_gattdb_data,
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

static int g_connection_id = -1;
static struct gsm_state g_gsm_state = GSM_STATE_INIT;
uint32_t g_gsm_encode_time;

void bluetooth_poll(int max_sleep_ms)
{
    struct gecko_cmd_packet* evt;
    evt = gecko_peek_event();
    
    if (!evt)
    {
        if (g_connection_id < 0)
        {
            CORE_DECLARE_IRQ_STATE;
            CORE_ENTER_ATOMIC();
            uint32_t gecko_max_sleep = gecko_can_sleep_ms();
            if (gecko_max_sleep > max_sleep_ms)
                gecko_max_sleep = max_sleep_ms;
            gecko_sleep_for_ms(gecko_max_sleep);
            CORE_EXIT_ATOMIC();
        }
        
        evt = gecko_peek_event();
    }
    
    if (evt)
    {
        uint32_t id = BGLIB_MSG_ID(evt->header);
        
        if (id == gecko_evt_system_boot_id)
        {
            // Initialization tasks
            gecko_cmd_le_gap_set_advertise_timing(0, 320, 640, 0, 0);
            gecko_cmd_le_gap_start_advertising(0, le_gap_general_discoverable, le_gap_connectable_scannable);
        }
        else if (id == gecko_evt_gatt_server_user_write_request_id)
        {
            if (evt->data.evt_gatt_server_user_write_request.characteristic == gattdb_audio)
            {
                // For now this just returns the same data back
                gecko_cmd_gatt_server_send_characteristic_notification(
                  evt->data.evt_gatt_server_user_write_request.connection,
                  gattdb_audio,
                  evt->data.evt_gatt_server_user_write_request.value.len,
                  evt->data.evt_gatt_server_user_write_request.value.data
                );
            }
        }
        else if (id == gecko_evt_le_connection_opened_id)
        {
            g_connection_id = evt->data.evt_le_connection_opened.connection;
            audio_init();
            audio_set_mic_gain(256 * 100);
            gecko_cmd_hardware_set_soft_timer(655, 1, false);
            gecko_cmd_le_connection_set_parameters(g_connection_id, 6, 10, 0, 100);
        }
        else if (id == gecko_evt_le_connection_closed_id)
        {
            g_connection_id = -1;
        }
        else if (id == gecko_evt_hardware_soft_timer_id)
        {
            if (g_connection_id > 0)
            {
                static int16_t samplebuf[160];
                uint8_t packet[33];

                if (audio_max_readcount() >= 160)
                {
                    unsigned start = DWT->CYCCNT;
                    audio_read(samplebuf, 160);
                    gsm_encode(&g_gsm_state, samplebuf, packet);
                    g_gsm_encode_time = DWT->CYCCNT - start;

                    gecko_cmd_gatt_server_send_characteristic_notification(g_connection_id, gattdb_audio, 33, packet);
                }
            }
        }
    }
}

