//
// SSE/AVX intrinsics based drawers for the r_draw family of drawers.
//
// Note: This header file is intentionally not guarded by a __R_DRAW_RGBA_SSE__ define.
//       It is because the code is nearly identical for SSE vs AVX. The file is included
//       multiple times by r_draw_rgba.cpp with different defines that changes the class
//       names outputted and the type of intrinsics used.

#ifdef _MSC_VER
#pragma warning(disable: 4752) // warning C4752: found Intel(R) Advanced Vector Extensions; consider using /arch:AVX
#endif

class VecCommand(DrawSpanRGBA) : public DrawerCommand
{
	const uint32_t * RESTRICT _source;
	fixed_t _xfrac;
	fixed_t _yfrac;
	fixed_t _xstep;
	fixed_t _ystep;
	int _x1;
	int _x2;
	int _y;
	int _xbits;
	int _ybits;
	BYTE * RESTRICT _destorg;
	fixed_t _light;
	ShadeConstants _shade_constants;
	bool _nearest_filter;

public:
	VecCommand(DrawSpanRGBA)()
	{
		_source = (const uint32_t*)ds_source;
		_xfrac = ds_xfrac;
		_yfrac = ds_yfrac;
		_xstep = ds_xstep;
		_ystep = ds_ystep;
		_x1 = ds_x1;
		_x2 = ds_x2;
		_y = ds_y;
		_xbits = ds_xbits;
		_ybits = ds_ybits;
		_destorg = dc_destorg;
		_light = ds_light;
		_shade_constants = ds_shade_constants;
		_nearest_filter = !SampleBgra::span_sampler_setup(_source, _xbits, _ybits, _xstep, _ystep, ds_source_mipmapped);
	}

	void Execute(DrawerThread *thread) override
	{
		if (thread->line_skipped_by_thread(_y))
			return;

		dsfixed_t			xfrac;
		dsfixed_t			yfrac;
		dsfixed_t			xstep;
		dsfixed_t			ystep;
		uint32_t*			dest;
		const uint32_t*		source = _source;
		int 				count;
		int 				spot;

		xfrac = _xfrac;
		yfrac = _yfrac;

		dest = ylookup[_y] + _x1 + (uint32_t*)_destorg;

		count = _x2 - _x1 + 1;

		xstep = _xstep;
		ystep = _ystep;

		uint32_t light = LightBgra::calc_light_multiplier(_light);
		ShadeConstants shade_constants = _shade_constants;

		if (_nearest_filter)
		{
			if (_xbits == 6 && _ybits == 6)
			{
				// 64x64 is the most common case by far, so special case it.

				int sse_count = count / 4;
				count -= sse_count * 4;

				if (shade_constants.simple_shade)
				{
					VEC_SHADE_VARS();
					VEC_SHADE_SIMPLE_INIT(light);

					while (sse_count--)
					{
						// Current texture index in u,v.
						spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
						uint32_t p0 = source[spot];
						xfrac += xstep;
						yfrac += ystep;

						spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
						uint32_t p1 = source[spot];
						xfrac += xstep;
						yfrac += ystep;

						spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
						uint32_t p2 = source[spot];
						xfrac += xstep;
						yfrac += ystep;

						spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
						uint32_t p3 = source[spot];
						xfrac += xstep;
						yfrac += ystep;

						// Lookup pixel from flat texture tile,
						//  re-index using light/colormap.
						__m128i fg = _mm_set_epi32(p3, p2, p1, p0);
						VEC_SHADE_SIMPLE(fg);
						_mm_storeu_si128((__m128i*)dest, fg);

						// Next step in u,v.
						dest += 4;
					}
				}
				else
				{
					VEC_SHADE_VARS();
					VEC_SHADE_INIT(light, shade_constants);

					while (sse_count--)
					{
						// Current texture index in u,v.
						spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
						uint32_t p0 = source[spot];
						xfrac += xstep;
						yfrac += ystep;

						spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
						uint32_t p1 = source[spot];
						xfrac += xstep;
						yfrac += ystep;

						spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
						uint32_t p2 = source[spot];
						xfrac += xstep;
						yfrac += ystep;

						spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));
						uint32_t p3 = source[spot];
						xfrac += xstep;
						yfrac += ystep;

						// Lookup pixel from flat texture tile,
						//  re-index using light/colormap.
						__m128i fg = _mm_set_epi32(p3, p2, p1, p0);
						VEC_SHADE(fg, shade_constants);
						_mm_storeu_si128((__m128i*)dest, fg);

						// Next step in u,v.
						dest += 4;
					}
				}

				if (count == 0)
					return;

				do
				{
					// Current texture index in u,v.
					spot = ((xfrac >> (32 - 6 - 6))&(63 * 64)) + (yfrac >> (32 - 6));

					// Lookup pixel from flat texture tile
					*dest++ = LightBgra::shade_bgra(source[spot], light, shade_constants);

					// Next step in u,v.
					xfrac += xstep;
					yfrac += ystep;
				} while (--count);
			}
			else
			{
				BYTE yshift = 32 - _ybits;
				BYTE xshift = yshift - _xbits;
				int xmask = ((1 << _xbits) - 1) << _ybits;

				int sse_count = count / 4;
				count -= sse_count * 4;

				if (shade_constants.simple_shade)
				{
					VEC_SHADE_VARS();
					VEC_SHADE_SIMPLE_INIT(light);

					while (sse_count--)
					{
						spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						uint32_t p0 = source[spot];
						xfrac += xstep;
						yfrac += ystep;

						spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						uint32_t p1 = source[spot];
						xfrac += xstep;
						yfrac += ystep;

						spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						uint32_t p2 = source[spot];
						xfrac += xstep;
						yfrac += ystep;

						spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						uint32_t p3 = source[spot];
						xfrac += xstep;
						yfrac += ystep;

						// Lookup pixel from flat texture tile
						__m128i fg = _mm_set_epi32(p3, p2, p1, p0);
						VEC_SHADE_SIMPLE(fg);
						_mm_storeu_si128((__m128i*)dest, fg);
						dest += 4;
					}
				}
				else
				{
					VEC_SHADE_VARS();
					VEC_SHADE_INIT(light, shade_constants);

					while (sse_count--)
					{
						spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						uint32_t p0 = source[spot];
						xfrac += xstep;
						yfrac += ystep;

						spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						uint32_t p1 = source[spot];
						xfrac += xstep;
						yfrac += ystep;

						spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						uint32_t p2 = source[spot];
						xfrac += xstep;
						yfrac += ystep;

						spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);
						uint32_t p3 = source[spot];
						xfrac += xstep;
						yfrac += ystep;

						// Lookup pixel from flat texture tile
						__m128i fg = _mm_set_epi32(p3, p2, p1, p0);
						VEC_SHADE(fg, shade_constants);
						_mm_storeu_si128((__m128i*)dest, fg);
						dest += 4;
					}
				}

