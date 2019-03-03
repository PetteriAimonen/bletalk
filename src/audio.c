#include "audio.h"
#include "cortexm4_dsp.h"

#ifdef TESTING
#include <stdio.h>
#endif

// Data from bluetooth is received asynchronously and goes through ring
// buffer to the interrupt handler.
#define AUDIO_RINGBUF_SIZE 1024
static int16_t g_audio_ringbuf[AUDIO_RINGBUF_SIZE];
static unsigned g_audio_ringbuf_writepos = 0;
static unsigned g_audio_ringbuf_readpos = 0;
static unsigned g_audio_ringbuf_irqpos = 0;
static int g_audio_mic_gain = 256;
static int g_audio_spk_gain = 256;

int audio_max_writecount()
{
    return (AUDIO_RINGBUF_SIZE + g_audio_ringbuf_readpos - g_audio_ringbuf_writepos) % AUDIO_RINGBUF_SIZE;
}

void audio_write(const int16_t *samples, int sample_count)
{
    for (int i = 0; i < sample_count; i++)
    {
        g_audio_ringbuf[g_audio_ringbuf_writepos % AUDIO_RINGBUF_SIZE] = samples[i];
        g_audio_ringbuf_writepos++;
    }
}

int audio_max_readcount()
{
    return (AUDIO_RINGBUF_SIZE + g_audio_ringbuf_irqpos - g_audio_ringbuf_readpos) % AUDIO_RINGBUF_SIZE;
}

void audio_read(int16_t *samples, int sample_count)
{
    for (int i = 0; i < sample_count; i++)
    {
        samples[i] = g_audio_ringbuf[g_audio_ringbuf_readpos % AUDIO_RINGBUF_SIZE];
        g_audio_ringbuf_readpos++;
    }
}

void audio_set_mic_gain(int gain)
{
    g_audio_mic_gain = gain;
}

void audio_set_spk_gain(int gain)
{
    g_audio_spk_gain = gain;
}

// Signal processing chain:
// Microphone PDM in at 3.84 MHz
//           -> /8 decimation by popcount -> 480 kHz
//           -> /12 decimation by CIC filter with N=5, M=2, R=12 -> 40 kHz
//           -> FIR filter lowpass at 3.5kHz cutoff
//           -> /5 decimation -> 8 kHz
//           -> PCM out to network

#define MIC_CIC_LEVELS 5
#define MIC_CIC_MEMORY 2
#define MIC_CIC_RATIO 12
#define MIC_CIC_GAIN (24*24*24*24*24)
#define MIC_FIR_TAPS 32
static int g_mic_cic_integrators[MIC_CIC_LEVELS];
static int g_mic_cic_combs[MIC_CIC_LEVELS][MIC_CIC_MEMORY];
static unsigned g_mic_cic_memidx = 0;
static int16_t g_mic_fir_state[MIC_FIR_TAPS];

static const int16_t g_mic_fir_coeffs[MIC_FIR_TAPS + 2] = {
    548,  687,  624,  337, -125, -651,-1086,-1264,-1054, -395,  677, 2031, 3459, 4717, 5579, 5918, 5579, 4717, 3459,2031,  677, -395,-1054,-1264,-1086, -651, -125,  337,  624,  687,  548
};

static inline void process_mic_pdm_word(uint32_t pdm_word)
{
    // This filter runs at 480 kHz samplerate and processes 4 samples at a time.

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
    g_mic_cic_integrators[4] += g_mic_cic_integrators[3] * 4 + x0 * 35 + x1 * 15 + x2 * 5 + x3;
    g_mic_cic_integrators[3] += g_mic_cic_integrators[2] * 4 + x0 * 20 + x1 * 10 + x2 * 4 + x3;
    g_mic_cic_integrators[2] += g_mic_cic_integrators[1] * 4 + x0 * 10 + x1 *  6 + x2 * 3 + x3;
    g_mic_cic_integrators[1] += g_mic_cic_integrators[0] * 4 + x0 *  4 + x1 *  3 + x2 * 2 + x3;
    g_mic_cic_integrators[0] += x0 + x1 + x2 + x3;
}

