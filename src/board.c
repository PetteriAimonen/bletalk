#include "board.h"
#include <em_chip.h>
#include <em_cmu.h>
#include <em_emu.h>
#include <em_rtcc.h>
#include <sl_module_bgm113a256v2.h>

void board_init()
{
    CHIP_Init();
    
    // DC-DC power supply
    {
        EMU_DCDCInit_TypeDef dcdcInit = BSP_DCDC_INIT;
        EMU_DCDCInit(&dcdcInit);
    }
    
    // HF crystal oscillator
    {
        CMU_HFXOInit_TypeDef hfxoInit = BSP_CLK_HFXO_INIT;
        hfxoInit.ctuneSteadyState = BSP_CLK_HFXO_CTUNE;
        
        if (!(DEVINFO->MODULEINFO & DEVINFO_MODULEINFO_HFXOCALVAL_NOTVALID)) {
            hfxoInit.ctuneSteadyState = DEVINFO->MODXOCAL & _DEVINFO_MODXOCAL_HFXOCTUNE_MASK;
        }
        CMU_HFXOInit(&hfxoInit);
        SystemHFXOClockSet(BSP_CLK_HFXO_FREQ);
        CMU_OscillatorEnable(cmuOsc_HFXO, true, true);
        CMU_ClockSelectSet(cmuClock_HF, cmuSelect_HFXO);
        CMU_OscillatorEnable(cmuOsc_HFRCO, false, false);
        CMU_ClockEnable(cmuClock_HFLE, true);
    }
    
    // LF crystal oscillator
    {
        CMU_LFXOInit_TypeDef lfxoInit = BSP_CLK_LFXO_INIT;
        lfxoInit.ctune = BSP_CLK_LFXO_CTUNE;
        CMU_LFXOInit(&lfxoInit);
        SystemLFXOClockSet(BSP_CLK_LFXO_FREQ);
        CMU_ClockSelectSet(cmuClock_LFA, cmuSelect_LFXO);
        CMU_ClockSelectSet(cmuClock_LFB, cmuSelect_LFXO);
        CMU_ClockSelectSet(cmuClock_LFE, cmuSelect_LFXO);
    }
    
    // Enable peripheral clocks
    {
        CMU_ClockEnable(cmuClock_CRYOTIMER, true);
        CMU_ClockEnable(cmuClock_PRS, true);
        CMU_ClockEnable(cmuClock_GPIO, true);
        CMU_ClockEnable(cmuClock_USART0, true);
    }
}
