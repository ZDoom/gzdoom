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
#include "swrenderer/r_main.h"
#include "swrenderer/things/r_visiblesprite.h"
#include "swrenderer/things/r_visiblespritelist.h"
#include "swrenderer/r_memory.h"

namespace swrenderer
{
	void VisibleSpriteList::Deinit()
	{
		// Free vissprites
		for (int i = 0; i < MaxVisSprites; ++i)
		{
			delete vissprites[i];
		}
		free(vissprites);
		vissprites = nullptr;
		vissprite_p = lastvissprite = nullptr;
		MaxVisSprites = 0;
	}

	void VisibleSpriteList::Clear()
	{
		vissprite_p = firstvissprite;
	}

	vissprite_t *VisibleSpriteList::Add()
	{
		if (vissprite_p == lastvissprite)
		{
			ptrdiff_t firstvisspritenum = firstvissprite - vissprites;
			ptrdiff_t prevvisspritenum = vissprite_p - vissprites;

			MaxVisSprites = MaxVisSprites ? MaxVisSprites * 2 : 128;
			vissprites = (vissprite_t **)M_Realloc(vissprites, MaxVisSprites * sizeof(vissprite_t));
			lastvissprite = &vissprites[MaxVisSprites];
			firstvissprite = &vissprites[firstvisspritenum];
			vissprite_p = &vissprites[prevvisspritenum];
			DPrintf(DMSG_NOTIFY, "MaxVisSprites increased to %d\n", MaxVisSprites);

			// Allocate sprites from the new pile
			for (vissprite_t **p = vissprite_p; p < lastvissprite; ++p)
			{
				*p = new vissprite_t;
			}
		}

		vissprite_p++;
		return *(vissprite_p - 1);
	}

	int VisibleSpriteList::MaxVisSprites;
	vissprite_t **VisibleSpriteList::vissprites;
	vissprite_t **VisibleSpriteList::firstvissprite;
	vissprite_t **VisibleSpriteList::vissprite_p;
	vissprite_t **VisibleSpriteList::lastvissprite;

	/////////////////////////////////////////////////////////////////////////

	void SortedVisibleSpriteList::Deinit()
	{
		delete[] spritesorter;
		spritesortersize = 0;
		spritesorter = nullptr;
	}

	// This is the standard version, which does a simple test based on depth.
	bool SortedVisibleSpriteList::sv_compare(vissprite_t *a, vissprite_t *b)
	{
		return a->idepth > b->idepth;
	}

	// This is an alternate version, for when one or more voxel is in view.
	// It does a 2D distance test based on whichever one is furthest from
	// the viewpoint.
	bool SortedVisibleSpriteList::sv_compare2d(vissprite_t *a, vissprite_t *b)
	{
		return DVector2(a->deltax, a->deltay).LengthSquared() < DVector2(b->deltax, b->deltay).LengthSquared();
	}

	void SortedVisibleSpriteList::Sort(bool(*compare)(vissprite_t *, vissprite_t *), size_t first)
	{
		int i;
		vissprite_t **spr;

		vsprcount = int(VisibleSpriteList::vissprite_p - &VisibleSpriteList::vissprites[first]);

		if (vsprcount == 0)
			return;

		if (spritesortersize < VisibleSpriteList::MaxVisSprites)
		{
			if (spritesorter != nullptr)
				delete[] spritesorter;
			spritesorter = new vissprite_t *[VisibleSpriteList::MaxVisSprites];
			spritesortersize = VisibleSpriteList::MaxVisSprites;
		}

		if (!(i_compatflags & COMPATF_SPRITESORT))
		{
			for (i = 0, spr = VisibleSpriteList::firstvissprite; i < vsprcount; i++, spr++)
			{
				spritesorter[i] = *spr;
			}
		}
		else
		{
			// If the compatibility option is on sprites of equal distance need to
			// be sorted in inverse order. This is most easily achieved by
			// filling the sort array backwards before the sort.
			for (i = 0, spr = VisibleSpriteList::firstvissprite + vsprcount - 1; i < vsprcount; i++, spr--)
			{
				spritesorter[i] = *spr;
			}
		}

		std::stable_sort(&spritesorter[0], &spritesorter[vsprcount], compare);
	}

	vissprite_t **SortedVisibleSpriteList::spritesorter;
	int SortedVisibleSpriteList::spritesortersize = 0;
	int SortedVisibleSpriteList::vsprcount;
}
