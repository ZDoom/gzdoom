/*  _______         ____    __         ___    ___
 * \    _  \       \    /  \  /       \   \  /   /       '   '  '
 *  |  | \  \       |  |    ||         |   \/   |         .      .
 *  |  |  |  |      |  |    ||         ||\  /|  |
 *  |  |  |  |      |  |    ||         || \/ |  |         '  '  '
 *  |  |  |  |      |  |    ||         ||    |  |         .      .
 *  |  |_/  /        \  \__//          ||    |  |
 * /_______/ynamic    \____/niversal  /__\  /____\usic   /|  .  . ibliotheque
 *                                                      /  \
 *                                                     / .  \
 * resamp2.inc - Resampling helper template.          / / \  \
 *                                                   | <  /   \_
 * By Bob and entheh.                                |  \/ /\   /
 *                                                    \_  /  > /
 * In order to find a good trade-off between            | \ / /
 * speed and accuracy in this code, some tests          |  ' /
 * were carried out regarding the behaviour of           \__/
 * long long ints with gcc. The following code
 * was tested:
 *
 * int a, b, c;
 * c = ((long long)a * b) >> 16;
 *
 * DJGPP GCC Version 3.0.3 generated the following assembly language code for
 * the multiplication and scaling, leaving the 32-bit result in EAX.
 *
 * movl  -8(%ebp), %eax    ; read one int into EAX
 * imull -4(%ebp)          ; multiply by the other; result goes in EDX:EAX
 * shrdl $16, %edx, %eax   ; shift EAX right 16, shifting bits in from EDX
 *
 * Note that a 32*32->64 multiplication is performed, allowing for high
 * accuracy. On the Pentium 2 and above, shrdl takes two cycles (generally),
 * so it is a minor concern when four multiplications are being performed
 * (the cubic resampler). On the Pentium MMX and earlier, it takes four or
 * more cycles, so this method is unsuitable for use in the low-quality
 * resamplers.
 *
 * Since "long long" is a gcc-specific extension, we use LONG_LONG instead,
 * defined in dumb.h. We may investigate later what code MSVC generates, but
 * if it seems too slow then we suggest you use a good compiler.
 *
 * FIXME: these comments are somewhat out of date now.
 */



#define SUFFIX3 _2

/* For convenience, returns nonzero on stop. */
static int process_pickup(DUMB_RESAMPLER *resampler)
{
	if (resampler->overshot < 0) {
		resampler->overshot = 0;
		dumb_resample(resampler, NULL, 2, MONO_DEST_VOLUME_ZEROS, 1.0f); /* Doesn't matter which SUFFIX3. */
		COPYSRC(resampler->X, 0, resampler->X, 1);
	}

	for (;;) {
		SRCTYPE *src = resampler->src;

		if (resampler->dir < 0) {
			if (resampler->overshot >= 3 && resampler->pos+3 >= resampler->start) COPYSRC(resampler->X, 0, src, resampler->pos+3);
			if (resampler->overshot >= 2 && resampler->pos+2 >= resampler->start) COPYSRC(resampler->X, 1, src, resampler->pos+2);
			if (resampler->overshot >= 1 && resampler->pos+1 >= resampler->start) COPYSRC(resampler->X, 2, src, resampler->pos+1);
			resampler->overshot = resampler->start - resampler->pos - 1;
		} else {
			if (resampler->overshot >= 3 && resampler->pos-3 < resampler->end) COPYSRC(resampler->X, 0, src, resampler->pos-3);
			if (resampler->overshot >= 2 && resampler->pos-2 < resampler->end) COPYSRC(resampler->X, 1, src, resampler->pos-2);
			if (resampler->overshot >= 1 && resampler->pos-1 < resampler->end) COPYSRC(resampler->X, 2, src, resampler->pos-1);
			resampler->overshot = resampler->pos - resampler->end;
		}

		if (resampler->overshot < 0) {
			resampler->overshot = 0;
			return 0;
		}

		if (!resampler->pickup) {
			resampler->dir = 0;
			return 1;
		}
		(*resampler->pickup)(resampler, resampler->pickup_data);
		if (resampler->dir == 0) return 1;
		ASSERT(resampler->dir == -1 || resampler->dir == 1);
	}
}



