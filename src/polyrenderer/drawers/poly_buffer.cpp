/*
**  Polygon Doom software renderer
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/

#include <stddef.h>
#include "templates.h"
#include "doomdef.h"
#include "i_system.h"
#include "w_wad.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "g_game.h"
#include "g_level.h"
#include "r_data/r_translate.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "poly_buffer.h"
#include "screen_triangle.h"

/////////////////////////////////////////////////////////////////////////////

PolyZBuffer *PolyZBuffer::Instance()
{
	static PolyZBuffer buffer;
	return &buffer;
}

void PolyZBuffer::Resize(int newwidth, int newheight)
{
	width = newwidth;
	height = newheight;
	values.resize(width * height);
}

/////////////////////////////////////////////////////////////////////////////

PolyStencilBuffer *PolyStencilBuffer::Instance()
{
	static PolyStencilBuffer buffer;
	return &buffer;
}

void PolyStencilBuffer::Resize(int newwidth, int newheight)
{
	width = newwidth;
	height = newheight;
	values.resize(width * height);
}
