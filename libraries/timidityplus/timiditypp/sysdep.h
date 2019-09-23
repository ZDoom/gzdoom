/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#ifndef SYSDEP_H_INCLUDED
#define SYSDEP_H_INCLUDED 1

#include <limits.h>

#define DEFAULT_AUDIO_BUFFER_BITS 12
#define SAMPLE_LENGTH_BITS 32

#include <stdint.h> // int types are defined here

#include "t_swap.h"

namespace TimidityPlus
{


	/* Instrument files are little-endian, MIDI files big-endian, so we
	   need to do some conversions. */
#define XCHG_SHORT(x) ((((x)&0xFF)<<8) | (((x)>>8)&0xFF))

#define LE_SHORT(x) LittleShort(x)
#define LE_LONG(x) LittleLong(x)
#define BE_SHORT(x)  BigShort(x)
#define BE_LONG(x) BigLong(x)

	   /* max_channels is defined in "timidity.h" */
typedef struct _ChannelBitMask
{
	uint32_t b; /* 32-bit bitvector */
} ChannelBitMask;
#define CLEAR_CHANNELMASK(bits)		((bits).b = 0)
#define FILL_CHANNELMASK(bits)		((bits).b = ~0)
#define IS_SET_CHANNELMASK(bits, c) ((bits).b &   (1u << (c)))
#define SET_CHANNELMASK(bits, c)    ((bits).b |=  (1u << (c)))
#define UNSET_CHANNELMASK(bits, c)  ((bits).b &= ~(1u << (c)))
#define TOGGLE_CHANNELMASK(bits, c) ((bits).b ^=  (1u << (c)))
#define COPY_CHANNELMASK(dest, src)	((dest).b = (src).b)
#define REVERSE_CHANNELMASK(bits)	((bits).b = ~(bits).b)
#define COMPARE_CHANNELMASK(bitsA, bitsB)	((bitsA).b == (bitsB).b)

	typedef int16_t sample_t;
	typedef int32_t final_volume_t;
#  define FINAL_VOLUME(v) (v)
#  define MAX_AMP_VALUE ((1<<(AMP_BITS+1))-1)
#define MIN_AMP_VALUE (MAX_AMP_VALUE >> 9)

	typedef uint32_t splen_t;
#define SPLEN_T_MAX (splen_t)((uint32_t)0xFFFFFFFF)

#  define TIM_FSCALE(a,b) ((a) * (double)(1<<(b)))
#  define TIM_FSCALENEG(a,b) ((a) * (1.0 / (double)(1<<(b))))


#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif /* M_PI */


#if defined(_MSC_VER)
#define strncasecmp(a,b,c)	_strnicmp((a),(b),(c))
#define strcasecmp(a,b)		_stricmp((a),(b))
#endif /* _MSC_VER */

#define SAFE_CONVERT_LENGTH(len) (6 * (len) + 1)

}
#endif /* SYSDEP_H_INCUDED */
