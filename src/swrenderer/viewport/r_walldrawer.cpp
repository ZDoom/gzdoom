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

#include <stddef.h>
#include "r_walldrawer.h"
#include "swrenderer/r_renderthread.h"

namespace swrenderer
{
	void WallDrawerArgs::SetDest(RenderViewport *viewport, int x, int y)
	{
		dc_viewport = viewport;
		dc_dest = viewport->GetDest(x, y);
		dc_dest_y = y;
	}

	void WallDrawerArgs::DrawDepthColumn(RenderThread *thread, float idepth)
	{
		thread->Drawers(dc_viewport)->DrawDepthWallColumn(*this, idepth);
	}

	void WallDrawerArgs::DrawColumn(RenderThread *thread)
	{
		(thread->Drawers(dc_viewport)->*wallfunc)(*this);
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
