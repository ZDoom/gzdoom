// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2006-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//

#include "w_wad.h"
#include "cmdlib.h"
#include "sc_man.h"
#include "m_crc32.h"
#include "r_data/models/models.h"

#define MAX_QPATH 64

#ifdef _MSC_VER
#pragma warning(disable:4244) // warning C4244: conversion from 'double' to 'float', possible loss of data
#endif

//===========================================================================
//
// decode the lat/lng normal to a 3 float normal
//
//===========================================================================

static void UnpackVector(unsigned short packed, float & nx, float & ny, float & nz)
{
	double lat = ( packed >> 8 ) & 0xff;
	double lng = ( packed & 0xff );
	lat *= M_PI/128;
	lng *= M_PI/128;

	nx = cos(lat) * sin(lng);
	ny = sin(lat) * sin(lng);
	nz = cos(lng);
}

//===========================================================================
//
// MD3 File structure
//
//===========================================================================

#pragma pack(4)
struct md3_header_t
{
	uint32_t Magic;
	uint32_t Version;
	char Name[MAX_QPATH];
	uint32_t Flags;
	uint32_t Num_Frames;
	uint32_t Num_Tags;
	uint32_t Num_Surfaces;
	uint32_t Num_Skins;
	uint32_t Ofs_Frames;
	uint32_t Ofs_Tags;
	uint32_t Ofs_Surfaces;
	uint32_t Ofs_Eof;
};

struct md3_surface_t
{
	uint32_t Magic;
	char Name[MAX_QPATH];
	uint32_t Flags;
	uint32_t Num_Frames;
	uint32_t Num_Shaders;
	uint32_t Num_Verts;
	uint32_t Num_Triangles;
	uint32_t Ofs_Triangles;
	uint32_t Ofs_Shaders;
	uint32_t Ofs_Texcoord;
	uint32_t Ofs_XYZNormal;
	uint32_t Ofs_End;
};

struct md3_triangle_t
{
	uint32_t vt_index[3];
};

struct md3_shader_t
{
	char Name[MAX_QPATH];
	uint32_t index;
};

struct md3_texcoord_t
{
	float s, t;
};

