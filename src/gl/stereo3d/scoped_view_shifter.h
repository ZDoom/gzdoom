// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2015 Christopher Bruns
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
/*
** scoped_view_shifter.h
** Stack-scoped class for temporarily changing camera viewpoint
** Used for stereoscopic 3D.
**
*/

#ifndef GL_STEREO3D_SCOPED_VIEW_SHIFTER_H_
#define GL_STEREO3D_SCOPED_VIEW_SHIFTER_H_

#include "basictypes.h"
#include "vectors.h"

/**
 * Temporarily shift
 */
class ScopedViewShifter
{
public:
    ScopedViewShifter(DVector3 &var, float dxyz[3]) // in meters
    {
        // save original values
        mVar = &var;
        cachedView = var;
        // modify values
        var += DVector3(dxyz[0], dxyz[1], dxyz[2]);
    }
    
    ~ScopedViewShifter()
    {
        // restore original values
        *mVar = cachedView;
    }

private:
    DVector3 *mVar;
    DVector3 cachedView;
};


#endif // GL_STEREO3D_SCOPED_VIEW_SHIFTER_H_
