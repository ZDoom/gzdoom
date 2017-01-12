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

	void R_SWRSetWindow(int windowSize, int fullWidth, int fullHeight, int stHeight, float trueratio);
	void R_InitTextureMapping();
	void R_SetupBuffer();
	void R_SetupFreelook();
	void R_InitRenderer();
}
