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

		uint32_t light = calc_light_multiplier(_light);
		ShadeConstants shade_constants = _shade_constants;

		fixed_t xmagnitude = abs((fixed_t)xstep) >> (32 - _xbits - FRACBITS);
		fixed_t ymagnitude = abs((fixed_t)ystep) >> (32 - _ybits - FRACBITS);
		fixed_t magnitude = xmagnitude + ymagnitude;

		bool magnifying = !r_bilinear || magnitude >> (FRACBITS - 1) == 0;
		if (magnifying)
		{
			if (_xbits == 6 && _ybits == 6)
			{
				// 64x64 is the most common case by far, so special case it.

				int sse_count = count / 4;
				count -= sse_count * 4;

				if (shade_constants.simple_shade)
				{
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
					*dest++ = shade_bgra(source[spot], light, shade_constants);

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
					*dest++ = shade_bgra(source[spot], light, shade_constants);

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
					VEC_SHADE_SIMPLE_INIT(light);
					while (sse_count--)
					{
						__m128i fg = sample_bilinear4_sse(source, xfrac, yfrac, xstep, ystep, 26, 26);
						VEC_SHADE_SIMPLE(fg);
						_mm_storeu_si128((__m128i*)dest, fg);
						dest += 4;
					}
				}
				else
				{
					VEC_SHADE_INIT(light, shade_constants);
					while (sse_count--)
					{
						__m128i fg = sample_bilinear4_sse(source, xfrac, yfrac, xstep, ystep, 26, 26);
						VEC_SHADE(fg, shade_constants);
						_mm_storeu_si128((__m128i*)dest, fg);
						dest += 4;
					}
				}

				if (count == 0)
					return;

				do
				{
					*dest++ = shade_bgra(sample_bilinear(source, xfrac, yfrac, 26, 26), light, shade_constants);
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
					VEC_SHADE_SIMPLE_INIT(light);
					while (sse_count--)
					{
						__m128i fg = sample_bilinear4_sse(source, xfrac, yfrac, xstep, ystep, 32 -_xbits, 32 - _ybits);
						VEC_SHADE_SIMPLE(fg);
						_mm_storeu_si128((__m128i*)dest, fg);
						dest += 4;
					}
				}
				else
				{
					VEC_SHADE_INIT(light, shade_constants);
					while (sse_count--)
					{
						__m128i fg = sample_bilinear4_sse(source, xfrac, yfrac, xstep, ystep, 32 - _xbits, 32 - _ybits);
						VEC_SHADE(fg, shade_constants);
						_mm_storeu_si128((__m128i*)dest, fg);
						dest += 4;
					}
				}

				if (count == 0)
					return;

				do
				{
					*dest++ = shade_bgra(sample_bilinear(source, xfrac, yfrac, 32 - _xbits, 32 - _ybits), light, shade_constants);
					xfrac += xstep;
					yfrac += ystep;
				} while (--count);
			}
		}
	}
};

class VecCommand(Vlinec4RGBA) : public DrawerCommand
{
	BYTE * RESTRICT _dest;
	int _count;
	int _pitch;
	ShadeConstants _shade_constants;
	int vlinebits;
	fixed_t palookuplight[4];
	DWORD vplce[4];
	DWORD vince[4];
	const uint32 * RESTRICT bufplce[4];
	const uint32_t * RESTRICT bufplce2[4];
	uint32_t buftexturefracx[4];

public:
	VecCommand(Vlinec4RGBA)()
	{
		_dest = dc_dest;
		_count = dc_count;
		_pitch = dc_pitch;
		_shade_constants = dc_shade_constants;
		vlinebits = ::vlinebits;
		for (int i = 0; i < 4; i++)
		{
			palookuplight[i] = ::palookuplight[i];
			vplce[i] = ::vplce[i];
			vince[i] = ::vince[i];
			bufplce[i] = (const uint32 *)::bufplce[i];
			bufplce2[i] = (const uint32_t *)::bufplce2[i];
			buftexturefracx[i] = ::buftexturefracx[i];
		}
	}

	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int bits = vlinebits;
		int pitch = _pitch * thread->num_cores;

		uint32_t light0 = calc_light_multiplier(palookuplight[0]);
		uint32_t light1 = calc_light_multiplier(palookuplight[1]);
		uint32_t light2 = calc_light_multiplier(palookuplight[2]);
		uint32_t light3 = calc_light_multiplier(palookuplight[3]);

		ShadeConstants shade_constants = _shade_constants;

