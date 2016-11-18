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


#ifndef __R_POLY_TRIANGLE__
#define __R_POLY_TRIANGLE__

#include "r_triangle.h"

struct TriDrawTriangleArgs;

class PolyDrawArgs
{
public:
	TriUniforms uniforms;
	const TriVertex *vinput = nullptr;
	int vcount = 0;
	TriangleDrawMode mode = TriangleDrawMode::Normal;
	bool ccw = false;
	const uint8_t *texturePixels = nullptr;
	int textureWidth = 0;
	int textureHeight = 0;
	uint32_t solidcolor = 0;
	uint8_t stenciltestvalue = 0;
	uint8_t stencilwritevalue = 0;

	void SetTexture(FTexture *texture)
	{
		textureWidth = texture->GetWidth();
		textureHeight = texture->GetHeight();
		if (r_swtruecolor)
			texturePixels = (const uint8_t *)texture->GetPixelsBgra();
		else
			texturePixels = texture->GetPixels();
	}
};

class PolyTriangleDrawer
{
public:
	static void set_viewport(int x, int y, int width, int height, DCanvas *canvas);
	static void draw(const PolyDrawArgs &args, TriDrawVariant variant);

private:
	static TriVertex shade_vertex(const TriUniforms &uniforms, TriVertex v);
	static void draw_arrays(const PolyDrawArgs &args, TriDrawVariant variant, WorkerThreadData *thread);
	static void draw_shaded_triangle(const TriVertex *vertices, bool ccw, TriDrawTriangleArgs *args, WorkerThreadData *thread, void(*drawfunc)(const TriDrawTriangleArgs *, WorkerThreadData *));
	static bool cullhalfspace(float clipdistance1, float clipdistance2, float &t1, float &t2);
	static void clipedge(const TriVertex *verts, TriVertex *clippedvert, int &numclipvert);

	static int viewport_x, viewport_y, viewport_width, viewport_height, dest_pitch, dest_width, dest_height;
	static bool dest_bgra;
	static uint8_t *dest;

	enum { max_additional_vertices = 16 };

	friend class DrawPolyTrianglesCommand;
};

// 8x8 block of stencil values, plus a mask indicating if values are the same for early out stencil testing
class PolyStencilBlock
{
public:
	PolyStencilBlock(int block, uint8_t *values, uint32_t *masks) : Values(values + block * 64), ValueMask(masks[block])
	{
	}

	void Set(int x, int y, uint8_t value)
	{
		if ((ValueMask & 0xffffff00) == 0xffffff00)
		{
			if ((ValueMask & 0xff) == value)
				return;

			for (int i = 0; i < 8 * 8; i++)
				Values[i] = (ValueMask & 0xff);
			ValueMask = 0;
		}

		Values[x + y * 8] = value;
	}

	uint8_t Get(int x, int y) const
	{
		if (IsSingleValue())
			return ValueMask & 0xff;
		else
			return Values[x + y * 8];
	}

	void Clear(uint8_t value)
	{
		ValueMask = 0xffffff00 | (uint32_t)value;
	}

	bool IsSingleValue() const
	{
		return (ValueMask & 0xffffff00) == 0xffffff00;
	}

private:
	uint8_t *Values;
	uint32_t &ValueMask;
};

class PolySubsectorGBuffer
{
public:
	static PolySubsectorGBuffer *Instance()
	{
		static PolySubsectorGBuffer buffer;
		return &buffer;
	}

	void Resize(int newwidth, int newheight)
	{
		width = newwidth;
		height = newheight;
		values.resize(width * height);
	}

	int Width() const { return width; }
	int Height() const { return height; }
	uint32_t *Values() { return values.data(); }

private:
	int width;
	int height;
	std::vector<uint32_t> values;
};

class PolyStencilBuffer
{
public:
	static PolyStencilBuffer *Instance()
	{
		static PolyStencilBuffer buffer;
		return &buffer;
	}

	void Clear(int newwidth, int newheight, uint8_t stencil_value = 0)
	{
		width = newwidth;
		height = newheight;
		int count = BlockWidth() * BlockHeight();
		values.resize(count * 64);
		masks.resize(count);

		uint8_t *v = Values();
		uint32_t *m = Masks();
		for (int i = 0; i < count; i++)
		{
			PolyStencilBlock block(i, v, m);
			block.Clear(stencil_value);
		}
	}

	int Width() const { return width; }
	int Height() const { return height; }
	int BlockWidth() const { return (width + 7) / 8; }
	int BlockHeight() const { return (height + 7) / 8; }
	uint8_t *Values() { return values.data(); }
	uint32_t *Masks() { return masks.data(); }

private:
	int width;
	int height;
	std::vector<uint8_t> values;
	std::vector<uint32_t> masks;
};

class ScreenPolyTriangleDrawer
{
public:
	static void draw(const TriDrawTriangleArgs *args, WorkerThreadData *thread);
	static void fill(const TriDrawTriangleArgs *args, WorkerThreadData *thread);

	static void stencil(const TriDrawTriangleArgs *args, WorkerThreadData *thread);

	static void draw32(const TriDrawTriangleArgs *args, WorkerThreadData *thread);
	static void drawsubsector32(const TriDrawTriangleArgs *args, WorkerThreadData *thread);
	static void fill32(const TriDrawTriangleArgs *args, WorkerThreadData *thread);

private:
	static float gradx(float x0, float y0, float x1, float y1, float x2, float y2, float c0, float c1, float c2);
	static float grady(float x0, float y0, float x1, float y1, float x2, float y2, float c0, float c1, float c2);
};

class DrawPolyTrianglesCommand : public DrawerCommand
{
public:
	DrawPolyTrianglesCommand(const PolyDrawArgs &args, TriDrawVariant variant);

	void Execute(DrawerThread *thread) override;
	FString DebugInfo() override;

private:
	PolyDrawArgs args;
	TriDrawVariant variant;
};

#endif
