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

CUSTOM_CVAR (Int, vid_scalemode, 0, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0 || self > 6)
	{
		self = 0;
	}
}

bool ViewportLinearScale()
{
	switch(vid_scalemode)
	{
	default: return false;
	case 4:
	case 5:
	case 6: return true;
	}
}

int ViewportScaledWidth(int width)
{
	switch (vid_scalemode)
	{
	default:
	case 0: return width;
	case 1: return 320;
	case 2: return 640;
	case 3: return (int)roundf(width * 0.5f);
	case 4: return (int)roundf(width * 0.75f);
	case 5: return width * 2;
	case 6: return 1280;
	}
}

int ViewportScaledHeight(int height)
{
	switch (vid_scalemode)
	{
	default:
	case 0: return height;
	case 1: return 200;
	case 2: return 400;
	case 3: return (int)roundf(height * 0.5f);
	case 4: return (int)roundf(height * 0.75f);
	case 5: return height * 2;
	case 6: return 800;
	}
}

bool ViewportIsScaled43()
{
	switch (vid_scalemode)
	{
	default: return false;
	case 1:
	case 2:
	case 6: return true;
	}
}

