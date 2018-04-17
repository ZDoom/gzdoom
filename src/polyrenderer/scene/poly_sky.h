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

#pragma once

#include "polyrenderer/drawers/poly_triangle.h"

class PolySkySetup
{
public:
	void Update();

	bool operator==(const PolySkySetup &that) const { return memcmp(this, &that, sizeof(PolySkySetup)) == 0; }
	bool operator!=(const PolySkySetup &that) const { return memcmp(this, &that, sizeof(PolySkySetup)) != 0; }

	FTexture *frontskytex = nullptr;
	FTexture *backskytex = nullptr;
	bool skyflip = 0;
	int frontpos = 0;
	int backpos = 0;
	fixed_t frontcyl = 0;
	fixed_t backcyl = 0;
	double skymid = 0.0;
	angle_t skyangle = 0;
};

class PolySkyDome
{
public:
	PolySkyDome();
	void Render(PolyRenderThread *thread, const Mat4f &worldToView, const Mat4f &worldToClip);

private:
	TArray<FVector2> mInitialUV;
	TArray<TriVertex> mVertices;
	TArray<unsigned int> mPrimStart;
	int mRows, mColumns;

	void SkyVertex(int r, int c, bool yflip);
	void CreateSkyHemisphere(bool zflip);
	void CreateDome();
	void RenderRow(PolyRenderThread *thread, PolyDrawArgs &args, int row, uint32_t capcolor, uint8_t capcolorindex);
	void RenderCapColorRow(PolyRenderThread *thread, PolyDrawArgs &args, FTexture *skytex, int row, bool bottomCap);

	TriVertex SetVertexXYZ(float xx, float yy, float zz, float uu = 0, float vv = 0);

	Mat4f GLSkyMath();

	PolySkySetup mCurrentSetup;
};
