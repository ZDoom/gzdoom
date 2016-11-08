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
#include "r_thread.h"

class FTexture;
struct ScreenTriangleDrawerArgs;

struct TriVertex
{
	TriVertex() { }
	TriVertex(float x, float y, float z, float w, float u, float v) : x(x), y(y), z(z), w(w) { varying[0] = u; varying[1] = v; }

	enum { NumVarying = 2 };
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
	static TriMatrix swapYZ();
	static TriMatrix perspective(float fovy, float aspect, float near, float far);
	static TriMatrix frustum(float left, float right, float bottom, float top, float near, float far);

	static TriMatrix worldToView(); // Software renderer world to view space transform
	static TriMatrix viewToClip(); // Software renderer shearing projection

	TriVertex operator*(TriVertex v) const;
	TriMatrix operator*(const TriMatrix &m) const;

	float matrix[16];
};

struct TriUniforms
{
	uint32_t light;

	uint16_t light_alpha;
	uint16_t light_red;
	uint16_t light_green;
	uint16_t light_blue;
	uint16_t fade_alpha;
	uint16_t fade_red;
	uint16_t fade_green;
	uint16_t fade_blue;
	uint16_t desaturate;
	uint32_t flags;
	enum Flags
	{
		simple_shade = 1,
		nearest_filter = 2,
		diminishing_lighting = 4
	};

	TriMatrix objectToClip;
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
	static void draw(const TriUniforms &uniforms, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, const short *cliptop, const short *clipbottom, FTexture *texture);
	static void fill(const TriUniforms &uniforms, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, const short *cliptop, const short *clipbottom, int solidcolor);

private:
	static TriVertex shade_vertex(const TriUniforms &uniforms, TriVertex v);
	static void draw_arrays(const TriUniforms &uniforms, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, const short *cliptop, const short *clipbottom, const uint8_t *texturePixels, int textureWidth, int textureHeight, int solidcolor, DrawerThread *thread, void(*drawfunc)(const ScreenTriangleDrawerArgs *, DrawerThread *));
	static void draw_shaded_triangle(const TriVertex *vertices, bool ccw, ScreenTriangleDrawerArgs *args, DrawerThread *thread, void(*drawfunc)(const ScreenTriangleDrawerArgs *, DrawerThread *));
	static bool cullhalfspace(float clipdistance1, float clipdistance2, float &t1, float &t2);
	static void clipedge(const TriVertex *verts, TriVertex *clippedvert, int &numclipvert);

	static void queue_arrays(const TriUniforms &uniforms, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, const short *cliptop, const short *clipbottom, const uint8_t *texturePixels, int textureWidth, int textureHeight, int solidcolor);

	friend class DrawTrianglesCommand;
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
	const TriUniforms *uniforms;
};

class ScreenTriangleDrawer
{
public:
	static void draw(const ScreenTriangleDrawerArgs *args, DrawerThread *thread);
	static void fill(const ScreenTriangleDrawerArgs *args, DrawerThread *thread);

	static void draw32(const ScreenTriangleDrawerArgs *args, DrawerThread *thread);
	static void fill32(const ScreenTriangleDrawerArgs *args, DrawerThread *thread);

private:
	static float gradx(float x0, float y0, float x1, float y1, float x2, float y2, float c0, float c1, float c2);
	static float grady(float x0, float y0, float x1, float y1, float x2, float y2, float c0, float c1, float c2);
};

class DrawTrianglesCommand : public DrawerCommand
{
public:
	DrawTrianglesCommand(const TriUniforms &uniforms, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, const short *clipdata, const uint8_t *texturePixels, int textureWidth, int textureHeight, int solidcolor);

	void Execute(DrawerThread *thread) override;
	FString DebugInfo() override;

private:
	const TriUniforms uniforms;
	const TriVertex *vinput;
	int vcount;
	TriangleDrawMode mode;
	bool ccw;
	int clipleft;
	int clipright;
	const short *clipdata;
	const uint8_t *texturePixels;
	int textureWidth;
	int textureHeight;
	int solidcolor;
};

#endif
