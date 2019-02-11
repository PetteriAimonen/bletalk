#include "audio.h"

// Data from bluetooth is received asynchronously and goes through ring
// buffer to the interrupt handler.
#define AUDIO_RINGBUF_SIZE 1024
static int16_t g_audio_ringbuf[AUDIO_RINGBUF_SIZE];
static unsigned g_audio_ringbuf_writepos = 0;
static unsigned g_audio_ringbuf_readpos = 0;
static unsigned g_audio_ringbuf_irqpos = 0;

int audio_max_writecount()
{
    if (g_audio_ringbuf_writepos <= g_audio_ringbuf_readpos)
        return g_audio_ringbuf_readpos - g_audio_ringbuf_writepos;
    else
        return AUDIO_RINGBUF_SIZE - g_audio_ringbuf_writepos;
}

int16_t *audio_get_writeptr(int sample_count)
{
    int16_t *result = g_audio_ringbuf + g_audio_ringbuf_writepos;
    g_audio_ringbuf_writepos = (g_audio_ringbuf_writepos + sample_count) % AUDIO_RINGBUF_SIZE;
    return result;
}

int audio_max_readcount()
{
    if (g_audio_ringbuf_readpos <= g_audio_ringbuf_irqpos)
        return g_audio_ringbuf_irqpos - g_audio_ringbuf_readpos;
    else
        return AUDIO_RINGBUF_SIZE - g_audio_ringbuf_readpos;
}

const int16_t *audio_get_readptr(int sample_count)
{
    const int16_t *result = g_audio_ringbuf + g_audio_ringbuf_readpos;
    g_audio_ringbuf_readpos = (g_audio_ringbuf_readpos + sample_count) % AUDIO_RINGBUF_SIZE;
    return result;
}

// Signal processing chain:
// Microphone PDM in at 1.92 MHz
//           -> /8 decimation by popcount -> 240 kHz
//           -> /4 decimation by CIC filter with N=3, M=4, R=4 -> 60 kHz
//           -> Biquad filter lowpass at 4kHz corner freq
//           -> /5 decimation -> 12 kHz
//           -> PCM out to network

#define MIC_CIC_LEVELS 3
#define MIC_CIC_MEMORY 8
#define MIC_CIC_RATIO 4
#define MIC_CIC_GAIN (32*32*32)
static int g_mic_cic_integrators[MIC_CIC_LEVELS];
static int g_mic_cic_combs[MIC_CIC_LEVELS][MIC_CIC_MEMORY];
static unsigned g_mic_cic_memidx = 0;

#define MIC_BIQUAD_SHIFT 14
#define MIC_BIQUAD_SCALE (1 << MIC_BIQUAD_SHIFT)
#define MIC_BIQUAD_A0 (int32_t)(0.057983629007272525 * MIC_BIQUAD_SCALE + 0.5)
#define MIC_BIQUAD_A1 (int32_t)(0.11596725801454505 * MIC_BIQUAD_SCALE + 0.5)
#define MIC_BIQUAD_A2 (int32_t)(0.057983629007272525 * MIC_BIQUAD_SCALE + 0.5)
#define MIC_BIQUAD_B1 (int32_t)(-1.4992482796698348 * MIC_BIQUAD_SCALE - 0.5)
#define MIC_BIQUAD_B2 (int32_t)(0.7311827956989247 * MIC_BIQUAD_SCALE + 0.5)
#define MIC_BIQUAD_RATIO 5
static int g_mic_biquad_state[4];

static inline void process_mic_pdm_word(uint32_t pdm_word)
{
    // This filter runs at 240 kHz samplerate and processes 4 samples at a time.

    // Parallel popcount of each byte in the 32-bit word
    pdm_word = pdm_word - ((pdm_word >> 1) & 0x55555555);
    pdm_word = (pdm_word & 0x33333333) + ((pdm_word >> 2) & 0x33333333);
    pdm_word = (pdm_word >> 4) + pdm_word;

    // Extract popcounts and subtract middle value
    pdm_word = pdm_word + 0x0C0C0C0C; // Subtract 4 from each popcount
    int x0 = (int)(pdm_word << 28) >> 28; // Compiles to SBFX
    int x1 = (int)(pdm_word << 20) >> 28;
    int x2 = (int)(pdm_word << 12) >> 28;
    int x3 = (int)(pdm_word <<  4) >> 28;

    // Normally we'd first add x0 to [0], then [0] to [1] etc.
    // This code adds everything in one operation.
    g_mic_cic_integrators[2] += g_mic_cic_integrators[1] * 4 + x0 * 10 + x1 * 6 + x2 * 3 + x3;
    g_mic_cic_integrators[1] += g_mic_cic_integrators[0] * 4 + x0 *  4 + x1 * 3 + x2 * 2 + x3;
    g_mic_cic_integrators[0] += x0 + x1 + x2 + x3;
}

