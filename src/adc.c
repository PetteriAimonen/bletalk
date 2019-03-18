#include "adc.h"
#include "board.h"
#include <em_cmu.h>
#include <em_adc.h>
#include <em_gpio.h>
#include <em_device.h>

static ADCChannel_t g_adc_conversion_index;
static int g_adc_results[ADC_CHANNEL_COUNT];
static uint32_t g_adc_prev_conversion;

#define CONVERSION_INTERVAL_MS 10

static void adc_convert_next()
{
  // This function sequences the conversions of ADC channels.
  // Because temprature sensor cannot be included in automatic
  // scan sequence, the conversions are sequenced manually.
  //
  // Sequence:
  // PB12: Volume+ button / USB sense   APORT3X CH28
  // PA1:  Battery voltage              APORT4X CH9
  // PD14: Headset sense                APORT3X CH6
  //       Internal temperature sensor

  ADC0->SINGLEFIFOCLEAR = 1;

  if (g_adc_conversion_index == ADC_CHANNEL_BUTTON_VOLPLUS)
  {
    ADC_InitSingle_TypeDef singleInit = ADC_INITSINGLE_DEFAULT;
    singleInit.acqTime = adcAcqTime256;
    singleInit.reference = _ADC_SINGLECTRL_REF_VDD;
    singleInit.posSel = BUTTON_VOLP_APORT;
    ADC_InitSingle(ADC0, &singleInit);
    ADC_Start(ADC0, adcStartSingle);
  }
  else if (g_adc_conversion_index == ADC_CHANNEL_VBATT_SENSE)
  {
    ADC_InitSingle_TypeDef singleInit = ADC_INITSINGLE_DEFAULT;
    singleInit.acqTime = adcAcqTime256;
    singleInit.reference = _ADC_SINGLECTRL_REF_VDD;
    singleInit.posSel = VBATT_SENSE_APORT;
    ADC_InitSingle(ADC0, &singleInit);
    ADC_Start(ADC0, adcStartSingle);
  }
  else if (g_adc_conversion_index == ADC_CHANNEL_HEADSET_SENSE)
  {
    ADC_InitSingle_TypeDef singleInit = ADC_INITSINGLE_DEFAULT;
    singleInit.acqTime = adcAcqTime256;
    singleInit.reference = _ADC_SINGLECTRL_REF_VDD;
    singleInit.posSel = HEADSET_SENSE_APORT;
    ADC_InitSingle(ADC0, &singleInit);
    ADC_Start(ADC0, adcStartSingle);
  }
  else if (g_adc_conversion_index == ADC_CHANNEL_TEMPERATURE)
  {
    ADC_InitSingle_TypeDef singleInit = ADC_INITSINGLE_DEFAULT;
    singleInit.acqTime = adcAcqTime16;
    singleInit.reference = _ADC_SINGLECTRL_REF_1V25;
    singleInit.posSel = adcPosSelTEMP;
    singleInit.resolution = adcResOVS;
    ADC_InitSingle(ADC0, &singleInit);
    ADC_Start(ADC0, adcStartSingle);
  }
}

void ADC0_IRQHandler(void)
{
  g_adc_results[g_adc_conversion_index] = ADC0->SINGLEDATA;
  ADC0->IFC = ADC0->IF;

  g_adc_conversion_index++;

  if (g_adc_conversion_index < ADC_CHANNEL_COUNT)
  {
    adc_convert_next();
  }
  else
  {
    ADC_Reset(ADC0);
    CMU_ClockEnable(cmuClock_ADC0, false);
  }
}

void adc_start_converting()
{
  CMU_ClockEnable(cmuClock_ADC0, true);
  GPIO_PinModeSet(VBATT_SENSE_PORT, VBATT_SENSE_PIN, gpioModeInput, 0);
  GPIO_PinModeSet(HEADSET_SENSE_PORT, HEADSET_SENSE_PIN, gpioModeInput, 0);

  ADC_Init_TypeDef adcInit = ADC_INIT_DEFAULT;
  adcInit.ovsRateSel = adcOvsRateSel16;
  ADC_Init(ADC0, &adcInit);

  NVIC_SetPriority(ADC0_IRQn, 255);
  NVIC_EnableIRQ(ADC0_IRQn);
  ADC0->IEN |= ADC_IEN_SINGLE;

  g_adc_conversion_index = 0;
  adc_convert_next();
}

int adc_get_result_mV(ADCChannel_t channel)
{
  return g_adc_results[channel] * 2400 / 4096;
}

int adc_get_temperature_mC()
{
  int calTemp = (DEVINFO->CAL & _DEVINFO_CAL_TEMP_MASK) >> _DEVINFO_CAL_TEMP_SHIFT;
  int calVal = (DEVINFO->ADC0CAL3 & 0xFFF0) >> _DEVINFO_ADC0CAL3_TEMPREAD1V25_SHIFT;

  int oversample = 16;
  int delta = calVal * oversample - g_adc_results[ADC_CHANNEL_TEMPERATURE];
  return calTemp * 1000 - delta * (1000.0f / (1.835f * oversample * 4096 / 1250));
}

void adc_poll()
{
  uint32_t timenow = get_ticks();

  if (timenow - g_adc_prev_conversion > MS2TICKS(CONVERSION_INTERVAL_MS))
  {
    adc_start_converting();
    g_adc_prev_conversion = timenow;
  }
}

