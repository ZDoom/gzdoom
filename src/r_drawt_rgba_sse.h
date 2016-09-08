//
// SSE/AVX intrinsics based drawers for the r_drawt family of drawers.
//
// Note: This header file is intentionally not guarded by a __R_DRAWT_RGBA_SSE__ define.
//       It is because the code is nearly identical for SSE vs AVX. The file is included
//       multiple times by r_drawt_rgba.cpp with different defines that changes the class
//       names outputted and the type of intrinsics used.

#ifdef _MSC_VER
#pragma warning(disable: 4752) // warning C4752: found Intel(R) Advanced Vector Extensions; consider using /arch:AVX
#endif

class VecCommand(RtMap4colsRGBA) : public DrawerCommand
{
	int sx;
	int yl;
	int yh;
	fixed_t _light;
	ShadeConstants _shade_constants;
	BYTE * RESTRICT _destorg;
	int _pitch;
	BYTE * RESTRICT _colormap;

public:
	VecCommand(RtMap4colsRGBA)(int sx, int yl, int yh)
	{
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		_light = dc_light;
		_shade_constants = dc_shade_constants;
		_destorg = dc_destorg;
		_pitch = dc_pitch;
		_colormap = dc_colormap;
	}

	void Execute(DrawerThread *thread) override
	{
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		ShadeConstants shade_constants = _shade_constants;
		uint32_t light = LightBgra::calc_light_multiplier(_light);
		uint32_t *palette = (uint32_t*)GPalette.BaseColors;

		dest = thread->dest_for_thread(yl, _pitch, ylookup[yl] + sx + (uint32_t*)_destorg);
		source = &thread->dc_temp_rgba[yl * 4] + thread->skipped_by_thread(yl) * 4;
		pitch = _pitch * thread->num_cores;
		sincr = thread->num_cores * 4;

		BYTE *colormap = _colormap;

		if (shade_constants.simple_shade)
		{
			VEC_SHADE_VARS();
			VEC_SHADE_SIMPLE_INIT(light);

			if (count & 1) {
				uint32_t p0 = colormap[source[0]];
				uint32_t p1 = colormap[source[1]];
				uint32_t p2 = colormap[source[2]];
				uint32_t p3 = colormap[source[3]];

				// shade_pal_index:
				__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
				VEC_SHADE_SIMPLE(fg);
				_mm_storeu_si128((__m128i*)dest, fg);

				source += sincr;
				dest += pitch;
			}
			if (!(count >>= 1))
				return;

			do {
				// shade_pal_index 0-3
				{
					uint32_t p0 = colormap[source[0]];
					uint32_t p1 = colormap[source[1]];
					uint32_t p2 = colormap[source[2]];
					uint32_t p3 = colormap[source[3]];

					__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
					VEC_SHADE_SIMPLE(fg);
					_mm_storeu_si128((__m128i*)dest, fg);
				}

				// shade_pal_index 4-7 (pitch)
				{
					uint32_t p0 = colormap[source[sincr]];
					uint32_t p1 = colormap[source[sincr + 1]];
					uint32_t p2 = colormap[source[sincr + 2]];
					uint32_t p3 = colormap[source[sincr + 3]];

					__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
					VEC_SHADE_SIMPLE(fg);
					_mm_storeu_si128((__m128i*)(dest + pitch), fg);
				}

				source += sincr * 2;
				dest += pitch * 2;
			} while (--count);
		}
		else
		{
			VEC_SHADE_VARS();
			VEC_SHADE_INIT(light, shade_constants);

			if (count & 1) {
				uint32_t p0 = colormap[source[0]];
				uint32_t p1 = colormap[source[1]];
				uint32_t p2 = colormap[source[2]];
				uint32_t p3 = colormap[source[3]];

				// shade_pal_index:
				__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
				VEC_SHADE(fg, shade_constants);
				_mm_storeu_si128((__m128i*)dest, fg);

				source += sincr;
				dest += pitch;
			}
			if (!(count >>= 1))
				return;

			do {
				// shade_pal_index 0-3
				{
					uint32_t p0 = colormap[source[0]];
					uint32_t p1 = colormap[source[1]];
					uint32_t p2 = colormap[source[2]];
					uint32_t p3 = colormap[source[3]];

					__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
					VEC_SHADE(fg, shade_constants);
					_mm_storeu_si128((__m128i*)dest, fg);
				}

				// shade_pal_index 4-7 (pitch)
				{
					uint32_t p0 = colormap[source[sincr]];
					uint32_t p1 = colormap[source[sincr + 1]];
					uint32_t p2 = colormap[source[sincr + 2]];
					uint32_t p3 = colormap[source[sincr + 3]];

					__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
					VEC_SHADE(fg, shade_constants);
					_mm_storeu_si128((__m128i*)(dest + pitch), fg);
				}

				source += sincr * 2;
				dest += pitch * 2;
			} while (--count);
		}
	}
};

