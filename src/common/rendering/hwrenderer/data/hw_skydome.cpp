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
** for FSkyVertexBuffer::SkyVertex only:
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
#include "filesystem.h"
#include "cmdlib.h"
#include "bitmap.h"
#include "skyboxtexture.h"
#include "hw_material.h"
#include "hw_skydome.h"
#include "hw_renderstate.h"
#include "v_video.h"
#include "hwrenderer/data/buffers.h"
#include "version.h"

//-----------------------------------------------------------------------------
//
// Shamelessly lifted from Doomsday (written by Jaakko Keränen)
// also shamelessly lifted from ZDoomGL! ;)
//
//-----------------------------------------------------------------------------
CVAR(Float, skyoffset, 0.f, 0)	// for testing


struct SkyColor
{
	FTextureID Texture;
	std::pair<PalEntry, PalEntry> Colors;
};

static TArray<SkyColor> SkyColors;

std::pair<PalEntry, PalEntry>& R_GetSkyCapColor(FGameTexture* tex)
{
	for (auto& sky : SkyColors)
	{
		if (sky.Texture == tex->GetID()) return sky.Colors;
	}

	auto itex = tex->GetTexture();
	SkyColor sky;

	FBitmap bitmap = itex->GetBgraBitmap(nullptr);
	int w = bitmap.GetWidth();
	int h = bitmap.GetHeight();

	const uint32_t* buffer = (const uint32_t*)bitmap.GetPixels();
	if (buffer)
	{
		sky.Colors.first = averageColor((uint32_t*)buffer, w * min(30, h), 0);
		if (h > 30)
		{
			sky.Colors.second = averageColor(((uint32_t*)buffer) + (h - 30) * w, w * 30, 0);
		}
		else sky.Colors.second = sky.Colors.first;
	}
	sky.Texture = tex->GetID();
	SkyColors.Push(sky);

	return SkyColors.Last().Colors;
}



//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

FSkyVertexBuffer::FSkyVertexBuffer()
{
	CreateDome();
	mVertexBuffer = screen->CreateVertexBuffer();

	static const FVertexBufferAttribute format[] = {
		{ 0, VATTR_VERTEX, VFmt_Float3, (int)myoffsetof(FSkyVertex, x) },
		{ 0, VATTR_TEXCOORD, VFmt_Float2, (int)myoffsetof(FSkyVertex, u) },
		{ 0, VATTR_COLOR, VFmt_Byte4, (int)myoffsetof(FSkyVertex, color) },
		{ 0, VATTR_LIGHTMAP, VFmt_Float3, (int)myoffsetof(FSkyVertex, lu) },
	};
	mVertexBuffer->SetFormat(1, 4, sizeof(FSkyVertex), format);
	mVertexBuffer->SetData(mVertices.Size() * sizeof(FSkyVertex), &mVertices[0], BufferUsageType::Static);
}

