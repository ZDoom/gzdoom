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

#include <stddef.h>
#include "templates.h"
#include "doomdef.h"
#include "i_system.h"
#include "w_wad.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "g_game.h"
#include "g_level.h"
#include "r_data/r_translate.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "poly_triangle.h"
#include "polyrenderer/poly_renderer.h"
#include "swrenderer/drawers/r_draw_rgba.h"
#include "screen_triangle.h"

CVAR(Bool, r_debug_trisetup, false, 0);

int PolyTriangleDrawer::viewport_x;
int PolyTriangleDrawer::viewport_y;
int PolyTriangleDrawer::viewport_width;
int PolyTriangleDrawer::viewport_height;
int PolyTriangleDrawer::dest_pitch;
int PolyTriangleDrawer::dest_width;
int PolyTriangleDrawer::dest_height;
uint8_t *PolyTriangleDrawer::dest;
bool PolyTriangleDrawer::dest_bgra;
bool PolyTriangleDrawer::mirror;

void PolyTriangleDrawer::set_viewport(int x, int y, int width, int height, DCanvas *canvas)
{
	dest = (uint8_t*)canvas->GetBuffer();
	dest_width = canvas->GetWidth();
	dest_height = canvas->GetHeight();
	dest_pitch = canvas->GetPitch();
	dest_bgra = canvas->IsBgra();

	int offsetx = clamp(x, 0, dest_width);
	int offsety = clamp(y, 0, dest_height);
	int pixelsize = dest_bgra ? 4 : 1;

	viewport_x = x - offsetx;
	viewport_y = y - offsety;
	viewport_width = width;
	viewport_height = height;

	dest += (offsetx + offsety * dest_pitch) * pixelsize;
	dest_width = clamp(viewport_x + viewport_width, 0, dest_width - offsetx);
	dest_height = clamp(viewport_y + viewport_height, 0, dest_height - offsety);

	mirror = false;
}

void PolyTriangleDrawer::toggle_mirror()
{
	mirror = !mirror;
}

void PolyTriangleDrawer::draw(const PolyDrawArgs &args)
{
	PolyRenderer::Instance()->Thread.DrawQueue->Push<DrawPolyTrianglesCommand>(args, mirror);
}

