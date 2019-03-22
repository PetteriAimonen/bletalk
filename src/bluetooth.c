#include "bluetooth.h"
#include "hal-config.h"
#include "gatt_db.h"
#include "audio.h"
#include "gsm_static.h"
#include "binlog.h"

#include <em_core.h>
#include <bg_types.h>
#include <native_gecko.h>
#include <infrastructure.h>

#define SAMPLES_PER_FRAME 160
#define MIN_LATENCY (1 * SAMPLES_PER_FRAME)
#define TARGET_LATENCY (2 * SAMPLES_PER_FRAME)
#define MAX_LATENCY (3 * SAMPLES_PER_FRAME)

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

// Each audio-containing data frame follows this format
typedef struct {
  uint8_t index; // Frame index
  uint8_t gsm_frame[33]; // libgsm encoded audio data
} data_frame_t;

// Structure for each peer
typedef struct {
  // Current mode, whether data is transferred by GATT connection or by advertisements
  enum { MODE_IDLE = 0, MODE_CONNECTED, MODE_SYNCADV } mode;

  // Connection or synchronized advertisement handle
  int handle;

  // Previous valid frame, for substituting any lost frames.
  int lost_frame_count;
  data_frame_t prev_frame;

  // libgsm decoder state
  struct gsm_state decoder_state;

  // Previous write position in audio ringbuffer
  uint32_t write_pos;
} peer_state_t;

// Structure for our server state
typedef struct {
  peer_state_t peers[MAX_CONNECTIONS];

  // Read position in audio ringbuffer
  uint32_t read_pos;

  // libgsm encoder state
  struct gsm_state encoder_state;

  // Previous transmitted frame index
  uint8_t frame_index;
} server_state_t;

void peer_init(peer_state_t *peer)
{
  *peer = (peer_state_t){};
  peer->decoder_state = (struct gsm_state)GSM_STATE_INIT;
}

void peer_connected(peer_state_t *peer, int connection_id)
{
  peer_init(peer);
  peer->mode = MODE_CONNECTED;
  peer->handle = connection_id;
  peer->write_pos = audio_write_pos() + TARGET_LATENCY;
}

void peer_disconnected(peer_state_t *peer)
{
  peer->mode = MODE_IDLE;
}

void peer_packet_received(peer_state_t *peer, const data_frame_t *data)
{
  if (data)
  {
    int delta = data->index - peer->prev_frame.index;
    if (delta == 0)
    {
      return; // Already have this frame
    }
    else if (delta == 2)
    {
      peer_packet_received(peer, NULL); // Lost a frame in between
    }

    peer->lost_frame_count = 0;
    peer->prev_frame = *data;
  }
  else
  {
    peer->lost_frame_count++;
    peer->prev_frame.index++;

    binlog("lost frame %d", peer->prev_frame.index, 0);

    if (peer->lost_frame_count > 16)
      return;
  }

  {
    int16_t samplebuf[SAMPLES_PER_FRAME];
    uint32_t start = DWT->CYCCNT;
    gsm_decode(&peer->decoder_state, peer->prev_frame.gsm_frame, samplebuf);
    binlog("packet %08x decoded in %d cycles", (peer->handle << 16) | peer->prev_frame.index, DWT->CYCCNT - start);

    if (peer->lost_frame_count > 1)
    {
      // Decrease block amplitude
      for (int i = 0; i < SAMPLES_PER_FRAME; i++)
      {
        samplebuf[i] >>= (peer->lost_frame_count - 1);
      }
    }

    // Perform compensation for small samplerate differences.
    // This allows +-1 sample per 160 sample packet, i.e. 6250 ppm which is more
    // than enough to account for crystal freq differences.
    int latency = peer->write_pos - audio_write_pos();
    if (latency < 0 || latency > 2 * MAX_LATENCY)
    {
      binlog("Jumping write_pos, latency was %d", latency, 0);
      peer->write_pos = audio_write_pos() + TARGET_LATENCY;
      audio_write(peer->write_pos, samplebuf, SAMPLES_PER_FRAME);
      peer->write_pos += SAMPLES_PER_FRAME;
    }
    else if (latency < MIN_LATENCY)
    {
      binlog("Falling behind (%d), add one sample", latency, 0);
      audio_write(peer->write_pos, samplebuf, 1);
      audio_write(peer->write_pos, samplebuf, SAMPLES_PER_FRAME);
      peer->write_pos += SAMPLES_PER_FRAME + 1;
    }
    else if (latency > MAX_LATENCY)
    {
      binlog("Getting ahead (%d), drop one sample", latency, 0);
      audio_write(peer->write_pos, samplebuf, SAMPLES_PER_FRAME - 1);
      peer->write_pos += SAMPLES_PER_FRAME - 1;
    }
    else
    {
      // Right on target
      audio_write(peer->write_pos, samplebuf, SAMPLES_PER_FRAME);
      peer->write_pos += SAMPLES_PER_FRAME;
    }
  }
}

