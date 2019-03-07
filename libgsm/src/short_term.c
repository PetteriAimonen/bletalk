/*
 * Copyright 1992 by Jutta Degener and Carsten Bormann, Technische
 * Universitaet Berlin.  See the accompanying file "COPYRIGHT" for
 * details.  THERE IS ABSOLUTELY NO WARRANTY FOR THIS SOFTWARE.
 */

/* $Header: /tmp_amd/presto/export/kbs/jutta/src/gsm/RCS/short_term.c,v 1.2 1994/05/10 20:18:47 jutta Exp $ */

#include <stdio.h>
#include <assert.h>

#include "private.h"

#include "gsm.h"
#include "proto.h"

/*
 *  SHORT TERM ANALYSIS FILTERING SECTION
 */

/* 4.2.8 */

static void Decoding_of_the_coded_Log_Area_Ratios P2((LARc,LARpp),
	word 	* LARc,		/* coded log area ratio	[0..7] 	IN	*/
	word	* LARpp)	/* out: decoded ..			*/
{
	register word	temp1 /* , temp2 */;
	register long	ltmp;	/* for GSM_ADD */

	/*  This procedure requires for efficient implementation
	 *  two tables.
 	 *
	 *  INVA[1..8] = integer( (32768 * 8) / real_A[1..8])
	 *  MIC[1..8]  = minimum value of the LARc[1..8]
	 */

	/*  Compute the LARpp[1..8]
	 */

	/* 	for (i = 1; i <= 8; i++, B++, MIC++, INVA++, LARc++, LARpp++) {
	 *
	 *		temp1  = GSM_ADD( *LARc, *MIC ) << 10;
	 *		temp2  = *B << 1;
	 *		temp1  = GSM_SUB( temp1, temp2 );
	 *
	 *		assert(*INVA != MIN_WORD);
	 *
	 *		temp1  = GSM_MULT_R( *INVA, temp1 );
	 *		*LARpp = GSM_ADD( temp1, temp1 );
	 *	}
	 */

#undef	STEP
#define	STEP( B_TIMES_TWO, MIC, INVA )	\
		temp1    = GSM_ADD( *LARc++, MIC ) << 10;	\
		temp1    = GSM_SUB( temp1, B_TIMES_TWO );	\
		temp1    = GSM_MULT_R( INVA, temp1 );		\
		*LARpp++ = GSM_ADD( temp1, temp1 );

	STEP(      0,  -32,  13107 );
	STEP(      0,  -32,  13107 );
	STEP(   4096,  -16,  13107 );
	STEP(  -5120,  -16,  13107 );

	STEP(    188,   -8,  19223 );
	STEP(  -3584,   -8,  17476 );
	STEP(   -682,   -4,  31454 );
	STEP(  -2288,   -4,  29708 );

	/* NOTE: the addition of *MIC is used to restore
	 * 	 the sign of *LARc.
	 */
}

/* 4.2.9 */
/* Computation of the quantized reflection coefficients
 */

/* 4.2.9.1  Interpolation of the LARpp[1..8] to get the LARp[1..8]
 */

/*
 *  Within each frame of 160 analyzed speech samples the short term
 *  analysis and synthesis filters operate with four different sets of
 *  coefficients, derived from the previous set of decoded LARs(LARpp(j-1))
 *  and the actual set of decoded LARs (LARpp(j))
 *
 * (Initial value: LARpp(j-1)[1..8] = 0.)
 */

static void Coefficients_0_12 P3((LARpp_j_1, LARpp_j, LARp),
	register word * LARpp_j_1,
	register word * LARpp_j,
	register word * LARp)
{
	register int 	i;
	register longword ltmp;

	for (i = 1; i <= 8; i++, LARp++, LARpp_j_1++, LARpp_j++) {
		*LARp = GSM_ADD( SASR( *LARpp_j_1, 2 ), SASR( *LARpp_j, 2 ));
		*LARp = GSM_ADD( *LARp,  SASR( *LARpp_j_1, 1));
	}
}

static void Coefficients_13_26 P3((LARpp_j_1, LARpp_j, LARp),
	register word * LARpp_j_1,
	register word * LARpp_j,
	register word * LARp)
{
	register int i;
	register longword ltmp;
	for (i = 1; i <= 8; i++, LARpp_j_1++, LARpp_j++, LARp++) {
		*LARp = GSM_ADD( SASR( *LARpp_j_1, 1), SASR( *LARpp_j, 1 ));
	}
}