		DWORD local_vplce[4] = { vplce[0], vplce[1], vplce[2], vplce[3] };
		DWORD local_vince[4] = { vince[0], vince[1], vince[2], vince[3] };
		int skipped = thread->skipped_by_thread(_dest_y);
		for (int i = 0; i < 4; i++)
		{
			local_vplce[i] += local_vince[i] * skipped;
			local_vince[i] *= thread->num_cores;
		}

		if (bufplce2[0] == nullptr)
		{
			if (shade_constants.simple_shade)
			{
				VEC_SHADE_SIMPLE_INIT4(light3, light2, light1, light0);
				do
				{
					DWORD place0 = local_vplce[0];
					DWORD place1 = local_vplce[1];
					DWORD place2 = local_vplce[2];
					DWORD place3 = local_vplce[3];

					uint32_t p0 = bufplce[0][place0 >> bits];
					uint32_t p1 = bufplce[1][place1 >> bits];
					uint32_t p2 = bufplce[2][place2 >> bits];
					uint32_t p3 = bufplce[3][place3 >> bits];

					local_vplce[0] = place0 + local_vince[0];
					local_vplce[1] = place1 + local_vince[1];
					local_vplce[2] = place2 + local_vince[2];
					local_vplce[3] = place3 + local_vince[3];

					__m128i fg = _mm_set_epi32(p3, p2, p1, p0);
					VEC_SHADE_SIMPLE(fg);
					_mm_storeu_si128((__m128i*)dest, fg);
					dest += pitch;
				} while (--count);
			}
			else
			{
				VEC_SHADE_INIT4(light3, light2, light1, light0, shade_constants);
				do
				{
					DWORD place0 = local_vplce[0];
					DWORD place1 = local_vplce[1];
					DWORD place2 = local_vplce[2];
					DWORD place3 = local_vplce[3];

					uint32_t p0 = bufplce[0][place0 >> bits];
					uint32_t p1 = bufplce[1][place1 >> bits];
					uint32_t p2 = bufplce[2][place2 >> bits];
					uint32_t p3 = bufplce[3][place3 >> bits];

					local_vplce[0] = place0 + local_vince[0];
					local_vplce[1] = place1 + local_vince[1];
					local_vplce[2] = place2 + local_vince[2];
					local_vplce[3] = place3 + local_vince[3];

					__m128i fg = _mm_set_epi32(p3, p2, p1, p0);
					VEC_SHADE(fg, shade_constants);
					_mm_storeu_si128((__m128i*)dest, fg);
					dest += pitch;
				} while (--count);
			}
		}
		else
		{
			if (shade_constants.simple_shade)
			{
				VEC_SHADE_SIMPLE_INIT4(light3, light2, light1, light0);
				do
				{
					__m128i fg = sample_bilinear4_sse(bufplce, bufplce2, buftexturefracx, local_vplce, bits);

					local_vplce[0] = local_vplce[0] + local_vince[0];
					local_vplce[1] = local_vplce[1] + local_vince[1];
					local_vplce[2] = local_vplce[2] + local_vince[2];
					local_vplce[3] = local_vplce[3] + local_vince[3];

					VEC_SHADE_SIMPLE(fg);
					_mm_storeu_si128((__m128i*)dest, fg);
					dest += pitch;
				} while (--count);
			}
			else
			{
				VEC_SHADE_INIT4(light3, light2, light1, light0, shade_constants);
				do
				{
					__m128i fg = sample_bilinear4_sse(bufplce, bufplce2, buftexturefracx, local_vplce, bits);

					local_vplce[0] = local_vplce[0] + local_vince[0];
					local_vplce[1] = local_vplce[1] + local_vince[1];
					local_vplce[2] = local_vplce[2] + local_vince[2];
					local_vplce[3] = local_vplce[3] + local_vince[3];

					VEC_SHADE(fg, shade_constants);
					_mm_storeu_si128((__m128i*)dest, fg);
					dest += pitch;
				} while (--count);
			}
		}
	}
};

class VecCommand(Mvlinec4RGBA) : public DrawerCommand
{
	BYTE * RESTRICT _dest;
	int _count;
	int _pitch;
	ShadeConstants _shade_constants;
	int mvlinebits;
	fixed_t palookuplight[4];
	DWORD vplce[4];
	DWORD vince[4];
	const uint32 * RESTRICT bufplce[4];
	const uint32 * RESTRICT bufplce2[4];
	uint32_t buftexturefracx[4];

public:
	VecCommand(Mvlinec4RGBA)()
	{
		_dest = dc_dest;
		_count = dc_count;
		_pitch = dc_pitch;
		_shade_constants = dc_shade_constants;
		mvlinebits = ::mvlinebits;
		for (int i = 0; i < 4; i++)
		{
			palookuplight[i] = ::palookuplight[i];
			vplce[i] = ::vplce[i];
			vince[i] = ::vince[i];
			bufplce[i] = (const uint32 *)::bufplce[i];
			bufplce2[i] = (const uint32_t *)::bufplce2[i];
			buftexturefracx[i] = ::buftexturefracx[i];
		}
	}

	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int pitch = _pitch * thread->num_cores;
		int bits = mvlinebits;

