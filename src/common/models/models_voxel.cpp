// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2010-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
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
/*
** gl_voxels.cpp
**
** Voxel management
**
**/

#include "filesystem.h"
#include "colormatcher.h"
#include "bitmap.h"
#include "model_kvx.h"
#include "image.h"
#include "texturemanager.h"
#include "modelrenderer.h"
#include "voxels.h"
#include "texturemanager.h"
#include "palettecontainer.h"
#include "textures.h"
#include "imagehelpers.h"

#ifdef _MSC_VER
#pragma warning(disable:4244) // warning C4244: conversion from 'double' to 'float', possible loss of data
#endif

//===========================================================================
//
// Creates a 16x16 texture from the palette so that we can
// use the existing palette manipulation code to render the voxel
// Otherwise all shaders had to be duplicated and the non-shader code
// would be a lot less efficient.
//
//===========================================================================

class FVoxelTexture : public FImageSource
{
public:
	FVoxelTexture(FVoxel *voxel);

	int CopyPixels(FBitmap *bmp, int conversion) override;
	TArray<uint8_t> CreatePalettedPixels(int conversion) override;

protected:
	FVoxel *SourceVox;
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
	//bNoCompress = true;
}

//===========================================================================
//
// 
//
//===========================================================================

TArray<uint8_t> FVoxelTexture::CreatePalettedPixels(int conversion)
{
	// GetPixels gets called when a translated palette is used so we still need to implement it here.
	TArray<uint8_t> Pixels(256, true);
	uint8_t *pp = SourceVox->Palette.Data();

	if(pp != NULL)
	{
		for(int i=0;i<256;i++, pp+=3)
		{
			PalEntry pe;
			pe.r = (pp[0] << 2) | (pp[0] >> 4);
			pe.g = (pp[1] << 2) | (pp[1] >> 4);
			pe.b = (pp[2] << 2) | (pp[2] >> 4);
			// Alphatexture handling is just for completeness, but rather unlikely to be used ever.
			Pixels[i] = conversion == luminance ? pe.r : ColorMatcher.Pick(pe);

		}
	}
	else 
	{
		for(int i=0;i<256;i++, pp+=3)
		{
			Pixels[i] = (uint8_t)i;
		}
	}  
	ImageHelpers::FlipSquareBlock(Pixels.Data(), Width);
	return Pixels;
}

//===========================================================================
//
// FVoxelTexture::CopyPixels
//
// This creates a dummy 16x16 paletted bitmap and converts that using the
// voxel palette
//
//===========================================================================

int FVoxelTexture::CopyPixels(FBitmap *bmp, int conversion)
{
	PalEntry pe[256];
	uint8_t bitmap[256];
	uint8_t *pp = SourceVox->Palette.Data();

	if(pp != nullptr)
	{
		for(int i=0;i<256;i++, pp+=3)
		{
			bitmap[i] = (uint8_t)i;
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
			bitmap[i] = (uint8_t)i;
			pe[i] = GPalette.BaseColors[i];
			pe[i].a = 255;
		}
	}    
	bmp->CopyPixelData(0, 0, bitmap, Width, Height, 1, 16, 0, pe);
	return 0;
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
	mPalette = TexMan.AddGameTexture(MakeGameTexture(new FImageTexture(new FVoxelTexture(voxel)), nullptr, ETextureType::Override));
}

//===========================================================================
//
// 
//
//===========================================================================

FVoxelModel::~FVoxelModel()
{
	if (mOwningVoxel) delete mVoxel;
}


//===========================================================================
//
// 
//
//===========================================================================

unsigned int FVoxelModel::AddVertex(FModelVertex &vert, FVoxelMap &check)
{
	unsigned int index = check[vert];
	if (index == 0xffffffff)
	{
		index = check[vert] =mVertices.Push(vert);
	}
	return index;
}

//===========================================================================
//
// 
//
//===========================================================================