static inline void process_mic_biquad()
{
    // This filter runs at 60 kHz samplerate
    int comb_in = g_mic_cic_integrators[2];
    int comb_out;
    for (int i = 0; i < MIC_CIC_LEVELS; i++)
    {
        comb_out = comb_in - g_mic_cic_combs[i][g_mic_cic_memidx];
        g_mic_cic_combs[i][g_mic_cic_memidx] = comb_in;
        comb_in = comb_out;
    }

    g_mic_cic_memidx = (g_mic_cic_memidx + 1) % MIC_CIC_MEMORY;

    int biquad_in = comb_out >> 2; // comb_out * (4 * MIC_CIC_GAIN / INT16_MAX);
    int biquad_out = (MIC_BIQUAD_A0 * biquad_in
                    + MIC_BIQUAD_A1 * g_mic_biquad_state[0]
                    + MIC_BIQUAD_A2 * g_mic_biquad_state[1]
                    - MIC_BIQUAD_B1 * g_mic_biquad_state[2]
                    - MIC_BIQUAD_B2 * g_mic_biquad_state[3]) >> MIC_BIQUAD_SHIFT;
    g_mic_biquad_state[1] = g_mic_biquad_state[0];
    g_mic_biquad_state[0] = biquad_in;
    g_mic_biquad_state[3] = g_mic_biquad_state[2];
    g_mic_biquad_state[2] = biquad_out;
}

static inline void process_mic_pcm()
{
    // This filter runs at 12 kHz samplerate
    int pcm_out = g_mic_biquad_state[2];
    if (pcm_out < INT16_MIN) pcm_out = INT16_MIN;
    if (pcm_out > INT16_MAX) pcm_out = INT16_MAX;

    g_audio_ringbuf[g_audio_ringbuf_irqpos] = pcm_out;
    g_audio_ringbuf_irqpos = (g_audio_ringbuf_irqpos + 1) % AUDIO_RINGBUF_SIZE;
}

// Signal processing chain:
// PCM in from network
//           -> x5 interpolation by CIC filter with N=3, M=1, R=5
//           -> x4 second-order sigma-delta coding
//           -> x8 PDM expansion
//           -> PDM out to speaker

#define SPK_CIC_LEVELS 3
#define SPK_CIC_RATIO 5
#define SPK_CIC_GAIN (5*5*5/5)
static int g_spk_cic_integrators[SPK_CIC_LEVELS];
static int g_spk_cic_combs[SPK_CIC_LEVELS];

#define SPK_SIGMADELTA_RATIO 5
static int g_spk_sigmadelta_in;
static int g_spk_sigmadelta_state[2];

static inline int saturate_3bit(int x) // Compiles to SSAT instruction
{
    return (x > 3) ? 3 : (x < -4) ? -4 : x;
}

static inline uint32_t process_speaker_pdm_word()
{
    // This filter runs at 240 kHz samplerate and generates 4 bytes at a time.

    // Do four steps of sigma delta modulation with c1 = 0.5, c2 = 1
    int input = g_spk_sigmadelta_in;
    int s0 = g_spk_sigmadelta_state[0];
    int s1 = g_spk_sigmadelta_state[1];

    int out1 = saturate_3bit(s1 >> 13);
    s0 += input - (out1 << 12);
    s1 += s0 - (out1 << 13);

    int out2 = saturate_3bit(s1 >> 13);
    s0 += input - (out2 << 12);
    s1 += s0 - (out2 << 13);

    int out3 = saturate_3bit(s1 >> 13);
    s0 += input - (out3 << 12);
    s1 += s0 - (out3 << 13);

    int out4 = saturate_3bit(s1 >> 13);
    s0 += input - (out4 << 12);
    s1 += s0 - (out4 << 13);

    g_spk_sigmadelta_state[0] = s0;
    g_spk_sigmadelta_state[1] = s1;

    // Do 8-bit PDM expansion for the results
    uint32_t out_counts = ((out4 + 4) << 24) | ((out3 + 4) << 16) | ((out2 + 4) << 8) | (out1 + 4);
    uint32_t out_pdm = out_counts | ((out_counts & 0x02020202) << 4) | (out_counts & 0x04040404) * 0x33;

    return out_pdm;
}

static inline void process_speaker_cic()
{
    // This filter runs at 60 kHz samplerate
    for (int i = 1; i < SPK_CIC_LEVELS; i++)
    {
        g_spk_cic_integrators[i] += g_spk_cic_integrators[i-1];
    }
    g_spk_sigmadelta_in = g_spk_cic_integrators[SPK_CIC_LEVELS - 1] / SPK_CIC_GAIN;
}

