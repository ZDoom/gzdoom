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

#ifndef OPTCODE_H_INCLUDED
#define OPTCODE_H_INCLUDED 1

#include <stdint.h>

namespace TimidityPlus
{

/*****************************************************************************/


/*****************************************************************************/

/* Generic version of imuldiv. */
inline int32_t imuldiv8(int32_t a, int32_t b)
{
	return (int32_t)(((int64_t)(a) * (int64_t)(b)) >> 8);
}

inline int32_t imuldiv16(int32_t a, int32_t b)
{
	return (int32_t)(((int64_t)(a) * (int64_t)(b)) >> 16);
}

inline int32_t imuldiv24(int32_t a, int32_t b)
{
	return (int32_t)(((int64_t)(a) * (int64_t)(b)) >> 24);
}

inline int32_t imuldiv28(int32_t a, int32_t b)
{
	return (int32_t)(((int64_t)(a) * (int64_t)(b)) >> 28);
}


static inline int32_t signlong(int32_t a)
{
	return ((a | 0x7fffffff) >> 30);
}


}
/*****************************************************************************/

#endif /* OPTCODE_H_INCLUDED */
