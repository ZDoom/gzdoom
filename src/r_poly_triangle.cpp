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
#include "r_local.h"
#include "v_video.h"
#include "doomstat.h"
#include "st_stuff.h"
#include "g_game.h"
#include "g_level.h"
#include "r_data/r_translate.h"
#include "v_palette.h"
#include "r_data/colormaps.h"
#include "r_poly_triangle.h"

int PolyTriangleDrawer::viewport_x;
int PolyTriangleDrawer::viewport_y;
int PolyTriangleDrawer::viewport_width;
int PolyTriangleDrawer::viewport_height;
int PolyTriangleDrawer::dest_pitch;
int PolyTriangleDrawer::dest_width;
int PolyTriangleDrawer::dest_height;
uint8_t *PolyTriangleDrawer::dest;
bool PolyTriangleDrawer::dest_bgra;

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
}

void PolyTriangleDrawer::draw(const PolyDrawArgs &args, TriDrawVariant variant, TriBlendMode blendmode)
{
	DrawerCommandQueue::QueueCommand<DrawPolyTrianglesCommand>(args, variant, blendmode);
}

void PolyTriangleDrawer::draw_arrays(const PolyDrawArgs &drawargs, TriDrawVariant variant, TriBlendMode blendmode, WorkerThreadData *thread)
{
	if (drawargs.vcount < 3)
		return;

	auto llvm = LLVMDrawers::Instance();
	void(*drawfunc)(const TriDrawTriangleArgs *, WorkerThreadData *);
	int bmode = (int)blendmode;
	switch (variant)
	{
	default:
	case TriDrawVariant::DrawNormal: drawfunc = dest_bgra ? llvm->TriDrawNormal32[bmode] : llvm->TriDrawNormal8[bmode]; break;
	case TriDrawVariant::FillNormal: drawfunc = dest_bgra ? llvm->TriFillNormal32[bmode] : llvm->TriFillNormal8[bmode]; break;
	case TriDrawVariant::DrawSubsector: drawfunc = dest_bgra ? llvm->TriDrawSubsector32[bmode] : llvm->TriDrawSubsector8[bmode]; break;
	case TriDrawVariant::FuzzSubsector:
	case TriDrawVariant::FillSubsector: drawfunc = dest_bgra ? llvm->TriFillSubsector32[bmode] : llvm->TriFillSubsector8[bmode]; break;
	case TriDrawVariant::Stencil: drawfunc = llvm->TriStencil; break;
	}

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
	args.RGB32k = RGB32k.All;
	args.BaseColors = (const uint8_t *)GPalette.BaseColors;

	bool ccw = drawargs.ccw;
	const TriVertex *vinput = drawargs.vinput;
	int vcount = drawargs.vcount;

	TriVertex vert[3];
	if (drawargs.mode == TriangleDrawMode::Normal)
	{
		for (int i = 0; i < vcount / 3; i++)
		{
			for (int j = 0; j < 3; j++)
				vert[j] = shade_vertex(*drawargs.objectToClip, *(vinput++));
			draw_shaded_triangle(vert, ccw, &args, thread, drawfunc);
		}
	}
	else if (drawargs.mode == TriangleDrawMode::Fan)
	{
		vert[0] = shade_vertex(*drawargs.objectToClip, *(vinput++));
		vert[1] = shade_vertex(*drawargs.objectToClip, *(vinput++));
		for (int i = 2; i < vcount; i++)
		{
			vert[2] = shade_vertex(*drawargs.objectToClip, *(vinput++));
			draw_shaded_triangle(vert, ccw, &args, thread, drawfunc);
			vert[1] = vert[2];
		}
	}
	else // TriangleDrawMode::Strip
	{
		vert[0] = shade_vertex(*drawargs.objectToClip, *(vinput++));
		vert[1] = shade_vertex(*drawargs.objectToClip, *(vinput++));
		for (int i = 2; i < vcount; i++)
		{
			vert[2] = shade_vertex(*drawargs.objectToClip, *(vinput++));
			draw_shaded_triangle(vert, ccw, &args, thread, drawfunc);
			vert[0] = vert[1];
			vert[1] = vert[2];
			ccw = !ccw;
		}
	}
}

TriVertex PolyTriangleDrawer::shade_vertex(const TriMatrix &objectToClip, TriVertex v)
{
	// Apply transform to get clip coordinates:
	return objectToClip * v;
}