#include <stdio.h>

static inline void process_speaker_pcm()
{
    // This filter runs at 12 kHz samplerate
    int comb_in = g_audio_ringbuf[g_audio_ringbuf_irqpos];
    int comb_out;
    for (int i = 0; i < SPK_CIC_LEVELS; i++)
    {
        comb_out = comb_in - g_spk_cic_combs[i];
        g_spk_cic_combs[i] = comb_in;
        comb_in = comb_out;
    }
    g_spk_cic_integrators[0] += comb_out;
}

void audio_pdm_convert(uint8_t* pdm_buf, int size)
{
    uint32_t *p = (uint32_t*)pdm_buf;

    for (int i = 0; i < size; i += 20)
    {
        for (int j = 0; j < 5; j++)
        {
            process_mic_pdm_word(*p);
            *p++ = process_speaker_pdm_word();
            process_mic_biquad();
            process_speaker_cic();
        }

        process_speaker_pcm();
        process_mic_pcm();
    }
}

#ifndef TESTING
#include <em_ldma.h>
#include <em_usart.h>
#include <em_gpio.h>

// LDMA and USART setup and config

// Audio input and output goes through USART1 in pulse density modulation
// format at 1.92MHz. This is converted in interrupt handler to 12kHz PCM
// format. Each PCM sample thus corresponds to 160 PDM bits, or 20 bytes.
// DMA interrupt occurs at 1kHz rate, converting 240 bytes each time.
#define PDM_BUF_SIZE 240
static uint8_t g_pdm_buf_1[PDM_BUF_SIZE];
static uint8_t g_pdm_buf_2[PDM_BUF_SIZE];
static uint8_t g_pdm_buf_idx = 0;
unsigned g_pdm_irq_cycles = 0;

static const LDMA_Descriptor_t g_pdm_dma_write_desc[2] = {
    LDMA_DESCRIPTOR_LINKREL_M2P_BYTE(g_pdm_buf_1, &USART1->TXDATA, PDM_BUF_SIZE, 1),
    LDMA_DESCRIPTOR_LINKREL_M2P_BYTE(g_pdm_buf_2, &USART1->TXDATA, PDM_BUF_SIZE, -1)
};

static const LDMA_Descriptor_t g_pdm_dma_read_desc[2] = {
    LDMA_DESCRIPTOR_LINKREL_P2M_BYTE(&USART1->RXDATA, g_pdm_buf_1, PDM_BUF_SIZE, 1),
    LDMA_DESCRIPTOR_LINKREL_P2M_BYTE(&USART1->RXDATA, g_pdm_buf_2, PDM_BUF_SIZE, -1)
};

static const LDMA_TransferCfg_t g_pdm_dma_write_cfg = LDMA_TRANSFER_CFG_PERIPHERAL(DMAREQ_USART1_TXBL);
static const LDMA_TransferCfg_t g_pdm_dma_read_cfg  = LDMA_TRANSFER_CFG_PERIPHERAL(DMAREQ_USART1_RXDATAV);

void audio_init()
{
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    USART_InitSync_TypeDef usart_init = USART_INITSYNC_DEFAULT;
    usart_init.baudrate = 1920000;
    USART_InitSync(USART1, &usart_init);

    GPIO_PinModeSet(gpioPortB, 11, gpioModeInput, 0);
    GPIO_PinModeSet(gpioPortB, 12, gpioModePushPull, 0);
    USART1->ROUTELOC0 = 0x05000F05; // RX: PB11, CLK: PB12, TX: PC10
    USART1->ROUTEPEN = USART_ROUTEPEN_CLKPEN | USART_ROUTEPEN_RXPEN;

    LDMA_Init_t ldma_init = LDMA_INIT_DEFAULT;
    LDMA_Init(&ldma_init);

    NVIC_SetPriority(LDMA_IRQn, 255);
    LDMA->IEN |= 1; // Enable channel 1 interrupt

    LDMA_StartTransfer(0, &g_pdm_dma_write_cfg, g_pdm_dma_write_desc);
    LDMA_StartTransfer(1, &g_pdm_dma_read_cfg, g_pdm_dma_read_desc);
}

void LDMA_IRQHandler(void)
{
    uint32_t flags = LDMA->IF;
    LDMA->IFC = flags;

    if (flags & 1)
    {
        unsigned start = DWT->CYCCNT;
        uint8_t *buf = g_pdm_buf_idx ? g_pdm_buf_2 : g_pdm_buf_1;
        g_pdm_buf_idx = !g_pdm_buf_idx;
        audio_pdm_convert(buf, PDM_BUF_SIZE);
        g_pdm_irq_cycles = DWT->CYCCNT - start;
    }
}

#endif