struct md3_vertex_t
{
	short x, y, z, n;
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


//===========================================================================
//
//
//
//===========================================================================

bool FMD3Model::Load(const char * path, int lumpnum, const char * buffer, int length)
{
	md3_header_t * hdr = (md3_header_t *)buffer;

	numFrames = LittleLong(hdr->Num_Frames);
	numTags = LittleLong(hdr->Num_Tags);
	numSurfaces = LittleLong(hdr->Num_Surfaces);

	md3_frame_t * frm = (md3_frame_t*)(buffer + LittleLong(hdr->Ofs_Frames));

	frames = new MD3Frame[numFrames];
	for (int i = 0; i < numFrames; i++)
	{
		strncpy(frames[i].Name, frm[i].Name, 16);
		for (int j = 0; j < 3; j++) frames[i].origin[j] = frm[i].localorigin[j];
	}

	md3_surface_t * surf = (md3_surface_t*)(buffer + LittleLong(hdr->Ofs_Surfaces));

	surfaces = new MD3Surface[numSurfaces];

	for (int i = 0; i < numSurfaces; i++)
	{
		MD3Surface * s = &surfaces[i];
		md3_surface_t * ss = surf;

		surf = (md3_surface_t *)(((char*)surf) + LittleLong(surf->Ofs_End));

		s->numSkins = LittleLong(ss->Num_Shaders);
		s->numTriangles = LittleLong(ss->Num_Triangles);
		s->numVertices = LittleLong(ss->Num_Verts);

		// copy shaders (skins)
		md3_shader_t * shader = (md3_shader_t*)(((char*)ss) + LittleLong(ss->Ofs_Shaders));
		s->skins = new FTextureID[s->numSkins];

		for (int i = 0; i < s->numSkins; i++)
		{
			// [BB] According to the MD3 spec, Name is supposed to include the full path.
			s->skins[i] = LoadSkin("", shader[i].Name);
			// [BB] Fall back and check if Name is relative.
			if (!s->skins[i].isValid())
				s->skins[i] = LoadSkin(path, shader[i].Name);
		}
	}
	mLumpNum = lumpnum;
	return true;
}

//===========================================================================
//
//
//
//===========================================================================

void FMD3Model::LoadGeometry()
{
	FMemLump lumpdata = Wads.ReadLump(mLumpNum);
	const char *buffer = (const char *)lumpdata.GetMem();
	md3_header_t * hdr = (md3_header_t *)buffer;
	md3_surface_t * surf = (md3_surface_t*)(buffer + LittleLong(hdr->Ofs_Surfaces));

	for(int i=0;i<numSurfaces;i++)
	{
		MD3Surface * s = &surfaces[i];
		md3_surface_t * ss = surf;

		surf = (md3_surface_t *)(((char*)surf) + LittleLong(surf->Ofs_End));

		// copy triangle indices
		md3_triangle_t * tris = (md3_triangle_t*)(((char*)ss)+LittleLong(ss->Ofs_Triangles));
		s->tris = new MD3Triangle[s->numTriangles];

		for(int i=0;i<s->numTriangles;i++) for (int j=0;j<3;j++)
		{
			s->tris[i].VertIndex[j]=LittleLong(tris[i].vt_index[j]);
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
			s->vertices[i].z = LittleShort(vt[i].z)/64.f;
			UnpackVector( LittleShort(vt[i].n), s->vertices[i].nx, s->vertices[i].ny, s->vertices[i].nz);
		}
	}
}

//===========================================================================
//
//
//
//===========================================================================

void FMD3Model::BuildVertexBuffer(FModelRenderer *renderer)
{
	if (mVBuf == nullptr)
	{
		LoadGeometry();

		unsigned int vbufsize = 0;
		unsigned int ibufsize = 0;

		for (int i = 0; i < numSurfaces; i++)
		{
			MD3Surface * surf = &surfaces[i];
			vbufsize += numFrames * surf->numVertices;
			ibufsize += 3 * surf->numTriangles;
		}

		mVBuf = renderer->CreateVertexBuffer(true, numFrames == 1);
		FModelVertex *vertptr = mVBuf->LockVertexBuffer(vbufsize);
		unsigned int *indxptr = mVBuf->LockIndexBuffer(ibufsize);

		assert(vertptr != nullptr && indxptr != nullptr);

		unsigned int vindex = 0, iindex = 0;

		for (int i = 0; i < numSurfaces; i++)
		{
			MD3Surface * surf = &surfaces[i];

			surf->vindex = vindex;
			surf->iindex = iindex;
			for (int j = 0; j < numFrames * surf->numVertices; j++)
			{
				MD3Vertex* vert = surf->vertices + j;

				FModelVertex *bvert = &vertptr[vindex++];

				int tc = j % surf->numVertices;
				bvert->Set(vert->x, vert->z, vert->y, surf->texcoords[tc].s, surf->texcoords[tc].t);
				bvert->SetNormal(vert->nx, vert->nz, vert->ny);
			}

			for (int k = 0; k < surf->numTriangles; k++)
			{
				for (int l = 0; l < 3; l++)
				{
					indxptr[iindex++] = surf->tris[k].VertIndex[l];
				}
			}
			surf->UnloadGeometry();
		}
		mVBuf->UnlockVertexBuffer();
		mVBuf->UnlockIndexBuffer();
	}
}


//===========================================================================
//
// for skin precaching
//
//===========================================================================

void FMD3Model::AddSkins(uint8_t *hitlist)
{
	for (int i = 0; i < numSurfaces; i++)
	{
		if (curSpriteMDLFrame->surfaceskinIDs[curMDLIndex][i].isValid())
		{
			hitlist[curSpriteMDLFrame->surfaceskinIDs[curMDLIndex][i].GetIndex()] |= FTexture::TEX_Flat;
		}

		MD3Surface * surf = &surfaces[i];
		for (int j = 0; j < surf->numSkins; j++)
		{
			if (surf->skins[j].isValid())
			{
				hitlist[surf->skins[j].GetIndex()] |= FTexture::TEX_Flat;
			}
		}
	}
}

//===========================================================================
//
//
//
//===========================================================================

int FMD3Model::FindFrame(const char * name)
{
	for (int i=0;i<numFrames;i++)
	{
		if (!stricmp(name, frames[i].Name)) return i;
	}
	return -1;
}

//===========================================================================
//
//
//
//===========================================================================

void FMD3Model::RenderFrame(FModelRenderer *renderer, FTexture * skin, int frameno, int frameno2, double inter, int translation)
{
	if (frameno>=numFrames || frameno2>=numFrames) return;

	renderer->SetInterpolation(inter);
	for(int i=0;i<numSurfaces;i++)
	{
		MD3Surface * surf = &surfaces[i];

		// [BB] In case no skin is specified via MODELDEF, check if the MD3 has a skin for the current surface.
		// Note: Each surface may have a different skin.
		FTexture *surfaceSkin = skin;
		if (!surfaceSkin)
		{
			if (curSpriteMDLFrame->surfaceskinIDs[curMDLIndex][i].isValid())
			{
				surfaceSkin = TexMan(curSpriteMDLFrame->surfaceskinIDs[curMDLIndex][i]);
			}
			else if(surf->numSkins > 0 && surf->skins[0].isValid())
			{
				surfaceSkin = TexMan(surf->skins[0]);
			}

			if (!surfaceSkin) return;
		}

		renderer->SetMaterial(surfaceSkin, false, translation);
		mVBuf->SetupFrame(renderer, surf->vindex + frameno * surf->numVertices, surf->vindex + frameno2 * surf->numVertices, surf->numVertices);
		renderer->DrawElements(surf->numTriangles * 3, surf->iindex * sizeof(unsigned int));
	}
	renderer->SetInterpolation(0.f);
}

//===========================================================================
//
//
//
//===========================================================================

FMD3Model::~FMD3Model()
{
	if (frames) delete [] frames;
	if (surfaces) delete [] surfaces;
	frames = nullptr;
	surfaces = nullptr;
}
