// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2003-2018 Christoph Oelckers
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
**
** Draws the sky.  Loosely based on the JDoom sky and the ZDoomGL 0.66.2 sky.
**
** for FSkyDomeCreator::SkyVertex only:
**---------------------------------------------------------------------------
** Copyright 2003 Tim Stump
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
*/
#include "doomtype.h"
#include "g_level.h"
#include "w_wad.h"
#include "r_state.h"
#include "r_utility.h"
#include "g_levellocals.h"
#include "r_sky.h"

#include "textures/skyboxtexture.h"
#include "hwrenderer/textures/hw_material.h"
#include "hw_skydome.h"

//-----------------------------------------------------------------------------
//
// Shamelessly lifted from Doomsday (written by Jaakko Keränen)
// also shamelessly lifted from ZDoomGL! ;)
//
//-----------------------------------------------------------------------------
EXTERN_CVAR(Float, skyoffset)

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

FSkyDomeCreator::FSkyDomeCreator()
{
	CreateDome();
}

FSkyDomeCreator::~FSkyDomeCreator()
{
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FSkyDomeCreator::SkyVertex(int r, int c, bool zflip)
{
	static const FAngle maxSideAngle = 60.f;
	static const float scale = 10000.;

	FAngle topAngle = (c / (float)mColumns * 360.f);
	FAngle sideAngle = maxSideAngle * float(mRows - r) / float(mRows);
	float height = sideAngle.Sin();
	float realRadius = scale * sideAngle.Cos();
	FVector2 pos = topAngle.ToVector(realRadius);
	float z = (!zflip) ? scale * height : -scale * height;

	FSkyVertex vert;

	vert.color = r == 0 ? 0xffffff : 0xffffffff;

	// And the texture coordinates.
	if (!zflip)	// Flipped Y is for the lower hemisphere.
	{
		vert.u = (-c / (float)mColumns);
		vert.v = (r / (float)mRows);
	}
	else
	{
		vert.u = (-c / (float)mColumns);
		vert.v = 1.0f + ((mRows - r) / (float)mRows);
	}

	if (r != 4) z += 300;
	// And finally the vertex.
	vert.x = -pos.X;	// Doom mirrors the sky vertically!
	vert.y = z - 1.f;
	vert.z = pos.Y;

	mVertices.Push(vert);
}


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FSkyDomeCreator::CreateSkyHemisphere(int hemi)
{
	int r, c;
	bool zflip = !!(hemi & SKYHEMI_LOWER);

	mPrimStart.Push(mVertices.Size());

	for (c = 0; c < mColumns; c++)
	{
		SkyVertex(1, c, zflip);
	}

	// The total number of triangles per hemisphere can be calculated
	// as follows: rows * columns * 2 + 2 (for the top cap).
	for (r = 0; r < mRows; r++)
	{
		mPrimStart.Push(mVertices.Size());
		for (c = 0; c <= mColumns; c++)
		{
			SkyVertex(r + zflip, c, zflip);
			SkyVertex(r + 1 - zflip, c, zflip);
		}
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FSkyDomeCreator::CreateDome()
{
	// the first thing we put into the buffer is the fog layer object which is just 4 triangles around the viewpoint.

	mVertices.Reserve(12);
	mVertices[0].Set(1.0f, 1.0f, -1.0f);
	mVertices[1].Set(1.0f, -1.0f, -1.0f);
	mVertices[2].Set(-1.0f, 0.0f, -1.0f);

	mVertices[3].Set(1.0f, 1.0f, -1.0f);
	mVertices[4].Set(1.0f, -1.0f, -1.0f);
	mVertices[5].Set(0.0f, 0.0f, 1.0f);

	mVertices[6].Set(-1.0f, 0.0f, -1.0f);
	mVertices[7].Set(1.0f, 1.0f, -1.0f);
	mVertices[8].Set(0.0f, 0.0f, 1.0f);

	mVertices[9].Set(1.0f, -1.0f, -1.0f);
	mVertices[10].Set(-1.0f, 0.0f, -1.0f);
	mVertices[11].Set(0.0f, 0.0f, 1.0f);

	mColumns = 128;
	mRows = 4;
	CreateSkyHemisphere(SKYHEMI_UPPER);
	CreateSkyHemisphere(SKYHEMI_LOWER);
	mPrimStart.Push(mVertices.Size());

	mSideStart = mVertices.Size();
	mFaceStart[0] = mSideStart + 10;
	mFaceStart[1] = mFaceStart[0] + 4;
	mFaceStart[2] = mFaceStart[1] + 4;
	mFaceStart[3] = mFaceStart[2] + 4;
	mFaceStart[4] = mFaceStart[3] + 4;
	mFaceStart[5] = mFaceStart[4] + 4;
	mFaceStart[6] = mFaceStart[5] + 4;
	mVertices.Reserve(10 + 7*4);
	FSkyVertex *ptr = &mVertices[mSideStart];

	// all sides
	ptr[0].SetXYZ(128.f, 128.f, -128.f, 0, 0);
	ptr[1].SetXYZ(128.f, -128.f, -128.f, 0, 1);
	ptr[2].SetXYZ(-128.f, 128.f, -128.f, 0.25f, 0);
	ptr[3].SetXYZ(-128.f, -128.f, -128.f, 0.25f, 1);
	ptr[4].SetXYZ(-128.f, 128.f, 128.f, 0.5f, 0);
	ptr[5].SetXYZ(-128.f, -128.f, 128.f, 0.5f, 1);
	ptr[6].SetXYZ(128.f, 128.f, 128.f, 0.75f, 0);
	ptr[7].SetXYZ(128.f, -128.f, 128.f, 0.75f, 1);
	ptr[8].SetXYZ(128.f, 128.f, -128.f, 1, 0);
	ptr[9].SetXYZ(128.f, -128.f, -128.f, 1, 1);

	// north face
	ptr[10].SetXYZ(128.f, 128.f, -128.f, 0, 0);
	ptr[11].SetXYZ(-128.f, 128.f, -128.f, 1, 0);
	ptr[12].SetXYZ(128.f, -128.f, -128.f, 0, 1);
	ptr[13].SetXYZ(-128.f, -128.f, -128.f, 1, 1);

	// east face
	ptr[14].SetXYZ(-128.f, 128.f, -128.f, 0, 0);
	ptr[15].SetXYZ(-128.f, 128.f, 128.f, 1, 0);
	ptr[16].SetXYZ(-128.f, -128.f, -128.f, 0, 1);
	ptr[17].SetXYZ(-128.f, -128.f, 128.f, 1, 1);

	// south face
	ptr[18].SetXYZ(-128.f, 128.f, 128.f, 0, 0);
	ptr[19].SetXYZ(128.f, 128.f, 128.f, 1, 0);
	ptr[20].SetXYZ(-128.f, -128.f, 128.f, 0, 1);
	ptr[21].SetXYZ(128.f, -128.f, 128.f, 1, 1);

	// west face
	ptr[22].SetXYZ(128.f, 128.f, 128.f, 0, 0);
	ptr[23].SetXYZ(128.f, 128.f, -128.f, 1, 0);
	ptr[24].SetXYZ(128.f, -128.f, 128.f, 0, 1);
	ptr[25].SetXYZ(128.f, -128.f, -128.f, 1, 1);

	// bottom face
	ptr[26].SetXYZ(128.f, -128.f, -128.f, 0, 0);
	ptr[27].SetXYZ(-128.f, -128.f, -128.f, 1, 0);
	ptr[28].SetXYZ(128.f, -128.f, 128.f, 0, 1);
	ptr[29].SetXYZ(-128.f, -128.f, 128.f, 1, 1);

	// top face
	ptr[30].SetXYZ(128.f, 128.f, -128.f, 0, 0);
	ptr[31].SetXYZ(-128.f, 128.f, -128.f, 1, 0);
	ptr[32].SetXYZ(128.f, 128.f, 128.f, 0, 1);
	ptr[33].SetXYZ(-128.f, 128.f, 128.f, 1, 1);

	// top face flipped
	ptr[34].SetXYZ(128.f, 128.f, -128.f, 0, 1);
	ptr[35].SetXYZ(-128.f, 128.f, -128.f, 1, 1);
	ptr[36].SetXYZ(128.f, 128.f, 128.f, 0, 0);
	ptr[37].SetXYZ(-128.f, 128.f, 128.f, 1, 0);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FSkyDomeCreator::SetupMatrices(FMaterial *tex, float x_offset, float y_offset, bool mirror, int mode, VSMatrix &modelMatrix, VSMatrix &textureMatrix)
{
	int texw = tex->TextureWidth();
	int texh = tex->TextureHeight();

	modelMatrix.loadIdentity();
	modelMatrix.rotate(-180.0f + x_offset, 0.f, 1.f, 0.f);

	float xscale = texw < 1024.f ? floor(1024.f / float(texw)) : 1.f;
	float yscale = 1.f;
	if (texh <= 128 && (level.flags & LEVEL_FORCETILEDSKY))
	{
		modelMatrix.translate(0.f, (-40 + tex->tex->SkyOffset + skyoffset)*skyoffsetfactor, 0.f);
		modelMatrix.scale(1.f, 1.2f * 1.17f, 1.f);
		yscale = 240.f / texh;
	}
	else if (texh < 128)
	{
		// smaller sky textures must be tiled. We restrict it to 128 sky pixels, though
		modelMatrix.translate(0.f, -1250.f, 0.f);
		modelMatrix.scale(1.f, 128 / 230.f, 1.f);
		yscale = float(128 / texh);	// intentionally left as integer.
	}
	else if (texh < 200)
	{
		modelMatrix.translate(0.f, -1250.f, 0.f);
		modelMatrix.scale(1.f, texh / 230.f, 1.f);
	}
	else if (texh <= 240)
	{
		modelMatrix.translate(0.f, (200 - texh + tex->tex->SkyOffset + skyoffset)*skyoffsetfactor, 0.f);
		modelMatrix.scale(1.f, 1.f + ((texh - 200.f) / 200.f) * 1.17f, 1.f);
	}
	else
	{
		modelMatrix.translate(0.f, (-40 + tex->tex->SkyOffset + skyoffset)*skyoffsetfactor, 0.f);
		modelMatrix.scale(1.f, 1.2f * 1.17f, 1.f);
		yscale = 240.f / texh;
	}
	textureMatrix.loadIdentity();
	textureMatrix.scale(mirror ? -xscale : xscale, yscale, 1.f);
	textureMatrix.translate(1.f, y_offset / texh, 1.f);
}