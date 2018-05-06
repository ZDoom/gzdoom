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

	void VisibleSpriteList::Sort(RenderThread *thread)
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

		if (r_models) // To do: only do this if models are spotted - just in case there's some lame renderhack somewhere that relies on Carmacks algorithm
		{
			for (unsigned int i = 0; i < count; i++)
			{
				FVector2 worldPos = SortedSprites[i]->WorldPos().XY();
				SortedSprites[i]->SubsectorDepth = FindSubsectorDepth(thread, { worldPos.X, worldPos.Y });
			}

			std::stable_sort(&SortedSprites[0], &SortedSprites[count], [](VisibleSprite *a, VisibleSprite *b) -> bool
			{
				if (a->SubsectorDepth != b->SubsectorDepth)
					return a->SubsectorDepth < b->SubsectorDepth;
				else
					return a->SortDist() > b->SortDist();
			});
		}
		else
		{
			std::stable_sort(&SortedSprites[0], &SortedSprites[count], [](VisibleSprite *a, VisibleSprite *b) -> bool
			{
				return a->SortDist() > b->SortDist();
			});
		}
	}

	uint32_t VisibleSpriteList::FindSubsectorDepth(RenderThread *thread, const DVector2 &worldPos)
	{
		if (level.nodes.Size() == 0)
		{
			subsector_t *sub = &level.subsectors[0];
			return thread->OpaquePass->GetSubsectorDepth(sub->Index());
		}
		else
		{
			return FindSubsectorDepth(thread, worldPos, level.HeadNode());
		}
	}

	uint32_t VisibleSpriteList::FindSubsectorDepth(RenderThread *thread, const DVector2 &worldPos, void *node)
	{
		while (!((size_t)node & 1))  // Keep going until found a subsector
		{
			node_t *bsp = (node_t *)node;

			DVector2 planePos(FIXED2DBL(bsp->x), FIXED2DBL(bsp->y));
			DVector2 planeNormal = DVector2(FIXED2DBL(-bsp->dy), FIXED2DBL(bsp->dx));
			double planeD = planeNormal | planePos;

			int side = (worldPos | planeNormal) > planeD;
			node = bsp->children[side];
		}

		subsector_t *sub = (subsector_t *)((uint8_t *)node - 1);
		return thread->OpaquePass->GetSubsectorDepth(sub->Index());
	}
}
