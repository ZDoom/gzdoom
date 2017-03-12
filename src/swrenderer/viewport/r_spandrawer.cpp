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
#include "r_spandrawer.h"
#include "swrenderer/r_renderthread.h"

namespace swrenderer
{
	SpanDrawerArgs::SpanDrawerArgs()
	{
		spanfunc = &SWPixelFormatDrawers::DrawSpan;
	}

	void SpanDrawerArgs::SetTexture(RenderViewport *viewport, FTexture *tex)
	{
		tex->GetWidth();
		ds_xbits = tex->WidthBits;
		ds_ybits = tex->HeightBits;
		if ((1 << ds_xbits) > tex->GetWidth())
		{
			ds_xbits--;
		}
		if ((1 << ds_ybits) > tex->GetHeight())
		{
			ds_ybits--;
		}

		ds_source = viewport->RenderTarget->IsBgra() ? (const uint8_t*)tex->GetPixelsBgra() : tex->GetPixels();
		ds_source_mipmapped = tex->Mipmapped() && tex->GetWidth() > 1 && tex->GetHeight() > 1;
	}

	void SpanDrawerArgs::SetStyle(bool masked, bool additive, fixed_t alpha)
	{
		if (masked)
		{
			if (alpha < OPAQUE || additive)
			{
				if (!additive)
				{
					spanfunc = &SWPixelFormatDrawers::DrawSpanMaskedTranslucent;
					dc_srcblend = Col2RGB8[alpha >> 10];
					dc_destblend = Col2RGB8[(OPAQUE - alpha) >> 10];
					dc_srcalpha = alpha;
					dc_destalpha = OPAQUE - alpha;
				}
				else
				{
					spanfunc = &SWPixelFormatDrawers::DrawSpanMaskedAddClamp;
					dc_srcblend = Col2RGB8_LessPrecision[alpha >> 10];
					dc_destblend = Col2RGB8_LessPrecision[FRACUNIT >> 10];
					dc_srcalpha = alpha;
					dc_destalpha = FRACUNIT;
				}
			}
			else
			{
				spanfunc = &SWPixelFormatDrawers::DrawSpanMasked;
			}
		}
		else
		{
			if (alpha < OPAQUE || additive)
			{
				if (!additive)
				{
					spanfunc = &SWPixelFormatDrawers::DrawSpanTranslucent;
					dc_srcblend = Col2RGB8[alpha >> 10];
					dc_destblend = Col2RGB8[(OPAQUE - alpha) >> 10];
					dc_srcalpha = alpha;
					dc_destalpha = OPAQUE - alpha;
				}
				else
				{
					spanfunc = &SWPixelFormatDrawers::DrawSpanAddClamp;
					dc_srcblend = Col2RGB8_LessPrecision[alpha >> 10];
					dc_destblend = Col2RGB8_LessPrecision[FRACUNIT >> 10];
					dc_srcalpha = alpha;
					dc_destalpha = FRACUNIT;
				}
			}
			else
			{
				spanfunc = &SWPixelFormatDrawers::DrawSpan;
			}
		}
	}

	void SpanDrawerArgs::DrawSpan(RenderThread *thread)
	{
		(thread->Drawers(ds_viewport)->*spanfunc)(*this);
	}

	void SpanDrawerArgs::DrawTiltedSpan(RenderThread *thread, int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int planeshade, float planelightfloat, fixed_t pviewx, fixed_t pviewy, FDynamicColormap *basecolormap)
	{
		SetDestY(thread->Viewport.get(), y);
		SetDestX1(x1);
		SetDestX2(x2);
		thread->Drawers(ds_viewport)->DrawTiltedSpan(*this, plane_sz, plane_su, plane_sv, plane_shade, planeshade, planelightfloat, pviewx, pviewy, basecolormap);
	}

	void SpanDrawerArgs::DrawFogBoundaryLine(RenderThread *thread, int y, int x1, int x2)
	{
		SetDestY(thread->Viewport.get(), y);
		SetDestX1(x1);
		SetDestX2(x2);
		thread->Drawers(ds_viewport)->DrawFogBoundaryLine(*this);
	}

	void SpanDrawerArgs::DrawColoredSpan(RenderThread *thread, int y, int x1, int x2)
	{
		SetDestY(thread->Viewport.get(), y);
		SetDestX1(x1);
		SetDestX2(x2);
		thread->Drawers(ds_viewport)->DrawColoredSpan(*this);
	}
}
