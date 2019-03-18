#include "da7212.h"
#include <em_cmu.h>
#include <em_i2c.h>

#define DA7212_ADDR 0x34

#define DA7212_STATUS1                 0x02
#define DA7212_PLL_STATUS              0x03
#define DA7212_AUX_L_GAIN_STATUS       0x04
#define DA7212_AUX_R_GAIN_STATUS       0x05
#define DA7212_MIC_1_GAIN_STATUS       0x06
#define DA7212_MIC_2_GAIN_STATUS       0x07
#define DA7212_MIXIN_L_GAIN_STATUS     0x08
#define DA7212_MIXIN_R_GAIN_STATUS     0x09
#define DA7212_ADC_L_GAIN_STATUS       0x0A
#define DA7212_ADC_R_GAIN_STATUS       0x0B
#define DA7212_DAC_L_GAIN_STATUS       0x0C
#define DA7212_DAC_R_GAIN_STATUS       0x0D
#define DA7212_HP_L_GAIN_STATUS        0x0E
#define DA7212_HP_R_GAIN_STATUS        0x0F
#define DA7212_LINE_GAIN_STATUS        0x10
#define DA7212_CIF_CTRL                0x1D
#define DA7212_DIG_ROUTING_DAI         0x21
#define DA7212_SR                      0x22
#define DA7212_REFERENCES              0x23
#define DA7212_PLL_FRAC_TOP            0x24
#define DA7212_PLL_FRAC_BOT            0x25
#define DA7212_PLL_INTEGER             0x26
#define DA7212_PLL_CTRL                0x27
#define DA7212_DAI_CLK_MODE            0x28
#define DA7212_DAI_CTRL                0x29
#define DA7212_DIG_ROUTING_DAC         0x2A
#define DA7212_ALC_CTRL1               0x2B
#define DA7212_AUX_L_GAIN              0x30
#define DA7212_AUX_R_GAIN              0x31
#define DA7212_MIXIN_L_SELECT          0x32
#define DA7212_MIXIN_R_SELECT          0x33
#define DA7212_MIXIN_L_GAIN            0x34
#define DA7212_MIXIN_R_GAIN            0x35
#define DA7212_ADC_L_GAIN              0x36
#define DA7212_ADC_R_GAIN              0x37
#define DA7212_ADC_FILTERS1            0x38
#define DA7212_MIC_1_GAIN              0x39
#define DA7212_MIC_2_GAIN              0x3A
#define DA7212_DAC_FILTERS5            0x40
#define DA7212_DAC_FILTERS2            0x41
#define DA7212_DAC_FILTERS3            0x42
#define DA7212_DAC_FILTERS4            0x43
#define DA7212_DAC_FILTERS1            0x44
#define DA7212_DAC_L_GAIN              0x45
#define DA7212_DAC_R_GAIN              0x46
#define DA7212_CP_CTRL                 0x47
#define DA7212_HP_L_GAIN               0x48
#define DA7212_HP_R_GAIN               0x49
#define DA7212_LINE_GAIN               0x4A
#define DA7212_MIXOUT_L_SELECT         0x4B
#define DA7212_MIXOUT_R_SELECT         0x4C
#define DA7212_SYSTEM_MODES_INPUT      0x50
#define DA7212_SYSTEM_MODES_OUTPUT     0x51
#define DA7212_AUX_L_CTRL              0x60
#define DA7212_AUX_R_CTRL              0x61
#define DA7212_MICBIAS_CTRL            0x62
#define DA7212_MIC_1_CTRL              0x63
#define DA7212_MIC_2_CTRL              0x64
#define DA7212_MIXIN_L_CTRL            0x65
#define DA7212_MIXIN_R_CTRL            0x66
#define DA7212_ADC_L_CTRL              0x67
#define DA7212_ADC_R_CTRL              0x68
#define DA7212_DAC_L_CTRL              0x69
#define DA7212_DAC_R_CTRL              0x6A
#define DA7212_HP_L_CTRL               0x6B
#define DA7212_HP_R_CTRL               0x6C
#define DA7212_LINE_CTRL               0x6D
#define DA7212_MIXOUT_L_CTRL           0x6E
#define DA7212_MIXOUT_R_CTRL           0x6F
#define DA7212_LDO_CTRL                0x90
#define DA7212_GAIN_RAMP_CTRL          0x92
#define DA7212_MIC_CONFIG              0x93
#define DA7212_PC_COUNT                0x94
#define DA7212_CP_VOL_THRESHOLD1       0x95
#define DA7212_CP_DELAY                0x96
#define DA7212_CP_DETECTOR             0x97
#define DA7212_DAI_OFFSET              0x98
#define DA7212_DIG_CTRL                0x99
#define DA7212_ALC_CTRL2               0x9A
#define DA7212_ALC_CTRL3               0x9B
#define DA7212_ALC_NOISE               0x9C
#define DA7212_ALC_TARGET_MIN          0x9D
#define DA7212_ALC_THRESHOLD_MAX       0x9E
#define DA7212_ALC_GAIN_LIMITS         0x9F
#define DA7212_ALC_ANTICLIP_CTRL       0xA1
#define DA7212_ALC_ANTICLIP_LEVEL      0xA2
#define DA7212_ALC_OFFSET_AUTO_M_L     0xA3
#define DA7212_ALC_OFFSET_AUTO_U_L     0xA4
#define DA7212_ALC_OFFSET_MAN_M_L      0xA6
#define DA7212_ALC_OFFSET_MAN_U_L      0xA7
#define DA7212_ALC_OFFSET_AUTO_M_R     0xA8
#define DA7212_ALC_OFFSET_MAN_M_R      0xAB
#define DA7212_ALC_OFFSET_MAN_U_R      0xAC
#define DA7212_ALC_CIC_OP_LVL_CTRL     0xAD
#define DA7212_ALC_CIC_OP_LVL_DATA     0xAE
#define DA7212_DAC_NG_SETUP_TIME       0xAF
#define DA7212_DAC_NG_OFF_THRESHOLD    0xB0
#define DA7212_DAC_NG_ON_THRESHOLD     0xB1
#define DA7212_DAC_NG_CTRL             0xB2
#define DA7212_TONE_GEN_CFG1           0xB4
#define DA7212_TONE_GEN_CFG2           0xB5
#define DA7212_TONE_GEN_CYCLES         0xB6
#define DA7212_TONE_GEN_FREQ1_L        0xB7
#define DA7212_TONE_GEN_FREQ1_U        0xB8
#define DA7212_TONE_GEN_FREQ2_L        0xB9
#define DA7212_TONE_GEN_FREQ2_U        0xBA
#define DA7212_TONE_GEN_ON_PER         0xBB
#define DA7212_TONE_GEN_OFF_PER        0xBC
#define DA7212_SYSTEM_STATUS           0xE0
#define DA7212_SYSTEM_ACTIVE           0xFD

