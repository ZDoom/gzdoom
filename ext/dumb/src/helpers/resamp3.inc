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
 * resamp3.inc - Resampling helper template.          / / \  \
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



int32 dumb_resample(DUMB_RESAMPLER *resampler, sample_t *dst, int32 dst_size, VOLUME_PARAMETERS, double delta)
{
	int dt, inv_dt;
	int VOLUME_VARIABLES;
	long done;
	long todo;
	double tododbl;
	int quality;

	if (!resampler || resampler->dir == 0) return 0;
	ASSERT(resampler->dir == -1 || resampler->dir == 1);

	done = 0;
	dt = xs_CRoundToInt(delta * 65536.0);
	if (dt == 0 || dt == 0x80000000) return 0;
	inv_dt = xs_CRoundToInt(1.0 / delta * 65536.0);
	SET_VOLUME_VARIABLES;

	if (VOLUMES_ARE_ZERO) dst = NULL;

	_dumb_init_cubic();

	quality = resampler->quality;

	while (done < dst_size) {
		if (process_pickup(resampler)) {
			RETURN_VOLUME_VARIABLES;
			return done;
		}

		if ((resampler->dir ^ dt) < 0)
			dt = -dt;

		if (resampler->dir < 0)
			tododbl = ((resampler->pos - resampler->start) * 65536.f + (resampler->subpos - dt)) / -dt;
		else
			tododbl = ((resampler->end - resampler->pos) * 65536.f - (resampler->subpos + 1 - dt)) / dt;

		if (tododbl <= 0)
			todo = 0;
		else if (tododbl >= dst_size - done)
			todo = dst_size - done;
		else
			todo = xs_FloorToInt(tododbl);

		done += todo;

		{
			SRCTYPE *src = resampler->src;
			long pos = resampler->pos;
			int subpos = resampler->subpos;
			long diff = pos;
			long overshot;
			if (resampler->dir < 0) {
				if (!dst) {
					/* Silence or simulation */
					LONG_LONG new_subpos = subpos + (LONG_LONG)dt * todo;
					pos += (long)(new_subpos >> 16);
					subpos = (long)new_subpos & 65535;
				} else if (quality <= DUMB_RQ_ALIASING) {
					/* Aliasing, backwards */
					SRCTYPE xbuf[2*SRC_CHANNELS];
					SRCTYPE *x = &xbuf[0];
					SRCTYPE *xstart;
					COPYSRC(xbuf, 0, resampler->X, 1);
					COPYSRC(xbuf, 1, resampler->X, 2);
					while (todo && x < &xbuf[2*SRC_CHANNELS]) {
						// TODO: check what happens when multiple tempo slides occur per row
						HEAVYASSERT(pos >= resampler->start);
						MIX_ALIAS(+=, 1, 0);
						subpos += dt;
						pos += subpos >> 16;
						x -= (subpos >> 16) * SRC_CHANNELS;
						subpos &= 65535;
						todo--;
					}
					x = xstart = &src[pos*SRC_CHANNELS];
					LOOP4(todo,
						MIX_ALIAS(+=, 1, 2);
						subpos += dt;
						x += (subpos >> 16) * SRC_CHANNELS;
						subpos &= 65535;
					);
					pos += DIVIDE_BY_SRC_CHANNELS(x - xstart);
				} else if (quality <= DUMB_LQ_LINEAR) {
					/* Linear interpolation, backwards */
					SRCTYPE xbuf[3*SRC_CHANNELS];
					SRCTYPE *x = &xbuf[1*SRC_CHANNELS];
					COPYSRC(xbuf, 0, resampler->X, 1);
					COPYSRC(xbuf, 1, resampler->X, 2);
					COPYSRC(xbuf, 2, src, pos);
					while (todo && x < &xbuf[3*SRC_CHANNELS]) {
						HEAVYASSERT(pos >= resampler->start);
						MIX_LINEAR(+=, 1, 0, -1);
						subpos += dt;
						pos += subpos >> 16;
						x -= (subpos >> 16) * SRC_CHANNELS;
						subpos &= 65535;
						todo--;
					}
					// TODO: use xstart for others too
					x = &src[pos*SRC_CHANNELS];
					LOOP4(todo,
						HEAVYASSERT(pos >= resampler->start);
						MIX_LINEAR(+=, 1, 1, 2);
						subpos += dt;
						pos += subpos >> 16;
						x += (subpos >> 16) * SRC_CHANNELS;
						subpos &= 65535;
					);
				} else if (quality <= DUMB_LQ_CUBIC) {
					/* Cubic interpolation, backwards */
					SRCTYPE xbuf[6*SRC_CHANNELS];
					SRCTYPE *x = &xbuf[3*SRC_CHANNELS];
					COPYSRC(xbuf, 0, resampler->X, 0);
					COPYSRC(xbuf, 1, resampler->X, 1);
					COPYSRC(xbuf, 2, resampler->X, 2);
					COPYSRC(xbuf, 3, src, pos);
					if (pos-1 >= resampler->start) COPYSRC(xbuf, 4, src, pos-1);
					if (pos-2 >= resampler->start) COPYSRC(xbuf, 5, src, pos-2);
					while (todo && x < &xbuf[6*SRC_CHANNELS]) {
						HEAVYASSERT(pos >= resampler->start);
						MIX_CUBIC(+=, 1, x, x, 0, -1, -2, -3);
						subpos += dt;
						pos += subpos >> 16;
						x -= (subpos >> 16) * SRC_CHANNELS;
						subpos &= 65535;
						todo--;
					}
					x = &src[pos*SRC_CHANNELS];
					LOOP4(todo,
						HEAVYASSERT(pos >= resampler->start);
						MIX_CUBIC(+=, 1, x, x, 0, 1, 2, 3);
						subpos += dt;
						pos += subpos >> 16;
						x += (subpos >> 16) * SRC_CHANNELS;
						subpos &= 65535;
					);
				} else {
					/* FIR resampling, backwards */
					SRCTYPE *x;
					if ( resampler->fir_resampler_ratio != delta ) {
						resampler_set_rate( resampler->fir_resampler[0], delta );
						resampler_set_rate( resampler->fir_resampler[1], delta );
						resampler->fir_resampler_ratio = delta;
					}
					x = &src[pos*SRC_CHANNELS];
					while ( todo ) {
							while ( ( resampler_get_free_count( resampler->fir_resampler[0] ) ||
							(!resampler_get_sample_count( resampler->fir_resampler[0] )
	#if SRC_CHANNELS == 2
							&& !resampler_get_sample_count( resampler->fir_resampler[1] )
	#endif
							) ) && pos >= resampler->start )
							{
									POKE_FIR(0);
									pos--;
									x -= SRC_CHANNELS;
							}
							if ( !resampler_get_sample_count( resampler->fir_resampler[0] ) ) break;
							MIX_FIR;
							ADVANCE_FIR;
							--todo;
					}
					done -= todo;
				}
				diff = diff - pos;
				overshot = resampler->start - pos - 1;
				if (diff >= 3) {
					COPYSRC2(resampler->X, 0, overshot < 3, src, pos+3);
					COPYSRC2(resampler->X, 1, overshot < 2, src, pos+2);
					COPYSRC2(resampler->X, 2, overshot < 1, src, pos+1);
				} else if (diff >= 2) {
					COPYSRC(resampler->X, 0, resampler->X, 2);
					COPYSRC2(resampler->X, 1, overshot < 2, src, pos+2);
					COPYSRC2(resampler->X, 2, overshot < 1, src, pos+1);
				} else if (diff >= 1) {
					COPYSRC(resampler->X, 0, resampler->X, 1);
					COPYSRC(resampler->X, 1, resampler->X, 2);
					COPYSRC2(resampler->X, 2, overshot < 1, src, pos+1);
				}
			} else {
				if (!dst) {
					/* Silence or simulation */
					LONG_LONG new_subpos = subpos + (LONG_LONG)dt * todo;
					pos += (long)(new_subpos >> 16);
					subpos = (long)new_subpos & 65535;
				} else if (quality <= DUMB_RQ_ALIASING) {
					/* Aliasing, forwards */
					SRCTYPE xbuf[2*SRC_CHANNELS];
					SRCTYPE *x = &xbuf[0];
					SRCTYPE *xstart;
					COPYSRC(xbuf, 0, resampler->X, 1);
					COPYSRC(xbuf, 1, resampler->X, 2);
					while (todo && x < &xbuf[2*SRC_CHANNELS]) {
						HEAVYASSERT(pos < resampler->end);
						MIX_ALIAS(+=, 1, 0);
						subpos += dt;
						pos += subpos >> 16;
						x += (subpos >> 16) * SRC_CHANNELS;
						subpos &= 65535;
						todo--;
					}
					x = xstart = &src[pos*SRC_CHANNELS];
					LOOP4(todo,
						MIX_ALIAS(+=, 1, -2);
						subpos += dt;
						x += (subpos >> 16) * SRC_CHANNELS;
						subpos &= 65535;
					);
					pos += DIVIDE_BY_SRC_CHANNELS(x - xstart);
				} else if (quality <= DUMB_LQ_LINEAR) {
					/* Linear interpolation, forwards */
					SRCTYPE xbuf[3*SRC_CHANNELS];
					SRCTYPE *x = &xbuf[1*SRC_CHANNELS];
					COPYSRC(xbuf, 0, resampler->X, 1);
					COPYSRC(xbuf, 1, resampler->X, 2);
					COPYSRC(xbuf, 2, src, pos);
					while (todo && x < &xbuf[3*SRC_CHANNELS]) {
						HEAVYASSERT(pos < resampler->end);
						MIX_LINEAR(+=, 1, -1, 0);
						subpos += dt;
						pos += subpos >> 16;
						x += (subpos >> 16) * SRC_CHANNELS;
						subpos &= 65535;
						todo--;
					}
					x = &src[pos*SRC_CHANNELS];
					LOOP4(todo,
						HEAVYASSERT(pos < resampler->end);
						MIX_LINEAR(+=, 1, -2, -1);
						subpos += dt;
						pos += subpos >> 16;
						x += (subpos >> 16) * SRC_CHANNELS;
						subpos &= 65535;
					);
				} else if (quality <= DUMB_LQ_CUBIC) {
					/* Cubic interpolation, forwards */
					SRCTYPE xbuf[6*SRC_CHANNELS];
					SRCTYPE *x = &xbuf[3*SRC_CHANNELS];
					COPYSRC(xbuf, 0, resampler->X, 0);
					COPYSRC(xbuf, 1, resampler->X, 1);
					COPYSRC(xbuf, 2, resampler->X, 2);
					COPYSRC(xbuf, 3, src, pos);
					if (pos+1 < resampler->end) COPYSRC(xbuf, 4, src, pos+1);
					if (pos+2 < resampler->end) COPYSRC(xbuf, 5, src, pos+2);
					while (todo && x < &xbuf[6*SRC_CHANNELS]) {
						HEAVYASSERT(pos < resampler->end);
						MIX_CUBIC(+=, 1, x, x, -3, -2, -1, 0);
						subpos += dt;
						pos += subpos >> 16;
						x += (subpos >> 16) * SRC_CHANNELS;
						subpos &= 65535;
						todo--;
					}
					x = &src[pos*SRC_CHANNELS];
					LOOP4(todo,
						HEAVYASSERT(pos < resampler->end);
						MIX_CUBIC(+=, 1, x, x, -3, -2, -1, 0);
						subpos += dt;
						pos += subpos >> 16;
						x += (subpos >> 16) * SRC_CHANNELS;
						subpos &= 65535;
					);
				} else {
					/* FIR resampling, forwards */
					SRCTYPE *x;
					if ( resampler->fir_resampler_ratio != delta ) {
						resampler_set_rate( resampler->fir_resampler[0], delta );
						resampler_set_rate( resampler->fir_resampler[1], delta );
						resampler->fir_resampler_ratio = delta;
					}
					x = &src[pos*SRC_CHANNELS];
					while ( todo ) {
							while ( ( resampler_get_free_count( resampler->fir_resampler[0] ) ||
							(!resampler_get_sample_count( resampler->fir_resampler[0] )
	#if SRC_CHANNELS == 2
							&& !resampler_get_sample_count( resampler->fir_resampler[1] )
	#endif
							) ) && pos < resampler->end )
							{
									POKE_FIR(0);
									pos++;
									x += SRC_CHANNELS;
							}
							if ( !resampler_get_sample_count( resampler->fir_resampler[0] ) ) break;
							MIX_FIR;
							ADVANCE_FIR;
							--todo;
					}
					done -= todo;
				}
				diff = pos - diff;
				overshot = pos - resampler->end;
				if (diff >= 3) {
					COPYSRC2(resampler->X, 0, overshot < 3, src, pos-3);
					COPYSRC2(resampler->X, 1, overshot < 2, src, pos-2);
					COPYSRC2(resampler->X, 2, overshot < 1, src, pos-1);
				} else if (diff >= 2) {
					COPYSRC(resampler->X, 0, resampler->X, 2);
					COPYSRC2(resampler->X, 1, overshot < 2, src, pos-2);
					COPYSRC2(resampler->X, 2, overshot < 1, src, pos-1);
				} else if (diff >= 1) {
					COPYSRC(resampler->X, 0, resampler->X, 1);
					COPYSRC(resampler->X, 1, resampler->X, 2);
					COPYSRC2(resampler->X, 2, overshot < 1, src, pos-1);
				}
			}
			resampler->pos = pos;
			resampler->subpos = subpos;
		}
	}

	RETURN_VOLUME_VARIABLES;
	return done;
}



