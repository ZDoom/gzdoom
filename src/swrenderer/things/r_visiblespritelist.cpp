//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2016 Magnus Norddahl
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

#include <stdio.h>
#include <stdlib.h>
#include <algorithm>
#include "p_lnspec.h"
#include "templates.h"
#include "doomdef.h"
#include "m_swap.h"
#include "i_system.h"
#include "w_wad.h"
#include "g_levellocals.h"
#include "p_maputl.h"
#include "swrenderer/things/r_visiblesprite.h"
#include "swrenderer/things/r_visiblespritelist.h"
#include "swrenderer/r_memory.h"

namespace swrenderer
{
	void VisibleSpriteList::Clear()
	{
		Sprites.Clear();
		StartIndices.Clear();
		SortedSprites.Clear();
	}

	void VisibleSpriteList::PushPortal()
	{
		StartIndices.Push(Sprites.Size());
	}

	void VisibleSpriteList::PopPortal()
	{
		Sprites.Resize(StartIndices.Last());
		StartIndices.Pop();
	}

	void VisibleSpriteList::Push(VisibleSprite *sprite)
	{
		Sprites.Push(sprite);
	}

	void VisibleSpriteList::Sort()
	{
		unsigned int first = StartIndices.Size() == 0 ? 0 : StartIndices.Last();
		unsigned int count = Sprites.Size() - first;

		SortedSprites.Resize(count);

		if (count == 0)
			return;

		if (!(i_compatflags & COMPATF_SPRITESORT))
		{
			for (unsigned int i = 0; i < count; i++)
				SortedSprites[i] = Sprites[first + i];
		}
		else
		{
			// If the compatibility option is on sprites of equal distance need to
			// be sorted in inverse order. This is most easily achieved by
			// filling the sort array backwards before the sort.
			for (unsigned int i = 0; i < count; i++)
				SortedSprites[i] = Sprites[first + count - i - 1];
		}

		std::stable_sort(&SortedSprites[0], &SortedSprites[count], [](VisibleSprite *a, VisibleSprite *b) -> bool
		{
			return a->SortDist() > b->SortDist();
		});
	}
}