void PolyTriangleDrawer::draw_arrays(const PolyDrawArgs &drawargs, WorkerThreadData *thread)
{
	if (drawargs.vcount < 3)
		return;

	PolyDrawFuncPtr drawfuncs[4];
	int num_drawfuncs = 0;
	
	drawfuncs[num_drawfuncs++] = drawargs.subsectorTest ? &ScreenTriangle::SetupSubsector : &ScreenTriangle::SetupNormal;

	if (!r_debug_trisetup) // For profiling how much time is spent in setup vs drawal
	{
		int bmode = (int)drawargs.blendmode;

		if (drawargs.writeColor && drawargs.texturePixels)
			drawfuncs[num_drawfuncs++] = dest_bgra ? ScreenTriangle::TriDraw32[bmode] : ScreenTriangle::TriDraw8[bmode];
		else if (drawargs.writeColor)
			drawfuncs[num_drawfuncs++] = dest_bgra ? ScreenTriangle::TriFill32[bmode] : ScreenTriangle::TriFill8[bmode];
	}

	if (drawargs.writeStencil)
		drawfuncs[num_drawfuncs++] = &ScreenTriangle::StencilWrite;

	if (drawargs.writeSubsector)
		drawfuncs[num_drawfuncs++] = &ScreenTriangle::SubsectorWrite;

	TriDrawTriangleArgs args;
	args.dest = dest;
	args.pitch = dest_pitch;
	args.clipleft = 0;
	args.clipright = dest_width;
	args.cliptop = 0;
	args.clipbottom = dest_height;
	args.texturePixels = drawargs.texturePixels;
	args.textureWidth = drawargs.textureWidth;
	args.textureHeight = drawargs.textureHeight;
	args.translation = drawargs.translation;
	args.uniforms = &drawargs.uniforms;
	args.stencilTestValue = drawargs.stenciltestvalue;
	args.stencilWriteValue = drawargs.stencilwritevalue;
	args.stencilPitch = PolyStencilBuffer::Instance()->BlockWidth();
	args.stencilValues = PolyStencilBuffer::Instance()->Values();
	args.stencilMasks = PolyStencilBuffer::Instance()->Masks();
	args.subsectorGBuffer = PolySubsectorGBuffer::Instance()->Values();
	args.colormaps = drawargs.colormaps;
	args.RGB256k = RGB256k.All;
	args.BaseColors = (const uint8_t *)GPalette.BaseColors;

	bool ccw = drawargs.ccw;
	const TriVertex *vinput = drawargs.vinput;
	int vcount = drawargs.vcount;

	ShadedTriVertex vert[3];
	if (drawargs.mode == TriangleDrawMode::Normal)
	{
		for (int i = 0; i < vcount / 3; i++)
		{
			for (int j = 0; j < 3; j++)
				vert[j] = shade_vertex(*drawargs.objectToClip, drawargs.clipPlane, *(vinput++));
			draw_shaded_triangle(vert, ccw, &args, thread, drawfuncs, num_drawfuncs);
		}
	}
	else if (drawargs.mode == TriangleDrawMode::Fan)
	{
		vert[0] = shade_vertex(*drawargs.objectToClip, drawargs.clipPlane, *(vinput++));
		vert[1] = shade_vertex(*drawargs.objectToClip, drawargs.clipPlane, *(vinput++));
		for (int i = 2; i < vcount; i++)
		{
			vert[2] = shade_vertex(*drawargs.objectToClip, drawargs.clipPlane, *(vinput++));
			draw_shaded_triangle(vert, ccw, &args, thread, drawfuncs, num_drawfuncs);
			vert[1] = vert[2];
		}
	}
	else // TriangleDrawMode::Strip
	{
		vert[0] = shade_vertex(*drawargs.objectToClip, drawargs.clipPlane, *(vinput++));
		vert[1] = shade_vertex(*drawargs.objectToClip, drawargs.clipPlane, *(vinput++));
		for (int i = 2; i < vcount; i++)
		{
			vert[2] = shade_vertex(*drawargs.objectToClip, drawargs.clipPlane, *(vinput++));
			draw_shaded_triangle(vert, ccw, &args, thread, drawfuncs, num_drawfuncs);
			vert[0] = vert[1];
			vert[1] = vert[2];
			ccw = !ccw;
		}
	}
}

ShadedTriVertex PolyTriangleDrawer::shade_vertex(const TriMatrix &objectToClip, const float *clipPlane, const TriVertex &v)
{
	// Apply transform to get clip coordinates:
	ShadedTriVertex sv = objectToClip * v;

	// Calculate gl_ClipDistance[0]
	sv.clipDistance0 = v.x * clipPlane[0] + v.y * clipPlane[1] + v.z * clipPlane[2] + v.w * clipPlane[3];

	return sv;
}

void PolyTriangleDrawer::draw_shaded_triangle(const ShadedTriVertex *vert, bool ccw, TriDrawTriangleArgs *args, WorkerThreadData *thread, PolyDrawFuncPtr *drawfuncs, int num_drawfuncs)
{
	// Cull, clip and generate additional vertices as needed
	TriVertex clippedvert[max_additional_vertices];
	int numclipvert;
	clipedge(vert, clippedvert, numclipvert);

	// Map to 2D viewport:
	for (int j = 0; j < numclipvert; j++)
	{
		auto &v = clippedvert[j];

		// Calculate normalized device coordinates:
		v.w = 1.0f / v.w;
		v.x *= v.w;
		v.y *= v.w;
		v.z *= v.w;

		// Apply viewport scale to get screen coordinates:
		v.x = viewport_x + viewport_width * (1.0f + v.x) * 0.5f;
		v.y = viewport_y + viewport_height * (1.0f - v.y) * 0.5f;
	}

	// Keep varyings in -128 to 128 range if possible
	if (numclipvert > 0)
	{
		for (int j = 0; j < TriVertex::NumVarying; j++)
		{
			float newOrigin = floorf(clippedvert[0].varying[j] * 0.1f) * 10.0f;
			for (int i = 0; i < numclipvert; i++)
			{
				clippedvert[i].varying[j] -= newOrigin;
			}
		}
	}

	// Draw screen triangles
	if (ccw)
	{
		for (int i = numclipvert; i > 1; i--)
		{
			args->v1 = &clippedvert[numclipvert - 1];
			args->v2 = &clippedvert[i - 1];
			args->v3 = &clippedvert[i - 2];
			
			for (int j = 0; j < num_drawfuncs; j++)
				drawfuncs[j](args, thread);
		}
	}
	else
	{
		for (int i = 2; i < numclipvert; i++)
		{
			args->v1 = &clippedvert[0];
			args->v2 = &clippedvert[i - 1];
			args->v3 = &clippedvert[i];
			
			for (int j = 0; j < num_drawfuncs; j++)
				drawfuncs[j](args, thread);
		}
	}
}