class VecCommand(RtAdd4colsRGBA) : public DrawerCommand
{
	int sx;
	int yl;
	int yh;
	BYTE * RESTRICT _destorg;
	int _pitch;
	fixed_t _light;
	ShadeConstants _shade_constants;
	BYTE * RESTRICT _colormap;
	fixed_t _srcalpha;
	fixed_t _destalpha;

public:
	VecCommand(RtAdd4colsRGBA)(int sx, int yl, int yh)
	{
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		_destorg = dc_destorg;
		_pitch = dc_pitch;
		_light = dc_light;
		_shade_constants = dc_shade_constants;
		_colormap = dc_colormap;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
	}

	void Execute(DrawerThread *thread) override
	{
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(yl, _pitch, ylookup[yl] + sx + (uint32_t*)_destorg);
		source = &thread->dc_temp_rgba[yl * 4] + thread->skipped_by_thread(yl) * 4;
		pitch = _pitch * thread->num_cores;
		sincr = 4 * thread->num_cores;

		uint32_t light = LightBgra::calc_light_multiplier(_light);
		uint32_t *palette = (uint32_t*)GPalette.BaseColors;
		BYTE *colormap = _colormap;

		uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

		ShadeConstants shade_constants = _shade_constants;

		if (shade_constants.simple_shade)
		{
			VEC_SHADE_VARS();
			VEC_SHADE_SIMPLE_INIT(light);

			__m128i mfg_alpha = _mm_set_epi16(256, fg_alpha, fg_alpha, fg_alpha, 256, fg_alpha, fg_alpha, fg_alpha);
			__m128i mbg_alpha = _mm_set_epi16(256, bg_alpha, bg_alpha, bg_alpha, 256, bg_alpha, bg_alpha, bg_alpha);

			do {
				uint32_t p0 = colormap[source[0]];
				uint32_t p1 = colormap[source[1]];
				uint32_t p2 = colormap[source[2]];
				uint32_t p3 = colormap[source[3]];

				// shade_pal_index:
				__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
				VEC_SHADE_SIMPLE(fg);

				__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128());
				__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128());

				// unpack bg:
				__m128i bg = _mm_loadu_si128((const __m128i*)dest);
				__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

				// (fg_red * fg_alpha + bg_red * bg_alpha) / 256:
				__m128i color_hi = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_hi, mfg_alpha), _mm_mullo_epi16(bg_hi, mbg_alpha)), 8);
				__m128i color_lo = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_lo, mfg_alpha), _mm_mullo_epi16(bg_lo, mbg_alpha)), 8);

				__m128i color = _mm_packus_epi16(color_lo, color_hi);
				_mm_storeu_si128((__m128i*)dest, color);

				source += sincr;
				dest += pitch;
			} while (--count);
		}
		else
		{
			VEC_SHADE_VARS();
			VEC_SHADE_INIT(light, shade_constants);

			__m128i mfg_alpha = _mm_set_epi16(256, fg_alpha, fg_alpha, fg_alpha, 256, fg_alpha, fg_alpha, fg_alpha);
			__m128i mbg_alpha = _mm_set_epi16(256, bg_alpha, bg_alpha, bg_alpha, 256, bg_alpha, bg_alpha, bg_alpha);

			do {
				uint32_t p0 = colormap[source[0]];
				uint32_t p1 = colormap[source[1]];
				uint32_t p2 = colormap[source[2]];
				uint32_t p3 = colormap[source[3]];

				// shade_pal_index:
				__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
				VEC_SHADE(fg, shade_constants);

				__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128());
				__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128());

				// unpack bg:
				__m128i bg = _mm_loadu_si128((const __m128i*)dest);
				__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

				// (fg_red * fg_alpha + bg_red * bg_alpha) / 256:
				__m128i color_hi = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_hi, mfg_alpha), _mm_mullo_epi16(bg_hi, mbg_alpha)), 8);
				__m128i color_lo = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_lo, mfg_alpha), _mm_mullo_epi16(bg_lo, mbg_alpha)), 8);

				__m128i color = _mm_packus_epi16(color_lo, color_hi);
				_mm_storeu_si128((__m128i*)dest, color);

				source += sincr;
				dest += pitch;
			} while (--count);
		}
	}
};