static void Coefficients_27_39 P3((LARpp_j_1, LARpp_j, LARp),
	register word * LARpp_j_1,
	register word * LARpp_j,
	register word * LARp)
{
	register int i;
	register longword ltmp;

	for (i = 1; i <= 8; i++, LARpp_j_1++, LARpp_j++, LARp++) {
		*LARp = GSM_ADD( SASR( *LARpp_j_1, 2 ), SASR( *LARpp_j, 2 ));
		*LARp = GSM_ADD( *LARp, SASR( *LARpp_j, 1 ));
	}
}


static void Coefficients_40_159 P2((LARpp_j, LARp),
	register word * LARpp_j,
	register word * LARp)
{
	register int i;

	for (i = 1; i <= 8; i++, LARp++, LARpp_j++)
		*LARp = *LARpp_j;
}

/* 4.2.9.2 */

static void LARp_to_rp P1((LARp),
	register word * LARp)	/* [0..7] IN/OUT  */
/*
 *  The input of this procedure is the interpolated LARp[0..7] array.
 *  The reflection coefficients, rp[i], are used in the analysis
 *  filter and in the synthesis filter.
 */
{
	register int 		i;
	register word		temp;
	register longword	ltmp;

	for (i = 1; i <= 8; i++, LARp++) {

		/* temp = GSM_ABS( *LARp );
	         *
		 * if (temp < 11059) temp <<= 1;
		 * else if (temp < 20070) temp += 11059;
		 * else temp = GSM_ADD( temp >> 2, 26112 );
		 *
		 * *LARp = *LARp < 0 ? -temp : temp;
		 */

		if (*LARp < 0) {
			temp = *LARp == MIN_WORD ? MAX_WORD : -(*LARp);
			*LARp = - ((temp < 11059) ? temp << 1
				: ((temp < 20070) ? temp + 11059
				:  GSM_ADD( temp >> 2, 26112 )));
		} else {
			temp  = *LARp;
			*LARp =    (temp < 11059) ? temp << 1
				: ((temp < 20070) ? temp + 11059
				:  GSM_ADD( temp >> 2, 26112 ));
		}
	}
}


/* 4.2.10 */
static void Short_term_analysis_filtering P4((S,rp,k_n,s),
	struct gsm_state * S,
	register word	* rp,	/* [0..7]	IN	*/
	register int 	k_n, 	/*   k_end - k_start	*/
	register word	* s	/* [0..n-1]	IN/OUT	*/
)
/*
 *  This procedure computes the short term residual signal d[..] to be fed
 *  to the RPE-LTP loop from the s[..] signal and from the local rp[..]
 *  array (quantized reflection coefficients).  As the call of this
 *  procedure can be done in many ways (see the interpolation of the LAR
 *  coefficient), it is assumed that the computation begins with index
 *  k_start (for arrays d[..] and s[..]) and stops with index k_end
 *  (k_start and k_end are defined in 4.2.9.1).  This procedure also
 *  needs to keep the array u[0..7] in memory for each call.
 */
{
	register word		* u = S->u;
	register int		i;
	register word		di, zzz, ui, sav, rpi;
	register longword 	ltmp;

	for (; k_n--; s++) {

		di = sav = *s;

		for (i = 0; i < 8; i++) {		/* YYY */

			ui    = u[i];
			rpi   = rp[i];
			u[i]  = sav;

			zzz   = GSM_MULT_R(rpi, di);
			sav   = GSM_ADD(   ui,  zzz);

			zzz   = GSM_MULT_R(rpi, ui);
			di    = GSM_ADD(   di,  zzz );
		}

		*s = di;
	}
}

#if defined(USE_FLOAT_MUL) && defined(FAST)

static void Fast_Short_term_analysis_filtering P4((S,rp,k_n,s),
	struct gsm_state * S,
	register word	* rp,	/* [0..7]	IN	*/
	register int 	k_n, 	/*   k_end - k_start	*/
	register word	* s	/* [0..n-1]	IN/OUT	*/
)
{
	register word		* u = S->u;
	register int		i;

	float 	  uf[8],
		 rpf[8];

	register float scalef = 3.0517578125e-5;
	register float		sav, di, temp;

	for (i = 0; i < 8; ++i) {
		uf[i]  = u[i];
		rpf[i] = rp[i] * scalef;
	}
	for (; k_n--; s++) {
		sav = di = *s;
		for (i = 0; i < 8; ++i) {
			register float rpfi = rpf[i];
			register float ufi  = uf[i];

			uf[i] = sav;
			temp  = rpfi * di + ufi;
			di   += rpfi * ufi;
			sav   = temp;
		}
		*s = di;
	}
	for (i = 0; i < 8; ++i) u[i] = uf[i];
}
#endif /* ! (defined (USE_FLOAT_MUL) && defined (FAST)) */

