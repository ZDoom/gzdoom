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
#include "r_spandrawer.h"
#include "swrenderer/r_renderthread.h"

namespace swrenderer
{
	SpanDrawerArgs::SpanDrawerArgs()
	{
		spanfunc = &SWPixelFormatDrawers::DrawSpan;
	}

	void SpanDrawerArgs::SetTexture(RenderThread *thread, FSoftwareTexture *tex)
	{
		thread->PrepareTexture(tex, DefaultRenderStyle());

		ds_texwidth = tex->GetPhysicalWidth();
		ds_texheight = tex->GetPhysicalHeight();
		ds_xbits = tex->GetWidthBits();
		ds_ybits = tex->GetHeightBits();
		if ((1 << ds_xbits) > tex->GetPhysicalWidth())
		{
			ds_xbits--;
		}
		if ((1 << ds_ybits) > tex->GetPhysicalHeight())
		{
			ds_ybits--;
		}

		ds_source = thread->Viewport->RenderTarget->IsBgra() ? (const uint8_t*)tex->GetPixelsBgra() : tex->GetPixels(DefaultRenderStyle()); // Get correct render style? Shaded won't get here.
		ds_source_mipmapped = tex->Mipmapped() && tex->GetPhysicalWidth() > 1 && tex->GetPhysicalHeight() > 1;
	}

	void SpanDrawerArgs::SetStyle(bool masked, bool additive, fixed_t alpha, FDynamicColormap *basecolormap)
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

		CameraLight *cameraLight = CameraLight::Instance();
		if (cameraLight->FixedLightLevel() >= 0)
		{
			SetBaseColormap((r_fullbrightignoresectorcolor) ? &FullNormalLight : basecolormap);
			SetLight(0, cameraLight->FixedLightLevelShade());
		}
		else if (cameraLight->FixedColormap())
		{
			SetBaseColormap(cameraLight->FixedColormap());
			SetLight(0, 0);
		}
		else
		{
			SetBaseColormap(basecolormap);
		}
	}

	void SpanDrawerArgs::DrawDepthSpan(RenderThread *thread, float idepth1, float idepth2)
	{
		thread->Drawers(ds_viewport)->DrawDepthSpan(*this, idepth1, idepth2);
	}

	void SpanDrawerArgs::DrawSpan(RenderThread *thread)
	{
		(thread->Drawers(ds_viewport)->*spanfunc)(*this);
	}

	void SpanDrawerArgs::DrawTiltedSpan(RenderThread *thread, int y, int x1, int x2, const FVector3 &plane_sz, const FVector3 &plane_su, const FVector3 &plane_sv, bool plane_shade, int lightlevel, bool foggy, float planelightfloat, fixed_t pviewx, fixed_t pviewy, FDynamicColormap *basecolormap)
	{
		SetDestY(thread->Viewport.get(), y);
		SetDestX1(x1);
		SetDestX2(x2);
		thread->Drawers(ds_viewport)->DrawTiltedSpan(*this, plane_sz, plane_su, plane_sv, plane_shade, LightVisibility::LightLevelToShade(lightlevel, foggy, thread->Viewport.get()), planelightfloat, pviewx, pviewy, basecolormap);
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
