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

#include <stdlib.h>
#include "templates.h"
#include "doomdef.h"
#include "m_bbox.h"
#include "i_system.h"
#include "p_lnspec.h"
#include "p_setup.h"
#include "a_sharedglobal.h"
#include "g_level.h"
#include "p_effect.h"
#include "doomstat.h"
#include "r_state.h"
#include "v_palette.h"
#include "r_sky.h"
#include "po_man.h"
#include "r_data/colormaps.h"
#include "d_net.h"
#include "swrenderer/r_memory.h"
#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/scene/r_3dfloors.h"
#include "swrenderer/scene/r_opaque_pass.h"
#include "swrenderer/scene/r_portal.h"
#include "swrenderer/line/r_wallsetup.h"
#include "swrenderer/line/r_walldraw.h"
#include "swrenderer/line/r_fogboundary.h"
#include "swrenderer/segments/r_drawsegment.h"
#include "swrenderer/things/r_visiblesprite.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/viewport/r_viewport.h"

namespace swrenderer
{
	DrawSegmentList *DrawSegmentList::Instance()
	{
		static DrawSegmentList instance;
		return &instance;
	}

	void DrawSegmentList::Clear()
	{
		Segments.Clear();
		StartIndices.Clear();
		StartIndices.Push(0);

		InterestingSegments.Clear();
		StartInterestingIndices.Clear();
		StartInterestingIndices.Push(0);
	}

	void DrawSegmentList::PushPortal()
	{
		StartIndices.Push(Segments.Size());
		StartInterestingIndices.Push(InterestingSegments.Size());
	}

	void DrawSegmentList::PopPortal()
	{
		Segments.Resize(StartIndices.Last());
		StartIndices.Pop();

		StartInterestingIndices.Resize(StartInterestingIndices.Last());
		StartInterestingIndices.Pop();
	}

	void DrawSegmentList::Push(DrawSegment *segment)
	{
		Segments.Push(segment);
	}

	void DrawSegmentList::PushInteresting(DrawSegment *segment)
	{
		InterestingSegments.Push(segment);
	}
}
