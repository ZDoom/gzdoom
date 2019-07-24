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

#ifndef __VIDEOSCALE_H__
#define __VIDEOSCALE_H__
EXTERN_CVAR (Int, vid_scalemode)
bool ViewportLinearScale();
int ViewportScaledWidth(int width, int height);
int ViewportScaledHeight(int width, int height);
bool ViewportIsScaled43();
#endif //__VIDEOSCALE_H__