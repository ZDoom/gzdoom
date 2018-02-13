// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2017 Magnus Norddahl
// Copyright(C) 2017 Rachael Alexanderson
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

#include <math.h>
#include "c_dispatch.h"
#include "c_cvars.h"
#include "v_video.h"

#define NUMSCALEMODES 5

namespace
{
	struct v_ScaleTable
	{
		bool isValid;
		bool isLinear;
		uint32_t(*GetScaledWidth)(uint32_t Width);
		uint32_t(*GetScaledHeight)(uint32_t Height);
		bool isScaled43;
	};
	v_ScaleTable vScaleTable[NUMSCALEMODES] =
	{
		//	isValid,	isLinear,	GetScaledWidth(),										GetScaledHeight(),											isScaled43
		{ true,			false,		[](uint32_t Width)->uint32_t { return Width; },			[](uint32_t Height)->uint32_t { return Height; },			false	},	// 0  - Native
		{ true,			true,		[](uint32_t Width)->uint32_t { return Width; },			[](uint32_t Height)->uint32_t { return Height; },			false	},	// 1  - Native (Linear)
		{ true,			false,		[](uint32_t Width)->uint32_t { return 320; },			[](uint32_t Height)->uint32_t { return 200; },				true	},	// 2  - 320x200
		{ true,			false,		[](uint32_t Width)->uint32_t { return 640; },			[](uint32_t Height)->uint32_t { return 400; },				true	},	// 3  - 640x400
		{ true,			true,		[](uint32_t Width)->uint32_t { return 1280; },			[](uint32_t Height)->uint32_t { return 800; },				true	},	// 4  - 1280x800		
	};
	bool isOutOfBounds(int x)
	{
		return (x < 0 || x >= NUMSCALEMODES || vScaleTable[x].isValid == false);
	}
}

CUSTOM_CVAR(Float, vid_scalefactor, 1.0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self <= 0.0 || self > 2.0)
		self = 1.0;
}

CUSTOM_CVAR(Int, vid_scalemode, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (isOutOfBounds(self))
		self = 0;
}

CVAR(Bool, vid_cropaspect, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

bool ViewportLinearScale()
{
	if (isOutOfBounds(vid_scalemode))
		vid_scalemode = 0;
	// vid_scalefactor > 1 == forced linear scale
	return (vid_scalefactor > 1.0) ? true : vScaleTable[vid_scalemode].isLinear;
}

int ViewportScaledWidth(int width, int height)
{
	if (isOutOfBounds(vid_scalemode))
		vid_scalemode = 0;
	if (vid_cropaspect && height > 0)
		width = ((float)width/height > ActiveRatio(width, height)) ? (int)(height * ActiveRatio(width, height)) : width;
	return vScaleTable[vid_scalemode].GetScaledWidth((int)((float)width * vid_scalefactor));
}

int ViewportScaledHeight(int width, int height)
{
	if (isOutOfBounds(vid_scalemode))
		vid_scalemode = 0;
	if (vid_cropaspect && height > 0)
		height = ((float)width/height < ActiveRatio(width, height)) ? (int)(width / ActiveRatio(width, height)) : height;
	return vScaleTable[vid_scalemode].GetScaledHeight((int)((float)height * vid_scalefactor));
}

bool ViewportIsScaled43()
{
	if (isOutOfBounds(vid_scalemode))
		vid_scalemode = 0;
	return vScaleTable[vid_scalemode].isScaled43;
}
