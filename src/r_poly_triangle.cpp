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
#include <immintrin.h>

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

void PolyTriangleDrawer::draw(const PolyDrawArgs &args, TriDrawVariant variant, TriBlendMode blendmode)
{
	DrawerCommandQueue::QueueCommand<DrawPolyTrianglesCommand>(args, variant, blendmode, mirror);
}

void PolyTriangleDrawer::draw_arrays(const PolyDrawArgs &drawargs, TriDrawVariant variant, TriBlendMode blendmode, WorkerThreadData *thread)
{
	if (drawargs.vcount < 3)
		return;

	auto llvm = Drawers::Instance();
	void(*drawfunc)(const TriDrawTriangleArgs *, WorkerThreadData *);
	int bmode = (int)blendmode;
	switch (variant)
	{
	default:
	//case TriDrawVariant::DrawNormal: drawfunc = &ScreenTriangle::DrawFunc; break;
	case TriDrawVariant::DrawNormal: drawfunc = dest_bgra ? llvm->TriDrawNormal32[bmode] : llvm->TriDrawNormal8[bmode]; break;
	case TriDrawVariant::FillNormal: drawfunc = dest_bgra ? llvm->TriFillNormal32[bmode] : llvm->TriFillNormal8[bmode]; break;
	case TriDrawVariant::DrawSubsector: drawfunc = dest_bgra ? llvm->TriDrawSubsector32[bmode] : llvm->TriDrawSubsector8[bmode]; break;
	case TriDrawVariant::FuzzSubsector:
	case TriDrawVariant::FillSubsector: drawfunc = dest_bgra ? llvm->TriFillSubsector32[bmode] : llvm->TriFillSubsector8[bmode]; break;
	case TriDrawVariant::Stencil: drawfunc = llvm->TriStencil; break;
	case TriDrawVariant::StencilClose: drawfunc = llvm->TriStencilClose; break;
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

	ShadedTriVertex vert[3];
	if (drawargs.mode == TriangleDrawMode::Normal)
	{
		for (int i = 0; i < vcount / 3; i++)
		{
			for (int j = 0; j < 3; j++)
				vert[j] = shade_vertex(*drawargs.objectToClip, drawargs.clipPlane, *(vinput++));
			draw_shaded_triangle(vert, ccw, &args, thread, drawfunc);
		}
	}
	else if (drawargs.mode == TriangleDrawMode::Fan)
	{
		vert[0] = shade_vertex(*drawargs.objectToClip, drawargs.clipPlane, *(vinput++));
		vert[1] = shade_vertex(*drawargs.objectToClip, drawargs.clipPlane, *(vinput++));
		for (int i = 2; i < vcount; i++)
		{
			vert[2] = shade_vertex(*drawargs.objectToClip, drawargs.clipPlane, *(vinput++));
			draw_shaded_triangle(vert, ccw, &args, thread, drawfunc);
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
			draw_shaded_triangle(vert, ccw, &args, thread, drawfunc);
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

void PolyTriangleDrawer::draw_shaded_triangle(const ShadedTriVertex *vert, bool ccw, TriDrawTriangleArgs *args, WorkerThreadData *thread, void(*drawfunc)(const TriDrawTriangleArgs *, WorkerThreadData *))
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

DrawPolyTrianglesCommand::DrawPolyTrianglesCommand(const PolyDrawArgs &args, TriDrawVariant variant, TriBlendMode blendmode, bool mirror)
	: args(args), variant(variant), blendmode(blendmode)
{
	if (mirror)
		this->args.ccw = !this->args.ccw;
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
	FString variantstr;
	switch (variant)
	{
	default: variantstr = "Unknown"; break;
	case TriDrawVariant::DrawNormal: variantstr = "DrawNormal"; break;
	case TriDrawVariant::FillNormal: variantstr = "FillNormal"; break;
	case TriDrawVariant::DrawSubsector: variantstr = "DrawSubsector"; break;
	case TriDrawVariant::FillSubsector: variantstr = "FillSubsector"; break;
	case TriDrawVariant::FuzzSubsector: variantstr = "FuzzSubsector"; break;
	case TriDrawVariant::Stencil: variantstr = "Stencil"; break;
	}

	FString blendmodestr;
	switch (blendmode)
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
	info.Format("DrawPolyTriangles: variant = %s, blend mode = %s, color = %d, light = %d, textureWidth = %d, textureHeight = %d, texture = %s, translation = %s, colormaps = %s",
		variantstr.GetChars(), blendmodestr.GetChars(), args.uniforms.color, args.uniforms.light, args.textureWidth, args.textureHeight,
		args.texturePixels ? "ptr" : "null", args.translation ? "ptr" : "null", args.colormaps ? "ptr" : "null");
	return info;
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
	float top = (float)(swrenderer::CenterY / swrenderer::InvZtoScale * near);
	float bottom = (float)(top - viewheight / swrenderer::InvZtoScale * near);
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

ShadedTriVertex TriMatrix::operator*(TriVertex v) const
{
	float vx = matrix[0 * 4 + 0] * v.x + matrix[1 * 4 + 0] * v.y + matrix[2 * 4 + 0] * v.z + matrix[3 * 4 + 0] * v.w;
	float vy = matrix[0 * 4 + 1] * v.x + matrix[1 * 4 + 1] * v.y + matrix[2 * 4 + 1] * v.z + matrix[3 * 4 + 1] * v.w;
	float vz = matrix[0 * 4 + 2] * v.x + matrix[1 * 4 + 2] * v.y + matrix[2 * 4 + 2] * v.z + matrix[3 * 4 + 2] * v.w;
	float vw = matrix[0 * 4 + 3] * v.x + matrix[1 * 4 + 3] * v.y + matrix[2 * 4 + 3] * v.z + matrix[3 * 4 + 3] * v.w;
	ShadedTriVertex sv;
	sv.x = vx;
	sv.y = vy;
	sv.z = vz;
	sv.w = vw;
	for (int i = 0; i < TriVertex::NumVarying; i++)
		sv.varying[i] = v.varying[i];
	return sv;
}

/////////////////////////////////////////////////////////////////////////////

namespace
{
	int NextBufferVertex = 0;
}

TriVertex *PolyVertexBuffer::GetVertices(int count)
{
	enum { VertexBufferSize = 256 * 1024 };
	static TriVertex Vertex[VertexBufferSize];

	if (NextBufferVertex + count > VertexBufferSize)
		return nullptr;
	TriVertex *v = Vertex + NextBufferVertex;
	NextBufferVertex += count;
	return v;
}

void PolyVertexBuffer::Clear()
{
	NextBufferVertex = 0;
}

/////////////////////////////////////////////////////////////////////////////
#if 0
void ScreenTriangle::Setup(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	const TriVertex &v1 = *args->v1;
	const TriVertex &v2 = *args->v2;
	const TriVertex &v3 = *args->v3;
	int clipright = args->clipright;
	int clipbottom = args->clipbottom;
	
	int stencilPitch = args->stencilPitch;
	uint8_t *stencilValues = args->stencilValues;
	uint32_t *stencilMasks = args->stencilMasks;
	uint8_t stencilTestValue = args->stencilTestValue;

	ScreenTriangleFullSpan *span = FullSpans;
	ScreenTrianglePartialBlock *partial = PartialBlocks;
	span->Length = 0;

	// 28.4 fixed-point coordinates
	const int Y1 = (int)round(16.0f * v1.y);
	const int Y2 = (int)round(16.0f * v2.y);
	const int Y3 = (int)round(16.0f * v3.y);

	const int X1 = (int)round(16.0f * v1.x);
	const int X2 = (int)round(16.0f * v2.x);
	const int X3 = (int)round(16.0f * v3.x);

	// Deltas
	const int DX12 = X1 - X2;
	const int DX23 = X2 - X3;
	const int DX31 = X3 - X1;

	const int DY12 = Y1 - Y2;
	const int DY23 = Y2 - Y3;
	const int DY31 = Y3 - Y1;

	// Fixed-point deltas
	const int FDX12 = DX12 << 4;
	const int FDX23 = DX23 << 4;
	const int FDX31 = DX31 << 4;

	const int FDY12 = DY12 << 4;
	const int FDY23 = DY23 << 4;
	const int FDY31 = DY31 << 4;

	// Bounding rectangle
	int minx = MAX((MIN(MIN(X1, X2), X3) + 0xF) >> 4, 0);
	int maxx = MIN((MAX(MAX(X1, X2), X3) + 0xF) >> 4, clipright - 1);
	int miny = MAX((MIN(MIN(Y1, Y2), Y3) + 0xF) >> 4, 0);
	int maxy = MIN((MAX(MAX(Y1, Y2), Y3) + 0xF) >> 4, clipbottom - 1);
	if (minx >= maxx || miny >= maxy)
		return;

	// Block size, standard 8x8 (must be power of two)
	const int q = 8;

	// Start in corner of 8x8 block
	minx &= ~(q - 1);
	miny &= ~(q - 1);

	// Half-edge constants
	int C1 = DY12 * X1 - DX12 * Y1;
	int C2 = DY23 * X2 - DX23 * Y2;
	int C3 = DY31 * X3 - DX31 * Y3;

	// Correct for fill convention
	if (DY12 < 0 || (DY12 == 0 && DX12 > 0)) C1++;
	if (DY23 < 0 || (DY23 == 0 && DX23 > 0)) C2++;
	if (DY31 < 0 || (DY31 == 0 && DX31 > 0)) C3++;

	// First block line for this thread
	int core = thread->core;
	int num_cores = thread->num_cores;
	int core_skip = (num_cores - ((miny / q) - core) % num_cores) % num_cores;
	miny += core_skip * q;

	__m128i mC1 = _mm_set1_epi32(C1);
	__m128i mC2 = _mm_set1_epi32(C2);
	__m128i mC3 = _mm_set1_epi32(C3);
	__m128i mDX12 = _mm_set1_epi32(DX12);
	__m128i mDX23 = _mm_set1_epi32(DX23);
	__m128i mDX31 = _mm_set1_epi32(DX31);
	__m128i mDY12 = _mm_set1_epi32(DY12);
	__m128i mDY23 = _mm_set1_epi32(DY23);
	__m128i mDY31 = _mm_set1_epi32(DY31);

	// Loop through blocks
	for (int y = miny; y < maxy; y += q * num_cores)
	{
		// Corners of block
		int x0 = minx << 4;
		int x1 = (minx + q - 1) << 4;
		int y0 = y << 4;
		int y1 = (y + q - 1) << 4;

		__m128i my0y1 = _mm_set_epi32(y0, y0, y1, y1);
		__m128i mx0x1 = _mm_set_epi32(x0, x1, x0, x1);
		__m128i mAxx = _mm_add_epi32(mC1, _mm_sub_epi32(_mm_mullo_epi32(mDX12, my0y1), _mm_mullo_epi32(mDY12, mx0x1)));
		__m128i mBxx = _mm_add_epi32(mC2, _mm_sub_epi32(_mm_mullo_epi32(mDX23, my0y1), _mm_mullo_epi32(mDY23, mx0x1)));
		__m128i mCxx = _mm_add_epi32(mC3, _mm_sub_epi32(_mm_mullo_epi32(mDX31, my0y1), _mm_mullo_epi32(mDY31, mx0x1)));

		for (int x = minx; x < maxx; x += q)
		{
			// Evaluate half-space functions
			int a = _mm_movemask_epi8(_mm_cmpgt_epi32(mAxx, _mm_setzero_si128()));
			int b = _mm_movemask_epi8(_mm_cmpgt_epi32(mBxx, _mm_setzero_si128()));
			int c = _mm_movemask_epi8(_mm_cmpgt_epi32(mCxx, _mm_setzero_si128()));

			mAxx = _mm_sub_epi32(mAxx, _mm_slli_epi32(mDY12, 7));
			mBxx = _mm_sub_epi32(mBxx, _mm_slli_epi32(mDY23, 7));
			mCxx = _mm_sub_epi32(mCxx, _mm_slli_epi32(mDY31, 7));
			
			// Stencil test the whole block, if possible
			int block = x / 8 + y / 8 * stencilPitch;
			uint8_t *stencilBlock = &stencilValues[block * 64];
			uint32_t *stencilBlockMask = &stencilMasks[block];
			bool blockIsSingleStencil = ((*stencilBlockMask) & 0xffffff00) == 0xffffff00;
			bool skipBlock = blockIsSingleStencil && ((*stencilBlockMask) & 0xff) != stencilTestValue;

			// Skip block when outside an edge
			if (a == 0 || b == 0 || c == 0 || skipBlock)
			{
				if (span->Length != 0)
				{
					span++;
					span->Length = 0;
				}
				continue;
			}

			// Accept whole block when totally covered
			if (a == 0xffff && b == 0xffff && c == 0xffff && x + q <= clipright && y + q <= clipbottom && blockIsSingleStencil)
			{
				if (span->Length != 0)
				{
					span->Length++;
				}
				else
				{
					span->X = x;
					span->Y = y;
					span->Length = 1;
				}
			}
			else // Partially covered block
			{
				x0 = x << 4;
				x1 = (x + q - 1) << 4;
				int CY1 = C1 + DX12 * y0 - DY12 * x0;
				int CY2 = C2 + DX23 * y0 - DY23 * x0;
				int CY3 = C3 + DX31 * y0 - DY31 * x0;

				uint32_t mask0 = 0;
				uint32_t mask1 = 0;

				for (int iy = 0; iy < 4; iy++)
				{
					int CX1 = CY1;
					int CX2 = CY2;
					int CX3 = CY3;

					for (int ix = 0; ix < q; ix++)
					{
						bool passStencilTest = blockIsSingleStencil || stencilBlock[ix + iy * q] == stencilTestValue;
						bool covered = (CX1 > 0 && CX2 > 0 && CX3 > 0 && (x + ix) < clipright && (y + iy) < clipbottom && passStencilTest);
						mask0 <<= 1;
						mask0 |= (uint32_t)covered;

						CX1 -= FDY12;
						CX2 -= FDY23;
						CX3 -= FDY31;
					}

					CY1 += FDX12;
					CY2 += FDX23;
					CY3 += FDX31;
				}

				for (int iy = 4; iy < q; iy++)
				{
					int CX1 = CY1;
					int CX2 = CY2;
					int CX3 = CY3;

					for (int ix = 0; ix < q; ix++)
					{
						bool passStencilTest = blockIsSingleStencil || stencilBlock[ix + iy * q] == stencilTestValue;
						bool covered = (CX1 > 0 && CX2 > 0 && CX3 > 0 && (x + ix) < clipright && (y + iy) < clipbottom && passStencilTest);
						mask1 <<= 1;
						mask1 |= (uint32_t)covered;

						CX1 -= FDY12;
						CX2 -= FDY23;
						CX3 -= FDY31;
					}

					CY1 += FDX12;
					CY2 += FDX23;
					CY3 += FDX31;
				}

				if (mask0 != 0xffffffff || mask1 != 0xffffffff)
				{
					if (span->Length > 0)
					{
						span++;
						span->Length = 0;
					}

					partial->X = x;
					partial->Y = y;
					partial->Mask0 = mask0;
					partial->Mask1 = mask1;
					partial++;
				}
				else if (span->Length != 0)
				{
					span->Length++;
				}
				else
				{
					span->X = x;
					span->Y = y;
					span->Length = 1;
				}
			}
		}

		if (span->Length > 0)
		{
			span++;
			span->Length = 0;
		}
	}

	NumFullSpans = (int)(span - FullSpans);
	NumPartialBlocks = (int)(partial - PartialBlocks);
}
#else
void ScreenTriangle::Setup(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	const TriVertex &v1 = *args->v1;
	const TriVertex &v2 = *args->v2;
	const TriVertex &v3 = *args->v3;
	int clipright = args->clipright;
	int clipbottom = args->clipbottom;
	
	int stencilPitch = args->stencilPitch;
	uint8_t *stencilValues = args->stencilValues;
	uint32_t *stencilMasks = args->stencilMasks;
	uint8_t stencilTestValue = args->stencilTestValue;
	
	ScreenTriangleFullSpan *span = FullSpans;
	ScreenTrianglePartialBlock *partial = PartialBlocks;
	span->Length = 0;
	
	// 28.4 fixed-point coordinates
	const int Y1 = (int)round(16.0f * v1.y);
	const int Y2 = (int)round(16.0f * v2.y);
	const int Y3 = (int)round(16.0f * v3.y);
	
	const int X1 = (int)round(16.0f * v1.x);
	const int X2 = (int)round(16.0f * v2.x);
	const int X3 = (int)round(16.0f * v3.x);
	
	// Deltas
	const int DX12 = X1 - X2;
	const int DX23 = X2 - X3;
	const int DX31 = X3 - X1;
	
	const int DY12 = Y1 - Y2;
	const int DY23 = Y2 - Y3;
	const int DY31 = Y3 - Y1;
	
	// Fixed-point deltas
	const int FDX12 = DX12 << 4;
	const int FDX23 = DX23 << 4;
	const int FDX31 = DX31 << 4;
	
	const int FDY12 = DY12 << 4;
	const int FDY23 = DY23 << 4;
	const int FDY31 = DY31 << 4;
	
	// Bounding rectangle
	int minx = MAX((MIN(MIN(X1, X2), X3) + 0xF) >> 4, 0);
	int maxx = MIN((MAX(MAX(X1, X2), X3) + 0xF) >> 4, clipright - 1);
	int miny = MAX((MIN(MIN(Y1, Y2), Y3) + 0xF) >> 4, 0);
	int maxy = MIN((MAX(MAX(Y1, Y2), Y3) + 0xF) >> 4, clipbottom - 1);
	if (minx >= maxx || miny >= maxy)
		return;
	
	// Block size, standard 8x8 (must be power of two)
	const int q = 8;
	
	// Start in corner of 8x8 block
	minx &= ~(q - 1);
	miny &= ~(q - 1);
	
	// Half-edge constants
	int C1 = DY12 * X1 - DX12 * Y1;
	int C2 = DY23 * X2 - DX23 * Y2;
	int C3 = DY31 * X3 - DX31 * Y3;
	
	// Correct for fill convention
	if (DY12 < 0 || (DY12 == 0 && DX12 > 0)) C1++;
	if (DY23 < 0 || (DY23 == 0 && DX23 > 0)) C2++;
	if (DY31 < 0 || (DY31 == 0 && DX31 > 0)) C3++;
	
	// First block line for this thread
	int core = thread->core;
	int num_cores = thread->num_cores;
	int core_skip = (num_cores - ((miny / q) - core) % num_cores) % num_cores;
	miny += core_skip * q;
	
	// Loop through blocks
	for (int y = miny; y < maxy; y += q * num_cores)
	{
		for (int x = minx; x < maxx; x += q)
		{
			// Corners of block
			int x0 = x << 4;
			int x1 = (x + q - 1) << 4;
			int y0 = y << 4;
			int y1 = (y + q - 1) << 4;
			
			// Evaluate half-space functions
			bool a00 = C1 + DX12 * y0 - DY12 * x0 > 0;
			bool a10 = C1 + DX12 * y0 - DY12 * x1 > 0;
			bool a01 = C1 + DX12 * y1 - DY12 * x0 > 0;
			bool a11 = C1 + DX12 * y1 - DY12 * x1 > 0;
			int a = (a00 << 0) | (a10 << 1) | (a01 << 2) | (a11 << 3);
			
			bool b00 = C2 + DX23 * y0 - DY23 * x0 > 0;
			bool b10 = C2 + DX23 * y0 - DY23 * x1 > 0;
			bool b01 = C2 + DX23 * y1 - DY23 * x0 > 0;
			bool b11 = C2 + DX23 * y1 - DY23 * x1 > 0;
			int b = (b00 << 0) | (b10 << 1) | (b01 << 2) | (b11 << 3);
			
			bool c00 = C3 + DX31 * y0 - DY31 * x0 > 0;
			bool c10 = C3 + DX31 * y0 - DY31 * x1 > 0;
			bool c01 = C3 + DX31 * y1 - DY31 * x0 > 0;
			bool c11 = C3 + DX31 * y1 - DY31 * x1 > 0;
			int c = (c00 << 0) | (c10 << 1) | (c01 << 2) | (c11 << 3);
			
			// Stencil test the whole block, if possible
			int block = x / 8 + y / 8 * stencilPitch;
			uint8_t *stencilBlock = &stencilValues[block * 64];
			uint32_t *stencilBlockMask = &stencilMasks[block];
			bool blockIsSingleStencil = ((*stencilBlockMask) & 0xffffff00) == 0xffffff00;
			bool skipBlock = blockIsSingleStencil && ((*stencilBlockMask) & 0xff) != stencilTestValue;

			// Skip block when outside an edge
			if (a == 0 || b == 0 || c == 0 || skipBlock)
			{
				if (span->Length != 0)
				{
					span++;
					span->Length = 0;
				}
				continue;
			}

			// Accept whole block when totally covered
			if (a == 0xf && b == 0xf && c == 0xf && x + q <= clipright && y + q <= clipbottom && blockIsSingleStencil)
			{
				if (span->Length != 0)
				{
					span->Length++;
				}
				else
				{
					span->X = x;
					span->Y = y;
					span->Length = 1;
				}
			}
			else // Partially covered block
			{
				x0 = x << 4;
				x1 = (x + q - 1) << 4;
				int CY1 = C1 + DX12 * y0 - DY12 * x0;
				int CY2 = C2 + DX23 * y0 - DY23 * x0;
				int CY3 = C3 + DX31 * y0 - DY31 * x0;

				uint32_t mask0 = 0;
				uint32_t mask1 = 0;

				for (int iy = 0; iy < 4; iy++)
				{
					int CX1 = CY1;
					int CX2 = CY2;
					int CX3 = CY3;

					for (int ix = 0; ix < q; ix++)
					{
						bool passStencilTest = blockIsSingleStencil || stencilBlock[ix + iy * q] == stencilTestValue;
						bool covered = (CX1 > 0 && CX2 > 0 && CX3 > 0 && (x + ix) < clipright && (y + iy) < clipbottom && passStencilTest);
						mask0 <<= 1;
						mask0 |= (uint32_t)covered;

						CX1 -= FDY12;
						CX2 -= FDY23;
						CX3 -= FDY31;
					}

					CY1 += FDX12;
					CY2 += FDX23;
					CY3 += FDX31;
				}

				for (int iy = 4; iy < q; iy++)
				{
					int CX1 = CY1;
					int CX2 = CY2;
					int CX3 = CY3;

					for (int ix = 0; ix < q; ix++)
					{
						bool passStencilTest = blockIsSingleStencil || stencilBlock[ix + iy * q] == stencilTestValue;
						bool covered = (CX1 > 0 && CX2 > 0 && CX3 > 0 && (x + ix) < clipright && (y + iy) < clipbottom && passStencilTest);
						mask1 <<= 1;
						mask1 |= (uint32_t)covered;

						CX1 -= FDY12;
						CX2 -= FDY23;
						CX3 -= FDY31;
					}

					CY1 += FDX12;
					CY2 += FDX23;
					CY3 += FDX31;
				}

				if (mask0 != 0xffffffff || mask1 != 0xffffffff)
				{
					if (span->Length > 0)
					{
						span++;
						span->Length = 0;
					}

					partial->X = x;
					partial->Y = y;
					partial->Mask0 = mask0;
					partial->Mask1 = mask1;
					partial++;
				}
				else if (span->Length != 0)
				{
					span->Length++;
				}
				else
				{
					span->X = x;
					span->Y = y;
					span->Length = 1;
				}
			}
		}
		
		if (span->Length != 0)
		{
			span++;
			span->Length = 0;
		}
	}
	
	NumFullSpans = (int)(span - FullSpans);
	NumPartialBlocks = (int)(partial - PartialBlocks);
}
#endif

void ScreenTriangle::Draw(const TriDrawTriangleArgs *args)
{
	float r = args->v1->x / 255.0f;
	float g = args->v1->y / 255.0f;
	float b = args->v1->z / 255.0f;
	r = (r - floor(r)) * 255;
	g = (g - floor(g)) * 255;
	b = (b - floor(b)) * 255;

	uint32_t red = (uint32_t)r;
	uint32_t green = (uint32_t)g;
	uint32_t blue = (uint32_t)b;
	uint32_t solidcolor = 0xff000000 | (red << 16) | (green << 8) | blue;

	for (int i = 0; i < NumFullSpans; i++)
	{
		const auto &span = FullSpans[i];
		
		uint32_t *dest = (uint32_t*)args->dest + span.X + span.Y * args->pitch;
		int pitch = args->pitch;
		int width = span.Length * 8;
		int height = 8;
		for (int y = 0; y < height; y++)
		{
			for (int x = 0; x < width; x++)
			{
				dest[x] = solidcolor;
			}
		
			dest += pitch;
		}
	}
	
	for (int i = 0; i < NumPartialBlocks; i++)
	{
		const auto &block = PartialBlocks[i];
		
		uint32_t *dest = (uint32_t*)args->dest + block.X + block.Y * args->pitch;
		int pitch = args->pitch;
		uint32_t mask0 = block.Mask0;
		uint32_t mask1 = block.Mask1;
		for (int y = 0; y < 4; y++)
		{
			for (int x = 0; x < 8; x++)
			{
				if (mask0 & (1<<31))
					dest[x] = solidcolor;
				mask0 <<= 1;
			}
			dest += pitch;
		}
		for (int y = 4; y < 8; y++)
		{
			for (int x = 0; x < 8; x++)
			{
				if (mask1 & (1<<31))
					dest[x] = solidcolor;
				mask1 <<= 1;
			}
			dest += pitch;
		}
	}
}

void ScreenTriangle::DrawFunc(const TriDrawTriangleArgs *args, WorkerThreadData *thread)
{
	static ScreenTriangle triangle[8];
	
	triangle[thread->core].Setup(args, thread);
	triangle[thread->core].Draw(args);
}

ScreenTriangle::ScreenTriangle()
{
	FullSpansBuffer.resize(MAXWIDTH / 8 * (MAXHEIGHT / 8));
	PartialBlocksBuffer.resize(MAXWIDTH / 8 * (MAXHEIGHT / 8));
	FullSpans = FullSpansBuffer.data();
	PartialBlocks = PartialBlocksBuffer.data();
}
