//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Mission begin melt/wipe screen special effect.
//
//-----------------------------------------------------------------------------

#include "v_video.h"
#include "m_random.h"
#include "f_wipe.h"
#include "templates.h"

int wipe_CalcBurn (uint8_t *burnarray, int width, int height, int density)
{
	// This is a modified version of the fire that was once used
	// on the player setup menu.
	static int voop;

	int a, b;
	uint8_t *from;

	// generator
	from = &burnarray[width * height];
	b = voop;
	voop += density / 3;
	for (a = 0; a < density/8; a++)
	{
		unsigned int offs = (a+b) & (width - 1);
		unsigned int v = M_Random();
		v = MIN(from[offs] + 4 + (v & 15) + (v >> 3) + (M_Random() & 31), 255u);
		from[offs] = from[width*2 + ((offs + width*3/2) & (width - 1))] = v;
	}

	density = MIN(density + 10, width * 7);

	from = burnarray;
	for (b = 0; b <= height; b += 2)
	{
		uint8_t *pixel = from;

		// special case: first pixel on line
		uint8_t *p = pixel + (width << 1);
		unsigned int top = *p + *(p + width - 1) + *(p + 1);
		unsigned int bottom = *(pixel + (width << 2));
		unsigned int c1 = (top + bottom) >> 2;
		if (c1 > 1) c1--;
		*pixel = c1;
		*(pixel + width) = (c1 + bottom) >> 1;
		pixel++;

		// main line loop
		for (a = 1; a < width-1; a++)
		{
			// sum top pixels
			p = pixel + (width << 1);
			top = *p + *(p - 1) + *(p + 1);

			// bottom pixel
			bottom = *(pixel + (width << 2));

			// combine pixels
			c1 = (top + bottom) >> 2;
			if (c1 > 1) c1--;

			// store pixels
			*pixel = c1;
			*(pixel + width) = (c1 + bottom) >> 1;		// interpolate

			// next pixel
			pixel++;
		}

		// special case: last pixel on line
		p = pixel + (width << 1);
		top = *p + *(p - 1) + *(p - width + 1);
		bottom = *(pixel + (width << 2));
		c1 = (top + bottom) >> 2;
		if (c1 > 1) c1--;
		*pixel = c1;
		*(pixel + width) = (c1 + bottom) >> 1;

		// next line
		from += width << 1;
	}

	// Check for done-ness. (Every pixel with level 126 or higher counts as done.)
	for (a = width * height, from = burnarray; a != 0; --a, ++from)
	{
		if (*from < 126)
		{
			return density;
		}
	}
	return -1;
}