		uint32_t light0 = calc_light_multiplier(palookuplight[0]);
		uint32_t light1 = calc_light_multiplier(palookuplight[1]);
		uint32_t light2 = calc_light_multiplier(palookuplight[2]);
		uint32_t light3 = calc_light_multiplier(palookuplight[3]);

		ShadeConstants shade_constants = _shade_constants;

		DWORD local_vplce[4] = { vplce[0], vplce[1], vplce[2], vplce[3] };
		DWORD local_vince[4] = { vince[0], vince[1], vince[2], vince[3] };
		int skipped = thread->skipped_by_thread(_dest_y);
		for (int i = 0; i < 4; i++)
		{
			local_vplce[i] += local_vince[i] * skipped;
			local_vince[i] *= thread->num_cores;
		}

		if (bufplce2[0] == nullptr)
		{
			if (shade_constants.simple_shade)
			{
				VEC_SHADE_SIMPLE_INIT4(light3, light2, light1, light0);
				do
				{
					DWORD place0 = local_vplce[0];
					DWORD place1 = local_vplce[1];
					DWORD place2 = local_vplce[2];
					DWORD place3 = local_vplce[3];

					uint32_t pix0 = bufplce[0][place0 >> bits];
					uint32_t pix1 = bufplce[1][place1 >> bits];
					uint32_t pix2 = bufplce[2][place2 >> bits];
					uint32_t pix3 = bufplce[3][place3 >> bits];

					local_vplce[0] = place0 + local_vince[0];
					local_vplce[1] = place1 + local_vince[1];
					local_vplce[2] = place2 + local_vince[2];
					local_vplce[3] = place3 + local_vince[3];

					__m128i fg = _mm_set_epi32(pix3, pix2, pix1, pix0);
					__m128i bg = _mm_loadu_si128((const __m128i*)dest);
					VEC_SHADE_SIMPLE(fg);
					VEC_ALPHA_BLEND(fg, bg);
					_mm_storeu_si128((__m128i*)dest, fg);
					dest += pitch;
				} while (--count);
			}
			else
			{
				VEC_SHADE_INIT4(light3, light2, light1, light0, shade_constants);
				do
				{
					DWORD place0 = local_vplce[0];
					DWORD place1 = local_vplce[1];
					DWORD place2 = local_vplce[2];
					DWORD place3 = local_vplce[3];

					uint32_t pix0 = bufplce[0][place0 >> bits];
					uint32_t pix1 = bufplce[1][place1 >> bits];
					uint32_t pix2 = bufplce[2][place2 >> bits];
					uint32_t pix3 = bufplce[3][place3 >> bits];

					local_vplce[0] = place0 + local_vince[0];
					local_vplce[1] = place1 + local_vince[1];
					local_vplce[2] = place2 + local_vince[2];
					local_vplce[3] = place3 + local_vince[3];

					__m128i fg = _mm_set_epi32(pix3, pix2, pix1, pix0);
					__m128i bg = _mm_loadu_si128((const __m128i*)dest);
					VEC_SHADE(fg, shade_constants);
					VEC_ALPHA_BLEND(fg, bg);
					_mm_storeu_si128((__m128i*)dest, fg);
					dest += pitch;
				} while (--count);
			}
		}
		else
		{
			if (shade_constants.simple_shade)
			{
				VEC_SHADE_SIMPLE_INIT4(light3, light2, light1, light0);
				do
				{
					__m128i fg = sample_bilinear4_sse(bufplce, bufplce2, buftexturefracx, local_vplce, bits);

					local_vplce[0] = local_vplce[0] + local_vince[0];
					local_vplce[1] = local_vplce[1] + local_vince[1];
					local_vplce[2] = local_vplce[2] + local_vince[2];
					local_vplce[3] = local_vplce[3] + local_vince[3];

					__m128i bg = _mm_loadu_si128((const __m128i*)dest);
					VEC_SHADE_SIMPLE(fg);
					VEC_ALPHA_BLEND(fg, bg);
					_mm_storeu_si128((__m128i*)dest, fg);
					dest += pitch;
				} while (--count);
			}
			else
			{
				VEC_SHADE_INIT4(light3, light2, light1, light0, shade_constants);
				do
				{
					__m128i fg = sample_bilinear4_sse(bufplce, bufplce2, buftexturefracx, local_vplce, bits);

					local_vplce[0] = local_vplce[0] + local_vince[0];
					local_vplce[1] = local_vplce[1] + local_vince[1];
					local_vplce[2] = local_vplce[2] + local_vince[2];
					local_vplce[3] = local_vplce[3] + local_vince[3];

					__m128i bg = _mm_loadu_si128((const __m128i*)dest);
					VEC_SHADE(fg, shade_constants);
					VEC_ALPHA_BLEND(fg, bg);
					_mm_storeu_si128((__m128i*)dest, fg);
					dest += pitch;
				} while (--count);
			}
		}
	}
};

