#pragma once

#include <stdint.h>
#include "palentry.h"

// extracted from v_video.h because this caused circular dependencies between v_video.h and textures.h

// Translucency tables

// RGB32k is a normal R5G5B5 -> palette lookup table.

// Use a union so we can "overflow" without warnings.
// Otherwise, we get stuff like this from Clang (when compiled
// with -fsanitize=bounds) while running:
//   src/v_video.cpp:390:12: runtime error: index 1068 out of bounds for type 'uint8_t [32]'
//   src/r_draw.cpp:273:11: runtime error: index 1057 out of bounds for type 'uint8_t [32]'
union ColorTable32k
{
	uint8_t RGB[32][32][32];
	uint8_t All[32 *32 *32];
};
extern ColorTable32k RGB32k;

// [SP] RGB666 support
union ColorTable256k
{
	uint8_t RGB[64][64][64];
	uint8_t All[64 *64 *64];
};
extern ColorTable256k RGB256k;

// Col2RGB8 is a pre-multiplied palette for color lookup. It is stored in a
// special R10B10G10 format for efficient blending computation.
//		--RRRRRrrr--BBBBBbbb--GGGGGggg--   at level 64
//		--------rrrr------bbbb------gggg   at level 1
extern uint32_t Col2RGB8[65][256];

// Col2RGB8_LessPrecision is the same as Col2RGB8, but the LSB for red
// and blue are forced to zero, so if the blend overflows, it won't spill
// over into the next component's value.
//		--RRRRRrrr-#BBBBBbbb-#GGGGGggg--  at level 64
//      --------rrr#------bbb#------gggg  at level 1
extern uint32_t *Col2RGB8_LessPrecision[65];

// Col2RGB8_Inverse is the same as Col2RGB8_LessPrecision, except the source
// palette has been inverted.
extern uint32_t Col2RGB8_Inverse[65][256];

// "Magic" numbers used during the blending:
//		--000001111100000111110000011111	= 0x01f07c1f
//		-0111111111011111111101111111111	= 0x3FEFFBFF
//		-1000000000100000000010000000000	= 0x40100400
//		------10000000001000000000100000	= 0x40100400 >> 5
//		--11111-----11111-----11111-----	= 0x40100400 - (0x40100400 >> 5) aka "white"
//		--111111111111111111111111111111	= 0x3FFFFFFF

void BuildTransTable (const PalEntry *palette);
