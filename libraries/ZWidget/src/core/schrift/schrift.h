/* This file is part of libschrift.
 *
 * © 2019-2022 Thomas Oltmann and contributors
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE. */

#ifndef SCHRIFT_H
#define SCHRIFT_H 1

#include <stddef.h> /* size_t */
#include <stdint.h> /* uint_fast32_t, uint_least32_t */

#ifdef __cplusplus
extern "C" {
#endif

#define SFT_DOWNWARD_Y 0x01

typedef struct SFT          SFT;
typedef struct SFT_Font     SFT_Font;
typedef uint_least32_t      SFT_UChar; /* Guaranteed to be compatible with char32_t. */
typedef uint_fast32_t       SFT_Glyph;
typedef struct SFT_LMetrics SFT_LMetrics;
typedef struct SFT_GMetrics SFT_GMetrics;
typedef struct SFT_Kerning  SFT_Kerning;
typedef struct SFT_Image    SFT_Image;

struct SFT
{
	SFT_Font *font;
	double    xScale;
	double    yScale;
	double    xOffset;
	double    yOffset;
	int       flags;
};

struct SFT_LMetrics
{
	double ascender;
	double descender;
	double lineGap;
};

struct SFT_GMetrics
{
	double advanceWidth;
	double leftSideBearing;
	int    yOffset;
	int    minWidth;
	int    minHeight;
};

struct SFT_Kerning
{
	double xShift;
	double yShift;
};

struct SFT_Image
{
	void *pixels;
	int   width;
	int   height;
};

const char *sft_version(void);

SFT_Font *sft_loadmem (const void *mem, size_t size);
SFT_Font *sft_loadfile(const char *filename);
void      sft_freefont(SFT_Font *font);

int sft_lmetrics(const SFT *sft, SFT_LMetrics *metrics);
int sft_lookup  (const SFT *sft, SFT_UChar codepoint, SFT_Glyph *glyph);
int sft_gmetrics(const SFT *sft, SFT_Glyph glyph, SFT_GMetrics *metrics);
int sft_kerning (const SFT *sft, SFT_Glyph leftGlyph, SFT_Glyph rightGlyph,
                 SFT_Kerning *kerning);
int sft_render  (const SFT *sft, SFT_Glyph glyph, SFT_Image image);

#ifdef __cplusplus
}
#endif

#endif

