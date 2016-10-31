/*
**  Triangle drawers
**  Copyright (c) 2016 Magnus Norddahl
**
**  This software is provided 'as-is', without any express or implied
**  warranty.  In no event will the authors be held liable for any damages
**  arising from the use of this software.
**
**  Permission is granted to anyone to use this software for any purpose,
**  including commercial applications, and to alter it and redistribute it
**  freely, subject to the following restrictions:
**
**  1. The origin of this software must not be misrepresented; you must not
**     claim that you wrote the original software. If you use this software
**     in a product, an acknowledgment in the product documentation would be
**     appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be
**     misrepresented as being the original software.
**  3. This notice may not be removed or altered from any source distribution.
**
*/


#ifndef __R_TRIANGLE__
#define __R_TRIANGLE__

#include "r_draw.h"

class FTexture;
struct ScreenTriangleDrawerArgs;

struct TriVertex
{
	TriVertex() { }
	TriVertex(float x, float y, float z, float w, float u, float v, float light) : x(x), y(y), z(z), w(w) { varying[0] = u; varying[1] = v; varying[2] = light; }

	enum { NumVarying = 3 };
	float x, y, z, w;
	float varying[NumVarying];
};

struct TriMatrix
{
	static TriMatrix null();
	static TriMatrix identity();
	static TriMatrix translate(float x, float y, float z);
	static TriMatrix scale(float x, float y, float z);
	static TriMatrix rotate(float angle, float x, float y, float z);

	TriVertex operator*(TriVertex v) const;
	TriMatrix operator*(const TriMatrix &m) const;

	float matrix[16];
};

enum class TriangleDrawMode
{
	Normal,
	Fan,
	Strip
};

class TriangleDrawer
{
public:
	static void draw(const TriMatrix &objectToWorld, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, const short *cliptop, const short *clipbottom, FTexture *texture);
	static void fill(const TriMatrix &objectToWorld, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, const short *cliptop, const short *clipbottom, int solidcolor);

private:
	static TriVertex shade_vertex(const TriMatrix &objectToWorld, TriVertex v);
	static void draw_arrays(const TriMatrix &objectToWorld, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, const short *cliptop, const short *clipbottom, FTexture *texture, int solidcolor, void(*drawfunc)(const ScreenTriangleDrawerArgs *));
	static void draw_shaded_triangle(const TriVertex *vertices, bool ccw, ScreenTriangleDrawerArgs *args, void(*drawfunc)(const ScreenTriangleDrawerArgs *));
	static bool cullhalfspace(float clipdistance1, float clipdistance2, float &t1, float &t2);
	static void clipedge(const TriVertex &v1, const TriVertex &v2, TriVertex *clippedvert, int &numclipvert);
};

struct ScreenTriangleDrawerArgs
{
	uint8_t *dest;
	int pitch;
	TriVertex *v1;
	TriVertex *v2;
	TriVertex *v3;
	int clipleft;
	int clipright;
	const short *cliptop;
	const short *clipbottom;
	const uint8_t *texturePixels;
	int textureWidth;
	int textureHeight;
	int solidcolor;
};

class ScreenTriangleDrawer
{
public:
	static void draw(const ScreenTriangleDrawerArgs *args);
	static void fill(const ScreenTriangleDrawerArgs *args);

private:
	static float gradx(float x0, float y0, float x1, float y1, float x2, float y2, float c0, float c1, float c2);
	static float grady(float x0, float y0, float x1, float y1, float x2, float y2, float c0, float c1, float c2);
};

#endif
