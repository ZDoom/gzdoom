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
#include "r_skydrawer.h"
#include "swrenderer/r_renderthread.h"

namespace swrenderer
{
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
			dc_source = texture->GetColumn(column, nullptr);
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
			dc_source2 = texture->GetColumn(column, nullptr);
			dc_sourceheight2 = texture->GetHeight();
		}
	}
}