				if (count == 0)
					return;

				do
				{
					// Current texture index in u,v.
					spot = ((xfrac >> xshift) & xmask) + (yfrac >> yshift);

					// Lookup pixel from flat texture tile
					*dest++ = LightBgra::shade_bgra(source[spot], light, shade_constants);

					// Next step in u,v.
					xfrac += xstep;
					yfrac += ystep;
				} while (--count);
			}
		}
		else
		{
			if (_xbits == 6 && _ybits == 6)
			{
				// 64x64 is the most common case by far, so special case it.

				int sse_count = count / 4;
				count -= sse_count * 4;

				if (shade_constants.simple_shade)
				{
					VEC_SHADE_VARS();
					VEC_SHADE_SIMPLE_INIT(light);
					while (sse_count--)
					{
						__m128i fg;
						VEC_SAMPLE_BILINEAR4_SPAN(fg, source, xfrac, yfrac, xstep, ystep, 26, 26);
						VEC_SHADE_SIMPLE(fg);
						_mm_storeu_si128((__m128i*)dest, fg);
						dest += 4;
					}
				}
				else
				{
					VEC_SHADE_VARS();
					VEC_SHADE_INIT(light, shade_constants);
					while (sse_count--)
					{
						__m128i fg;
						VEC_SAMPLE_BILINEAR4_SPAN(fg, source, xfrac, yfrac, xstep, ystep, 26, 26);
						VEC_SHADE(fg, shade_constants);
						_mm_storeu_si128((__m128i*)dest, fg);
						dest += 4;
					}
				}

				if (count == 0)
					return;

				do
				{
					*dest++ = LightBgra::shade_bgra(SampleBgra::sample_bilinear(source, xfrac, yfrac, 26, 26), light, shade_constants);
					xfrac += xstep;
					yfrac += ystep;
				} while (--count);
			}
			else
			{
				int sse_count = count / 4;
				count -= sse_count * 4;

				if (shade_constants.simple_shade)
				{
					VEC_SHADE_VARS();
					VEC_SHADE_SIMPLE_INIT(light);
					while (sse_count--)
					{
						__m128i fg;
						int tmpx = 32 - _xbits;
						int tmpy = 32 - _ybits;
						VEC_SAMPLE_BILINEAR4_SPAN(fg, source, xfrac, yfrac, xstep, ystep, tmpx, tmpy);
						VEC_SHADE_SIMPLE(fg);
						_mm_storeu_si128((__m128i*)dest, fg);
						dest += 4;
					}
				}
				else
				{
					VEC_SHADE_VARS();
					VEC_SHADE_INIT(light, shade_constants);
					while (sse_count--)
					{
						__m128i fg;
						int tmpx = 32 - _xbits;
						int tmpy = 32 - _ybits;
						VEC_SAMPLE_BILINEAR4_SPAN(fg, source, xfrac, yfrac, xstep, ystep, tmpx, tmpy);
						VEC_SHADE(fg, shade_constants);
						_mm_storeu_si128((__m128i*)dest, fg);
						dest += 4;
					}
				}

				if (count == 0)
					return;

				do
				{
					*dest++ = LightBgra::shade_bgra(SampleBgra::sample_bilinear(source, xfrac, yfrac, 32 - _xbits, 32 - _ybits), light, shade_constants);
					xfrac += xstep;
					yfrac += ystep;
				} while (--count);
			}
		}
	}
};
