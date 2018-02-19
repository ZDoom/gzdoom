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

    output.c

    Audio output (to file / device) functions.
*/

#include "timidity.h"
#include "common.h"
#include "sysdep.h"
#include "tables.h"

namespace TimidityPlus
{


int audio_buffer_bits = DEFAULT_AUDIO_BUFFER_BITS;

extern PlayMode w32_play_mode;
#define DEV_PLAY_MODE &w32_play_mode

extern PlayMode raw_play_mode;


PlayMode *play_mode = DEV_PLAY_MODE;

/*****************************************************************/
/* Some functions to convert signed 32-bit data to other formats */

void s32tos16(int32_t *lp, int32_t c)
{
  int16_t *sp=(int16_t *)(lp);
  int32_t l, i;

  for(i = 0; i < c; i++)
    {
      l=(lp[i])>>(32-16-GUARD_BITS);
      if (l > 32767) l=32767;
      else if (l<-32768) l=-32768;
      sp[i] = (int16_t)(l);
    }
}

// This only gets used as intermediate so we do not care about byte order
#define STORE_S24(cp, l) *cp++ = l & 0xFF, *cp++ = l >> 8 & 0xFF, *cp++ = l >> 16
#define MAX_24BIT_SIGNED (8388607)
#define MIN_24BIT_SIGNED (-8388608)

void s32tos24(int32_t *lp, int32_t c)
{
	uint8_t *cp = (uint8_t *)(lp);
	int32_t l, i;

	for(i = 0; i < c; i++)
	{
		l = (lp[i]) >> (32 - 24 - GUARD_BITS);
		l = (l > MAX_24BIT_SIGNED) ? MAX_24BIT_SIGNED
				: (l < MIN_24BIT_SIGNED) ? MIN_24BIT_SIGNED : l;
		STORE_S24(cp, l);
	}
}

/* return: number of bytes */
int32_t general_output_convert(int32_t *buf, int32_t count)
{
    int32_t bytes;

	count *= 2; /* Stereo samples */
    bytes = count;
	bytes *= 2;
    s32tos16(buf, count);
    return bytes;
}

}