class VecCommand(Tmvline4AddRGBA) : public DrawerCommand
{
	BYTE * RESTRICT _dest;
	int _count;
	int _pitch;
	ShadeConstants _shade_constants;
	fixed_t _srcalpha;
	fixed_t _destalpha;
	int tmvlinebits;
	fixed_t palookuplight[4];
	DWORD vplce[4];
	DWORD vince[4];
	const uint32 * RESTRICT bufplce[4];

public:
	VecCommand(Tmvline4AddRGBA)()
	{
		_dest = dc_dest;
		_count = dc_count;
		_pitch = dc_pitch;
		_shade_constants = dc_shade_constants;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
		tmvlinebits = ::tmvlinebits;
		for (int i = 0; i < 4; i++)
		{
			palookuplight[i] = ::palookuplight[i];
			vplce[i] = ::vplce[i];
			vince[i] = ::vince[i];
			bufplce[i] = (const uint32 *)::bufplce[i];
		}
	}

	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int pitch = _pitch * thread->num_cores;
		int bits = tmvlinebits;

		uint32_t light[4];
		light[0] = calc_light_multiplier(palookuplight[0]);
		light[1] = calc_light_multiplier(palookuplight[1]);
		light[2] = calc_light_multiplier(palookuplight[2]);
		light[3] = calc_light_multiplier(palookuplight[3]);

		ShadeConstants shade_constants = _shade_constants;

		uint32_t src_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t dest_alpha = _destalpha >> (FRACBITS - 8);

		DWORD local_vplce[4] = { vplce[0], vplce[1], vplce[2], vplce[3] };
		DWORD local_vince[4] = { vince[0], vince[1], vince[2], vince[3] };
		int skipped = thread->skipped_by_thread(_dest_y);
		for (int i = 0; i < 4; i++)
		{
			local_vplce[i] += local_vince[i] * skipped;
			local_vince[i] *= thread->num_cores;
		}

		if (shade_constants.simple_shade)
		{
			VEC_SHADE_SIMPLE_INIT4(light[3], light[2], light[1], light[0]);
			VEC_CALC_BLEND_ALPHA_INIT(src_alpha, dest_alpha);

			do
			{
				uint32_t pix0 = bufplce[0][local_vplce[0] >> bits];
				uint32_t pix1 = bufplce[1][local_vplce[1] >> bits];
				uint32_t pix2 = bufplce[2][local_vplce[2] >> bits];
				uint32_t pix3 = bufplce[3][local_vplce[3] >> bits];

				local_vplce[0] = local_vplce[0] + local_vince[0];
				local_vplce[1] = local_vplce[1] + local_vince[1];
				local_vplce[2] = local_vplce[2] + local_vince[2];
				local_vplce[3] = local_vplce[3] + local_vince[3];

				__m128i fg = _mm_set_epi32(pix3, pix2, pix1, pix0);

				VEC_CALC_BLEND_ALPHA(fg);
				VEC_SHADE_SIMPLE(fg);

				__m128i bg = _mm_loadu_si128((const __m128i*)dest);

				__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128());
				__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128());
				__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

