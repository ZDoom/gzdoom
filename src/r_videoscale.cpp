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

#define NUMSCALEMODES 11

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
		{ true,			false,		[](uint32_t Width)->uint32_t { return 320; },			[](uint32_t Height)->uint32_t { return 200; },				true	},	// 1  - 320x200
		{ true,			true,		[](uint32_t Width)->uint32_t { return 640; },			[](uint32_t Height)->uint32_t { return 400; },				true	},	// 2  - 640x400
		{ true,			true,		[](uint32_t Width)->uint32_t { return 1280; },			[](uint32_t Height)->uint32_t { return 800; },				true	},	// 3  - 1280x800		
		{ false,		false,		nullptr,												nullptr,													false	},	// 4
		{ false,		false,		nullptr,												nullptr,													false	},	// 5
		{ false,		false,		nullptr,												nullptr,													false	},	// 6
		{ false,		false,		nullptr,												nullptr,													false	},	// 7
		{ true,			false,		[](uint32_t Width)->uint32_t { return Width / 2; },		[](uint32_t Height)->uint32_t { return Height / 2; },		false	},	// 8  - Half-Res
		{ true,			true,		[](uint32_t Width)->uint32_t { return Width * 0.75; },	[](uint32_t Height)->uint32_t { return Height * 0.75; },	false	},	// 9  - Res * 0.75
		{ true,			true,		[](uint32_t Width)->uint32_t { return Width * 2; },		[](uint32_t Height)->uint32_t { return Height * 2; },		false	},	// 10 - SSAAx2
	};
}

CUSTOM_CVAR(Int, vid_scalemode, 0, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < 0 || self >= NUMSCALEMODES || vScaleTable[self].isValid == false)
	{
		self = 0;
	}
}

bool ViewportLinearScale()
{
	return vScaleTable[vid_scalemode].isLinear;
}

int ViewportScaledWidth(int width)
{
	return vScaleTable[vid_scalemode].GetScaledWidth(width);
}

int ViewportScaledHeight(int height)
{
	return vScaleTable[vid_scalemode].GetScaledHeight(height);
}

bool ViewportIsScaled43()
{
	return vScaleTable[vid_scalemode].isScaled43;
}


