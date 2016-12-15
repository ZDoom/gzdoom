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

#pragma once

#include "r_draw.h"
#include "r_thread.h"
#include "r_drawers.h"
#include "r_data/r_translate.h"
#include "r_data/colormaps.h"

class FTexture;

enum class TriangleDrawMode
{
	Normal,
	Fan,
	Strip
};

struct TriDrawTriangleArgs;
struct TriMatrix;

class PolyDrawArgs
{
public:
	TriUniforms uniforms;
	const TriMatrix *objectToClip = nullptr;
	const TriVertex *vinput = nullptr;
	int vcount = 0;
	TriangleDrawMode mode = TriangleDrawMode::Normal;
	bool ccw = false;
	const uint8_t *texturePixels = nullptr;
	int textureWidth = 0;
	int textureHeight = 0;
	const uint8_t *translation = nullptr;
	uint8_t stenciltestvalue = 0;
	uint8_t stencilwritevalue = 0;
	const uint8_t *colormaps = nullptr;
	float clipPlane[4];

	void SetClipPlane(float a, float b, float c, float d)
	{
		clipPlane[0] = a;
		clipPlane[1] = b;
		clipPlane[2] = c;
		clipPlane[3] = d;
	}

	void SetTexture(FTexture *texture)
	{
		textureWidth = texture->GetWidth();
		textureHeight = texture->GetHeight();
		if (swrenderer::r_swtruecolor)
			texturePixels = (const uint8_t *)texture->GetPixelsBgra();
		else
			texturePixels = texture->GetPixels();
		translation = nullptr;
	}

	void SetTexture(FTexture *texture, uint32_t translationID, bool forcePal = false)
	{
		if (translationID != 0xffffffff && translationID != 0)
		{
			FRemapTable *table = TranslationToTable(translationID);
			if (table != nullptr && !table->Inactive)
			{
				if (swrenderer::r_swtruecolor)
					translation = (uint8_t*)table->Palette;
				else
					translation = table->Remap;

				textureWidth = texture->GetWidth();
				textureHeight = texture->GetHeight();
				texturePixels = texture->GetPixels();
				return;
			}
		}
		
		if (forcePal)
		{
			textureWidth = texture->GetWidth();
			textureHeight = texture->GetHeight();
			texturePixels = texture->GetPixels();
		}
		else
		{
			SetTexture(texture);
		}
	}

	void SetColormap(FSWColormap *base_colormap)
	{
		uniforms.light_red = base_colormap->Color.r * 256 / 255;
		uniforms.light_green = base_colormap->Color.g * 256 / 255;
		uniforms.light_blue = base_colormap->Color.b * 256 / 255;
		uniforms.light_alpha = base_colormap->Color.a * 256 / 255;
		uniforms.fade_red = base_colormap->Fade.r;
		uniforms.fade_green = base_colormap->Fade.g;
		uniforms.fade_blue = base_colormap->Fade.b;
		uniforms.fade_alpha = base_colormap->Fade.a;
		uniforms.desaturate = MIN(abs(base_colormap->Desaturate), 255) * 255 / 256;
		bool simple_shade = (base_colormap->Color.d == 0x00ffffff && base_colormap->Fade.d == 0x00000000 && base_colormap->Desaturate == 0);
		if (simple_shade)
			uniforms.flags |= TriUniforms::simple_shade;
		else
			uniforms.flags &= ~TriUniforms::simple_shade;
		colormaps = base_colormap->Maps;
	}
};

struct ShadedTriVertex : public TriVertex
{
	float clipDistance0;
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

	ShadedTriVertex operator*(TriVertex v) const;
	TriMatrix operator*(const TriMatrix &m) const;

	float matrix[16];
};

typedef void(*PolyDrawFuncPtr)(const TriDrawTriangleArgs *, WorkerThreadData *);

class PolyTriangleDrawer
{
public:
	static void set_viewport(int x, int y, int width, int height, DCanvas *canvas);
	static void draw(const PolyDrawArgs &args, TriDrawVariant variant, TriBlendMode blendmode);
	static void toggle_mirror();

private:
	static ShadedTriVertex shade_vertex(const TriMatrix &objectToClip, const float *clipPlane, const TriVertex &v);
	static void draw_arrays(const PolyDrawArgs &args, TriDrawVariant variant, TriBlendMode blendmode, WorkerThreadData *thread);
	static void draw_shaded_triangle(const ShadedTriVertex *vertices, bool ccw, TriDrawTriangleArgs *args, WorkerThreadData *thread, PolyDrawFuncPtr setupfunc, PolyDrawFuncPtr drawfunc);
	static bool cullhalfspace(float clipdistance1, float clipdistance2, float &t1, float &t2);
	static void clipedge(const ShadedTriVertex *verts, TriVertex *clippedvert, int &numclipvert);

	static int viewport_x, viewport_y, viewport_width, viewport_height, dest_pitch, dest_width, dest_height;
	static bool dest_bgra;
	static uint8_t *dest;
	static bool mirror;

	enum { max_additional_vertices = 16 };

	friend class DrawPolyTrianglesCommand;
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
			m[i] = 0xffffff00 | stencil_value;
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

	// 8x8 blocks of stencil values, plus a mask for each block indicating if values are the same for early out stencil testing
	std::vector<uint8_t> values;
	std::vector<uint32_t> masks;
};

class DrawPolyTrianglesCommand : public DrawerCommand
{
public:
	DrawPolyTrianglesCommand(const PolyDrawArgs &args, TriDrawVariant variant, TriBlendMode blendmode, bool mirror);

	void Execute(DrawerThread *thread) override;
	FString DebugInfo() override;

private:
	PolyDrawArgs args;
	TriDrawVariant variant;
	TriBlendMode blendmode;
};

class PolyVertexBuffer
{
public:
	static TriVertex *GetVertices(int count);
	static void Clear();
};

struct ScreenTriangleStepVariables
{
	float W;
	float Varying[TriVertex::NumVarying];
};

class ScreenTriangle
{
public:
	static void StencilFunc(const TriDrawTriangleArgs *args, WorkerThreadData *thread);
	static void StencilCloseFunc(const TriDrawTriangleArgs *args, WorkerThreadData *thread);
	
	static void SetupNormal(const TriDrawTriangleArgs *args, WorkerThreadData *thread);
	static void SetupSubsector(const TriDrawTriangleArgs *args, WorkerThreadData *thread);
	static void StencilWrite(const TriDrawTriangleArgs *args, WorkerThreadData *thread);
	static void SubsectorWrite(const TriDrawTriangleArgs *args, WorkerThreadData *thread);
};