/* Create mono destination resampler. */
/* SUFFIX3 was set above. */
#if 0
#define VOLUME_PARAMETERS MONO_DEST_VOLUME_PARAMETERS
#define VOLUME_VARIABLES MONO_DEST_VOLUME_VARIABLES
#define SET_VOLUME_VARIABLES SET_MONO_DEST_VOLUME_VARIABLES
#define RETURN_VOLUME_VARIABLES RETURN_MONO_DEST_VOLUME_VARIABLES
#define VOLUMES_ARE_ZERO MONO_DEST_VOLUMES_ARE_ZERO
#define MIX_ALIAS(op, upd, offset) MONO_DEST_MIX_ALIAS(op, upd, offset)
#define MIX_LINEAR(op, upd, o0, o1) MONO_DEST_MIX_LINEAR(op, upd, o0, o1)
#define MIX_CUBIC(op, upd, x0, x3, o0, o1, o2, o3) MONO_DEST_MIX_CUBIC(op, upd, x0, x3, o0, o1, o2, o3)
#define MIX_ZEROS(op) *dst++ op 0
#include "resamp3.inc"
#else
#undef SUFFIX3
#endif

/* Create stereo destination resampler. */
#define SUFFIX3 _2
#define VOLUME_PARAMETERS DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right
#define VOLUME_VARIABLES lvol, lvolr, lvold, lvolt, lvolm, rvol, rvolr, rvold, rvolt, rvolm
#define SET_VOLUME_VARIABLES { \
	if ( volume_left ) { \
		lvolr = (int)(volume_left->volume * 16777216.0); \
		lvold = (int)(volume_left->delta * 16777216.0); \
		lvolt = (int)(volume_left->target * 16777216.0); \
		lvolm = (int)(volume_left->mix * 16777216.0); \
		lvol = MULSCV( lvolr, lvolm ); \
		if ( lvolr == lvolt ) volume_left = NULL; \
	} else { \
		lvol = 0; \
		lvolt = 0; \
	} \
	if ( volume_right ) { \
		rvolr = (int)(volume_right->volume * 16777216.0); \
		rvold = (int)(volume_right->delta * 16777216.0); \
		rvolt = (int)(volume_right->target * 16777216.0); \
		rvolm = (int)(volume_right->mix * 16777216.0); \
		rvol = MULSCV( rvolr, rvolm ); \
		if ( rvolr == rvolt ) volume_right = NULL; \
	} else { \
		rvol = 0; \
		rvolt = 0; \
	} \
}
#define RETURN_VOLUME_VARIABLES { \
	if ( volume_left ) volume_left->volume = (float)lvolr / 16777216.0f; \
	if ( volume_right ) volume_right->volume = (float)rvolr / 16777216.0f; \
}
#define VOLUMES_ARE_ZERO (lvol == 0 && lvolt == 0 && rvol == 0 && rvolt == 0)
#define MIX_ALIAS(op, upd, offset) STEREO_DEST_MIX_ALIAS(op, upd, offset)
#define MIX_LINEAR(op, upd, o0, o1) STEREO_DEST_MIX_LINEAR(op, upd, o0, o1)
#define MIX_CUBIC(op, upd, x0, x3, o0, o1, o2, o3) STEREO_DEST_MIX_CUBIC(op, upd, x0, x3, o0, o1, o2, o3)
#define MIX_ZEROS(op) { *dst++ op 0; *dst++ op 0; }
#include "resamp3.inc"



#undef STEREO_DEST_MIX_CUBIC
#undef MONO_DEST_MIX_CUBIC
#undef STEREO_DEST_MIX_LINEAR
#undef MONO_DEST_MIX_LINEAR
#undef STEREO_DEST_MIX_ALIAS
#undef MONO_DEST_MIX_ALIAS
#undef MONO_DEST_VOLUMES_ARE_ZERO
#undef SET_MONO_DEST_VOLUME_VARIABLES
#undef RETURN_MONO_DEST_VOLUME_VARIABLES
#undef MONO_DEST_VOLUME_ZEROS
#undef MONO_DEST_VOLUME_VARIABLES
#undef MONO_DEST_VOLUME_PARAMETERS
#undef COPYSRC2
#undef COPYSRC
#undef DIVIDE_BY_SRC_CHANNELS
#undef SRC_CHANNELS
#undef SUFFIX2
