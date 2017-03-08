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

class PolySkyDome
{
public:
	PolySkyDome();
	void Render(const TriMatrix &worldToClip);

private:
	TArray<TriVertex> mVertices;
	TArray<unsigned int> mPrimStart;
	int mRows, mColumns;
	TriMatrix objectToClip;

	void SkyVertex(int r, int c, bool yflip);
	void CreateSkyHemisphere(bool zflip);
	void CreateDome();
	void RenderRow(PolyDrawArgs &args, int row, uint32_t capcolor);
	void RenderCapColorRow(PolyDrawArgs &args, FTexture *skytex, int row, bool bottomCap);

	TriVertex SetVertexXYZ(float xx, float yy, float zz, float uu = 0, float vv = 0);
};