#ifdef __arm__

static void Short_term_synthesis_filtering P5((S,rrp,k,wt,sr),
	struct gsm_state * S,
	word	* rrp,	/* [0..7]	IN	*/
	int	k,	/* k_end - k_start	*/
	word	* wt,	/* [0..k-1]	IN	*/
	word	* sr	/* [0..k-1]	OUT	*/
)
{
    word  * v = S->v;

    while (k--) {
        word sri = *wt++;

        asm("mov r2, #16384\n\t"          // For rounding multiplication result

            "ldr r0, [%1, #12]\n\t"       // Load rrp[6] and rrp[7]
            "ldr r1, [%2, #12]\n\t"       // Load v[6] and v[7]
            "smlatt r3, r0, r1, r2\n\t"   // r3 = rrp[7] * v[7] + 16384
            "sub %0, %0, r3, asr #15\n\t" // sri -= r3 >> 15
            "ssat %0, #16, %0\n\t"        // Saturate sri
            "sxth r3, r1, ror #16\n\t"    // Sign-extend v[7]
            "add r3, r2, r3, lsl #15\n\t" // r3 = 16384 + v[7] << 15
            "smlatb r3, r0, %0, r3\n\t"   // r3 = rrp[7] * sri + 16384 + v[7] << 15
            "ssat r3, #16, r3,asr #15\n\t"// Saturate r3 >> 15
            "strh r3, [%2, #16]\n\t"      // Store to v[8]
            "smlabb r3, r0, r1, r2\n\t"   // r3 = rrp[6] * v[6] + 16384
            "sub %0, %0, r3, asr #15\n\t" // sri -= r3 >> 15
            "ssat %0, #16, %0\n\t"        // Saturate sri
            "sxth r3, r1\n\t"             // Sign-extend v[6]
            "add r3, r2, r3, lsl #15\n\t" // r3 = 16384 + v[6] << 15
            "smlabb r3, r0, %0, r3\n\t"   // r3 = rrp[6] * sri + 16384 + v[6] << 15
            "ssat r3, #16, r3,asr #15\n\t"// Saturate r3 >> 15
            "strh r3, [%2, #14]\n\t"      // Store to v[7]

            "ldr r0, [%1, #8]\n\t"        // Load rrp[4] and rrp[5]
            "ldr r1, [%2, #8]\n\t"        // Load v[4] and v[5]
            "smlatt r3, r0, r1, r2\n\t"   // r3 = rrp[5] * v[5] + 16384
            "sub %0, %0, r3, asr #15\n\t" // sri -= r3 >> 15
            "ssat %0, #16, %0\n\t"        // Saturate sri
            "sxth r3, r1, ror #16\n\t"    // Sign-extend v[5]
            "add r3, r2, r3, lsl #15\n\t" // r3 = 16384 + v[5] << 15
            "smlatb r3, r0, %0, r3\n\t"   // r3 = rrp[5] * sri + 16384 + v[5]
            "ssat r3, #16, r3,asr #15\n\t"// Saturate r3 >> 15
            "strh r3, [%2, #12]\n\t"      // Store to v[6]
            "smlabb r3, r0, r1, r2\n\t"   // r3 = rrp[4] * v[4] + 16384
            "sub %0, %0, r3, asr #15\n\t" // sri -= r3 >> 15
            "ssat %0, #16, %0\n\t"        // Saturate sri
            "sxth r3, r1\n\t"             // Sign-extend v[4]
            "add r3, r2, r3, lsl #15\n\t" // r3 = 16384 + v[4] << 15
            "smlabb r3, r0, %0, r3\n\t"   // r3 = rrp[4] * sri + 16384 + v[4]
            "ssat r3, #16, r3,asr #15\n\t"// Saturate r3 >> 15
            "strh r3, [%2, #10]\n\t"      // Store to v[5]

            "ldr r0, [%1, #4]\n\t"        // Load rrp[2] and rrp[3]
            "ldr r1, [%2, #4]\n\t"        // Load v[2] and v[3]
            "smlatt r3, r0, r1, r2\n\t"   // r3 = rrp[3] * v[3] + 16384
            "sub %0, %0, r3, asr #15\n\t" // sri -= r3 >> 15
            "ssat %0, #16, %0\n\t"        // Saturate sri
            "sxth r3, r1, ror #16\n\t"    // Sign-extend v[3]
            "add r3, r2, r3, lsl #15\n\t" // r3 = 16384 + v[3] << 15
            "smlatb r3, r0, %0, r3\n\t"   // r3 = rrp[3] * sri + 16384 + v[3]
            "ssat r3, #16, r3,asr #15\n\t"// Saturate r3 >> 15
            "strh r3, [%2, #8]\n\t"       // Store to v[4]
            "smlabb r3, r0, r1, r2\n\t"   // r3 = rrp[2] * v[2] + 16384
            "sub %0, %0, r3, asr #15\n\t" // sri -= r3 >> 15
            "ssat %0, #16, %0\n\t"        // Saturate sri
            "sxth r3, r1\n\t"             // Sign-extend v[2]
            "add r3, r2, r3, lsl #15\n\t" // r3 = 16384 + v[2] << 15
            "smlabb r3, r0, %0, r3\n\t"   // r3 = rrp[2] * sri + 16384 + v[2]
            "ssat r3, #16, r3,asr #15\n\t"// Saturate r3 >> 15
            "strh r3, [%2, #6]\n\t"       // Store to v[3]

            "ldr r0, [%1, #0]\n\t"        // Load rrp[0] and rrp[1]
            "ldr r1, [%2, #0]\n\t"        // Load v[0] and v[1]
            "smlatt r3, r0, r1, r2\n\t"   // r3 = rrp[1] * v[1] + 16384
            "sub %0, %0, r3, asr #15\n\t" // sri -= r3 >> 15
            "ssat %0, #16, %0\n\t"        // Saturate sri
            "sxth r3, r1, ror #16\n\t"    // Sign-extend v[1]
            "add r3, r2, r3, lsl #15\n\t" // r3 = 16384 + v[1] << 15
            "smlatb r3, r0, %0, r3\n\t"   // r3 = rrp[7] * sri + 16384 + v[1]
            "ssat r3, #16, r3,asr #15\n\t"// Saturate r3 >> 15
            "strh r3, [%2, #4]\n\t"       // Store to v[2]
            "smlabb r3, r0, r1, r2\n\t"   // r3 = rrp[0] * v[0] + 16384
            "sub %0, %0, r3, asr #15\n\t" // sri -= r3 >> 15
            "ssat %0, #16, %0\n\t"        // Saturate sri
            "sxth r3, r1\n\t"             // Sign-extend v[0]
            "add r3, r2, r3, lsl #15\n\t" // r3 = 16384 + v[0] << 15
            "smlabb r3, r0, %0, r3\n\t"   // r3 = rrp[0] * sri + 16384 + v[0]
            "ssat r3, #16, r3,asr #15\n\t"// Saturate r3 >> 15
            "strh r3, [%2, #2]\n\t"       // Store to v[1]
            : "+r"(sri)
            : "r"(rrp), "r"(v)
            : "r0", "r1", "r2", "r3", "memory");

        *sr++ = v[0] = sri;
    }
}

