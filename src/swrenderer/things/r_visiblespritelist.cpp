//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//

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
	VisibleSpriteList *VisibleSpriteList::Instance()
	{
		static VisibleSpriteList instance;
		return &instance;
	}

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

	void VisibleSpriteList::Sort(bool compare2d)
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

		if (compare2d)
		{
			// This is an alternate version, for when one or more voxel is in view.
			// It does a 2D distance test based on whichever one is furthest from
			// the viewpoint.

			std::stable_sort(&SortedSprites[0], &SortedSprites[count], [](VisibleSprite *a, VisibleSprite *b) -> bool
			{
				return DVector2(a->deltax, a->deltay).LengthSquared() < DVector2(b->deltax, b->deltay).LengthSquared();
			});
		}
		else
		{
			// This is the standard version, which does a simple test based on depth.

			std::stable_sort(&SortedSprites[0], &SortedSprites[count], [](VisibleSprite *a, VisibleSprite *b) -> bool
			{
				return a->idepth > b->idepth;
			});
		}
	}
}
