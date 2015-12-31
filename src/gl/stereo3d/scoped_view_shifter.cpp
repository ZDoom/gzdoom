/*
** scoped_view_shifter.cpp
** Stack-scoped class for temporarily changing player viewpoint global variables viewx, viewy, viewz.
** Used for stereoscopic 3D.
**
**---------------------------------------------------------------------------
** Copyright 2015 Christopher Bruns
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
**
*/

#include "scoped_view_shifter.h"
#include "r_utility.h"

namespace s3d {

ScopedViewShifter::ScopedViewShifter(float dxyz[3]) // in meters
{
	// save original values
	cachedViewx = viewx;
	cachedViewy = viewy;
	cachedViewz = viewz;
	// modify values
	float fViewx = FIXED2FLOAT(viewx) - dxyz[0];
	float fViewy = FIXED2FLOAT(viewy) + dxyz[1];
	float fViewz = FIXED2FLOAT(viewz) + dxyz[2];
	viewx = FLOAT2FIXED(fViewx);
	viewy = FLOAT2FIXED(fViewy);
	viewz = FLOAT2FIXED(fViewz);
}

ScopedViewShifter::~ScopedViewShifter()
{
	// restore original values
	viewx = cachedViewx;
	viewy = cachedViewy;
	viewz = cachedViewz;
}

}