int da7212_readreg(uint8_t reg)
{
  uint8_t rxbuf;
  I2C_TransferSeq_TypeDef transfer = {
    .addr = DA7212_ADDR,
    .flags = I2C_FLAG_WRITE_READ,
    .buf = {{&reg, 1}, {&rxbuf, 1}}
  };

  I2C_TransferReturn_TypeDef status = I2C_TransferInit(I2C0, &transfer);
  while (status == i2cTransferInProgress) status = I2C_Transfer(I2C0);

  if (status >= 0)
    return rxbuf;
  else
    return status; // Negative error value
}

bool da7212_writereg(uint8_t reg, uint8_t value)
{
  I2C_TransferSeq_TypeDef transfer = {
    .addr = DA7212_ADDR,
    .flags = I2C_FLAG_WRITE_WRITE,
    .buf = {{&reg, 1}, {&value, 1}}
  };

  I2C_TransferReturn_TypeDef status = I2C_TransferInit(I2C0, &transfer);
  while (status == i2cTransferInProgress) status = I2C_Transfer(I2C0);

  return status == i2cTransferDone;
}

bool da7212_init()
{
  // Configure 9.6 MHz clock output to codec
  CMU_ClockPrescSet(cmuClock_EXPORT, 3);
  CMU_ClockSelectSet(cmuClock_EXPORT, cmuSelect_HFXO);
  CMU_ClockEnable(cmuClock_EXPORT, true);
  CMU->CTRL = (CMU->CTRL & ~_CMU_CTRL_CLKOUTSEL0_MASK) | CMU_CTRL_CLKOUTSEL0_HFEXPCLK;

  GPIO_PinModeSet(gpioPortF, 2, gpioModePushPull, 0);
  CMU->ROUTELOC0 = 0x0006; // CLK0 = PF2 (6)
  CMU->ROUTEPEN = CMU_ROUTEPEN_CLKOUT0PEN;

  // Configure I2C
  CMU_ClockEnable(cmuClock_I2C0, true);

  I2C_Init_TypeDef i2c_init = I2C_INIT_DEFAULT;
  i2c_init.freq = 50000;
  I2C_Init(I2C0, &i2c_init);

  GPIO_PinModeSet(gpioPortC, 11, gpioModePushPull, 0);
  GPIO_PinModeSet(gpioPortF, 3, gpioModeWiredAndPullUp, 0);
  I2C0->ROUTELOC0 = 0x0F1B; // SCL = PC11 (15), SDA = PF3 (27)
  I2C0->ROUTEPEN = I2C_ROUTEPEN_SCLPEN | I2C_ROUTEPEN_SDAPEN;

  return da7212_readreg(DA7212_STATUS1) >= 0;
}

