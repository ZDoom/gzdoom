/*
**  Polygon Doom software renderer
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

#pragma once

#include "swrenderer/drawers/r_draw.h"
#include "swrenderer/drawers/r_thread.h"
#include "polyrenderer/drawers/screen_triangle.h"
#include "polyrenderer/math/tri_matrix.h"
#include "polyrenderer/drawers/poly_buffer.h"
#include "polyrenderer/drawers/poly_draw_args.h"

typedef void(*PolyDrawFuncPtr)(const TriDrawTriangleArgs *, WorkerThreadData *);

class PolyTriangleDrawer
{
public:
	static void set_viewport(int x, int y, int width, int height, DCanvas *canvas);
	static void toggle_mirror();
	static bool is_mirror();

private:
	static ShadedTriVertex shade_vertex(const PolyDrawArgs &drawargs, const TriVertex &v);
	static void draw_elements(const PolyDrawArgs &args, WorkerThreadData *thread);
	static void draw_arrays(const PolyDrawArgs &args, WorkerThreadData *thread);
	static void draw_shaded_triangle(const ShadedTriVertex *vertices, bool ccw, TriDrawTriangleArgs *args, WorkerThreadData *thread);
	static bool is_degenerate(const ShadedTriVertex *vertices);
	static bool is_frontfacing(TriDrawTriangleArgs *args);

	static int clipedge(const ShadedTriVertex *verts, ShadedTriVertex *clippedvert);

	static int viewport_x, viewport_y, viewport_width, viewport_height, dest_pitch, dest_width, dest_height;
	static bool dest_bgra;
	static uint8_t *dest;
	static bool mirror;

	enum { max_additional_vertices = 16 };

	friend class DrawPolyTrianglesCommand;
};

class DrawPolyTrianglesCommand : public DrawerCommand
{
public:
	DrawPolyTrianglesCommand(const PolyDrawArgs &args, bool mirror);

	void Execute(DrawerThread *thread) override;
	FString DebugInfo() override { return "DrawPolyTriangles"; }

private:
	PolyDrawArgs args;
};

class DrawRectCommand : public DrawerCommand
{
public:
	DrawRectCommand(const RectDrawArgs &args) : args(args) { }

	void Execute(DrawerThread *thread) override;
	FString DebugInfo() override { return "DrawRect"; }

private:
	RectDrawArgs args;
};
