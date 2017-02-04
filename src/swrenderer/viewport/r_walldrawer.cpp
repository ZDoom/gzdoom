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

#include <stddef.h>
#include "r_walldrawer.h"
#include "swrenderer/r_renderthread.h"

namespace swrenderer
{
	void WallDrawerArgs::SetDest(int x, int y)
	{
		auto viewport = RenderViewport::Instance();
		dc_dest = viewport->GetDest(x, y);
		dc_dest_y = y;
	}

	void WallDrawerArgs::DrawColumn(RenderThread *thread)
	{
		(thread->Drawers()->*wallfunc)(*this);
	}

	void WallDrawerArgs::SetStyle(bool masked, bool additive, fixed_t alpha)
	{
		if (alpha < OPAQUE || additive)
		{
			if (!additive)
			{
				wallfunc = &SWPixelFormatDrawers::DrawWallAddColumn;
				dc_srcblend = Col2RGB8[alpha >> 10];
				dc_destblend = Col2RGB8[(OPAQUE - alpha) >> 10];
				dc_srcalpha = alpha;
				dc_destalpha = OPAQUE - alpha;
			}
			else
			{
				wallfunc = &SWPixelFormatDrawers::DrawWallAddClampColumn;
				dc_srcblend = Col2RGB8_LessPrecision[alpha >> 10];
				dc_destblend = Col2RGB8_LessPrecision[FRACUNIT >> 10];
				dc_srcalpha = alpha;
				dc_destalpha = FRACUNIT;
			}
		}
		else if (masked)
		{
			wallfunc = &SWPixelFormatDrawers::DrawWallMaskedColumn;
		}
		else
		{
			wallfunc = &SWPixelFormatDrawers::DrawWallColumn;
		}
	}

	bool WallDrawerArgs::IsMaskedDrawer() const
	{
		return wallfunc == &SWPixelFormatDrawers::DrawWallMaskedColumn;
	}
}