bool da7212_set_powersave(bool powersave)
{
  if (powersave)
  {
    bool status = true;

    // Disable all inputs and outputs and enter standby
    status &= da7212_writereg(DA7212_SYSTEM_MODES_INPUT, 0x00);
    status &= da7212_writereg(DA7212_SYSTEM_MODES_OUTPUT, 0x01);
    status &= da7212_writereg(DA7212_SYSTEM_ACTIVE, 0x00);

    return status;
  }
  else
  {
    bool status = true;

    da7212_writereg(DA7212_SYSTEM_ACTIVE, 0x01); // NACK on this is normal

    da7212_writereg(DA7212_CIF_CTRL, 0x00); // Reset all regs

//     for (volatile int i = 0; i < 2000000; i++); // FIXME: 40ms delay

    da7212_writereg(DA7212_REFERENCES, 0x08); // Bias enable
//     da7212_writereg(DA7212_LDO_CTRL, 0x80); // Enable digital supply LDO

    // Configure PLL to get 12.288 MHz from 9.6 MHz
    // From DA7212 datasheet page 50 table 36.
    status &= da7212_writereg(DA7212_PLL_FRAC_TOP, 0x0F);
    status &= da7212_writereg(DA7212_PLL_FRAC_BOT, 0x5C);
    status &= da7212_writereg(DA7212_PLL_INTEGER, 0x14);
    status &= da7212_writereg(DA7212_PLL_CTRL, 0x80);

    // Configure audio interface as 16-bit mono in DSP mode
    status &= da7212_writereg(DA7212_DAI_CLK_MODE, 0x00);
    status &= da7212_writereg(DA7212_DAI_OFFSET, 0x00);
    status &= da7212_writereg(DA7212_DAI_CTRL, 0xD3);

    // Samplerate is 8kHz
    status &= da7212_writereg(DA7212_SR, 0x01);

    // Configure output mixing from DAC
    status &= da7212_writereg(DA7212_DIG_ROUTING_DAC, 0xBB);
    status &= da7212_writereg(DA7212_MIXOUT_L_SELECT, 0x08);
    status &= da7212_writereg(DA7212_MIXOUT_R_SELECT, 0x08);

    // Configure default gains
    status &= da7212_writereg(DA7212_LINE_GAIN, 0x30 - 3);

    // Configure input mode from MIC1 and output mode into speaker
    status &= da7212_writereg(DA7212_SYSTEM_MODES_INPUT, 0xC4);
    status &= da7212_writereg(DA7212_SYSTEM_MODES_OUTPUT, 0xC9);

    return status;
  }
}

bool da7212_beep(int freq, int length_ms)
{
  bool status = true;
  freq = freq * 65536 / 12000 - 1;

  int length = length_ms / 10;
  if (length > 0x14) length = (length_ms - 250) / 50 + 0x19;
  if (length > 0x3C) length = 0x3C;

  status &= da7212_writereg(DA7212_TONE_GEN_CYCLES, 0);
  status &= da7212_writereg(DA7212_TONE_GEN_ON_PER, length);
  status &= da7212_writereg(DA7212_TONE_GEN_OFF_PER, 1);
  status &= da7212_writereg(DA7212_TONE_GEN_FREQ1_L, freq & 0xFF);
  status &= da7212_writereg(DA7212_TONE_GEN_FREQ1_U, freq >> 8);
  status &= da7212_writereg(DA7212_TONE_GEN_CFG2, 0x41);
  status &= da7212_writereg(DA7212_TONE_GEN_CFG1, 0x80);

  return status;
}




