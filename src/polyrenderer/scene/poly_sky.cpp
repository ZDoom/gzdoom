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
#include "poly_sky.h"
#include "poly_portal.h"
#include "r_sky.h" // for skyflatnum
#include "g_levellocals.h"
#include "polyrenderer/scene/poly_light.h"

PolySkyDome::PolySkyDome()
{
	CreateDome();
}

void PolySkyDome::Render(const TriMatrix &worldToClip)
{
	PolySkySetup frameSetup;
	frameSetup.Update();

	if (frameSetup != mCurrentSetup)
	{
		double frontTexWidth = (double)frameSetup.frontskytex->GetWidth();
		float scaleFrontU = (float)(frameSetup.frontcyl / frontTexWidth);
		if (frameSetup.skyflip)
			scaleFrontU = -scaleFrontU;

		float baseOffset = (float)(frameSetup.skyangle / 65536.0 / frontTexWidth);
		float offsetFrontU = baseOffset * scaleFrontU + (float)(frameSetup.frontpos / 65536.0 / frontTexWidth);

		float scaleFrontV = (float)(frameSetup.frontskytex->Scale.Y * 1.6);
		float offsetFrontV;

		// BTSX
		/*{
			offsetFrontU += 0.5f;
			offsetFrontV = (float)((28.0f - frameSetup.skymid) / frameSetup.frontskytex->GetHeight());
		}*/
		// E1M1
		{
			offsetFrontV = (float)((28.0f + frameSetup.skymid) / frameSetup.frontskytex->GetHeight());
		}

		unsigned int count = mVertices.Size();
		for (unsigned int i = 0; i < count; i++)
		{
			mVertices[i].u = offsetFrontU + mInitialUV[i].X * scaleFrontU;
			mVertices[i].v = offsetFrontV + mInitialUV[i].Y * scaleFrontV;
		}

		mCurrentSetup = frameSetup;
	}

	const auto &viewpoint = PolyRenderer::Instance()->Viewpoint;
	TriMatrix objectToWorld = TriMatrix::translate((float)viewpoint.Pos.X, (float)viewpoint.Pos.Y, (float)viewpoint.Pos.Z);
	objectToClip = worldToClip * objectToWorld;

	int rc = mRows + 1;

	PolyDrawArgs args;
	args.SetLight(&NormalLight, 255, PolyRenderer::Instance()->Light.WallGlobVis(false), true);
	args.SetSubsectorDepth(RenderPolyScene::SkySubsectorDepth);
	args.SetTransform(&objectToClip);
	args.SetStencilTestValue(255);
	args.SetWriteStencil(true, 1);
	args.SetClipPlane(PolyClipPlane(0.0f, 0.0f, 0.0f, 1.0f));

	RenderCapColorRow(args, mCurrentSetup.frontskytex, 0, false);
	RenderCapColorRow(args, mCurrentSetup.frontskytex, rc, true);

	args.SetTexture(mCurrentSetup.frontskytex);

	uint32_t topcapcolor = mCurrentSetup.frontskytex->GetSkyCapColor(false);
	uint32_t bottomcapcolor = mCurrentSetup.frontskytex->GetSkyCapColor(true);
	uint8_t topcapindex = RGB256k.All[((RPART(topcapcolor) >> 2) << 12) | ((GPART(topcapcolor) >> 2) << 6) | (BPART(topcapcolor) >> 2)];
	uint8_t bottomcapindex = RGB256k.All[((RPART(bottomcapcolor) >> 2) << 12) | ((GPART(bottomcapcolor) >> 2) << 6) | (BPART(bottomcapcolor) >> 2)];

	for (int i = 1; i <= mRows; i++)
	{
		RenderRow(args, i, topcapcolor, topcapindex);
		RenderRow(args, rc + i, bottomcapcolor, bottomcapindex);
	}
}

void PolySkyDome::RenderRow(PolyDrawArgs &args, int row, uint32_t capcolor, uint8_t capcolorindex)
{
	args.SetFaceCullCCW(false);
	args.SetColor(capcolor, capcolorindex);
	args.SetStyle(TriBlendMode::Skycap);
	args.DrawArray(&mVertices[mPrimStart[row]], mPrimStart[row + 1] - mPrimStart[row], PolyDrawMode::TriangleStrip);
}