void server_init(server_state_t *server)
{
  server->read_pos = 0;
  server->encoder_state = (struct gsm_state)GSM_STATE_INIT;
  server->frame_index = 0;

  for (int i = 0; i < MAX_CONNECTIONS; i++)
  {
    peer_init(&server->peers[i]);
  }
}

peer_state_t *server_find_free_peer(server_state_t *server)
{
  for (int i = 0; i < MAX_CONNECTIONS; i++)
  {
    if (server->peers[i].mode == MODE_IDLE)
    {
      return &server->peers[i];
    }
  }

  return NULL;
}

peer_state_t *server_find_peer_by_connection(server_state_t *server, int connection_id)
{
  for (int i = 0; i < MAX_CONNECTIONS; i++)
  {
    if (server->peers[i].mode == MODE_CONNECTED && server->peers[i].handle == connection_id)
    {
      return &server->peers[i];
    }
  }

  return NULL;
}

void server_send_packets(server_state_t *server)
{
  int16_t samplebuf[SAMPLES_PER_FRAME];
  audio_read(server->read_pos, samplebuf, SAMPLES_PER_FRAME);
  server->read_pos += SAMPLES_PER_FRAME;

  server->frame_index++;

  uint8_t adv_packet[18 + sizeof(data_frame_t)] = {
    17 + sizeof(data_frame_t), 0x21,
    0xa6, 0x79, 0xc7, 0xdd, 0xab, 0xee, 0xd8, 0xbc,
    0xd3, 0x40, 0x9a, 0x53, 0x4e, 0xea, 0xbb, 0xe2,
  };
  data_frame_t *frame = (data_frame_t*)&adv_packet[18];
  uint32_t start = DWT->CYCCNT;
  frame->index = server->frame_index;
  gsm_encode(&server->encoder_state, samplebuf, frame->gsm_frame);
  binlog("gsm_encode time: %d", DWT->CYCCNT - start, 0);

  gecko_cmd_le_gap_bt5_set_adv_data(1, 8, sizeof(adv_packet), adv_packet);

  for (int i = 0; i < MAX_CONNECTIONS; i++)
  {
    if (server->peers[i].mode == MODE_CONNECTED)
    {
      gecko_cmd_gatt_server_send_characteristic_notification(server->peers[i].handle, gattdb_audio,
                                                             sizeof(data_frame_t), (uint8_t*)frame);
    }
  }
}

void bluetooth_poll(int max_sleep_ms)
{
  static server_state_t server;

  struct gecko_cmd_packet* evt;
  evt = gecko_peek_event();

  if (!evt)
  {
      if (false)
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
      server_init(&server);
      gecko_cmd_le_gap_set_advertise_timing(0, 320, 640, 0, 0);
      gecko_cmd_le_gap_start_advertising(0, le_gap_general_discoverable, le_gap_connectable_scannable);
      gecko_cmd_hardware_set_lazy_soft_timer(MS2TICKS(20), MS2TICKS(2), 0, true);
    }
    else if (id == gecko_evt_le_connection_opened_id)
    {
      int connid = evt->data.evt_le_connection_opened.connection;
      peer_state_t *peer = server_find_free_peer(&server);
      if (peer)
      {
        peer_connected(peer, connid);
        gecko_cmd_le_connection_set_parameters(peer->handle, 14, 16, 0, 100);
      }
      else
      {
        gecko_cmd_le_connection_close(connid);
      }
    }
    else if (id == gecko_evt_gatt_server_user_write_request_id)
    {
      if (evt->data.evt_gatt_server_user_write_request.characteristic == gattdb_audio)
      {
        if (evt->data.evt_gatt_server_user_write_request.value.len == sizeof(data_frame_t))
        {
          int connid = evt->data.evt_gatt_server_user_write_request.connection;
          data_frame_t *frame = (data_frame_t*)evt->data.evt_gatt_server_user_write_request.value.data;
          peer_state_t *peer = server_find_peer_by_connection(&server, connid);
          if (peer)
          {
            peer_packet_received(peer, frame);
          }
        }
      }
    }
    else if (id == gecko_evt_le_connection_closed_id)
    {
      int connid = evt->data.evt_le_connection_closed.connection;
      peer_state_t *peer = server_find_peer_by_connection(&server, connid);
      if (peer)
      {
        peer_disconnected(peer);
      }
    }
    else if (id == gecko_evt_hardware_soft_timer_id)
    {
      int latency = audio_read_pos() - server.read_pos;
      if (latency < 0 || latency > 2 * SAMPLES_PER_FRAME)
      {
        server.read_pos = audio_read_pos() - SAMPLES_PER_FRAME;
      }

      if (audio_read_pos() - server.read_pos > SAMPLES_PER_FRAME)
      {
        server_send_packets(&server);
      }

      int readable = audio_read_pos() - server.read_pos;
      int ticks_until_next = (SAMPLES_PER_FRAME - readable + 16) * TICK_FREQ / AUDIO_SAMPLERATE;
      if (ticks_until_next < MS2TICKS(10)) ticks_until_next = MS2TICKS(10);

      gecko_cmd_hardware_set_lazy_soft_timer(ticks_until_next, MS2TICKS(2), 0, true);
    }
  }
}