class VecCommand(RtShaded4colsRGBA) : public DrawerCommand
{
	int sx;
	int yl;
	int yh;
	lighttable_t * RESTRICT _colormap;
	int _color;
	BYTE * RESTRICT _destorg;
	int _pitch;
	fixed_t _light;

public:
	VecCommand(RtShaded4colsRGBA)(int sx, int yl, int yh)
	{
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		_colormap = dc_colormap;
		_color = dc_color;
		_destorg = dc_destorg;
		_pitch = dc_pitch;
		_light = dc_light;
	}

	void Execute(DrawerThread *thread) override
	{
		BYTE *colormap;
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		colormap = _colormap;
		dest = thread->dest_for_thread(yl, _pitch, ylookup[yl] + sx + (uint32_t*)_destorg);
		source = &thread->dc_temp_rgba[yl * 4] + thread->skipped_by_thread(yl) * 4;
		pitch = _pitch * thread->num_cores;
		sincr = 4 * thread->num_cores;

		__m128i fg = _mm_unpackhi_epi8(_mm_set1_epi32(LightBgra::shade_pal_index_simple(_color, LightBgra::calc_light_multiplier(_light))), _mm_setzero_si128());
		__m128i alpha_one = _mm_set1_epi16(64);

		do {
			uint32_t p0 = colormap[source[0]];
			uint32_t p1 = colormap[source[1]];
			uint32_t p2 = colormap[source[2]];
			uint32_t p3 = colormap[source[3]];

			__m128i alpha_hi = _mm_set_epi16(64, p3, p3, p3, 64, p2, p2, p2);
			__m128i alpha_lo = _mm_set_epi16(64, p1, p1, p1, 64, p0, p0, p0);
			__m128i inv_alpha_hi = _mm_subs_epu16(alpha_one, alpha_hi);
			__m128i inv_alpha_lo = _mm_subs_epu16(alpha_one, alpha_lo);

			// unpack bg:
			__m128i bg = _mm_loadu_si128((const __m128i*)dest);
			__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
			__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

			// (fg_red * alpha + bg_red * inv_alpha) / 64:
			__m128i color_hi = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg, alpha_hi), _mm_mullo_epi16(bg_hi, inv_alpha_hi)), 6);
			__m128i color_lo = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg, alpha_lo), _mm_mullo_epi16(bg_lo, inv_alpha_lo)), 6);

			__m128i color = _mm_packus_epi16(color_lo, color_hi);
			_mm_storeu_si128((__m128i*)dest, color);

			source += sincr;
			dest += pitch;
		} while (--count);
	}
};

class VecCommand(RtAddClamp4colsRGBA) : public DrawerCommand
{
	int sx;
	int yl;
	int yh;
	BYTE * RESTRICT _destorg;
	int _pitch;
	fixed_t _light;
	fixed_t _srcalpha;
	fixed_t _destalpha;
	ShadeConstants _shade_constants;

public:
	VecCommand(RtAddClamp4colsRGBA)(int sx, int yl, int yh)
	{
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		_destorg = dc_destorg;
		_pitch = dc_pitch;
		_light = dc_light;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
		_shade_constants = dc_shade_constants;
	}

