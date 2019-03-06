// Simple helpers to use libgsm with static allocation.

#pragma once
#include <gsm.h>
#include <stdint.h>

// This should match libgsm private.h
struct gsm_state {
    short  dp0[ 280 ];
    short  e[ 50 ]; /* code.c    */

    short  z1;  /* preprocessing.c, Offset_com. */
    long L_z2;  /*                  Offset_com. */
    int  mp;  /*                  Preemphasis */

    short  u[8];  /* short_term_aly_filter.c */
    short  LARpp[2][8];  /*                              */
    short  j;  /*                              */

    short            ltp_cut;        /* long_term.c, LTP crosscorr.  */
    short  nrp; /* 40 */ /* long_term.c, synthesis */
    short  v[9];  /* short_term.c, synthesis */
    short  msr;  /* decoder.c, Postprocessing */

    char  verbose; /* only used if !NDEBUG  */
    char  fast;  /* only used if FAST  */

    char  wav_fmt; /* only used if WAV49 defined */
    unsigned char frame_index; /*            odd/even chaining */
    unsigned char frame_chain; /*   half-byte to carry forward */
};

#define GSM_STATE_INIT {{0}, {0}, 0, 0, 0, {0}, {{0}}, 0, 0, 40, {0}, 0, 0, 1, 0, 0, 0}