				__m128i out_hi = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_hi, fg_alpha_hi), _mm_mullo_epi16(bg_hi, bg_alpha_hi)), 8);
				__m128i out_lo = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_lo, fg_alpha_lo), _mm_mullo_epi16(bg_lo, bg_alpha_lo)), 8);
				__m128i out = _mm_packus_epi16(out_lo, out_hi);

				_mm_storeu_si128((__m128i*)dest, out);
				dest += pitch;
			} while (--count);
		}
		else
		{
			VEC_SHADE_INIT4(light[3], light[2], light[1], light[0], shade_constants);
			VEC_CALC_BLEND_ALPHA_INIT(src_alpha, dest_alpha);

			do
			{
				uint32_t pix0 = bufplce[0][local_vplce[0] >> bits];
				uint32_t pix1 = bufplce[1][local_vplce[1] >> bits];
				uint32_t pix2 = bufplce[2][local_vplce[2] >> bits];
				uint32_t pix3 = bufplce[3][local_vplce[3] >> bits];

				local_vplce[0] = local_vplce[0] + local_vince[0];
				local_vplce[1] = local_vplce[1] + local_vince[1];
				local_vplce[2] = local_vplce[2] + local_vince[2];
				local_vplce[3] = local_vplce[3] + local_vince[3];

				__m128i fg = _mm_set_epi32(pix3, pix2, pix1, pix0);
				VEC_CALC_BLEND_ALPHA(fg);
				VEC_SHADE(fg, shade_constants);

				__m128i bg = _mm_loadu_si128((const __m128i*)dest);

				__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128());
				__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128());
				__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

				__m128i out_hi = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_hi, fg_alpha_hi), _mm_mullo_epi16(bg_hi, bg_alpha_hi)), 8);
				__m128i out_lo = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_lo, fg_alpha_lo), _mm_mullo_epi16(bg_lo, bg_alpha_lo)), 8);
				__m128i out = _mm_packus_epi16(out_lo, out_hi);

				_mm_storeu_si128((__m128i*)dest, out);
				dest += pitch;
			} while (--count);
		}
	}
};

class VecCommand(Tmvline4AddClampRGBA) : public DrawerCommand
{
	BYTE * RESTRICT _dest;
	int _count;
	int _pitch;
	ShadeConstants _shade_constants;
	fixed_t _srcalpha;
	fixed_t _destalpha;
	int tmvlinebits;
	fixed_t palookuplight[4];
	DWORD vplce[4];
	DWORD vince[4];
	const uint32 *RESTRICT bufplce[4];

public:
	VecCommand(Tmvline4AddClampRGBA)()
	{
		_dest = dc_dest;
		_count = dc_count;
		_pitch = dc_pitch;
		_shade_constants = dc_shade_constants;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
		tmvlinebits = ::tmvlinebits;
		for (int i = 0; i < 4; i++)
		{
			palookuplight[i] = ::palookuplight[i];
			vplce[i] = ::vplce[i];
			vince[i] = ::vince[i];
			bufplce[i] = (const uint32 *)::bufplce[i];
		}
	}

	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int pitch = _pitch * thread->num_cores;
		int bits = tmvlinebits;

		uint32_t light[4];
		light[0] = calc_light_multiplier(palookuplight[0]);
		light[1] = calc_light_multiplier(palookuplight[1]);
		light[2] = calc_light_multiplier(palookuplight[2]);
		light[3] = calc_light_multiplier(palookuplight[3]);

		ShadeConstants shade_constants = _shade_constants;

		uint32_t src_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t dest_alpha = _destalpha >> (FRACBITS - 8);

		DWORD local_vplce[4] = { vplce[0], vplce[1], vplce[2], vplce[3] };
		DWORD local_vince[4] = { vince[0], vince[1], vince[2], vince[3] };
		int skipped = thread->skipped_by_thread(_dest_y);
		for (int i = 0; i < 4; i++)
		{
			local_vplce[i] += local_vince[i] * skipped;
			local_vince[i] *= thread->num_cores;
		}

		if (shade_constants.simple_shade)
		{
			VEC_SHADE_SIMPLE_INIT4(light[3], light[2], light[1], light[0]);
			VEC_CALC_BLEND_ALPHA_INIT(src_alpha, dest_alpha);

			do
			{
				uint32_t pix0 = bufplce[0][local_vplce[0] >> bits];
				uint32_t pix1 = bufplce[1][local_vplce[1] >> bits];
				uint32_t pix2 = bufplce[2][local_vplce[2] >> bits];
				uint32_t pix3 = bufplce[3][local_vplce[3] >> bits];

				local_vplce[0] = local_vplce[0] + local_vince[0];
				local_vplce[1] = local_vplce[1] + local_vince[1];
				local_vplce[2] = local_vplce[2] + local_vince[2];
				local_vplce[3] = local_vplce[3] + local_vince[3];

				__m128i fg = _mm_set_epi32(pix3, pix2, pix1, pix0);
				VEC_CALC_BLEND_ALPHA(fg);
				VEC_SHADE_SIMPLE(fg);

				__m128i bg = _mm_loadu_si128((const __m128i*)dest);

				__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128());
				__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128());
				__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