	void Execute(DrawerThread *thread) override
	{
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(yl, _pitch, ylookup[yl] + sx + (uint32_t*)_destorg);
		source = &thread->dc_temp_rgba[yl * 4] + thread->skipped_by_thread(yl) * 4;
		pitch = _pitch * thread->num_cores;
		sincr = 4 * thread->num_cores;

		uint32_t light = LightBgra::calc_light_multiplier(_light);
		uint32_t *palette = (uint32_t*)GPalette.BaseColors;

		uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

		ShadeConstants shade_constants = _shade_constants;

		if (shade_constants.simple_shade)
		{
			VEC_SHADE_VARS();
			VEC_SHADE_SIMPLE_INIT(light);

			__m128i mfg_alpha = _mm_set_epi16(256, fg_alpha, fg_alpha, fg_alpha, 256, fg_alpha, fg_alpha, fg_alpha);
			__m128i mbg_alpha = _mm_set_epi16(256, bg_alpha, bg_alpha, bg_alpha, 256, bg_alpha, bg_alpha, bg_alpha);

			do {
				uint32_t p0 = source[0];
				uint32_t p1 = source[1];
				uint32_t p2 = source[2];
				uint32_t p3 = source[3];

				// shade_pal_index:
				__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
				VEC_SHADE_SIMPLE(fg);

				__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128());
				__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128());

				// unpack bg:
				__m128i bg = _mm_loadu_si128((const __m128i*)dest);
				__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

				// (fg_red * fg_alpha + bg_red * bg_alpha) / 256:
				__m128i color_hi = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_hi, mfg_alpha), _mm_mullo_epi16(bg_hi, mbg_alpha)), 8);
				__m128i color_lo = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_lo, mfg_alpha), _mm_mullo_epi16(bg_lo, mbg_alpha)), 8);

				__m128i color = _mm_packus_epi16(color_lo, color_hi);
				_mm_storeu_si128((__m128i*)dest, color);

				source += sincr;
				dest += pitch;
			} while (--count);
		}
		else
		{
			VEC_SHADE_VARS();
			VEC_SHADE_INIT(light, shade_constants);

			__m128i mfg_alpha = _mm_set_epi16(256, fg_alpha, fg_alpha, fg_alpha, 256, fg_alpha, fg_alpha, fg_alpha);
			__m128i mbg_alpha = _mm_set_epi16(256, bg_alpha, bg_alpha, bg_alpha, 256, bg_alpha, bg_alpha, bg_alpha);

			do {
				uint32_t p0 = source[0];
				uint32_t p1 = source[1];
				uint32_t p2 = source[2];
				uint32_t p3 = source[3];

				// shade_pal_index:
				__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
				VEC_SHADE(fg, shade_constants);

				__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128());
				__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128());

				// unpack bg:
				__m128i bg = _mm_loadu_si128((const __m128i*)dest);
				__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

				// (fg_red * fg_alpha + bg_red * bg_alpha) / 256:
				__m128i color_hi = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_hi, mfg_alpha), _mm_mullo_epi16(bg_hi, mbg_alpha)), 8);
				__m128i color_lo = _mm_srli_epi16(_mm_adds_epu16(_mm_mullo_epi16(fg_lo, mfg_alpha), _mm_mullo_epi16(bg_lo, mbg_alpha)), 8);

				__m128i color = _mm_packus_epi16(color_lo, color_hi);
				_mm_storeu_si128((__m128i*)dest, color);

				source += sincr;
				dest += pitch;
			} while (--count);
		}
	}
};

class VecCommand(RtSubClamp4colsRGBA) : public DrawerCommand
{
	int sx;
	int yl;
	int yh;
	BYTE * RESTRICT _destorg;
	int _pitch;
	fixed_t _light;
	fixed_t _srcalpha;
	fixed_t _destalpha;
	ShadeConstants _shade_constants;

public:
	VecCommand(RtSubClamp4colsRGBA)(int sx, int yl, int yh)
	{
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		_destorg = dc_destorg;
		_pitch = dc_pitch;
		_light = dc_light;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
		_shade_constants = dc_shade_constants;
	}

