#include "buttons.h"
#include "board.h"
#include "adc.h"
#include "da7212.h"
#include <em_gpio.h>

uint8_t g_prev_button_state;
uint32_t g_prev_button_press;
bool g_volplus_has_been_low;

#define DEBOUNCE_MS 100

void buttons_init()
{
  GPIO_PinModeSet(BUTTON_VOLM_PORT, BUTTON_VOLM_PIN, gpioModeInputPullFilter, 0);
  GPIO_PinModeSet(BUTTON_VOLP_PORT, BUTTON_VOLP_PIN, gpioModeInputPullFilter, 0);
  GPIO_PinOutClear(BUTTON_VOLM_PORT, BUTTON_VOLM_PIN);
  GPIO_PinOutSet(BUTTON_VOLP_PORT, BUTTON_VOLP_PIN);

  g_volplus_has_been_low = false;

  // Boot beep
  da7212_beep(700, 200);
  delay_ms(300);
  da7212_beep(1000, 200);
  delay_ms(300);
  da7212_beep(1300, 200);
}

uint8_t buttons_read()
{
  uint8_t result = 0;

  if (GPIO_PinInGet(BUTTON_VOLM_PORT, BUTTON_VOLM_PIN))
    result |= 1;

  // When button is not pressed, internal pull-up will pull the voltage to
  // 2.4V * 470kohm / (470kohm + 43kohm) = 2.20V
  // If in addition USB cable is connected, the voltage will rise by
  // (5V - 2.4V) * 43kohm / 470kohm = 0.24V to ~2.44V
  // When button is pressed, button will force the voltage to 1.2V.
  if (adc_get_result_mV(ADC_CHANNEL_BUTTON_VOLPLUS) < 1800)
    result |= 2;
  else
    g_volplus_has_been_low = true;

  if (!g_volplus_has_been_low)
    result &= ~2; // Used for testing, volplus held pressed externally

  return result;
}

void buttons_poll()
{
  uint8_t state = buttons_read();

  if (state != 0 && g_prev_button_state == 0)
  {
    int time_now = get_ticks();

    if (time_now - g_prev_button_press > MS2TICKS(DEBOUNCE_MS))
    {
      da7212_beep(1000, 100);
    }

    g_prev_button_press = time_now;
  }

  if ((state & 2) && get_ticks() - g_prev_button_press > MS2TICKS(1000))
  {
    // Poweroff
    da7212_beep(1500, 100);
    delay_ms(500);
    da7212_beep(1000, 100);
    delay_ms(500);
    da7212_beep(500, 100);
    da7212_set_powersave(true);
    GPIO_PinOutClear(BUTTON_VOLP_PORT, BUTTON_VOLP_PIN);
    while(1);
  }

  g_prev_button_state = state;
}