void PolyTriangleDrawer::draw_shaded_triangle(const TriVertex *vert, bool ccw, TriDrawTriangleArgs *args, WorkerThreadData *thread, void(*drawfunc)(const TriDrawTriangleArgs *, WorkerThreadData *))
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

	// Draw screen triangles
	if (ccw)
	{
		for (int i = numclipvert; i > 1; i--)
		{
			args->v1 = &clippedvert[numclipvert - 1];
			args->v2 = &clippedvert[i - 1];
			args->v3 = &clippedvert[i - 2];
			drawfunc(args, thread);
		}
	}
	else
	{
		for (int i = 2; i < numclipvert; i++)
		{
			args->v1 = &clippedvert[0];
			args->v2 = &clippedvert[i - 1];
			args->v3 = &clippedvert[i];
			drawfunc(args, thread);
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

void PolyTriangleDrawer::clipedge(const TriVertex *verts, TriVertex *clippedvert, int &numclipvert)
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
	float clipdistance[6 * 3];
	for (int i = 0; i < 3; i++)
	{
		const auto &v = verts[i];
		clipdistance[i * 6 + 0] = v.x + v.w;
		clipdistance[i * 6 + 1] = v.w - v.x;
		clipdistance[i * 6 + 2] = v.y + v.w;
		clipdistance[i * 6 + 3] = v.w - v.y;
		clipdistance[i * 6 + 4] = v.z + v.w;
		clipdistance[i * 6 + 5] = v.w - v.z;
	}
	
	// Clip against each halfspace
	float *input = weights;
	float *output = weights + max_additional_vertices * 3;
	int inputverts = 3;
	int outputverts = 0;
	for (int p = 0; p < 6; p++)
	{
		// Clip each edge
		outputverts = 0;
		for (int i = 0; i < inputverts; i++)
		{
			int j = (i + 1) % inputverts;
			float clipdistance1 =
				clipdistance[0 * 6 + p] * input[i * 3 + 0] +
				clipdistance[1 * 6 + p] * input[i * 3 + 1] +
				clipdistance[2 * 6 + p] * input[i * 3 + 2];

			float clipdistance2 =
				clipdistance[0 * 6 + p] * input[j * 3 + 0] +
				clipdistance[1 * 6 + p] * input[j * 3 + 1] +
				clipdistance[2 * 6 + p] * input[j * 3 + 2];
				
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

DrawPolyTrianglesCommand::DrawPolyTrianglesCommand(const PolyDrawArgs &args, TriDrawVariant variant, TriBlendMode blendmode)
	: args(args), variant(variant), blendmode(blendmode)
{
}

void DrawPolyTrianglesCommand::Execute(DrawerThread *thread)
{
	WorkerThreadData thread_data;
	thread_data.core = thread->core;
	thread_data.num_cores = thread->num_cores;
	thread_data.pass_start_y = thread->pass_start_y;
	thread_data.pass_end_y = thread->pass_end_y;
	thread_data.temp = thread->dc_temp_rgba;

	PolyTriangleDrawer::draw_arrays(args, variant, blendmode, &thread_data);
}

FString DrawPolyTrianglesCommand::DebugInfo()
{
	return "DrawPolyTriangles";
}

/////////////////////////////////////////////////////////////////////////////

TriMatrix TriMatrix::null()
{
	TriMatrix m;
	memset(m.matrix, 0, sizeof(m.matrix));
	return m;
}

TriMatrix TriMatrix::identity()
{
	TriMatrix m = null();
	m.matrix[0] = 1.0f;
	m.matrix[5] = 1.0f;
	m.matrix[10] = 1.0f;
	m.matrix[15] = 1.0f;
	return m;
}

TriMatrix TriMatrix::translate(float x, float y, float z)
{
	TriMatrix m = identity();
	m.matrix[0 + 3 * 4] = x;
	m.matrix[1 + 3 * 4] = y;
	m.matrix[2 + 3 * 4] = z;
	return m;
}

TriMatrix TriMatrix::scale(float x, float y, float z)
{
	TriMatrix m = null();
	m.matrix[0 + 0 * 4] = x;
	m.matrix[1 + 1 * 4] = y;
	m.matrix[2 + 2 * 4] = z;
	m.matrix[3 + 3 * 4] = 1;
	return m;
}

TriMatrix TriMatrix::rotate(float angle, float x, float y, float z)
{
	float c = cosf(angle);
	float s = sinf(angle);
	TriMatrix m = null();
	m.matrix[0 + 0 * 4] = (x*x*(1.0f - c) + c);
	m.matrix[0 + 1 * 4] = (x*y*(1.0f - c) - z*s);
	m.matrix[0 + 2 * 4] = (x*z*(1.0f - c) + y*s);
	m.matrix[1 + 0 * 4] = (y*x*(1.0f - c) + z*s);
	m.matrix[1 + 1 * 4] = (y*y*(1.0f - c) + c);
	m.matrix[1 + 2 * 4] = (y*z*(1.0f - c) - x*s);
	m.matrix[2 + 0 * 4] = (x*z*(1.0f - c) - y*s);
	m.matrix[2 + 1 * 4] = (y*z*(1.0f - c) + x*s);
	m.matrix[2 + 2 * 4] = (z*z*(1.0f - c) + c);
	m.matrix[3 + 3 * 4] = 1.0f;
	return m;
}

TriMatrix TriMatrix::swapYZ()
{
	TriMatrix m = null();
	m.matrix[0 + 0 * 4] = 1.0f;
	m.matrix[1 + 2 * 4] = 1.0f;
	m.matrix[2 + 1 * 4] = -1.0f;
	m.matrix[3 + 3 * 4] = 1.0f;
	return m;
}

TriMatrix TriMatrix::perspective(float fovy, float aspect, float z_near, float z_far)
{
	float f = (float)(1.0 / tan(fovy * M_PI / 360.0));
	TriMatrix m = null();
	m.matrix[0 + 0 * 4] = f / aspect;
	m.matrix[1 + 1 * 4] = f;
	m.matrix[2 + 2 * 4] = (z_far + z_near) / (z_near - z_far);
	m.matrix[2 + 3 * 4] = (2.0f * z_far * z_near) / (z_near - z_far);
	m.matrix[3 + 2 * 4] = -1.0f;
	return m;
}

TriMatrix TriMatrix::frustum(float left, float right, float bottom, float top, float near, float far)
{
	float a = (right + left) / (right - left);
	float b = (top + bottom) / (top - bottom);
	float c = -(far + near) / (far - near);
	float d = -(2.0f * far) / (far - near);
	TriMatrix m = null();
	m.matrix[0 + 0 * 4] = 2.0f * near / (right - left);
	m.matrix[1 + 1 * 4] = 2.0f * near / (top - bottom);
	m.matrix[0 + 2 * 4] = a;
	m.matrix[1 + 2 * 4] = b;
	m.matrix[2 + 2 * 4] = c;
	m.matrix[2 + 3 * 4] = d;
	m.matrix[3 + 2 * 4] = -1;
	return m;
}

TriMatrix TriMatrix::worldToView()
{
	TriMatrix m = null();
	m.matrix[0 + 0 * 4] = (float)ViewSin;
	m.matrix[0 + 1 * 4] = (float)-ViewCos;
	m.matrix[1 + 2 * 4] = 1.0f;
	m.matrix[2 + 0 * 4] = (float)-ViewCos;
	m.matrix[2 + 1 * 4] = (float)-ViewSin;
	m.matrix[3 + 3 * 4] = 1.0f;
	return m * translate((float)-ViewPos.X, (float)-ViewPos.Y, (float)-ViewPos.Z);
}

TriMatrix TriMatrix::viewToClip()
{
	float near = 5.0f;
	float far = 65536.0f;
	float width = (float)(FocalTangent * near);
	float top = (float)(CenterY / InvZtoScale * near);
	float bottom = (float)(top - viewheight / InvZtoScale * near);
	return frustum(-width, width, bottom, top, near, far);
}

TriMatrix TriMatrix::operator*(const TriMatrix &mult) const
{
	TriMatrix result;
	for (int x = 0; x < 4; x++)
	{
		for (int y = 0; y < 4; y++)
		{
			result.matrix[x + y * 4] =
				matrix[0 * 4 + x] * mult.matrix[y * 4 + 0] +
				matrix[1 * 4 + x] * mult.matrix[y * 4 + 1] +
				matrix[2 * 4 + x] * mult.matrix[y * 4 + 2] +
				matrix[3 * 4 + x] * mult.matrix[y * 4 + 3];
		}
	}
	return result;
}

TriVertex TriMatrix::operator*(TriVertex v) const
{
	float vx = matrix[0 * 4 + 0] * v.x + matrix[1 * 4 + 0] * v.y + matrix[2 * 4 + 0] * v.z + matrix[3 * 4 + 0] * v.w;
	float vy = matrix[0 * 4 + 1] * v.x + matrix[1 * 4 + 1] * v.y + matrix[2 * 4 + 1] * v.z + matrix[3 * 4 + 1] * v.w;
	float vz = matrix[0 * 4 + 2] * v.x + matrix[1 * 4 + 2] * v.y + matrix[2 * 4 + 2] * v.z + matrix[3 * 4 + 2] * v.w;
	float vw = matrix[0 * 4 + 3] * v.x + matrix[1 * 4 + 3] * v.y + matrix[2 * 4 + 3] * v.z + matrix[3 * 4 + 3] * v.w;
	v.x = vx;
	v.y = vy;
	v.z = vz;
	v.w = vw;
	return v;
}
