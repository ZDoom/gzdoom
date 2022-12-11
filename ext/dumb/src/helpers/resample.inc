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
 * resample.inc - Resampling helper template.         / / \  \
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



void dumb_reset_resampler(DUMB_RESAMPLER *resampler, SRCTYPE *src, int src_channels, int32 pos, int32 start, int32 end, int quality)
{
	int i;
	resampler->src = src;
	resampler->pos = pos;
	resampler->subpos = 0;
	resampler->start = start;
	resampler->end = end;
	resampler->dir = 1;
	resampler->pickup = NULL;
	resampler->pickup_data = NULL;
	if (quality < 0)
	{
		resampler->quality = 0;
	}
	else if (quality > DUMB_RQ_N_LEVELS - 1)
	{
		resampler->quality = DUMB_RQ_N_LEVELS - 1;
	}
	else
	{
		resampler->quality = quality;
	}
	for (i = 0; i < src_channels*3; i++) resampler->X[i] = 0;
	resampler->overshot = -1;
	resampler->fir_resampler_ratio = 0;
	resampler_clear(resampler->fir_resampler[0]);
	resampler_clear(resampler->fir_resampler[1]);
	resampler_set_quality(resampler->fir_resampler[0], resampler->quality - DUMB_RESAMPLER_BASE);
	resampler_set_quality(resampler->fir_resampler[1], resampler->quality - DUMB_RESAMPLER_BASE);
}



DUMB_RESAMPLER *dumb_start_resampler(SRCTYPE *src, int src_channels, int32 pos, int32 start, int32 end, int quality)
{
	DUMB_RESAMPLER *resampler = malloc(sizeof(*resampler));
	if (!resampler) return NULL;
	dumb_reset_resampler(resampler, src, src_channels, pos, start, end, quality);
	return resampler;
}



#define UPDATE_VOLUME( pvol, vol ) {                               \
	if (pvol) {                                                    \
		vol##r += vol##d;                                          \
		if ((vol##d < 0 && vol##r <= vol##t) ||                    \
			(vol##d > 0 && vol##r >= vol##t)) {                    \
			pvol->volume = pvol->target;                           \
            if ( pvol->declick_stage == 0 ||                       \
                 pvol->declick_stage >= 3)                         \
                 pvol->declick_stage++;                            \
			pvol = NULL;                                           \
			vol = MULSCV( vol##t, vol##m );                        \
		} else {                                                   \
			vol = MULSCV( vol##r, vol##m );                        \
		}                                                          \
	}                                                              \
}



/* Create mono source resampler. */
#define SUFFIX2 _1
#define SRC_CHANNELS 1
#define DIVIDE_BY_SRC_CHANNELS(x) (int)(x)
#define COPYSRC(dstarray, dstindex, srcarray, srcindex) (dstarray)[dstindex] = (srcarray)[srcindex]
#define COPYSRC2(dstarray, dstindex, condition, srcarray, srcindex) (dstarray)[dstindex] = condition ? (srcarray)[srcindex] : 0
#define MONO_DEST_VOLUME_PARAMETERS DUMB_VOLUME_RAMP_INFO * volume
#define MONO_DEST_VOLUME_VARIABLES vol, volr, vold, volt, volm
#define MONO_DEST_VOLUME_ZEROS 0, 0
#define SET_MONO_DEST_VOLUME_VARIABLES { \
	if ( volume ) { \
		volr = xs_FloorToInt(volume->volume * 16777216.f); \
		vold = xs_FloorToInt(volume->delta * 16777216.f); \
		volt = xs_FloorToInt(volume->target * 16777216.f); \
		volm = xs_FloorToInt(volume->mix * 16777216.f); \
		vol = MULSCV( volr, volm ); \
		if ( volr == volt ) volume = NULL; \
	} else { \
		vol = 0; \
		vold = 0; \
		volt = 0; \
		volm = 0; \
	} \
}
#define RETURN_MONO_DEST_VOLUME_VARIABLES if ( volume ) volume->volume = (float)volr / 16777216.0f
#define MONO_DEST_VOLUMES_ARE_ZERO (vol == 0 && volt == 0)
#define STEREO_DEST_MIX_ALIAS(op, upd, offset) { \
	int xm = x[offset]; \
	*dst++ op ALIAS(xm, lvol); \
	*dst++ op ALIAS(xm, rvol); \
	if ( upd ) UPDATE_VOLUME( volume_left, lvol ); \
	if ( upd ) UPDATE_VOLUME( volume_right, rvol ); \
}
#define STEREO_DEST_MIX_LINEAR(op, upd, o0, o1) { \
	int xm = LINEAR(x[o0], x[o1]); \
	*dst++ op MULSC(xm, lvol); \
	*dst++ op MULSC(xm, rvol); \
	if ( upd ) UPDATE_VOLUME( volume_left, lvol ); \
	if ( upd ) UPDATE_VOLUME( volume_right, rvol ); \
}
#define STEREO_DEST_MIX_CUBIC(op, upd, x0, x3, o0, o1, o2, o3) { \
	int xm = CUBIC(x0[o0], x[o1], x[o2], x3[o3]); \
	*dst++ op CUBICVOL(xm, lvol); \
	*dst++ op CUBICVOL(xm, rvol); \
	if ( upd ) UPDATE_VOLUME( volume_left, lvol ); \
	if ( upd ) UPDATE_VOLUME( volume_right, rvol ); \
}
#define POKE_FIR(offset) { \
        resampler_write_sample( resampler->fir_resampler[0], FIR(x[offset]) ); \
}
#define MONO_DEST_PEEK_FIR *dst = MULSC( resampler_get_sample( resampler->fir_resampler[0] ), vol )
#define MONO_DEST_MIX_FIR { \
        *dst++ += MULSC( resampler_get_sample( resampler->fir_resampler[0] ), vol ); \
        UPDATE_VOLUME( volume, vol ); \
}
#define ADVANCE_FIR resampler_remove_sample( resampler->fir_resampler[0], 1 )
#define STEREO_DEST_PEEK_FIR { \
        int sample = resampler_get_sample( resampler->fir_resampler[0] ); \
        *dst++ = MULSC( sample, lvol ); \
        *dst++ = MULSC( sample, rvol ); \
}
#define STEREO_DEST_MIX_FIR { \
        int sample = resampler_get_sample( resampler->fir_resampler[0] ); \
        *dst++ += MULSC( sample, lvol ); \
        *dst++ += MULSC( sample, rvol ); \
        UPDATE_VOLUME( volume_left, lvol ); \
        UPDATE_VOLUME( volume_right, rvol ); \
}
#include "resamp2.inc"

