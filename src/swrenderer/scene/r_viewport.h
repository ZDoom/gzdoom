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

#pragma once

#include <stddef.h>
#include "r_defs.h"

namespace swrenderer
{
	extern DCanvas *RenderTarget;
	extern bool bRenderingToCanvas;
	extern fixed_t viewingrangerecip;
	extern double FocalLengthX;
	extern double FocalLengthY;
	extern double InvZtoScale;
	extern double WallTMapScale2;
	extern double CenterX;
	extern double CenterY;
	extern double YaspectMul;
	extern double IYaspectMul;
	extern bool r_swtruecolor;
	extern double globaluclip;
	extern double globaldclip;
	extern angle_t xtoviewangle[MAXWIDTH + 1];

	extern uint8_t *dc_destorg;
	extern int dc_destheight;
	extern int dc_pitch;

	class RenderViewport
	{
	public:
		static RenderViewport *Instance();

		void SetViewport(int width, int height, float trueratio);
		void SetupFreelook();

	private:
		void InitTextureMapping();
		void SetupBuffer();
	};
}