				__m128i out_hi = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_hi, fg_alpha_hi), _mm_mullo_epi16(bg_hi, bg_alpha_hi)), 8);
				__m128i out_lo = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_lo, fg_alpha_lo), _mm_mullo_epi16(bg_lo, bg_alpha_lo)), 8);
				__m128i out = _mm_packus_epi16(out_lo, out_hi);

				_mm_storeu_si128((__m128i*)dest, out);
				dest += pitch;
			} while (--count);
		}
		else
		{
			VEC_SHADE_INIT4(light[3], light[2], light[1], light[0], shade_constants);
			VEC_CALC_BLEND_ALPHA_INIT(src_alpha, dest_alpha);

			do
			{
				uint32_t pix0 = bufplce[0][local_vplce[0] >> bits];
				uint32_t pix1 = bufplce[1][local_vplce[1] >> bits];
				uint32_t pix2 = bufplce[2][local_vplce[2] >> bits];
				uint32_t pix3 = bufplce[3][local_vplce[3] >> bits];

				local_vplce[0] = local_vplce[0] + local_vince[0];
				local_vplce[1] = local_vplce[1] + local_vince[1];
				local_vplce[2] = local_vplce[2] + local_vince[2];
				local_vplce[3] = local_vplce[3] + local_vince[3];

				__m128i fg = _mm_set_epi32(pix3, pix2, pix1, pix0);
				VEC_CALC_BLEND_ALPHA(fg);
				VEC_SHADE(fg, shade_constants);

				__m128i bg = _mm_loadu_si128((const __m128i*)dest);

				__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128());
				__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128());
				__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

				__m128i out_hi = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_hi, fg_alpha_hi), _mm_mullo_epi16(bg_hi, bg_alpha_hi)), 8);
				__m128i out_lo = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_lo, fg_alpha_lo), _mm_mullo_epi16(bg_lo, bg_alpha_lo)), 8);
				__m128i out = _mm_packus_epi16(out_lo, out_hi);

				_mm_storeu_si128((__m128i*)dest, out);
				dest += pitch;
			} while (--count);
		}
	}
};

class VecCommand(Tmvline4SubClampRGBA) : public DrawerCommand
{
	BYTE * RESTRICT _dest;
	int _count;
	int _pitch;
	ShadeConstants _shade_constants;
	fixed_t _srcalpha;
	fixed_t _destalpha;
	int tmvlinebits;
	fixed_t palookuplight[4];
	DWORD vplce[4];
	DWORD vince[4];
	const uint32 *RESTRICT bufplce[4];

public:
	VecCommand(Tmvline4SubClampRGBA)()
	{
		_dest = dc_dest;
		_count = dc_count;
		_pitch = dc_pitch;
		_shade_constants = dc_shade_constants;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
		tmvlinebits = ::tmvlinebits;
		for (int i = 0; i < 4; i++)
		{
			palookuplight[i] = ::palookuplight[i];
			vplce[i] = ::vplce[i];
			vince[i] = ::vince[i];
			bufplce[i] = (const uint32 *)::bufplce[i];
		}
	}

	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int pitch = _pitch * thread->num_cores;
		int bits = tmvlinebits;

		uint32_t light[4];
		light[0] = calc_light_multiplier(palookuplight[0]);
		light[1] = calc_light_multiplier(palookuplight[1]);
		light[2] = calc_light_multiplier(palookuplight[2]);
		light[3] = calc_light_multiplier(palookuplight[3]);

		ShadeConstants shade_constants = _shade_constants;

		uint32_t src_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t dest_alpha = _destalpha >> (FRACBITS - 8);

		DWORD local_vplce[4] = { vplce[0], vplce[1], vplce[2], vplce[3] };
		DWORD local_vince[4] = { vince[0], vince[1], vince[2], vince[3] };
		int skipped = thread->skipped_by_thread(_dest_y);
		for (int i = 0; i < 4; i++)
		{
			local_vplce[i] += local_vince[i] * skipped;
			local_vince[i] *= thread->num_cores;
		}