FSkyVertexBuffer::~FSkyVertexBuffer()
{
	delete mVertexBuffer;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FSkyVertexBuffer::SkyVertexDoom(int r, int c, bool zflip)
{
	static const FAngle maxSideAngle = FAngle::fromDeg(60.f);
	static const float scale = 10000.;

	FAngle topAngle = FAngle::fromDeg((c / (float)mColumns * 360.f));
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

void FSkyVertexBuffer::SkyVertexBuild(int r, int c, bool zflip)
{
	static const FAngle maxSideAngle = FAngle::fromDeg(60.f);
	static const float scale = 10000.;

	FAngle topAngle = FAngle::fromDeg((c / (float)mColumns * 360.f));
	FVector2 pos = topAngle.ToVector(scale);
	float z = (!zflip) ? (mRows - r) * 4000.f : -(mRows - r) * 4000.f;

	FSkyVertex vert;

	vert.color = r == 0 ? 0xffffff : 0xffffffff;

	// And the texture coordinates.
	if (zflip) r = mRows * 2 - r;
	vert.u = 0.5f + (-c / (float)mColumns);
	vert.v = (r / (float)(2*mRows));

	// And finally the vertex.
	vert.x = pos.X;
	vert.y = z - 1.f;
	vert.z = pos.Y;

	mVertices.Push(vert);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FSkyVertexBuffer::CreateSkyHemisphereDoom(int hemi)
{
	int r, c;
	bool zflip = !!(hemi & SKYHEMI_LOWER);

	mPrimStartDoom.Push(mVertices.Size());

	for (c = 0; c < mColumns; c++)
	{
		SkyVertexDoom(1, c, zflip);
	}

	// The total number of triangles per hemisphere can be calculated
	// as follows: rows * columns * 2 + 2 (for the top cap).
	for (r = 0; r < mRows; r++)
	{
		mPrimStartDoom.Push(mVertices.Size());
		for (c = 0; c <= mColumns; c++)
		{
			SkyVertexDoom(r + zflip, c, zflip);
			SkyVertexDoom(r + 1 - zflip, c, zflip);
		}
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FSkyVertexBuffer::CreateSkyHemisphereBuild(int hemi)
{
	int r, c;
	bool zflip = !!(hemi & SKYHEMI_LOWER);

	mPrimStartBuild.Push(mVertices.Size());

	for (c = 0; c < mColumns; c++)
	{
		SkyVertexBuild(1, c, zflip);
	}

	// The total number of triangles per hemisphere can be calculated
	// as follows: rows * columns * 2 + 2 (for the top cap).
	for (r = 0; r < mRows; r++)
	{
		mPrimStartBuild.Push(mVertices.Size());
		for (c = 0; c <= mColumns; c++)
		{
			SkyVertexBuild(r + zflip, c, zflip);
			SkyVertexBuild(r + 1 - zflip, c, zflip);
		}
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FSkyVertexBuffer::CreateDome()
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
	CreateSkyHemisphereDoom(SKYHEMI_UPPER);
	CreateSkyHemisphereDoom(SKYHEMI_LOWER);
	mPrimStartDoom.Push(mVertices.Size());

	CreateSkyHemisphereBuild(SKYHEMI_UPPER);
	CreateSkyHemisphereBuild(SKYHEMI_LOWER);
	mPrimStartBuild.Push(mVertices.Size());

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

void FSkyVertexBuffer::SetupMatrices(FGameTexture *tex, float x_offset, float y_offset, bool mirror, int mode, VSMatrix &modelMatrix, VSMatrix &textureMatrix, bool tiled, float xscale, float yscale)
{
	float texw = tex->GetDisplayWidth();
	float texh = tex->GetDisplayHeight();

	modelMatrix.loadIdentity();
	modelMatrix.rotate(-180.0f + x_offset, 0.f, 1.f, 0.f);

	if (xscale == 0) xscale = texw < 1024.f ? floorf(1024.f / float(texw)) : 1.f;
	auto texskyoffset = tex->GetSkyOffset() + skyoffset;
	if (yscale == 0)
	{
		if (texh <= 128 && tiled)
		{
			modelMatrix.translate(0.f, (-40 + texskyoffset) * skyoffsetfactor, 0.f);
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
			yscale = 1.f;
		}
		else if (texh <= 240)
		{
			modelMatrix.translate(0.f, (200 - texh + texskyoffset) * skyoffsetfactor, 0.f);
			modelMatrix.scale(1.f, 1.f + ((texh - 200.f) / 200.f) * 1.17f, 1.f);
			yscale = 1.f;
		}
		else
		{
			modelMatrix.translate(0.f, (-40 + texskyoffset) * skyoffsetfactor, 0.f);
			modelMatrix.scale(1.f, 1.2f * 1.17f, 1.f);
			yscale = 240.f / texh;
		}
	}
	else
	{
		modelMatrix.translate(0.f, (-40 + texskyoffset) * skyoffsetfactor, 0.f);
		modelMatrix.scale(1.f, 0.8f * 1.17f, 1.f);
	}
	textureMatrix.loadIdentity();
	textureMatrix.scale(mirror ? -xscale : xscale, yscale, 1.f);
	textureMatrix.translate(1.f, y_offset / texh, 1.f);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FSkyVertexBuffer::RenderRow(FRenderState& state, EDrawType prim, int row, TArray<unsigned int>& mPrimStart, bool apply)
{
	state.Draw(prim, mPrimStart[row], mPrimStart[row + 1] - mPrimStart[row]);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FSkyVertexBuffer::DoRenderDome(FRenderState& state, FGameTexture* tex, int mode, bool which, PalEntry color)
{
	auto& primStart = which ? mPrimStartBuild : mPrimStartDoom;
	if (tex && tex->isValid())
	{
		state.SetMaterial(tex, UF_Texture, 0, CLAMP_NONE, 0, -1);
		state.EnableModelMatrix(true);
		state.EnableTextureMatrix(true);
	}

	int rc = mRows + 1;

	// The caps only get drawn for the main layer but not for the overlay.
	if (mode == FSkyVertexBuffer::SKYMODE_MAINLAYER && tex != nullptr)
	{
		auto col = R_GetSkyCapColor(tex);

		col.first.r = col.first.r * color.r / 255;
		col.first.g = col.first.g * color.g / 255;
		col.first.b = col.first.b * color.b / 255;
		col.second.r = col.second.r * color.r / 255;
		col.second.g = col.second.g * color.g / 255;
		col.second.b = col.second.b * color.b / 255;

		state.SetObjectColor(col.first);
		state.EnableTexture(false);
		RenderRow(state, DT_TriangleFan, 0, primStart);

		state.SetObjectColor(col.second);
		RenderRow(state, DT_TriangleFan, rc, primStart);
		state.EnableTexture(true);
	}
	state.SetObjectColor(0xffffffff);
	for (int i = 1; i <= mRows; i++)
	{
		RenderRow(state, DT_TriangleStrip, i, primStart, i == 1);
		RenderRow(state, DT_TriangleStrip, rc + i, primStart, false);
	}

	state.EnableTextureMatrix(false);
	state.EnableModelMatrix(false);
}


//-----------------------------------------------------------------------------
//
// This is only for Doom-style skies.
//
//-----------------------------------------------------------------------------

void FSkyVertexBuffer::RenderDome(FRenderState& state, FGameTexture* tex, float x_offset, float y_offset, bool mirror, int mode, bool tiled, float xscale, float yscale, PalEntry color)
{
	if (tex)
	{
		SetupMatrices(tex, x_offset, y_offset, mirror, mode, state.mModelMatrix, state.mTextureMatrix, tiled, xscale, yscale);
	}
	DoRenderDome(state, tex, mode, false, color);
}


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FSkyVertexBuffer::RenderBox(FRenderState& state, FSkyBox* tex, float x_offset, bool sky2, float stretch, const FVector3& skyrotatevector, const FVector3& skyrotatevector2, PalEntry color)
{
	int faces;

	state.SetObjectColor(color);
	state.EnableModelMatrix(true);
	state.mModelMatrix.loadIdentity();
	state.mModelMatrix.scale(1, 1 / stretch, 1); // Undo the map's vertical scaling as skyboxes are true cubes.

	if (!sky2)
		state.mModelMatrix.rotate(-180.0f + x_offset, skyrotatevector.X, skyrotatevector.Z, skyrotatevector.Y);
	else
		state.mModelMatrix.rotate(-180.0f + x_offset, skyrotatevector2.X, skyrotatevector2.Z, skyrotatevector2.Y);

	if (tex->GetSkyFace(5))
	{
		faces = 4;

		// north
		state.SetMaterial(tex->GetSkyFace(0), UF_Texture, 0, CLAMP_XY, 0, -1);
		state.Draw(DT_TriangleStrip, FaceStart(0), 4);

		// east
		state.SetMaterial(tex->GetSkyFace(1), UF_Texture, 0, CLAMP_XY, 0, -1);
		state.Draw(DT_TriangleStrip, FaceStart(1), 4);

		// south
		state.SetMaterial(tex->GetSkyFace(2), UF_Texture, 0, CLAMP_XY, 0, -1);
		state.Draw(DT_TriangleStrip, FaceStart(2), 4);

		// west
		state.SetMaterial(tex->GetSkyFace(3), UF_Texture, 0, CLAMP_XY, 0, -1);
		state.Draw(DT_TriangleStrip, FaceStart(3), 4);
	}
	else
	{
		faces = 1;
		state.SetMaterial(tex->GetSkyFace(0), UF_Texture, 0, CLAMP_XY, 0, -1);
		state.Draw(DT_TriangleStrip, FaceStart(-1), 10);
	}

	// top
	state.SetMaterial(tex->GetSkyFace(faces), UF_Texture, 0, CLAMP_XY, 0, -1);
	state.Draw(DT_TriangleStrip, FaceStart(tex->GetSkyFlip() ? 6 : 5), 4);

	// bottom
	state.SetMaterial(tex->GetSkyFace(faces + 1), UF_Texture, 0, CLAMP_XY, 0, -1);
	state.Draw(DT_TriangleStrip, FaceStart(4), 4);

	state.EnableModelMatrix(false);
	state.SetObjectColor(0xffffffff);
}