	void Execute(DrawerThread *thread) override
	{
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(yl, _pitch, ylookup[yl] + sx + (uint32_t*)_destorg);
		source = &thread->dc_temp_rgba[yl * 4] + thread->skipped_by_thread(yl) * 4;
		pitch = _pitch * thread->num_cores;
		sincr = 4 * thread->num_cores;

		uint32_t light = LightBgra::calc_light_multiplier(_light);
		uint32_t *palette = (uint32_t*)GPalette.BaseColors;
		ShadeConstants shade_constants = _shade_constants;

		uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

		if (shade_constants.simple_shade)
		{
			VEC_SHADE_VARS();
			VEC_SHADE_SIMPLE_INIT(light);

			__m128i mfg_alpha = _mm_set_epi16(256, fg_alpha, fg_alpha, fg_alpha, 256, fg_alpha, fg_alpha, fg_alpha);
			__m128i mbg_alpha = _mm_set_epi16(256, bg_alpha, bg_alpha, bg_alpha, 256, bg_alpha, bg_alpha, bg_alpha);

			do {
				uint32_t p0 = source[0];
				uint32_t p1 = source[1];
				uint32_t p2 = source[2];
				uint32_t p3 = source[3];

				// shade_pal_index:
				__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
				VEC_SHADE_SIMPLE(fg);

				__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128());
				__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128());

				// unpack bg:
				__m128i bg = _mm_loadu_si128((const __m128i*)dest);
				__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

				// (bg_red * bg_alpha - fg_red * fg_alpha) / 256:
				__m128i color_hi = _mm_srli_epi16(_mm_subs_epu16(_mm_mullo_epi16(bg_hi, mbg_alpha), _mm_mullo_epi16(fg_hi, mfg_alpha)), 8);
				__m128i color_lo = _mm_srli_epi16(_mm_subs_epu16(_mm_mullo_epi16(bg_lo, mbg_alpha), _mm_mullo_epi16(fg_lo, mfg_alpha)), 8);

				__m128i color = _mm_packus_epi16(color_lo, color_hi);
				_mm_storeu_si128((__m128i*)dest, color);

				source += sincr;
				dest += pitch;
			} while (--count);
		}
		else
		{
			VEC_SHADE_VARS();
			VEC_SHADE_INIT(light, shade_constants);

			__m128i mfg_alpha = _mm_set_epi16(256, fg_alpha, fg_alpha, fg_alpha, 256, fg_alpha, fg_alpha, fg_alpha);
			__m128i mbg_alpha = _mm_set_epi16(256, bg_alpha, bg_alpha, bg_alpha, 256, bg_alpha, bg_alpha, bg_alpha);

			do {
				uint32_t p0 = source[0];
				uint32_t p1 = source[1];
				uint32_t p2 = source[2];
				uint32_t p3 = source[3];

				// shade_pal_index:
				__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
				VEC_SHADE(fg, shade_constants);

				__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128());
				__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128());

				// unpack bg:
				__m128i bg = _mm_loadu_si128((const __m128i*)dest);
				__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

				// (bg_red * bg_alpha - fg_red * fg_alpha) / 256:
				__m128i color_hi = _mm_srli_epi16(_mm_subs_epu16(_mm_mullo_epi16(bg_hi, mbg_alpha), _mm_mullo_epi16(fg_hi, mfg_alpha)), 8);
				__m128i color_lo = _mm_srli_epi16(_mm_subs_epu16(_mm_mullo_epi16(bg_lo, mbg_alpha), _mm_mullo_epi16(fg_lo, mfg_alpha)), 8);

				__m128i color = _mm_packus_epi16(color_lo, color_hi);
				_mm_storeu_si128((__m128i*)dest, color);

				source += sincr;
				dest += pitch;
			} while (--count);
		}
	}
};

class VecCommand(RtRevSubClamp4colsRGBA) : public DrawerCommand
{
	int sx;
	int yl;
	int yh;
	BYTE * RESTRICT _destorg;
	int _pitch;
	fixed_t _light;
	fixed_t _srcalpha;
	fixed_t _destalpha;
	ShadeConstants _shade_constants;

public:
	VecCommand(RtRevSubClamp4colsRGBA)(int sx, int yl, int yh)
	{
		this->sx = sx;
		this->yl = yl;
		this->yh = yh;

		_destorg = dc_destorg;
		_pitch = dc_pitch;
		_light = dc_light;
		_srcalpha = dc_srcalpha;
		_destalpha = dc_destalpha;
		_shade_constants = dc_shade_constants;
	}