void PolySkyDome::RenderCapColorRow(PolyDrawArgs &args, FTexture *skytex, int row, bool bottomCap)
{
	uint32_t solid = skytex->GetSkyCapColor(bottomCap);
	uint8_t palsolid = RGB32k.RGB[(RPART(solid) >> 3)][(GPART(solid) >> 3)][(BPART(solid) >> 3)];

	args.SetFaceCullCCW(bottomCap);
	args.SetColor(solid, palsolid);
	args.SetStyle(TriBlendMode::FillOpaque);
	args.DrawArray(&mVertices[mPrimStart[row]], mPrimStart[row + 1] - mPrimStart[row], PolyDrawMode::TriangleFan);
}

void PolySkyDome::CreateDome()
{
	mColumns = 16;// 128;
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

TriVertex PolySkyDome::SetVertexXYZ(float xx, float yy, float zz, float uu, float vv)
{
	TriVertex v;
	v.x = xx;
	v.y = zz;
	v.z = yy;
	v.w = 1.0f;
	v.u = uu;
	v.v = vv;
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
	vert = SetVertexXYZ(-pos.X, z - 1.f, pos.Y, u, v - 0.5f);
	mVertices.Push(vert);
	mInitialUV.Push({ vert.u, vert.v });
}

/////////////////////////////////////////////////////////////////////////////

void PolySkySetup::Update()
{
	FTextureID sky1tex, sky2tex;
	double frontdpos = 0, backdpos = 0;

	if ((level.flags & LEVEL_SWAPSKIES) && !(level.flags & LEVEL_DOUBLESKY))
	{
		sky1tex = sky2texture;
	}
	else
	{
		sky1tex = sky1texture;
	}
	sky2tex = sky2texture;
	skymid = skytexturemid;
	skyangle = 0;

	int sectorSky = 0;// sector->sky;

	if (!(sectorSky & PL_SKYFLAT))
	{	// use sky1
	sky1:
		frontskytex = TexMan(sky1tex, true);
		if (level.flags & LEVEL_DOUBLESKY)
			backskytex = TexMan(sky2tex, true);
		else
			backskytex = nullptr;
		skyflip = false;
		frontdpos = sky1pos;
		backdpos = sky2pos;
		frontcyl = sky1cyl;
		backcyl = sky2cyl;
	}
	else if (sectorSky == PL_SKYFLAT)
	{	// use sky2
		frontskytex = TexMan(sky2tex, true);
		backskytex = nullptr;
		frontcyl = sky2cyl;
		skyflip = false;
		frontdpos = sky2pos;
	}
	else
	{	// MBF's linedef-controlled skies
		// Sky Linedef
		const line_t *l = &level.lines[(sectorSky & ~PL_SKYFLAT) - 1];

		// Sky transferred from first sidedef
		const side_t *s = l->sidedef[0];
		int pos;

		// Texture comes from upper texture of reference sidedef
		// [RH] If swapping skies, then use the lower sidedef
		if (level.flags & LEVEL_SWAPSKIES && s->GetTexture(side_t::bottom).isValid())
		{
			pos = side_t::bottom;
		}
		else
		{
			pos = side_t::top;
		}

		frontskytex = TexMan(s->GetTexture(pos), true);
		if (frontskytex == nullptr || frontskytex->UseType == FTexture::TEX_Null)
		{ // [RH] The blank texture: Use normal sky instead.
			goto sky1;
		}
		backskytex = nullptr;

		// Horizontal offset is turned into an angle offset,
		// to allow sky rotation as well as careful positioning.
		// However, the offset is scaled very small, so that it
		// allows a long-period of sky rotation.
		skyangle += FLOAT2FIXED(s->GetTextureXOffset(pos));

		// Vertical offset allows careful sky positioning.
		skymid = s->GetTextureYOffset(pos);

		// We sometimes flip the picture horizontally.
		//
		// Doom always flipped the picture, so we make it optional,
		// to make it easier to use the new feature, while to still
		// allow old sky textures to be used.
		skyflip = l->args[2] ? false : true;

		int frontxscale = int(frontskytex->Scale.X * 1024);
		frontcyl = MAX(frontskytex->GetWidth(), frontxscale);
		if (skystretch)
		{
			skymid = skymid * frontskytex->GetScaledHeightDouble() / SKYSTRETCH_HEIGHT;
		}
	}

	frontpos = int(fmod(frontdpos, sky1cyl * 65536.0));
	if (backskytex != nullptr)
	{
		backpos = int(fmod(backdpos, sky2cyl * 65536.0));
	}
}
