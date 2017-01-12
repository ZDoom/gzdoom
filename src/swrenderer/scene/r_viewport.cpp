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
#include <float.h>

#include "templates.h"
#include "i_system.h"
#include "w_wad.h"
#include "doomdef.h"
#include "doomstat.h"
#include "r_sky.h"
#include "stats.h"
#include "v_video.h"
#include "a_sharedglobal.h"
#include "c_console.h"
#include "cmdlib.h"
#include "d_net.h"
#include "g_level.h"
#include "r_utility.h"
#include "swrenderer/scene/r_viewport.h"
#include "swrenderer/scene/r_light.h"
#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/things/r_playersprite.h"
#include "swrenderer/plane/r_flatplane.h"

CVAR(String, r_viewsize, "", CVAR_NOSET)

namespace swrenderer
{
	bool r_swtruecolor;

	fixed_t viewingrangerecip;
	double FocalLengthX;
	double FocalLengthY;

	bool bRenderingToCanvas;
	double globaluclip, globaldclip;
	double CenterX, CenterY;
	double YaspectMul;
	double BaseYaspectMul; // yaspectmul without a forced aspect ratio
	double IYaspectMul;
	double InvZtoScale;

	double WallTMapScale2;

	// The xtoviewangleangle[] table maps a screen pixel
	// to the lowest viewangle that maps back to x ranges
	// from clipangle to -clipangle.
	angle_t xtoviewangle[MAXWIDTH + 1];

	void R_SWRSetWindow(int windowSize, int fullWidth, int fullHeight, int stHeight, float trueratio)
	{
		int virtheight, virtwidth, virtwidth2, virtheight2;

		if (!bRenderingToCanvas)
		{ // Set r_viewsize cvar to reflect the current view size
			UCVarValue value;
			char temp[16];

			mysnprintf(temp, countof(temp), "%d x %d", viewwidth, viewheight);
			value.String = temp;
			r_viewsize.ForceSet(value, CVAR_String);
		}

		fuzzviewheight = viewheight - 2;	// Maximum row the fuzzer can draw to

		CenterX = centerx;
		CenterY = centery;

		virtwidth = virtwidth2 = fullWidth;
		virtheight = virtheight2 = fullHeight;

		if (AspectTallerThanWide(trueratio))
		{
			virtheight2 = virtheight2 * AspectMultiplier(trueratio) / 48;
		}
		else
		{
			virtwidth2 = virtwidth2 * AspectMultiplier(trueratio) / 48;
		}

		if (AspectTallerThanWide(WidescreenRatio))
		{
			virtheight = virtheight * AspectMultiplier(WidescreenRatio) / 48;
		}
		else
		{
			virtwidth = virtwidth * AspectMultiplier(WidescreenRatio) / 48;
		}

		BaseYaspectMul = 320.0 * virtheight2 / (r_Yaspect * virtwidth2);
		YaspectMul = 320.0 * virtheight / (r_Yaspect * virtwidth);
		IYaspectMul = (double)virtwidth * r_Yaspect / 320.0 / virtheight;
		InvZtoScale = YaspectMul * CenterX;

		WallTMapScale2 = IYaspectMul / CenterX;

		// psprite scales
		RenderPlayerSprite::SetupSpriteScale();

		// thing clipping
		fillshort(screenheightarray, viewwidth, (short)viewheight);

		R_InitTextureMapping();

		// Reset r_*Visibility vars
		R_SetVisibility(R_GetVisibility());
	}

	void R_SetupFreelook()
	{
		double dy;

		if (camera != NULL)
		{
			dy = FocalLengthY * (-ViewPitch).Tan();
		}
		else
		{
			dy = 0;
		}

		CenterY = (viewheight / 2.0) + dy;
		centery = xs_ToInt(CenterY);
		globaluclip = -CenterY / InvZtoScale;
		globaldclip = (viewheight - CenterY) / InvZtoScale;

		RenderFlatPlane::SetupSlope();
	}

	void R_SetupBuffer()
	{
		using namespace drawerargs;

		static BYTE *lastbuff = NULL;

		int pitch = RenderTarget->GetPitch();
		int pixelsize = r_swtruecolor ? 4 : 1;
		BYTE *lineptr = RenderTarget->GetBuffer() + (viewwindowy*pitch + viewwindowx) * pixelsize;

		if (dc_pitch != pitch || lineptr != lastbuff)
		{
			if (dc_pitch != pitch)
			{
				dc_pitch = pitch;
				R_InitFuzzTable(pitch);
			}
			dc_destorg = lineptr;
			dc_destheight = RenderTarget->GetHeight() - viewwindowy;
			for (int i = 0; i < RenderTarget->GetHeight(); i++)
			{
				ylookup[i] = i * pitch;
			}
		}
	}

	void R_InitTextureMapping()
	{
		int i;

		// Calc focallength so FieldOfView angles cover viewwidth.
		FocalLengthX = CenterX / FocalTangent;
		FocalLengthY = FocalLengthX * YaspectMul;

		// This is 1/FocalTangent before the widescreen extension of FOV.
		viewingrangerecip = FLOAT2FIXED(1. / tan(FieldOfView.Radians() / 2));


		// Now generate xtoviewangle for sky texture mapping.
		// [RH] Do not generate viewangletox, because texture mapping is no
		// longer done with trig, so it's not needed.
		const double slopestep = FocalTangent / centerx;
		double slope;

		for (i = centerx, slope = 0; i <= viewwidth; i++, slope += slopestep)
		{
			xtoviewangle[i] = angle_t((2 * M_PI - atan(slope)) * (ANGLE_180 / M_PI));
		}
		for (i = 0; i < centerx; i++)
		{
			xtoviewangle[i] = 0 - xtoviewangle[viewwidth - i - 1];
		}
	}
}