	void Execute(DrawerThread *thread) override
	{
		uint32_t *source;
		uint32_t *dest;
		int count;
		int pitch;
		int sincr;

		count = thread->count_for_thread(yl, yh - yl + 1);
		if (count <= 0)
			return;

		dest = thread->dest_for_thread(yl, _pitch, ylookup[yl] + sx + (uint32_t*)_destorg);
		source = &thread->dc_temp_rgba[yl * 4] + thread->skipped_by_thread(yl) * 4;
		pitch = _pitch * thread->num_cores;
		sincr = 4 * thread->num_cores;

		uint32_t light = LightBgra::calc_light_multiplier(_light);
		uint32_t *palette = (uint32_t*)GPalette.BaseColors;
		ShadeConstants shade_constants = _shade_constants;

		uint32_t fg_alpha = _srcalpha >> (FRACBITS - 8);
		uint32_t bg_alpha = _destalpha >> (FRACBITS - 8);

		if (shade_constants.simple_shade)
		{
			VEC_SHADE_VARS();
			VEC_SHADE_SIMPLE_INIT(light);

			__m128i mfg_alpha = _mm_set_epi16(256, fg_alpha, fg_alpha, fg_alpha, 256, fg_alpha, fg_alpha, fg_alpha);
			__m128i mbg_alpha = _mm_set_epi16(256, bg_alpha, bg_alpha, bg_alpha, 256, bg_alpha, bg_alpha, bg_alpha);

			do {
				uint32_t p0 = source[0];
				uint32_t p1 = source[1];
				uint32_t p2 = source[2];
				uint32_t p3 = source[3];

				// shade_pal_index:
				__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
				VEC_SHADE_SIMPLE(fg);

				__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128());
				__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128());

				// unpack bg:
				__m128i bg = _mm_loadu_si128((const __m128i*)dest);
				__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

				// (fg_red * fg_alpha - bg_red * bg_alpha) / 256:
				__m128i color_hi = _mm_srli_epi16(_mm_subs_epu16(_mm_mullo_epi16(fg_hi, mfg_alpha), _mm_mullo_epi16(bg_hi, mbg_alpha)), 8);
				__m128i color_lo = _mm_srli_epi16(_mm_subs_epu16(_mm_mullo_epi16(fg_lo, mfg_alpha), _mm_mullo_epi16(bg_lo, mbg_alpha)), 8);

				__m128i color = _mm_packus_epi16(color_lo, color_hi);
				_mm_storeu_si128((__m128i*)dest, color);

				source += sincr;
				dest += pitch;
			} while (--count);
		}
		else
		{
			VEC_SHADE_VARS();
			VEC_SHADE_INIT(light, shade_constants);

			__m128i mfg_alpha = _mm_set_epi16(256, fg_alpha, fg_alpha, fg_alpha, 256, fg_alpha, fg_alpha, fg_alpha);
			__m128i mbg_alpha = _mm_set_epi16(256, bg_alpha, bg_alpha, bg_alpha, 256, bg_alpha, bg_alpha, bg_alpha);

			do {
				uint32_t p0 = source[0];
				uint32_t p1 = source[1];
				uint32_t p2 = source[2];
				uint32_t p3 = source[3];

				// shade_pal_index:
				__m128i fg = _mm_set_epi32(palette[p3], palette[p2], palette[p1], palette[p0]);
				VEC_SHADE(fg, shade_constants);

				__m128i fg_hi = _mm_unpackhi_epi8(fg, _mm_setzero_si128());
				__m128i fg_lo = _mm_unpacklo_epi8(fg, _mm_setzero_si128());

				// unpack bg:
				__m128i bg = _mm_loadu_si128((const __m128i*)dest);
				__m128i bg_hi = _mm_unpackhi_epi8(bg, _mm_setzero_si128());
				__m128i bg_lo = _mm_unpacklo_epi8(bg, _mm_setzero_si128());

				// (fg_red * fg_alpha - bg_red * bg_alpha) / 256:
				__m128i color_hi = _mm_srli_epi16(_mm_subs_epu16(_mm_mullo_epi16(fg_hi, mfg_alpha), _mm_mullo_epi16(bg_hi, mbg_alpha)), 8);
				__m128i color_lo = _mm_srli_epi16(_mm_subs_epu16(_mm_mullo_epi16(fg_lo, mfg_alpha), _mm_mullo_epi16(bg_lo, mbg_alpha)), 8);

				__m128i color = _mm_packus_epi16(color_lo, color_hi);
				_mm_storeu_si128((__m128i*)dest, color);

				source += sincr;
				dest += pitch;
			} while (--count);
		}
	}
};
