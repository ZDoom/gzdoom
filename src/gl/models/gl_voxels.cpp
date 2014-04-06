/*
** gl_voxels.cpp
**
** Voxel management
**
**---------------------------------------------------------------------------
** Copyright 2010 Christoph Oelckers
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
#include "c_console.h"
#include "g_game.h"
#include "doomstat.h"
#include "g_level.h"
#include "colormatcher.h"
#include "textures/bitmap.h"
//#include "gl/gl_intern.h"

#include "gl/system/gl_interface.h"
#include "gl/renderer/gl_renderer.h"
#include "gl/data/gl_vertexbuffer.h"
#include "gl/scene/gl_drawinfo.h"
#include "gl/models/gl_models.h"
#include "gl/textures/gl_material.h"
#include "gl/utility/gl_geometric.h"
#include "gl/utility/gl_convert.h"
#include "gl/renderer/gl_renderstate.h"


//===========================================================================
//
// Creates a 16x16 texture from the palette so that we can
// use the existing palette manipulation code to render the voxel
// Otherwise all shaders had to be duplicated and the non-shader code
// would be a lot less efficient.
//
//===========================================================================

class FVoxelTexture : public FTexture
{
public:

	FVoxelTexture(FVoxel *voxel);
	~FVoxelTexture();
	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();

	int CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf);
	bool UseBasePalette() { return false; }

protected:
	FVoxel *SourceVox;
	BYTE *Pixels;

};

//===========================================================================
//
// 
//
//===========================================================================

FVoxelTexture::FVoxelTexture(FVoxel *vox)
{
	SourceVox = vox;
	Width = 16;
	Height = 16;
	WidthBits = 4;
	HeightBits = 4;
	WidthMask = 15;
	Pixels = NULL;
	gl_info.bNoFilter = true;
	gl_info.bNoCompress = true;
}

//===========================================================================
//
// 
//
//===========================================================================

FVoxelTexture::~FVoxelTexture()
{
}

const BYTE *FVoxelTexture::GetColumn (unsigned int column, const Span **spans_out)
{
	// not needed
	return NULL;
}

const BYTE *FVoxelTexture::GetPixels ()
{
	// GetPixels gets called when a translated palette is used so we still need to implement it here.
	if (Pixels == NULL)
	{
		Pixels = new BYTE[256];

		BYTE *pp = SourceVox->Palette;

		if(pp != NULL)
		{
			for(int i=0;i<256;i++, pp+=3)
			{
				PalEntry pe;
				pe.r = (pp[0] << 2) | (pp[0] >> 4);
				pe.g = (pp[1] << 2) | (pp[1] >> 4);
				pe.b = (pp[2] << 2) | (pp[2] >> 4);
				Pixels[i] = ColorMatcher.Pick(pe);
			}
		}
		else 
		{
			for(int i=0;i<256;i++, pp+=3)
			{
				Pixels[i] = (BYTE)i;
			}
		}  
	}
	return Pixels;
}

void FVoxelTexture::Unload ()
{
	if (Pixels != NULL)
	{
		delete[] Pixels;
		Pixels = NULL;
	}
}

//===========================================================================
//
// FVoxelTexture::CopyTrueColorPixels
//
// This creates a dummy 16x16 paletted bitmap and converts that using the
// voxel palette
//
//===========================================================================

int FVoxelTexture::CopyTrueColorPixels(FBitmap *bmp, int x, int y, int rotate, FCopyInfo *inf)
{
	PalEntry pe[256];
	BYTE bitmap[256];
	BYTE *pp = SourceVox->Palette;

	if(pp != NULL)
	{
		for(int i=0;i<256;i++, pp+=3)
		{
			bitmap[i] = (BYTE)i;
			pe[i].r = (pp[0] << 2) | (pp[0] >> 4);
			pe[i].g = (pp[1] << 2) | (pp[1] >> 4);
			pe[i].b = (pp[2] << 2) | (pp[2] >> 4);
			pe[i].a = 255;
		}
	}
	else 
	{
		for(int i=0;i<256;i++, pp+=3)
		{
			bitmap[i] = (BYTE)i;
			pe[i] = GPalette.BaseColors[i];
			pe[i].a = 255;
		}
	}    
	bmp->CopyPixelData(x, y, bitmap, Width, Height, 1, 16, rotate, pe, inf);
	return 0;
}	

//===========================================================================
//
// 
//
//===========================================================================


class FVoxelVertexBuffer : public FVertexBuffer
{
	unsigned int ibo_id;
	bool isint;

public:
	FVoxelVertexBuffer(TArray<FVoxelVertex> &verts, TArray<unsigned int> &indices);
	~FVoxelVertexBuffer();
	void BindVBO();
	bool IsInt() const { return isint; }
};


//===========================================================================
//
// 
//
//===========================================================================

FVoxelVertexBuffer::FVoxelVertexBuffer(TArray<FVoxelVertex> &verts, TArray<unsigned int> &indices)
{
	ibo_id = 0;
	glGenBuffers(1, &ibo_id);

	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	glBufferData(GL_ARRAY_BUFFER, verts.Size() * sizeof(FVoxelVertex), &verts[0], GL_STATIC_DRAW);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
	if (verts.Size() > 65535)
	{
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.Size() * sizeof(unsigned int), &indices[0], GL_STATIC_DRAW);
		isint = true;
	}
	else
	{
		unsigned short *sbuffer = new unsigned short[indices.Size()];
		for(unsigned i=0;i<indices.Size();i++)
		{
			sbuffer[i] = (unsigned short)indices[i];
		}
		glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.Size() * sizeof(unsigned short), sbuffer, GL_STATIC_DRAW);
		delete [] sbuffer;
		isint = false;
	}
}

//===========================================================================
//
// 
//
//===========================================================================

FVoxelVertexBuffer::~FVoxelVertexBuffer()
{
	if (ibo_id != 0)
	{
		glDeleteBuffers(1, &ibo_id);
	}
}

//===========================================================================
//
// 
//
//===========================================================================
#define VVO ((FVoxelVertex*)NULL)

void FVoxelVertexBuffer::BindVBO()
{
	glBindBuffer(GL_ARRAY_BUFFER, vbo_id);
	glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo_id);
	glVertexPointer(3,GL_FLOAT, sizeof(FVoxelVertex), &VVO->x);
	glTexCoordPointer(2,GL_FLOAT, sizeof(FVoxelVertex), &VVO->u);
	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_TEXTURE_COORD_ARRAY);
	glEnableClientState(GL_INDEX_ARRAY);
}



//===========================================================================
//
// 
//
//===========================================================================

FVoxelModel::FVoxelModel(FVoxel *voxel, bool owned)
{
	mVoxel = voxel;
	mOwningVoxel = owned;
	mVBO = NULL;
	mPalette = new FVoxelTexture(voxel);
	Initialize();
}

//===========================================================================
//
// 
//
//===========================================================================

FVoxelModel::~FVoxelModel()
{
	CleanGLData();
	delete mPalette;
	if (mOwningVoxel) delete mVoxel;
}

//===========================================================================
//
// 
//
//===========================================================================

void FVoxelModel::AddVertex(FVoxelVertex &vert, FVoxelMap &check)
{
	unsigned int index = check[vert];
	if (index == 0xffffffff)
	{
		index = check[vert] =mVertices.Push(vert);
	}
	mIndices.Push(index);
}

//===========================================================================
//
// 
//
//===========================================================================

void FVoxelModel::AddFace(int x1, int y1, int z1, int x2, int y2, int z2, int x3, int y3, int z3, int x4, int y4, int z4, BYTE col, FVoxelMap &check)
{
	float PivotX = mVoxel->Mips[0].PivotX / 256.f;
	float PivotY = mVoxel->Mips[0].PivotY / 256.f;
	float PivotZ = mVoxel->Mips[0].PivotZ / 256.f;
	int h = mVoxel->Mips[0].SizeZ;
	FVoxelVertex vert;


	vert.u = (((col & 15) * 255 / 16) + 7) / 255.f;
	vert.v = (((col / 16) * 255 / 16) + 7) / 255.f;

	vert.x =  x1 - PivotX;
	vert.z = -y1 + PivotY;
	vert.y = -z1 + PivotZ;
	AddVertex(vert, check);

	vert.x =  x2 - PivotX;
	vert.z = -y2 + PivotY;
	vert.y = -z2 + PivotZ;
	AddVertex(vert, check);

	vert.x =  x4 - PivotX;
	vert.z = -y4 + PivotY;
	vert.y = -z4 + PivotZ;
	AddVertex(vert, check);

	vert.x =  x3 - PivotX;
	vert.z = -y3 + PivotY;
	vert.y = -z3 + PivotZ;
	AddVertex(vert, check);

}

//===========================================================================
//
// 
//
//===========================================================================

void FVoxelModel::MakeSlabPolys(int x, int y, kvxslab_t *voxptr, FVoxelMap &check)
{
	const BYTE *col = voxptr->col;
	int zleng = voxptr->zleng;
	int ztop = voxptr->ztop;
	int cull = voxptr->backfacecull;

	if (cull & 16)
	{
		AddFace(x, y, ztop, x+1, y, ztop, x, y+1, ztop, x+1, y+1, ztop, *col, check);
	}
	int z = ztop;
	while (z < ztop+zleng)
	{
		int c = 0;
		while (z+c < ztop+zleng && col[c] == col[0]) c++;

		if (cull & 1)
		{
			AddFace(x, y, z, x, y+1, z, x, y, z+c, x, y+1, z+c, *col, check);
		}
		if (cull & 2)
		{
			AddFace(x+1, y+1, z, x+1, y, z, x+1, y+1, z+c, x+1, y, z+c, *col, check);
		}
		if (cull & 4)
		{
			AddFace(x, y, z, x+1, y, z, x, y, z+c, x+1, y, z+c, *col, check);
		}
		if (cull & 8)
		{
			AddFace(x+1, y+1, z, x, y+1, z, x+1, y+1, z+c, x, y+1, z+c, *col, check);
		}
		z+=c;
		col+=c;
	}
	if (cull & 32)
	{
		int z = ztop+zleng-1;
		AddFace(x+1, y, z+1, x, y, z+1, x+1, y+1, z+1, x, y+1, z+1, voxptr->col[zleng-1], check);
	}
}

//===========================================================================
//
// 
//
//===========================================================================

void FVoxelModel::Initialize()
{
	FVoxelMap check;
	FVoxelMipLevel *mip = &mVoxel->Mips[0];
	for (int x = 0; x < mip->SizeX; x++)
	{
		BYTE *slabxoffs = &mip->SlabData[mip->OffsetX[x]];
		short *xyoffs = &mip->OffsetXY[x * (mip->SizeY + 1)];
		for (int y = 0; y < mip->SizeY; y++)
		{
			kvxslab_t *voxptr = (kvxslab_t *)(slabxoffs + xyoffs[y]);
			kvxslab_t *voxend = (kvxslab_t *)(slabxoffs + xyoffs[y+1]);
			for (; voxptr < voxend; voxptr = (kvxslab_t *)((BYTE *)voxptr + voxptr->zleng + 3))
			{
				MakeSlabPolys(x, y, voxptr, check);
			}
		}
	}
}

//===========================================================================
//
// 
//
//===========================================================================

bool FVoxelModel::Load(const char * fn, int lumpnum, const char * buffer, int length)
{
	return false;	// not needed
}

//===========================================================================
//
// 
//
//===========================================================================

void FVoxelModel::MakeGLData()
{
	mVBO = new FVoxelVertexBuffer(mVertices, mIndices);
}

//===========================================================================
//
// 
//
//===========================================================================

void FVoxelModel::CleanGLData()
{
	if (mVBO != NULL)
	{
		delete mVBO;
		mVBO = NULL;
	}
}

//===========================================================================
//
// Voxels don't have frames so always return 0
//
//===========================================================================

int FVoxelModel::FindFrame(const char * name)
{
	return 0;
}

//===========================================================================
//
// 
//
//===========================================================================

void FVoxelModel::RenderFrame(FTexture * skin, int frame, int cm, int translation)
{
	FMaterial * tex = FMaterial::ValidateTexture(skin);
	tex->Bind(cm, 0, translation);
	gl_RenderState.Apply();

	if (mVBO == NULL) MakeGLData();
	if (mVBO != NULL)
	{
		mVBO->BindVBO();
		glDrawElements(GL_QUADS, mIndices.Size(), mVBO->IsInt()? GL_UNSIGNED_INT:GL_UNSIGNED_SHORT, 0);
		GLRenderer->mVBO->BindVBO();
		return;
	}

	glBegin(GL_QUADS);
	for(unsigned i=0;i < mIndices.Size(); i++)
	{
		FVoxelVertex *vert = &mVertices[mIndices[i]];
		glTexCoord2fv(&vert->u);
		glVertex3fv(&vert->x);
	}
	glEnd();
}

//===========================================================================
//
// Voxels never interpolate between frames
//
//===========================================================================

void FVoxelModel::RenderFrameInterpolated(FTexture * skin, int frame, int frame2, double inter, int cm, int translation)
{
	RenderFrame(skin, frame, cm, translation);
}

