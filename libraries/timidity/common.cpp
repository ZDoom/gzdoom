/*

	TiMidity -- Experimental MIDI to WAVE converter
	Copyright (C) 1995 Tuukka Toivonen <toivonen@clinet.fi>

	This library is free software; you can redistribute it and/or
	modify it under the terms of the GNU Lesser General Public
	License as published by the Free Software Foundation; either
	version 2.1 of the License, or (at your option) any later version.

	This library is distributed in the hope that it will be useful,
	but WITHOUT ANY WARRANTY; without even the implied warranty of
	MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
	Lesser General Public License for more details.

	You should have received a copy of the GNU Lesser General Public
	License along with this library; if not, write to the Free Software
	Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

	common.c

*/

#include <stdio.h>
#include <stdlib.h>
#include <stdexcept>


namespace Timidity
{



/* This'll allocate memory or die. */
void *safe_malloc(size_t count)
{
	char buffer[80];
	void *p;
	if (count > (1 << 21))
	{
		snprintf(buffer, 80, "Timidity: Tried allocating %zu bytes. This must be a bug.", count);
		throw std::runtime_error(buffer);
	}
	else if ((p = malloc(count)))
	{
		return p;
	}
	else
	{
		snprintf(buffer, 80, "Timidity: Couldn't malloc %zu bytes.", count);
		throw std::runtime_error(buffer);
	}
	return 0;	// Unreachable.
}


}