void dumb_resample_get_current_sample(DUMB_RESAMPLER *resampler, VOLUME_PARAMETERS, sample_t *dst)
{
	int VOLUME_VARIABLES;
	SRCTYPE *src;
	long pos;
	int subpos;
	int quality;
	SRCTYPE *x;

	if (!resampler || resampler->dir == 0) { MIX_ZEROS(=); return; }
	ASSERT(resampler->dir == -1 || resampler->dir == 1);

	if (process_pickup(resampler)) { MIX_ZEROS(=); return; }

	SET_VOLUME_VARIABLES;

	if (VOLUMES_ARE_ZERO) { MIX_ZEROS(=); return; }

	_dumb_init_cubic();

	quality = resampler->quality;

	src = resampler->src;
	pos = resampler->pos;
	subpos = resampler->subpos;
	x = resampler->X;

	if (resampler->dir < 0) {
		HEAVYASSERT(pos >= resampler->start);
		if (quality <= DUMB_RQ_ALIASING) {
			/* Aliasing, backwards */
			MIX_ALIAS(=, 0, 1);
		} else if (quality <= DUMB_LQ_LINEAR) {
			/* Linear interpolation, backwards */
			MIX_LINEAR(=, 0, 2, 1);
		} else if (quality <= DUMB_LQ_CUBIC) {
			/* Cubic interpolation, backwards */
			MIX_CUBIC(=, 0, src, x, pos, 2, 1, 0);
		} else {
			/* FIR resampling, backwards */
			PEEK_FIR;
		}
	} else {
		HEAVYASSERT(pos < resampler->end);
		if (quality <= DUMB_RQ_ALIASING) {
			/* Aliasing */
			MIX_ALIAS(=, 0, 1);
		} else if (quality <= DUMB_LQ_LINEAR) {
			/* Linear interpolation, forwards */
			MIX_LINEAR(=, 0, 1, 2);
		} else if (quality <= DUMB_LQ_CUBIC) {
			/* Cubic interpolation, forwards */
			MIX_CUBIC(=, 0, x, src, 0, 1, 2, pos);
		} else {
			/* FIR resampling, forwards */
			PEEK_FIR;
		}
	}
}



#undef MIX_ZEROS
#undef MIX_FIR
#undef PEEK_FIR
#undef VOLUMES_ARE_ZERO
#undef SET_VOLUME_VARIABLES
#undef RETURN_VOLUME_VARIABLES
#undef VOLUME_VARIABLES
#undef VOLUME_PARAMETERS
#undef SUFFIX3
