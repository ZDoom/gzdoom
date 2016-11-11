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
#include "r_compiler/llvmdrawers.h"

class FTexture;
struct ScreenTriangleDrawerArgs;

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
	static void draw_arrays(const TriUniforms &uniforms, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, const short *cliptop, const short *clipbottom, const uint8_t *texturePixels, int textureWidth, int textureHeight, int solidcolor, WorkerThreadData *thread, void(*drawfunc)(const ScreenTriangleDrawerArgs *, WorkerThreadData *));
	static void draw_shaded_triangle(const TriVertex *vertices, bool ccw, ScreenTriangleDrawerArgs *args, WorkerThreadData *thread, void(*drawfunc)(const ScreenTriangleDrawerArgs *, WorkerThreadData *));
	static bool cullhalfspace(float clipdistance1, float clipdistance2, float &t1, float &t2);
	static void clipedge(const TriVertex *verts, TriVertex *clippedvert, int &numclipvert);

	static void queue_arrays(const TriUniforms &uniforms, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, const short *cliptop, const short *clipbottom, const uint8_t *texturePixels, int textureWidth, int textureHeight, int solidcolor);

	enum { max_additional_vertices = 16 };

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
	static void draw(const ScreenTriangleDrawerArgs *args, WorkerThreadData *thread);
	static void fill(const ScreenTriangleDrawerArgs *args, WorkerThreadData *thread);

	static void draw32(const ScreenTriangleDrawerArgs *args, WorkerThreadData *thread);
	static void fill32(const ScreenTriangleDrawerArgs *args, WorkerThreadData *thread);

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