bool PolyTriangleDrawer::cullhalfspace(float clipdistance1, float clipdistance2, float &t1, float &t2)
{
	if (clipdistance1 < 0.0f && clipdistance2 < 0.0f)
		return true;

	if (clipdistance1 < 0.0f)
		t1 = MAX(-clipdistance1 / (clipdistance2 - clipdistance1), 0.0f);
	else
		t1 = 0.0f;

	if (clipdistance2 < 0.0f)
		t2 = MIN(1.0f + clipdistance2 / (clipdistance1 - clipdistance2), 1.0f);
	else
		t2 = 1.0f;

	return false;
}

void PolyTriangleDrawer::clipedge(const ShadedTriVertex *verts, TriVertex *clippedvert, int &numclipvert)
{
	// Clip and cull so that the following is true for all vertices:
	// -v.w <= v.x <= v.w
	// -v.w <= v.y <= v.w
	// -v.w <= v.z <= v.w
	
	// use barycentric weights while clipping vertices
	float weights[max_additional_vertices * 3 * 2];
	for (int i = 0; i < 3; i++)
	{
		weights[i * 3 + 0] = 0.0f;
		weights[i * 3 + 1] = 0.0f;
		weights[i * 3 + 2] = 0.0f;
		weights[i * 3 + i] = 1.0f;
	}
	
	// halfspace clip distances
	static const int numclipdistances = 7;
	float clipdistance[numclipdistances * 3];
	for (int i = 0; i < 3; i++)
	{
		const auto &v = verts[i];
		clipdistance[i * numclipdistances + 0] = v.x + v.w;
		clipdistance[i * numclipdistances + 1] = v.w - v.x;
		clipdistance[i * numclipdistances + 2] = v.y + v.w;
		clipdistance[i * numclipdistances + 3] = v.w - v.y;
		clipdistance[i * numclipdistances + 4] = v.z + v.w;
		clipdistance[i * numclipdistances + 5] = v.w - v.z;
		clipdistance[i * numclipdistances + 6] = v.clipDistance0;
	}
	
	// Clip against each halfspace
	float *input = weights;
	float *output = weights + max_additional_vertices * 3;
	int inputverts = 3;
	int outputverts = 0;
	for (int p = 0; p < numclipdistances; p++)
	{
		// Clip each edge
		outputverts = 0;
		for (int i = 0; i < inputverts; i++)
		{
			int j = (i + 1) % inputverts;
			float clipdistance1 =
				clipdistance[0 * numclipdistances + p] * input[i * 3 + 0] +
				clipdistance[1 * numclipdistances + p] * input[i * 3 + 1] +
				clipdistance[2 * numclipdistances + p] * input[i * 3 + 2];

			float clipdistance2 =
				clipdistance[0 * numclipdistances + p] * input[j * 3 + 0] +
				clipdistance[1 * numclipdistances + p] * input[j * 3 + 1] +
				clipdistance[2 * numclipdistances + p] * input[j * 3 + 2];
				
			float t1, t2;
			if (!cullhalfspace(clipdistance1, clipdistance2, t1, t2) && outputverts + 1 < max_additional_vertices)
			{
				// add t1 vertex
				for (int k = 0; k < 3; k++)
					output[outputverts * 3 + k] = input[i * 3 + k] * (1.0f - t1) + input[j * 3 + k] * t1;
				outputverts++;
			
				if (t2 != 1.0f && t2 > t1)
				{
					// add t2 vertex
					for (int k = 0; k < 3; k++)
						output[outputverts * 3 + k] = input[i * 3 + k] * (1.0f - t2) + input[j * 3 + k] * t2;
					outputverts++;
				}
			}
		}
		std::swap(input, output);
		std::swap(inputverts, outputverts);
		if (inputverts == 0)
			break;
	}
	
	// Convert barycentric weights to actual vertices
	numclipvert = inputverts;
	for (int i = 0; i < numclipvert; i++)
	{
		auto &v = clippedvert[i];
		memset(&v, 0, sizeof(TriVertex));
		for (int w = 0; w < 3; w++)
		{
			float weight = input[i * 3 + w];
			v.x += verts[w].x * weight;
			v.y += verts[w].y * weight;
			v.z += verts[w].z * weight;
			v.w += verts[w].w * weight;
			for (int iv = 0; iv < TriVertex::NumVarying; iv++)
				v.varying[iv] += verts[w].varying[iv] * weight;
		}
	}
}