static inline void process_mic_comb()
{
    // This filter runs at 40 kHz samplerate
    int comb_in = g_mic_cic_integrators[MIC_CIC_LEVELS-1];
    int comb_out;
    for (int i = 0; i < MIC_CIC_LEVELS; i++)
    {
        comb_out = comb_in - g_mic_cic_combs[i][g_mic_cic_memidx % MIC_CIC_MEMORY];
        g_mic_cic_combs[i][g_mic_cic_memidx % MIC_CIC_MEMORY] = comb_in;
        comb_in = comb_out;
    }

    comb_out = ((comb_out >> 8) * g_audio_mic_gain) >> 10;
    int16_t fir_in = (comb_out > INT16_MAX) ? INT16_MAX : (comb_out < INT16_MIN) ? INT16_MIN : comb_out;
    g_mic_fir_state[g_mic_cic_memidx % MIC_FIR_TAPS] = fir_in;

    g_mic_cic_memidx++;
}

static inline void process_mic_pcm()
{
    // This filter runs at 8 kHz samplerate

    int sum = 0;
    for (int i = 0; i < MIC_FIR_TAPS; i += 2)
    {
        uint32_t src = *(uint32_t*)&g_mic_fir_state[i];
        uint32_t coeff = *(uint32_t*)&g_mic_fir_coeffs[(MIC_FIR_TAPS - g_mic_cic_memidx + i) % MIC_FIR_TAPS];
        sum = SMLAD(src, coeff, sum);
    }

    int pcm_out = sum >> 15;
    if (pcm_out < INT16_MIN) pcm_out = INT16_MIN;
    if (pcm_out > INT16_MAX) pcm_out = INT16_MAX;

    g_audio_ringbuf[g_audio_ringbuf_irqpos] = pcm_out;
    g_audio_ringbuf_irqpos = (g_audio_ringbuf_irqpos + 1) % AUDIO_RINGBUF_SIZE;
}

// Signal processing chain:
// PCM in from network at 8 kHz
//           -> x5 interpolation by FIR filter -> 40 kHz
//           -> x12 second-order sigma-delta coding -> 480 kHz
//           -> x8 PDM expansion -> 3.84 MHz
//           -> PDM out to speaker

#define SPK_FIR_MEMLEN 8
#define SPK_FIR_RATIO 5
#define SPK_FIR_TAPS 8
static int16_t g_spk_fir_state[SPK_FIR_TAPS];
static unsigned g_spk_fir_memidx;
static int g_spk_fir_ipolstep;
static int g_spk_sigmadelta_in;
static int g_spk_sigmadelta_state[2];

static const int16_t g_spk_fir_coeffs[SPK_FIR_RATIO][SPK_FIR_TAPS + 2] = {
    { 17, -497, 5596, 1932,-1017,  694},
    {464,-1113, 4699, 3397,-1261,  701},
    {701,-1261, 3397, 4699,-1113,  464},
    {694,-1017, 1932, 5596, -497,   17},
    {482, -531,  562, 6243,  562, -531,  482},
};

static inline int saturate_3bit(int x) // Compiles to SSAT instruction
{
    return (x > 3) ? 3 : (x < -4) ? -4 : x;
}