void FVoxelModel::AddFace(int x1, int y1, int z1, int x2, int y2, int z2, int x3, int y3, int z3, int x4, int y4, int z4, uint8_t col, FVoxelMap &check)
{
	float PivotX = mVoxel->Mips[0].Pivot.X;
	float PivotY = mVoxel->Mips[0].Pivot.Y;
	float PivotZ = mVoxel->Mips[0].Pivot.Z;
	FModelVertex vert;
	unsigned int indx[4];

	vert.packedNormal = 0;	// currently this is not being used for voxels.
	vert.u = (((col & 15) + 0.5f) / 16.f);
	vert.v = (((col / 16) + 0.5f) / 16.f);

	vert.x =  x1 - PivotX;
	vert.z = -y1 + PivotY;
	vert.y = -z1 + PivotZ;
	indx[0] = AddVertex(vert, check);

	vert.x =  x2 - PivotX;
	vert.z = -y2 + PivotY;
	vert.y = -z2 + PivotZ;
	indx[1] = AddVertex(vert, check);

	vert.x =  x4 - PivotX;
	vert.z = -y4 + PivotY;
	vert.y = -z4 + PivotZ;
	indx[2] = AddVertex(vert, check);

	vert.x =  x3 - PivotX;
	vert.z = -y3 + PivotY;
	vert.y = -z3 + PivotZ;
	indx[3] = AddVertex(vert, check);


	mIndices.Push(indx[0]);
	mIndices.Push(indx[1]);
	mIndices.Push(indx[3]);
	mIndices.Push(indx[1]);
	mIndices.Push(indx[2]);
	mIndices.Push(indx[3]);
}

//===========================================================================
//
// 
//
//===========================================================================

void FVoxelModel::MakeSlabPolys(int x, int y, kvxslab_t *voxptr, FVoxelMap &check)
{
	const uint8_t *col = voxptr->col;
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
			AddFace(x+1, y, z, x, y, z, x+1, y, z+c, x, y, z+c, *col, check);
		}
		if (cull & 8)
		{
			AddFace(x, y+1, z, x+1, y+1, z, x, y+1, z+c, x+1, y+1, z+c, *col, check);
		}	
		z+=c;
		col+=c;
	}
	if (cull & 32)
	{
		int zz = ztop+zleng-1;
		AddFace(x+1, y, zz+1, x, y, zz+1, x+1, y+1, zz+1, x, y+1, zz+1, voxptr->col[zleng-1], check);
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
		uint8_t *slabxoffs = &mip->GetSlabData(false)[mip->OffsetX[x]];
		short *xyoffs = &mip->OffsetXY[x * (mip->SizeY + 1)];
		for (int y = 0; y < mip->SizeY; y++)
		{
			kvxslab_t *voxptr = (kvxslab_t *)(slabxoffs + xyoffs[y]);
			kvxslab_t *voxend = (kvxslab_t *)(slabxoffs + xyoffs[y+1]);
			for (; voxptr < voxend; voxptr = (kvxslab_t *)((uint8_t *)voxptr + voxptr->zleng + 3))
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

void FVoxelModel::BuildVertexBuffer(FModelRenderer *renderer)
{
	if (!GetVertexBuffer(renderer->GetType()))
	{
		Initialize();

		auto vbuf = renderer->CreateVertexBuffer(true, true);
		SetVertexBuffer(renderer->GetType(), vbuf);

		FModelVertex *vertptr = vbuf->LockVertexBuffer(mVertices.Size());
		unsigned int *indxptr = vbuf->LockIndexBuffer(mIndices.Size());

		memcpy(vertptr, &mVertices[0], sizeof(FModelVertex)* mVertices.Size());
		memcpy(indxptr, &mIndices[0], sizeof(unsigned int)* mIndices.Size());

		vbuf->UnlockVertexBuffer();
		vbuf->UnlockIndexBuffer();
		mNumIndices = mIndices.Size();

		// delete our temporary buffers
		mVertices.Clear();
		mIndices.Clear();
		mVertices.ShrinkToFit();
		mIndices.ShrinkToFit();
	}
}


//===========================================================================
//
// for skin precaching
//
//===========================================================================

void FVoxelModel::AddSkins(uint8_t *hitlist, const FTextureID*)
{
	hitlist[mPalette.GetIndex()] |= FTextureManager::HIT_Flat;
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
// Voxels don't have frames so always return 0
//
//===========================================================================

int FVoxelModel::FindFrame(const char * name)
{
	return 0;
}

//===========================================================================
//
// Voxels need aspect ratio correction according to the current map's setting
//
//===========================================================================

float FVoxelModel::getAspectFactor(float stretch)
{
	return stretch;
}

//===========================================================================
//
// Voxels never interpolate between frames, they only have one.
//
//===========================================================================

void FVoxelModel::RenderFrame(FModelRenderer *renderer, FGameTexture * skin, int frame, int frame2, double inter, int translation, const FTextureID*)
{
	renderer->SetMaterial(skin, true, translation);
	renderer->SetupFrame(this, 0, 0, 0);
	renderer->DrawElements(mNumIndices, 0);
}

