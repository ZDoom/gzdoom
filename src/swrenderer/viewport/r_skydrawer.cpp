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
		thread->Drawers()->DrawSingleSkyColumn(*this);
	}

	void SkyDrawerArgs::DrawDoubleSkyColumn(RenderThread *thread)
	{
		thread->Drawers()->DrawDoubleSkyColumn(*this);
	}

	void SkyDrawerArgs::SetDest(int x, int y)
	{
		auto viewport = RenderViewport::Instance();
		dc_dest = viewport->GetDest(x, y);
		dc_dest_y = y;
	}

	void SkyDrawerArgs::SetFrontTexture(FTexture *texture, uint32_t column)
	{
		if (RenderViewport::Instance()->RenderTarget->IsBgra())
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

	void SkyDrawerArgs::SetBackTexture(FTexture *texture, uint32_t column)
	{
		if (texture == nullptr)
		{
			dc_source2 = nullptr;
			dc_sourceheight2 = 1;
		}
		else if (RenderViewport::Instance()->RenderTarget->IsBgra())
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
