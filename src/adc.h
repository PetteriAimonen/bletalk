// Handles ADC conversion sequences
#pragma once

typedef enum {
  ADC_CHANNEL_BUTTON_VOLPLUS = 0,
  ADC_CHANNEL_VBATT_SENSE = 1,
  ADC_CHANNEL_HEADSET_SENSE = 2,
  ADC_CHANNEL_TEMPERATURE = 3,
  ADC_CHANNEL_COUNT = 4
} ADCChannel_t;

// Returns latest result in millivolts
int adc_get_result_mV(ADCChannel_t channel);

// Returns temperature in millicelsius
int adc_get_temperature_mC();

// Start a new conversion sequence
void adc_start_converting();

// Start new conversions at regular interval
void adc_poll();
