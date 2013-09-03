/*
** gl_models_md3.cpp
**
**---------------------------------------------------------------------------
** Copyright 2006 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. When not used as part of GZDoom or a GZDoom derivative, this code will be
**    covered by the terms of the GNU Lesser General Public License as published
**    by the Free Software Foundation; either version 2.1 of the License, or (at
**    your option) any later version.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "gl/system/gl_system.h"
#include "w_wad.h"
#include "cmdlib.h"
#include "sc_man.h"
#include "m_crc32.h"

#include "gl/renderer/gl_renderstate.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/models/gl_models.h"
#include "gl/textures/gl_material.h"
#include "gl/shaders/gl_shader.h"

#define MAX_QPATH 64

static void UnpackVector(unsigned short packed, float & nx, float & ny, float & nz)
{
	// decode the lat/lng normal to a 3 float normal
	double lat = ( packed >> 8 ) & 0xff;
	double lng = ( packed & 0xff );
	lat *= PI/128;
	lng *= PI/128;

	nx = cos(lat) * sin(lng);
	ny = sin(lat) * sin(lng);
	nz = cos(lng);
}



bool FMD3Model::Load(const char * path, int, const char * buffer, int length)
{
	#pragma pack(4)
	struct md3_header_t
	{
		DWORD Magic;
		DWORD Version;
		char Name[MAX_QPATH];
		DWORD Flags;
		DWORD Num_Frames;
		DWORD Num_Tags;
		DWORD Num_Surfaces;
		DWORD Num_Skins;
		DWORD Ofs_Frames;
		DWORD Ofs_Tags;
		DWORD Ofs_Surfaces;
		DWORD Ofs_Eof;
	};

	struct md3_surface_t
	{
		DWORD Magic;
		char Name[MAX_QPATH];
		DWORD Flags;
		DWORD Num_Frames;
		DWORD Num_Shaders;
		DWORD Num_Verts;
		DWORD Num_Triangles;
		DWORD Ofs_Triangles;
		DWORD Ofs_Shaders;
		DWORD Ofs_Texcoord;
		DWORD Ofs_XYZNormal;
		DWORD Ofs_End;
	};

	struct md3_triangle_t
	{
		DWORD vt_index[3];
	};

	struct md3_shader_t
	{
		char Name[MAX_QPATH];
		DWORD index;
	};

	struct md3_texcoord_t
	{
		float s,t;
	};

	struct md3_vertex_t
	{
		short x,y,z,n;
	};

	struct md3_frame_t
	{
		float min_Bounds[3];
		float max_Bounds[3];
		float localorigin[3];
		float radius;
		char Name[16];
	};
	#pragma pack()

	md3_header_t * hdr=(md3_header_t *)buffer;

	numFrames = LittleLong(hdr->Num_Frames);
	numTags = LittleLong(hdr->Num_Tags);
	numSurfaces = LittleLong(hdr->Num_Surfaces);

	md3_frame_t * frm = (md3_frame_t*)(buffer + LittleLong(hdr->Ofs_Frames));

	frames = new MD3Frame[numFrames];
	for(int i=0;i<numFrames;i++)
	{
		strncpy(frames[i].Name, frm[i].Name, 16);
		for(int j=0;j<3;j++) frames[i].origin[j] = frm[i].localorigin[j];
	}

	md3_surface_t * surf = (md3_surface_t*)(buffer + LittleLong(hdr->Ofs_Surfaces));

	surfaces = new MD3Surface[numSurfaces];
	for(int i=0;i<numSurfaces;i++)
	{
		MD3Surface * s = &surfaces[i];
		md3_surface_t * ss = surf;

		surf = (md3_surface_t *)(((char*)surf) + LittleLong(surf->Ofs_End));

		s->numSkins = LittleLong(ss->Num_Shaders);
		s->numTriangles = LittleLong(ss->Num_Triangles);
		s->numVertices = LittleLong(ss->Num_Verts);

		// copy triangle indices
		md3_triangle_t * tris = (md3_triangle_t*)(((char*)ss)+LittleLong(ss->Ofs_Triangles));
		s->tris = new MD3Triangle[s->numTriangles];

		for(int i=0;i<s->numTriangles;i++) for (int j=0;j<3;j++)
		{
			s->tris[i].VertIndex[j]=LittleLong(tris[i].vt_index[j]);
		}

		// copy shaders (skins)
		md3_shader_t * shader = (md3_shader_t*)(((char*)ss)+LittleLong(ss->Ofs_Shaders));
		s->skins = new FTexture *[s->numSkins];

		for(int i=0;i<s->numSkins;i++)
		{
			// [BB] According to the MD3 spec, Name is supposed to include the full path.
			s->skins[i] = LoadSkin("", shader[i].Name);
			// [BB] Fall back and check if Name is relative.
			if ( s->skins[i] == NULL )
				s->skins[i] = LoadSkin(path, shader[i].Name);
		}

		// Load texture coordinates
		md3_texcoord_t * tc = (md3_texcoord_t*)(((char*)ss)+LittleLong(ss->Ofs_Texcoord));
		s->texcoords = new MD3TexCoord[s->numVertices];

		for(int i=0;i<s->numVertices;i++)
		{
			s->texcoords[i].s = tc[i].s;
			s->texcoords[i].t = tc[i].t;
		}

		// Load vertices and texture coordinates
		md3_vertex_t * vt = (md3_vertex_t*)(((char*)ss)+LittleLong(ss->Ofs_XYZNormal));
		s->vertices = new MD3Vertex[s->numVertices * numFrames];

		for(int i=0;i<s->numVertices * numFrames;i++)
		{
			s->vertices[i].x = LittleShort(vt[i].x)/64.f;
			s->vertices[i].y = LittleShort(vt[i].y)/64.f;
			s->vertices[i].z = LittleShort(vt[i].z)/64.f * rModelAspectMod;
			UnpackVector( LittleShort(vt[i].n), s->vertices[i].nx, s->vertices[i].ny, s->vertices[i].nz);
		}
	}
	return true;
}

int FMD3Model::FindFrame(const char * name)
{
	for (int i=0;i<numFrames;i++)
	{
		if (!stricmp(name, frames[i].Name)) return i;
	}
	return -1;
}

void FMD3Model::RenderTriangles(MD3Surface * surf, MD3Vertex * vert)
{
	gl_RenderState.Apply();
	glBegin(GL_TRIANGLES);
	for(int i=0; i<surf->numTriangles;i++)
	{
		for(int j=0;j<3;j++)
		{
			int x = surf->tris[i].VertIndex[j];

			glTexCoord2fv(&surf->texcoords[x].s);
			glVertex3f(vert[x].x, vert[x].z, vert[x].y);
		}
	}
	glEnd();
}

void FMD3Model::RenderFrame(FTexture * skin, int frameno, int cm, int translation)
{
	if (frameno>=numFrames) return;

	MD3Frame * frame = &frames[frameno];

	// I can't confirm correctness of this because no model I have tested uses this information
	// glMatrixMode(GL_MODELVIEW);
	// glTranslatef(frame->origin[0], frame->origin[1], frame->origin[2]);

	for(int i=0;i<numSurfaces;i++)
	{
		MD3Surface * surf = &surfaces[i];

		// [BB] In case no skin is specified via MODELDEF, check if the MD3 has a skin for the current surface.
		// Note: Each surface may have a different skin.
		FTexture *surfaceSkin = skin;
		if (!surfaceSkin)
		{
			if (surf->numSkins==0) return;
			surfaceSkin = surf->skins[0];
			if (!surfaceSkin) return;
		}

		FMaterial * tex = FMaterial::ValidateTexture(surfaceSkin);

		tex->Bind(cm, 0, translation);
		RenderTriangles(surf, surf->vertices + frameno * surf->numVertices);
	}
}

void FMD3Model::RenderFrameInterpolated(FTexture * skin, int frameno, int frameno2, double inter, int cm, int translation)
{
	if (frameno>=numFrames || frameno2>=numFrames) return;

	for(int i=0;i<numSurfaces;i++)
	{
		MD3Surface * surf = &surfaces[i];

		// [BB] In case no skin is specified via MODELDEF, check if the MD3 has a skin for the current surface.
		// Note: Each surface may have a different skin.
		FTexture *surfaceSkin = skin;
		if (!surfaceSkin)
		{
			if (surf->numSkins==0) return;
			surfaceSkin = surf->skins[0];
			if (!surfaceSkin) return;
		}

		FMaterial * tex = FMaterial::ValidateTexture(surfaceSkin);

		tex->Bind(cm, 0, translation);

		MD3Vertex* verticesInterpolated = new MD3Vertex[surfaces[i].numVertices];
		MD3Vertex* vertices1 = surf->vertices + frameno * surf->numVertices;
		MD3Vertex* vertices2 = surf->vertices + frameno2 * surf->numVertices;

		// [BB] Calculate the interpolated vertices by linear interpolation.
		for( int k = 0; k < surf->numVertices; k++ )
		{
			verticesInterpolated[k].x = (1-inter)*vertices1[k].x+ (inter)*vertices2[k].x;
			verticesInterpolated[k].y = (1-inter)*vertices1[k].y+ (inter)*vertices2[k].y;
			verticesInterpolated[k].z = (1-inter)*vertices1[k].z+ (inter)*vertices2[k].z;
			// [BB] Apparently RenderTriangles doesn't use nx, ny, nz, so don't interpolate them.
		}

		RenderTriangles(surf, verticesInterpolated);

		delete[] verticesInterpolated;
	}
}

FMD3Model::~FMD3Model()
{
	if (frames) delete [] frames;
	if (surfaces) delete [] surfaces;
	frames = NULL;
	surfaces = NULL;
}
