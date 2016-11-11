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

struct ScreenPolyTriangleDrawerArgs;

class PolyTriangleDrawer
{
public:
	static void draw(const TriUniforms &uniforms, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, int cliptop, int clipbottom, FTexture *texture, int stenciltestvalue);
	static void fill(const TriUniforms &uniforms, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, int cliptop, int clipbottom, int solidcolor, int stenciltestvalue);
	static void stencil(const TriUniforms &uniforms, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, int cliptop, int clipbottom, int stenciltestvalue, int stencilwritevalue);

private:
	static TriVertex shade_vertex(const TriUniforms &uniforms, TriVertex v);
	static void draw_arrays(const TriUniforms &uniforms, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, int cliptop, int clipbottom, const uint8_t *texturePixels, int textureWidth, int textureHeight, int solidcolor, int stenciltestvalue, int stencilwritevalue, DrawerThread *thread, void(*drawfunc)(const ScreenPolyTriangleDrawerArgs *, DrawerThread *));
	static void draw_shaded_triangle(const TriVertex *vertices, bool ccw, ScreenPolyTriangleDrawerArgs *args, DrawerThread *thread, void(*drawfunc)(const ScreenPolyTriangleDrawerArgs *, DrawerThread *));
	static bool cullhalfspace(float clipdistance1, float clipdistance2, float &t1, float &t2);
	static void clipedge(const TriVertex *verts, TriVertex *clippedvert, int &numclipvert);

	static void queue_arrays(const TriUniforms &uniforms, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, int cliptop, int clipbottom, const uint8_t *texturePixels, int textureWidth, int textureHeight, int solidcolor, int stenciltestvalue, int stencilwritevalue);

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
		if (ValueMask == 0xffffffff)
		{
			if (Values[0] == value)
				return;

			for (int i = 1; i < 8 * 8; i++)
				Values[i] = Values[0];
		}

		if (Values[x + y * 8] == value)
			return;

		Values[x + y * 8] = value;

		int leveloffset = 0;
		for (int i = 1; i < 4; i++)
		{
			x >>= 1;
			y >>= 1;

			bool same =
				Values[(x << i)       + (y << i) * 8]       != value ||
				Values[((x + 1) << i) + (y << i) * 8]       != value ||
				Values[(x << i)       + ((y + 1) << i) * 8] != value ||
				Values[((x + 1) << i) + ((y + 1) << i) * 8] != value;

			int levelbit = 1 << (leveloffset + x + y * (8 >> i));

			if (same)
				ValueMask = ValueMask & ~levelbit;
			else
				ValueMask = ValueMask | levelbit;

			leveloffset += (8 >> leveloffset) * (8 >> leveloffset);
		}

		if (Values[0] != value || Values[4] != value || Values[4 * 8] != value || Values[4 * 8 + 4] != value)
			ValueMask = ValueMask & ~(1 << 22);
		else
			ValueMask = ValueMask | (1 << 22);
	}

	uint8_t Get(int x, int y) const
	{
		if (ValueMask == 0xffffffff)
			return Values[0];
		else
			return Values[x + y * 8];
	}

	void Clear(uint8_t value)
	{
		Values[0] = value;
		ValueMask = 0xffffffff;
	}

	bool IsSingleValue() const
	{
		return ValueMask == 0xffffffff;
	}

private:
	uint8_t *Values; // [8 * 8];
	uint32_t &ValueMask; // 4 * 4 + 2 * 2 + 1 bits indicating is Values are the same
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

struct ScreenPolyTriangleDrawerArgs
{
	uint8_t *dest;
	int pitch;
	TriVertex *v1;
	TriVertex *v2;
	TriVertex *v3;
	int clipleft;
	int clipright;
	int cliptop;
	int clipbottom;
	const uint8_t *texturePixels;
	int textureWidth;
	int textureHeight;
	int solidcolor;
	const TriUniforms *uniforms;
	uint8_t *stencilValues;
	uint32_t *stencilMasks;
	int stencilPitch;
	uint8_t stencilTestValue;
	uint8_t stencilWriteValue;
	uint32_t *subsectorGBuffer;
};

class ScreenPolyTriangleDrawer
{
public:
	static void draw(const ScreenPolyTriangleDrawerArgs *args, DrawerThread *thread);
	static void fill(const ScreenPolyTriangleDrawerArgs *args, DrawerThread *thread);

	static void stencil(const ScreenPolyTriangleDrawerArgs *args, DrawerThread *thread);

	static void draw32(const ScreenPolyTriangleDrawerArgs *args, DrawerThread *thread);
	static void fill32(const ScreenPolyTriangleDrawerArgs *args, DrawerThread *thread);

private:
	static float gradx(float x0, float y0, float x1, float y1, float x2, float y2, float c0, float c1, float c2);
	static float grady(float x0, float y0, float x1, float y1, float x2, float y2, float c0, float c1, float c2);
};

class DrawPolyTrianglesCommand : public DrawerCommand
{
public:
	DrawPolyTrianglesCommand(const TriUniforms &uniforms, const TriVertex *vinput, int vcount, TriangleDrawMode mode, bool ccw, int clipleft, int clipright, int cliptop, int clipbottom, const uint8_t *texturePixels, int textureWidth, int textureHeight, int solidcolor, int stenciltestvalue, int stencilwritevalue);

	void Execute(DrawerThread *thread) override;
	FString DebugInfo() override;

private:
	TriUniforms uniforms;
	const TriVertex *vinput;
	int vcount;
	TriangleDrawMode mode;
	bool ccw;
	int clipleft;
	int clipright;
	int cliptop;
	int clipbottom;
	const uint8_t *texturePixels;
	int textureWidth;
	int textureHeight;
	int solidcolor;
	int stenciltestvalue;
	int stencilwritevalue;
};

#endif