/////////////////////////////////////////////////////////////////////////////

DrawPolyTrianglesCommand::DrawPolyTrianglesCommand(const PolyDrawArgs &args, bool mirror)
	: args(args)
{
	if (mirror)
		this->args.ccw = !this->args.ccw;
}

void DrawPolyTrianglesCommand::Execute(DrawerThread *thread)
{
	WorkerThreadData thread_data;
	thread_data.core = thread->core;
	thread_data.num_cores = thread->num_cores;
	thread_data.FullSpans = thread->FullSpansBuffer.data();
	thread_data.PartialBlocks = thread->PartialBlocksBuffer.data();

	PolyTriangleDrawer::draw_arrays(args, &thread_data);
}

FString DrawPolyTrianglesCommand::DebugInfo()
{
	FString blendmodestr;
	switch (args.blendmode)
	{
	default: blendmodestr = "Unknown"; break;
	case TriBlendMode::Copy: blendmodestr = "Copy"; break;
	case TriBlendMode::AlphaBlend: blendmodestr = "AlphaBlend"; break;
	case TriBlendMode::AddSolid: blendmodestr = "AddSolid"; break;
	case TriBlendMode::Add: blendmodestr = "Add"; break;
	case TriBlendMode::Sub: blendmodestr = "Sub"; break;
	case TriBlendMode::RevSub: blendmodestr = "RevSub"; break;
	case TriBlendMode::Stencil: blendmodestr = "Stencil"; break;
	case TriBlendMode::Shaded: blendmodestr = "Shaded"; break;
	case TriBlendMode::TranslateCopy: blendmodestr = "TranslateCopy"; break;
	case TriBlendMode::TranslateAlphaBlend: blendmodestr = "TranslateAlphaBlend"; break;
	case TriBlendMode::TranslateAdd: blendmodestr = "TranslateAdd"; break;
	case TriBlendMode::TranslateSub: blendmodestr = "TranslateSub"; break;
	case TriBlendMode::TranslateRevSub: blendmodestr = "TranslateRevSub"; break;
	case TriBlendMode::AddSrcColorOneMinusSrcColor: blendmodestr = "AddSrcColorOneMinusSrcColor"; break;
	}

	FString info;
	info.Format("DrawPolyTriangles: blend mode = %s, color = %d, light = %d, textureWidth = %d, textureHeight = %d, texture = %s, translation = %s, colormaps = %s",
		blendmodestr.GetChars(), args.uniforms.color, args.uniforms.light, args.textureWidth, args.textureHeight,
		args.texturePixels ? "ptr" : "null", args.translation ? "ptr" : "null", args.colormaps ? "ptr" : "null");
	return info;
}