#else

void Short_term_synthesis_filtering P5((S,rrp,k,wt,sr),
	struct gsm_state * S,
	register word	* rrp,	/* [0..7]	IN	*/
	register int	k,	/* k_end - k_start	*/
	register word	* wt,	/* [0..k-1]	IN	*/
	register word	* sr	/* [0..k-1]	OUT	*/
)
{
	register word		* v = S->v;
	register int		i;
	register word		sri, tmp1, tmp2;
	register longword	ltmp;	/* for GSM_ADD  & GSM_SUB */

	while (k--) {
		sri = *wt++;
		for (i = 8; i--;) {

			/* sri = GSM_SUB( sri, gsm_mult_r( rrp[i], v[i] ) );
			 */
			tmp1 = rrp[i];
			tmp2 = v[i];
			tmp2 =  ( tmp1 == MIN_WORD && tmp2 == MIN_WORD
				? MAX_WORD
				: 0x0FFFF & (( (longword)tmp1 * (longword)tmp2
					     + 16384) >> 15)) ;

			sri  = GSM_SUB( sri, tmp2 );

			/* v[i+1] = GSM_ADD( v[i], gsm_mult_r( rrp[i], sri ) );
			 */
			tmp1  = ( tmp1 == MIN_WORD && sri == MIN_WORD
				? MAX_WORD
				: 0x0FFFF & (( (longword)tmp1 * (longword)sri
					     + 16384) >> 15)) ;

			v[i+1] = GSM_ADD( v[i], tmp1);
		}
		*sr++ = v[0] = sri;
	}
}

#endif


