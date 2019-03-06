#include "audio.h"
#include "cortexm4_dsp.h"
#include <em_ldma.h>
#include <em_usart.h>
#include <em_gpio.h>
#include <em_acmp.h>
#include <em_cmu.h>

// Data from bluetooth is received asynchronously and goes through ring
// buffer to I2S bus. Different clients can mix audio to buffer until
// it is transmitted out.
#define AUDIO_RINGBUF_SIZE 1024
#define AUDIO_RINGBUF_MASK (AUDIO_RINGBUF_SIZE-1)
static int16_t g_audio_ringbuf[AUDIO_RINGBUF_SIZE];
static volatile uint32_t g_audio_dmapos;

uint32_t audio_write_pos()
{
  // Keep track on how many times we've gone around the ringbuffer.
  // This is just so that the indexes don't get reused immediately.
  uint32_t offset = (LDMA->CH[0].SRC - (uint32_t)g_audio_ringbuf) / sizeof(int16_t);
  uint32_t result = g_audio_dmapos;
  if (offset < (result & AUDIO_RINGBUF_MASK))
    result += AUDIO_RINGBUF_SIZE;
  result = (result & ~AUDIO_RINGBUF_MASK) | offset;
  g_audio_dmapos = result;
  return result;
}

void audio_write(uint32_t write_pos, const int16_t *samples, int sample_count)
{
  if (write_pos < audio_write_pos())
    return; // Too old position, don't overwrite other data

  for (int i = 0; i < sample_count; i++)
  {
    int v = g_audio_ringbuf[(write_pos + i) & AUDIO_RINGBUF_MASK] + samples[i];
    v = (v < INT16_MIN) ? INT16_MIN : (v > INT16_MAX) ? INT16_MAX : v;
    g_audio_ringbuf[(write_pos + i) & AUDIO_RINGBUF_MASK] = v;
  }
}

uint32_t audio_read_pos()
{
  return audio_write_pos() - 2;
}

void audio_read(uint32_t read_pos, int16_t* samples, int sample_count)
{
  for (int i = 0; i < sample_count; i++)
  {
    samples[i] = g_audio_ringbuf[(read_pos + i) & AUDIO_RINGBUF_MASK];
    g_audio_ringbuf[(read_pos + i) & AUDIO_RINGBUF_MASK] = 0;
  }
}

// LDMA and USART setup and config

static const LDMA_Descriptor_t g_audio_dma_write_desc[2] = {
  {.xfer = {.structType = ldmaCtrlStructTypeXfer, .structReq = 0,
            .xferCnt = (AUDIO_RINGBUF_SIZE-1), .byteSwap = 0,
            .blockSize = ldmaCtrlBlockSizeUnit1, .doneIfs = 0,
            .reqMode = ldmaCtrlReqModeBlock, .decLoopCnt = 0,
            .ignoreSrec = 0, .srcInc = ldmaCtrlSrcIncOne,
            .size = ldmaCtrlSizeHalf, .dstInc = ldmaCtrlDstIncNone,
            .srcAddrMode = ldmaCtrlSrcAddrModeAbs, .dstAddrMode = ldmaCtrlDstAddrModeAbs,
            .srcAddr = (uint32_t)g_audio_ringbuf, .dstAddr = (uint32_t)&USART1->TXDOUBLE,
            .linkMode = ldmaLinkModeRel, .link = 1, .linkAddr = 0}},
};

static const LDMA_Descriptor_t g_audio_dma_read_desc[2] = {
  {.xfer = {.structType = ldmaCtrlStructTypeXfer, .structReq = 0,
            .xferCnt = (AUDIO_RINGBUF_SIZE-1), .byteSwap = 0,
            .blockSize = ldmaCtrlBlockSizeUnit1, .doneIfs = 0,
            .reqMode = ldmaCtrlReqModeBlock, .decLoopCnt = 0,
            .ignoreSrec = 0, .srcInc = ldmaCtrlSrcIncNone,
            .size = ldmaCtrlSizeHalf, .dstInc = ldmaCtrlDstIncOne,
            .srcAddrMode = ldmaCtrlSrcAddrModeAbs, .dstAddrMode = ldmaCtrlDstAddrModeAbs,
            .srcAddr = (uint32_t)&USART1->RXDOUBLE, .dstAddr = (uint32_t)g_audio_ringbuf,
            .linkMode = ldmaLinkModeRel, .link = 1, .linkAddr = 0}},
};

static const LDMA_TransferCfg_t g_audio_dma_write_cfg = LDMA_TRANSFER_CFG_PERIPHERAL(DMAREQ_USART1_TXBL);
static const LDMA_TransferCfg_t g_audio_dma_read_cfg  = LDMA_TRANSFER_CFG_PERIPHERAL(DMAREQ_USART1_RXDATAV);

void audio_init()
{
    CMU_ClockEnable(cmuClock_USART1, true);
    CMU_ClockEnable(cmuClock_LDMA, true);
    CMU_ClockEnable(cmuClock_ACMP0, true);

    USART_InitI2s_TypeDef usart_init = USART_INITI2S_DEFAULT;
    usart_init.sync.baudrate = AUDIO_SAMPLERATE * 16;
    usart_init.mono = true;
    USART_InitI2s(USART1, &usart_init);

    GPIO_PinModeSet(gpioPortC, 10, gpioModeInput, 0);    // MISO
    GPIO_PinModeSet(gpioPortB, 11, gpioModePushPull, 0); // WCLK
    GPIO_PinModeSet(gpioPortA, 0, gpioModePushPull, 0);  // MOSI
    GPIO_PinModeSet(gpioPortD, 15, gpioModePushPull, 0); // BCLK

    USART1->ROUTELOC0 = 0x1503000E;
    USART1->ROUTEPEN = USART_ROUTEPEN_CLKPEN | USART_ROUTEPEN_CSPEN | USART_ROUTEPEN_RXPEN | USART_ROUTEPEN_TXPEN;

    LDMA_Init_t ldma_init = LDMA_INIT_DEFAULT;
    LDMA_Init(&ldma_init);

    LDMA_StartTransfer(0, &g_audio_dma_write_cfg, g_audio_dma_write_desc);
    LDMA_StartTransfer(1, &g_audio_dma_read_cfg, g_audio_dma_read_desc);
}