static inline uint32_t process_speaker_pdm_word()
{
    // This filter runs at 480 kHz samplerate and generates 4 samples at a time.

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

static inline void process_speaker_fir()
{
    // This filter runs at 40 kHz samplerate

    // Do interpolation by FIR filter
    int sum = 0;
    const int16_t *coeffs = &g_spk_fir_coeffs[g_spk_fir_ipolstep][0];
    for (int i = 0; i < SPK_FIR_TAPS; i += 2)
    {
        uint32_t src = *(uint32_t*)&g_spk_fir_state[i];
        uint32_t coeff = *(uint32_t*)&coeffs[(SPK_FIR_TAPS - g_spk_fir_memidx + i) % SPK_FIR_TAPS];
        sum = SMLAD(src, coeff, sum);
    }

    int fir_out = (((sum * SPK_FIR_RATIO) >> 8) * g_audio_spk_gain) >> 16;
    fir_out = (fir_out > INT16_MAX) ? INT16_MAX : (fir_out < INT16_MIN) ? INT16_MIN : fir_out;
    g_spk_sigmadelta_in = fir_out;

//     fprintf(stderr, "%5d %5d\n", g_spk_fir_state[(g_spk_fir_memidx - 1) % SPK_FIR_MEMLEN], g_spk_sigmadelta_in);

    g_spk_fir_ipolstep++;
}

static inline void process_speaker_pcm()
{
    // This filter runs at 8 kHz samplerate
    int pcm_in = g_audio_ringbuf[g_audio_ringbuf_irqpos];
    g_spk_fir_state[g_spk_fir_memidx % SPK_FIR_MEMLEN] = pcm_in;
    g_spk_fir_memidx++;

    g_spk_fir_ipolstep = 0;
}

void audio_pdm_convert(uint8_t* pdm_buf, int size)
{
    uint32_t *p = (uint32_t*)pdm_buf;

    for (int i = 0; i < size; i += AUDIO_PDM_BYTES_PER_PCM_SAMPLE)
    {
        for (int j = 0; j < 5; j++)
        {
            process_mic_pdm_word(*p);
            *p++ = process_speaker_pdm_word();
            process_mic_pdm_word(*p);
            *p++ = process_speaker_pdm_word();
            process_mic_pdm_word(*p);
            *p++ = process_speaker_pdm_word();
            process_mic_comb();
            process_speaker_fir();
        }

        process_speaker_pcm();
        process_mic_pcm();
    }
}

#ifndef TESTING
#include <em_ldma.h>
#include <em_usart.h>
#include <em_gpio.h>
#include <em_acmp.h>
#include <em_cmu.h>

// LDMA and USART setup and config

// Audio input and output goes through USART1 in pulse density modulation
// format at 3.84MHz. This is converted in interrupt handler to 8kHz PCM
// format. Each PCM sample thus corresponds to 960 PDM bits, or 60 bytes.
// DMA interrupt occurs at 1kHz rate, converting 480 bytes each time.
#define PDM_BUF_SIZE 480
static uint8_t g_pdm_buf_1[PDM_BUF_SIZE];
static uint8_t g_pdm_buf_2[PDM_BUF_SIZE];
static uint8_t g_pdm_buf_idx = 0;
unsigned g_audio_irq_cycles = 0;

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

    CMU_ClockEnable(cmuClock_USART1, true);
    CMU_ClockEnable(cmuClock_LDMA, true);
    CMU_ClockEnable(cmuClock_ACMP0, true);

    USART_InitSync_TypeDef usart_init = USART_INITSYNC_DEFAULT;
    usart_init.baudrate = 3840000;
    USART_InitSync(USART1, &usart_init);

    ACMP_Init_TypeDef acmp_init = ACMP_INIT_DEFAULT;
    ACMP_VBConfig_TypeDef input_B = ACMP_VBCONFIG_DEFAULT;
    acmp_init.fullBias = true;
    acmp_init.biasProg = 0x20;
    input_B.input = acmpVBInput1V25;
    ACMP_Init(ACMP0, &acmp_init);
    ACMP_GPIOSetup(ACMP0, 16, true, true);
    ACMP_VBSetup(ACMP0, &input_B);
    ACMP_ChannelSet(ACMP0, acmpInputVBDIV, acmpInputAPORT1XCH18);

    GPIO_PinModeSet(gpioPortB, 11, gpioModeInput, 0);
    GPIO_PinModeSet(gpioPortB, 12, gpioModePushPull, 0);
    GPIO_PinModeSet(gpioPortC, 10, gpioModePushPull, 0);
    GPIO_PinModeSet(gpioPortC, 11, gpioModePushPull, 0);
    GPIO_PinModeSet(gpioPortF, 2, gpioModeInput, 0);

    USART1->ROUTELOC0 = 0x05000F05; // RX: PB11, CLK: PB12, TX: PC10
    USART1->ROUTEPEN = USART_ROUTEPEN_CLKPEN | USART_ROUTEPEN_RXPEN | USART_ROUTEPEN_TXPEN;

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
        g_audio_irq_cycles = DWT->CYCCNT - start;
    }
}

#endif