		if (shade_constants.simple_shade)
		{
			VEC_SHADE_SIMPLE_INIT4(light[3], light[2], light[1], light[0]);
			VEC_CALC_BLEND_ALPHA_INIT(src_alpha, dest_alpha);

			do
			{
				uint32_t pix0 = bufplce[0][local_vplce[0] >> bits];
				uint32_t pix1 = bufplce[1][local_vplce[1] >> bits];
				uint32_t pix2 = bufplce[2][local_vplce[2] >> bits];
				uint32_t pix3 = bufplce[3][local_vplce[3] >> bits];

				local_vplce[0] = local_vplce[0] + local_vince[0];
				local_vplce[1] = local_vplce[1] + local_vince[1];
				local_vplce[2] = local_vplce[2] + local_vince[2];
				local_vplce[3] = local_vplce[3] + local_vince[3];

				__m128i fg = _mm_set_epi32(pix3, pix2, pix1, pix0);
				VEC_CALC_BLEND_ALPHA(fg);
				VEC_SHADE_SIMPLE(fg);

				__m128i bg = _mm_loadu_si128((const __m128i*)dest);

				__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128());
				__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128());
				__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

				__m128i out_hi = _mm_srli_epi16(_mm_subs_epu16(_mm_mullo_epi16(bg_hi, bg_alpha_hi), _mm_mullo_epi16(fg_hi, fg_alpha_hi)), 8);
				__m128i out_lo = _mm_srli_epi16(_mm_subs_epu16(_mm_mullo_epi16(bg_lo, bg_alpha_lo), _mm_mullo_epi16(fg_lo, fg_alpha_lo)), 8);
				__m128i out = _mm_packus_epi16(out_lo, out_hi);

				_mm_storeu_si128((__m128i*)dest, out);
				dest += pitch;
			} while (--count);
		}
		else
		{
			VEC_SHADE_INIT4(light[3], light[2], light[1], light[0], shade_constants);
			VEC_CALC_BLEND_ALPHA_INIT(src_alpha, dest_alpha);

			do
			{
				uint32_t pix0 = bufplce[0][local_vplce[0] >> bits];
				uint32_t pix1 = bufplce[1][local_vplce[1] >> bits];
				uint32_t pix2 = bufplce[2][local_vplce[2] >> bits];
				uint32_t pix3 = bufplce[3][local_vplce[3] >> bits];

				local_vplce[0] = local_vplce[0] + local_vince[0];
				local_vplce[1] = local_vplce[1] + local_vince[1];
				local_vplce[2] = local_vplce[2] + local_vince[2];
				local_vplce[3] = local_vplce[3] + local_vince[3];

				__m128i fg = _mm_set_epi32(pix3, pix2, pix1, pix0);
				VEC_CALC_BLEND_ALPHA(fg);
				VEC_SHADE(fg, shade_constants);

				__m128i bg = _mm_loadu_si128((const __m128i*)dest);

				__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128());
				__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128());
				__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

				__m128i out_hi = _mm_srli_epi16(_mm_subs_epu16(_mm_mullo_epi16(bg_hi, bg_alpha_hi), _mm_mullo_epi16(fg_hi, fg_alpha_hi)), 8);
				__m128i out_lo = _mm_srli_epi16(_mm_subs_epu16(_mm_mullo_epi16(bg_lo, bg_alpha_lo), _mm_mullo_epi16(fg_lo, fg_alpha_lo)), 8);
				__m128i out = _mm_packus_epi16(out_lo, out_hi);

				_mm_storeu_si128((__m128i*)dest, out);
				dest += pitch;
			} while (--count);
		}
	}
};

class VecCommand(Tmvline4RevSubClampRGBA) : public DrawerCommand
{
	BYTE * RESTRICT _dest;
	int _count;
	int _pitch;
	ShadeConstants _shade_constants;
	fixed_t _srcalpha;
	fixed_t _destalpha;
	int tmvlinebits;
	fixed_t palookuplight[4];
	DWORD vplce[4];
	DWORD vince[4];
	const uint32 *RESTRICT bufplce[4];

public:
	VecCommand(Tmvline4RevSubClampRGBA)()
	{
		_dest = dc_dest;
		_count = dc_count;
		_pitch = dc_pitch;
		_shade_constants = dc_shade_constants;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
		tmvlinebits = ::tmvlinebits;
		for (int i = 0; i < 4; i++)
		{
			palookuplight[i] = ::palookuplight[i];
			vplce[i] = ::vplce[i];
			vince[i] = ::vince[i];
			bufplce[i] = (const uint32 *)::bufplce[i];
		}
	}

