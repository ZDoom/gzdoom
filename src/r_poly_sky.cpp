/*
**  Sky dome rendering
**  Copyright(C) 2003-2016 Christoph Oelckers
**  All rights reserved.
**
**  This program is free software: you can redistribute it and/or modify
**  it under the terms of the GNU Lesser General Public License as published by
**  the Free Software Foundation, either version 3 of the License, or
**  (at your option) any later version.
**
**  This program is distributed in the hope that it will be useful,
**  but WITHOUT ANY WARRANTY; without even the implied warranty of
**  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
**  GNU Lesser General Public License for more details.
**
**  You should have received a copy of the GNU Lesser General Public License
**  along with this program.  If not, see http:**www.gnu.org/licenses/
**
**  Loosely based on the JDoom sky and the ZDoomGL 0.66.2 sky.
*/

#include <stdlib.h>
#include "templates.h"
#include "doomdef.h"
#include "sbar.h"
#include "r_data/r_translate.h"
#include "r_poly_sky.h"
#include "r_poly_portal.h"
#include "r_sky.h" // for skyflatnum

PolySkyDome::PolySkyDome()
{
	CreateDome();
}

void PolySkyDome::Render(const TriMatrix &worldToClip)
{
	FTextureID sky1tex, sky2tex;
	if ((level.flags & LEVEL_SWAPSKIES) && !(level.flags & LEVEL_DOUBLESKY))
		sky1tex = sky2texture;
	else
		sky1tex = sky1texture;
	sky2tex = sky2texture;

	FTexture *frontskytex = TexMan(sky1tex, true);
	FTexture *backskytex = nullptr;
	if (level.flags & LEVEL_DOUBLESKY)
		backskytex = TexMan(sky2tex, true);

	TriMatrix objectToWorld = TriMatrix::translate((float)ViewPos.X, (float)ViewPos.Y, (float)ViewPos.Z);
	objectToClip = worldToClip * objectToWorld;

	TriUniforms uniforms;
	uniforms.light = 256;
	uniforms.flags = 0;
	uniforms.subsectorDepth = RenderPolyPortal::SkySubsectorDepth;

	int rc = mRows + 1;

	PolyDrawArgs args;
	args.uniforms = uniforms;
	args.objectToClip = &objectToClip;
	args.stenciltestvalue = 255;
	args.stencilwritevalue = 1;
	args.SetTexture(frontskytex);
	args.SetColormap(&NormalLight);

	RenderCapColorRow(args, frontskytex, 0, false);
	RenderCapColorRow(args, frontskytex, rc, true);

	for (int i = 1; i <= mRows; i++)
	{
		RenderRow(args, i);
		RenderRow(args, rc + i);
	}
}

void PolySkyDome::RenderRow(PolyDrawArgs &args, int row)
{
	args.vinput = &mVertices[mPrimStart[row]];
	args.vcount = mPrimStart[row + 1] - mPrimStart[row];
	args.mode = TriangleDrawMode::Strip;
	args.ccw = false;
	PolyTriangleDrawer::draw(args, TriDrawVariant::DrawNormal, TriBlendMode::Copy);
}

void PolySkyDome::RenderCapColorRow(PolyDrawArgs &args, FTexture *skytex, int row, bool bottomCap)
{
	uint32_t solid = skytex->GetSkyCapColor(bottomCap);
	if (!r_swtruecolor)
		solid = RGB32k.RGB[(RPART(solid) >> 3)][(GPART(solid) >> 3)][(BPART(solid) >> 3)];

	args.vinput = &mVertices[mPrimStart[row]];
	args.vcount = mPrimStart[row + 1] - mPrimStart[row];
	args.mode = TriangleDrawMode::Fan;
	args.ccw = bottomCap;
	args.uniforms.color = solid;
	PolyTriangleDrawer::draw(args, TriDrawVariant::FillNormal, TriBlendMode::Copy);
}

void PolySkyDome::CreateDome()
{
	mColumns = 128;
	mRows = 4;
	CreateSkyHemisphere(false);
	CreateSkyHemisphere(true);
	mPrimStart.Push(mVertices.Size());
}

void PolySkyDome::CreateSkyHemisphere(bool zflip)
{
	int r, c;

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

TriVertex PolySkyDome::SetVertex(float xx, float yy, float zz, float uu, float vv)
{
	TriVertex v;
	v.x = xx;
	v.y = yy;
	v.z = zz;
	v.w = 1.0f;
	v.varying[0] = uu;
	v.varying[1] = vv;
	return v;
}

TriVertex PolySkyDome::SetVertexXYZ(float xx, float yy, float zz, float uu, float vv)
{
	TriVertex v;
	v.x = xx;
	v.y = zz;
	v.z = yy;
	v.w = 1.0f;
	v.varying[0] = uu;
	v.varying[1] = vv;
	return v;
}

void PolySkyDome::SkyVertex(int r, int c, bool zflip)
{
	static const FAngle maxSideAngle = 60.f;
	static const float scale = 10000.;

	FAngle topAngle = (c / (float)mColumns * 360.f);
	FAngle sideAngle = maxSideAngle * (float)(mRows - r) / (float)mRows;
	float height = sideAngle.Sin();
	float realRadius = scale * sideAngle.Cos();
	FVector2 pos = topAngle.ToVector(realRadius);
	float z = (!zflip) ? scale * height : -scale * height;

	float u, v;
	//uint32_t color = r == 0 ? 0xffffff : 0xffffffff;

	// And the texture coordinates.
	if (!zflip)	// Flipped Y is for the lower hemisphere.
	{
		u = (-c / (float)mColumns);
		v = (r / (float)mRows);
	}
	else
	{
		u = (-c / (float)mColumns);
		v = 1.0f + ((mRows - r) / (float)mRows);
	}

	if (r != 4) z += 300;

	// And finally the vertex.
	TriVertex vert;
	vert = SetVertexXYZ(-pos.X, z - 1.f, pos.Y, u * 4.0f, v * 1.2f + 0.5f/*, color*/);
	mVertices.Push(vert);
}