/* Create stereo source resampler. */
#define SUFFIX2 _2
#define SRC_CHANNELS 2
#define DIVIDE_BY_SRC_CHANNELS(x) (int)((x) >> 1)
#define COPYSRC(dstarray, dstindex, srcarray, srcindex) { \
	(dstarray)[(dstindex)*2] = (srcarray)[(srcindex)*2]; \
	(dstarray)[(dstindex)*2+1] = (srcarray)[(srcindex)*2+1]; \
}
#define COPYSRC2(dstarray, dstindex, condition, srcarray, srcindex) { \
	if (condition) { \
		(dstarray)[(dstindex)*2] = (srcarray)[(srcindex)*2]; \
		(dstarray)[(dstindex)*2+1] = (srcarray)[(srcindex)*2+1]; \
	} else { \
		(dstarray)[(dstindex)*2] = 0; \
		(dstarray)[(dstindex)*2+1] = 0; \
	} \
}

#define MONO_DEST_VOLUME_PARAMETERS DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right
#define MONO_DEST_VOLUME_VARIABLES lvol, lvolr, lvold, lvolt, lvolm, rvol, rvolr, rvold, rvolt, rvolm
#define MONO_DEST_VOLUME_ZEROS 0, 0
#define SET_MONO_DEST_VOLUME_VARIABLES { \
	if ( volume_left ) { \
		lvolr = xs_FloorToInt(volume_left->volume * 16777216.f); \
		lvold = xs_FloorToInt(volume_left->delta * 16777216.f); \
		lvolt = xs_FloorToInt(volume_left->target * 16777216.f); \
		lvolm = xs_FloorToInt(volume_left->mix * 16777216.f); \
		lvol = MULSCV( lvolr, lvolm ); \
		if ( lvolr == lvolt ) volume_left = NULL; \
	} else { \
		lvol = 0; \
		lvold = 0; \
		lvolt = 0; \
		lvolm = 0; \
	} \
	if ( volume_right ) { \
		rvolr = xs_FloorToInt(volume_right->volume * 16777216.f); \
		rvold = xs_FloorToInt(volume_right->delta * 16777216.f); \
		rvolt = xs_FloorToInt(volume_right->target * 16777216.f); \
		rvolm = xs_FloorToInt(volume_right->mix * 16777216.f); \
		rvol = MULSCV( rvolr, rvolm ); \
		if ( rvolr == rvolt ) volume_right = NULL; \
	} else { \
		rvol = 0; \
		rvold = 0; \
		rvolt = 0; \
		rvolm = 0; \
	} \
}
#define RETURN_MONO_DEST_VOLUME_VARIABLES { \
	if ( volume_left ) volume_left->volume = (float)lvolr / 16777216.0f; \
	if ( volume_right ) volume_right->volume = (float)rvolr / 16777216.0f; \
}
#define MONO_DEST_VOLUMES_ARE_ZERO (lvol == 0 && lvolt == 0 && rvol == 0 && rvolt == 0)
#define STEREO_DEST_MIX_ALIAS(op, upd, offset) { \
	*dst++ op ALIAS(x[(offset)*2], lvol); \
	*dst++ op ALIAS(x[(offset)*2+1], rvol); \
	if ( upd ) UPDATE_VOLUME( volume_left, lvol ); \
	if ( upd ) UPDATE_VOLUME( volume_right, rvol ); \
}
#define STEREO_DEST_MIX_LINEAR(op, upd, o0, o1) { \
	*dst++ op MULSC(LINEAR(x[(o0)*2], x[(o1)*2]), lvol); \
	*dst++ op MULSC(LINEAR(x[(o0)*2+1], x[(o1)*2+1]), rvol); \
	if ( upd ) UPDATE_VOLUME( volume_left, lvol ); \
	if ( upd ) UPDATE_VOLUME( volume_right, rvol ); \
}
#define STEREO_DEST_MIX_CUBIC(op, upd, x0, x3, o0, o1, o2, o3) { \
	*dst++ op CUBICVOL(CUBIC(x0[(o0)*2], x[(o1)*2], x[(o2)*2], x3[(o3)*2]), lvol); \
	*dst++ op CUBICVOL(CUBIC(x0[(o0)*2+1], x[(o1)*2+1], x[(o2)*2+1], x3[(o3)*2+1]), rvol); \
	if ( upd ) UPDATE_VOLUME( volume_left, lvol ); \
	if ( upd ) UPDATE_VOLUME( volume_right, rvol ); \
}
#define POKE_FIR(offset) { \
        resampler_write_sample( resampler->fir_resampler[0], FIR(x[(offset)*2+0]) ); \
        resampler_write_sample( resampler->fir_resampler[1], FIR(x[(offset)*2+1]) ); \
}
#define MONO_DEST_PEEK_FIR { \
        *dst = MULSC( resampler_get_sample( resampler->fir_resampler[0] ), lvol ) + \
                MULSC( resampler_get_sample( resampler->fir_resampler[1] ), rvol ); \
}
#define MONO_DEST_MIX_FIR { \
        *dst++ += MULSC( resampler_get_sample( resampler->fir_resampler[0] ), lvol ) + \
                MULSC( resampler_get_sample( resampler->fir_resampler[1] ), rvol ); \
        UPDATE_VOLUME( volume_left, lvol ); \
        UPDATE_VOLUME( volume_right, rvol ); \
}
#define ADVANCE_FIR { \
        resampler_remove_sample( resampler->fir_resampler[0], 1 ); \
        resampler_remove_sample( resampler->fir_resampler[1], 1 ); \
}
#define STEREO_DEST_PEEK_FIR { \
        *dst++ = MULSC( resampler_get_sample( resampler->fir_resampler[0] ), lvol ); \
        *dst++ = MULSC( resampler_get_sample( resampler->fir_resampler[1] ), rvol ); \
}
#define STEREO_DEST_MIX_FIR { \
        *dst++ += MULSC( resampler_get_sample( resampler->fir_resampler[0] ), lvol ); \
        *dst++ += MULSC( resampler_get_sample( resampler->fir_resampler[1] ), rvol ); \
        UPDATE_VOLUME( volume_left, lvol ); \
        UPDATE_VOLUME( volume_right, rvol ); \
}
#include "resamp2.inc"



void dumb_end_resampler(DUMB_RESAMPLER *resampler)
{
	if (resampler)
		free(resampler);
}



#undef FIR
#undef CUBICVOL
#undef CUBIC
#undef LINEAR
#undef ALIAS
#undef SRCBITS
#undef SRCTYPE
#undef SUFFIX
