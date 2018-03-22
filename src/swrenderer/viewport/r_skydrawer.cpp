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
#include "r_skydrawer.h"
#include "swrenderer/r_renderthread.h"

namespace swrenderer
{
	void SkyDrawerArgs::DrawDepthSkyColumn(RenderThread *thread, float idepth)
	{
		thread->Drawers(dc_viewport)->DrawDepthSkyColumn(*this, idepth);
	}

	void SkyDrawerArgs::DrawSingleSkyColumn(RenderThread *thread)
	{
		thread->Drawers(dc_viewport)->DrawSingleSkyColumn(*this);
	}

	void SkyDrawerArgs::DrawDoubleSkyColumn(RenderThread *thread)
	{
		thread->Drawers(dc_viewport)->DrawDoubleSkyColumn(*this);
	}

	void SkyDrawerArgs::SetDest(RenderViewport *viewport, int x, int y)
	{
		dc_dest = viewport->GetDest(x, y);
		dc_dest_y = y;
		dc_viewport = viewport;
	}

	void SkyDrawerArgs::SetFrontTexture(RenderThread *thread, FTexture *texture, uint32_t column)
	{
		if (thread->Viewport->RenderTarget->IsBgra())
		{
			dc_source = (const uint8_t *)texture->GetColumnBgra(column, nullptr);
			dc_sourceheight = texture->GetHeight();
		}
		else
		{
			dc_source = texture->GetColumn(DefaultRenderStyle(), column, nullptr);
			dc_sourceheight = texture->GetHeight();
		}
	}

	void SkyDrawerArgs::SetBackTexture(RenderThread *thread, FTexture *texture, uint32_t column)
	{
		if (texture == nullptr)
		{
			dc_source2 = nullptr;
			dc_sourceheight2 = 1;
		}
		else if (thread->Viewport->RenderTarget->IsBgra())
		{
			dc_source2 = (const uint8_t *)texture->GetColumnBgra(column, nullptr);
			dc_sourceheight2 = texture->GetHeight();
		}
		else
		{
			dc_source2 = texture->GetColumn(DefaultRenderStyle(), column, nullptr);
			dc_sourceheight2 = texture->GetHeight();
		}
	}
}