	void Execute(DrawerThread *thread) override
	{
		int count = thread->count_for_thread(_dest_y, _count);
		if (count <= 0)
			return;

		uint32_t *dest = thread->dest_for_thread(_dest_y, _pitch, (uint32_t*)_dest);
		int pitch = _pitch * thread->num_cores;
		int bits = tmvlinebits;

		uint32_t light[4];
		light[0] = calc_light_multiplier(palookuplight[0]);
		light[1] = calc_light_multiplier(palookuplight[1]);
		light[2] = calc_light_multiplier(palookuplight[2]);
		light[3] = calc_light_multiplier(palookuplight[3]);

		ShadeConstants shade_constants = _shade_constants;

		uint32_t src_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t dest_alpha = _destalpha >> (FRACBITS - 8);

		DWORD local_vplce[4] = { vplce[0], vplce[1], vplce[2], vplce[3] };
		DWORD local_vince[4] = { vince[0], vince[1], vince[2], vince[3] };
		int skipped = thread->skipped_by_thread(_dest_y);
		for (int i = 0; i < 4; i++)
		{
			local_vplce[i] += local_vince[i] * skipped;
			local_vince[i] *= thread->num_cores;
		}

		if (shade_constants.simple_shade)
		{
			VEC_SHADE_SIMPLE_INIT4(light[3], light[2], light[1], light[0]);
			VEC_CALC_BLEND_ALPHA_INIT(src_alpha, dest_alpha);

			do
			{
				uint32_t pix0 = bufplce[0][local_vplce[0] >> bits];
				uint32_t pix1 = bufplce[1][local_vplce[1] >> bits];
				uint32_t pix2 = bufplce[2][local_vplce[2] >> bits];
				uint32_t pix3 = bufplce[3][local_vplce[3] >> bits];

				local_vplce[0] = local_vplce[0] + local_vince[0];
				local_vplce[1] = local_vplce[1] + local_vince[1];
				local_vplce[2] = local_vplce[2] + local_vince[2];
				local_vplce[3] = local_vplce[3] + local_vince[3];

				__m128i fg = _mm_set_epi32(pix3, pix2, pix1, pix0);
				VEC_CALC_BLEND_ALPHA(fg);
				VEC_SHADE_SIMPLE(fg);

				__m128i bg = _mm_loadu_si128((const __m128i*)dest);

				__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128());
				__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128());
				__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

				__m128i out_hi = _mm_srli_epi16(_mm_subs_epu16(_mm_mullo_epi16(fg_hi, fg_alpha_hi), _mm_mullo_epi16(bg_hi, bg_alpha_hi)), 8);
				__m128i out_lo = _mm_srli_epi16(_mm_subs_epu16(_mm_mullo_epi16(fg_lo, fg_alpha_lo), _mm_mullo_epi16(bg_lo, bg_alpha_lo)), 8);
				__m128i out = _mm_packus_epi16(out_lo, out_hi);

				_mm_storeu_si128((__m128i*)dest, out);
				dest += pitch;
			} while (--count);
		}
		else
		{
			VEC_SHADE_INIT4(light[3], light[2], light[1], light[0], shade_constants);
			VEC_CALC_BLEND_ALPHA_INIT(src_alpha, dest_alpha);

			do
			{
				uint32_t pix0 = bufplce[0][local_vplce[0] >> bits];
				uint32_t pix1 = bufplce[1][local_vplce[1] >> bits];
				uint32_t pix2 = bufplce[2][local_vplce[2] >> bits];
				uint32_t pix3 = bufplce[3][local_vplce[3] >> bits];

				local_vplce[0] = local_vplce[0] + local_vince[0];
				local_vplce[1] = local_vplce[1] + local_vince[1];
				local_vplce[2] = local_vplce[2] + local_vince[2];
				local_vplce[3] = local_vplce[3] + local_vince[3];

				__m128i fg = _mm_set_epi32(pix3, pix2, pix1, pix0);
				VEC_CALC_BLEND_ALPHA(fg);
				VEC_SHADE(fg, shade_constants);

				__m128i bg = _mm_loadu_si128((const __m128i*)dest);

				__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128());
				__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128());
				__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

				__m128i out_hi = _mm_srli_epi16(_mm_subs_epu16(_mm_mullo_epi16(fg_hi, fg_alpha_hi), _mm_mullo_epi16(bg_hi, bg_alpha_hi)), 8);
				__m128i out_lo = _mm_srli_epi16(_mm_subs_epu16(_mm_mullo_epi16(fg_lo, fg_alpha_lo), _mm_mullo_epi16(bg_lo, bg_alpha_lo)), 8);
				__m128i out = _mm_packus_epi16(out_lo, out_hi);

				_mm_storeu_si128((__m128i*)dest, out);
				dest += pitch;
			} while (--count);
		}
	}
};