#if defined(FAST) && defined(USE_FLOAT_MUL)

static void Fast_Short_term_synthesis_filtering P5((S,rrp,k,wt,sr),
	struct gsm_state * S,
	register word	* rrp,	/* [0..7]	IN	*/
	register int	k,	/* k_end - k_start	*/
	register word	* wt,	/* [0..k-1]	IN	*/
	register word	* sr	/* [0..k-1]	OUT	*/
)
{
	register word		* v = S->v;
	register int		i;

	float va[9], rrpa[8];
	register float scalef = 3.0517578125e-5, temp;

	for (i = 0; i < 8; ++i) {
		va[i]   = v[i];
		rrpa[i] = (float)rrp[i] * scalef;
	}
	while (k--) {
		register float sri = *wt++;
		for (i = 8; i--;) {
			sri -= rrpa[i] * va[i];
			if     (sri < -32768.) sri = -32768.;
			else if (sri > 32767.) sri =  32767.;

			temp = va[i] + rrpa[i] * sri;
			if     (temp < -32768.) temp = -32768.;
			else if (temp > 32767.) temp =  32767.;
			va[i+1] = temp;
		}
		*sr++ = va[0] = sri;
	}
	for (i = 0; i < 9; ++i) v[i] = va[i];
}

#endif /* defined(FAST) && defined(USE_FLOAT_MUL) */

void Gsm_Short_Term_Analysis_Filter P3((S,LARc,s),

	struct gsm_state * S,

	word	* LARc,		/* coded log area ratio [0..7]  IN	*/
	word	* s		/* signal [0..159]		IN/OUT	*/
)
{
	word		* LARpp_j	= S->LARpp[ S->j      ];
	word		* LARpp_j_1	= S->LARpp[ S->j ^= 1 ];

	word		LARp[8];

#undef	FILTER
#if 	defined(FAST) && defined(USE_FLOAT_MUL)
# 	define	FILTER 	(* (S->fast			\
			   ? Fast_Short_term_analysis_filtering	\
		    	   : Short_term_analysis_filtering	))

#else
# 	define	FILTER	Short_term_analysis_filtering
#endif

	Decoding_of_the_coded_Log_Area_Ratios( LARc, LARpp_j );

	Coefficients_0_12(  LARpp_j_1, LARpp_j, LARp );
	LARp_to_rp( LARp );
	FILTER( S, LARp, 13, s);

	Coefficients_13_26( LARpp_j_1, LARpp_j, LARp);
	LARp_to_rp( LARp );
	FILTER( S, LARp, 14, s + 13);

	Coefficients_27_39( LARpp_j_1, LARpp_j, LARp);
	LARp_to_rp( LARp );
	FILTER( S, LARp, 13, s + 27);

	Coefficients_40_159( LARpp_j, LARp);
	LARp_to_rp( LARp );
	FILTER( S, LARp, 120, s + 40);
}

void Gsm_Short_Term_Synthesis_Filter P4((S, LARcr, wt, s),
	struct gsm_state * S,

	word	* LARcr,	/* received log area ratios [0..7] IN  */
	word	* wt,		/* received d [0..159]		   IN  */

	word	* s		/* signal   s [0..159]		  OUT  */
)
{
	word		* LARpp_j	= S->LARpp[ S->j     ];
	word		* LARpp_j_1	= S->LARpp[ S->j ^=1 ];

	word		LARp[8];

#undef	FILTER
#if 	defined(FAST) && defined(USE_FLOAT_MUL)

# 	define	FILTER 	(* (S->fast			\
			   ? Fast_Short_term_synthesis_filtering	\
		    	   : Short_term_synthesis_filtering	))
#else
#	define	FILTER	Short_term_synthesis_filtering
#endif

	Decoding_of_the_coded_Log_Area_Ratios( LARcr, LARpp_j );

	Coefficients_0_12( LARpp_j_1, LARpp_j, LARp );
	LARp_to_rp( LARp );
	FILTER( S, LARp, 13, wt, s );

	Coefficients_13_26( LARpp_j_1, LARpp_j, LARp);
	LARp_to_rp( LARp );
	FILTER( S, LARp, 14, wt + 13, s + 13 );

	Coefficients_27_39( LARpp_j_1, LARpp_j, LARp);
	LARp_to_rp( LARp );
	FILTER( S, LARp, 13, wt + 27, s + 27 );

	Coefficients_40_159( LARpp_j, LARp );
	LARp_to_rp( LARp );
	FILTER(S, LARp, 120, wt + 40, s + 40);
}
