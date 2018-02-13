// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2005-2016 Christoph Oelckers
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

#ifndef __VERTEXBUFFER_H
#define __VERTEXBUFFER_H

#include "tarray.h"
#include "gl/utility/gl_clock.h"
#include "gl/system/gl_interface.h"
#include "r_data/models/models.h"

struct vertex_t;
struct secplane_t;
struct subsector_t;
struct sector_t;
class FMaterial;

enum
{
	VATTR_VERTEX_BIT,
	VATTR_TEXCOORD_BIT,
	VATTR_COLOR_BIT,
	VATTR_VERTEX2_BIT,
	VATTR_NORMAL_BIT
};


class FVertexBuffer
{
protected:
	unsigned int vbo_id;

public:
	FVertexBuffer(bool wantbuffer = true);
	virtual ~FVertexBuffer();
	virtual void BindVBO() = 0;
	void EnableBufferArrays(int enable, int disable);
};

struct FFlatVertex
{
	float x,z,y;	// world position
	float u,v;		// texture coordinates

	void SetFlatVertex(vertex_t *vt, const secplane_t &plane);
	void Set(float xx, float zz, float yy, float uu, float vv)
	{
		x = xx;
		z = zz;
		y = yy;
		u = uu;
		v = vv;
	}
};

struct FSimpleVertex
{
	float x, z, y;	// world position
	float u, v;		// texture coordinates
	PalEntry color;

	void Set(float xx, float zz, float yy, float uu = 0, float vv = 0, PalEntry col = 0xffffffff)
	{
		x = xx;
		z = zz;
		y = yy;
		u = uu;
		v = vv;
		color = col;
	}
};

#define VTO ((FFlatVertex*)NULL)
#define VSiO ((FSimpleVertex*)NULL)

class FSimpleVertexBuffer : public FVertexBuffer
{
	TArray<FSimpleVertex> mBuffer;
public:
	FSimpleVertexBuffer()
	{
	}
	void BindVBO();
	void set(FSimpleVertex *verts, int count);
	void EnableColorArray(bool on);
};

class FFlatVertexBuffer : public FVertexBuffer
{
	FFlatVertex *map;
	unsigned int mIndex;
	unsigned int mCurIndex;
	unsigned int mNumReserved;

	void CheckPlanes(sector_t *sector);

	static const unsigned int BUFFER_SIZE = 2000000;
	static const unsigned int BUFFER_SIZE_TO_USE = 1999500;

public:
	enum
	{
		QUAD_INDEX = 0,
		FULLSCREEN_INDEX = 4,
		PRESENT_INDEX = 8,
		STENCILTOP_INDEX = 12,
		STENCILBOTTOM_INDEX = 16,

		NUM_RESERVED = 20
	};

	TArray<FFlatVertex> vbo_shadowdata;	// this is kept around for updating the actual (non-readable) buffer and as stand-in for pre GL 4.x

	FFlatVertexBuffer(int width, int height);
	~FFlatVertexBuffer();

	void OutputResized(int width, int height);

	void BindVBO();

	void CreateVBO();
	void CheckUpdate(sector_t *sector);

	FFlatVertex *GetBuffer()
	{
		return &map[mCurIndex];
	}
	FFlatVertex *Alloc(int num, int *poffset)
	{
		FFlatVertex *p = GetBuffer();
		*poffset = mCurIndex;
		mCurIndex += num;
		if (mCurIndex >= BUFFER_SIZE_TO_USE) mCurIndex = mIndex;
		return p;
	}

	unsigned int GetCount(FFlatVertex *newptr, unsigned int *poffset)
	{
		unsigned int newofs = (unsigned int)(newptr - map);
		unsigned int diff = newofs - mCurIndex;
		*poffset = mCurIndex;
		mCurIndex = newofs;
		if (mCurIndex >= BUFFER_SIZE_TO_USE) mCurIndex = mIndex;
		return diff;
	}
#ifdef __GL_PCH_H	// we need the system includes for this but we cannot include them ourselves without creating #define clashes. The affected files wouldn't try to draw anyway.
	void RenderArray(unsigned int primtype, unsigned int offset, unsigned int count)
	{
		drawcalls.Clock();
		glDrawArrays(primtype, offset, count);
		drawcalls.Unclock();
	}

	void RenderCurrent(FFlatVertex *newptr, unsigned int primtype, unsigned int *poffset = NULL, unsigned int *pcount = NULL)
	{
		unsigned int offset;
		unsigned int count = GetCount(newptr, &offset);
		RenderArray(primtype, offset, count);
		if (poffset) *poffset = offset;
		if (pcount) *pcount = count;
	}

#endif
	void Reset()
	{
		mCurIndex = mIndex;
	}

	void Map();
	void Unmap();

private:
	int CreateSubsectorVertices(subsector_t *sub, const secplane_t &plane, int floor);
	int CreateSectorVertices(sector_t *sec, const secplane_t &plane, int floor);
	int CreateVertices(int h, sector_t *sec, const secplane_t &plane, int floor);
	void CreateFlatVBO();
	void UpdatePlaneVertices(sector_t *sec, int plane);

};


struct FSkyVertex
{
	float x, y, z, u, v;
	PalEntry color;

	void Set(float xx, float zz, float yy, float uu=0, float vv=0, PalEntry col=0xffffffff)
	{
		x = xx;
		z = zz;
		y = yy;
		u = uu;
		v = vv;
		color = col;
	}

	void SetXYZ(float xx, float yy, float zz, float uu = 0, float vv = 0, PalEntry col = 0xffffffff)
	{
		x = xx;
		y = yy;
		z = zz;
		u = uu;
		v = vv;
		color = col;
	}

};

class FSkyVertexBuffer : public FVertexBuffer
{
public:
	static const int SKYHEMI_UPPER = 1;
	static const int SKYHEMI_LOWER = 2;

	enum
	{
		SKYMODE_MAINLAYER = 0,
		SKYMODE_SECONDLAYER = 1,
		SKYMODE_FOGLAYER = 2
	};

private:
	TArray<FSkyVertex> mVertices;
	TArray<unsigned int> mPrimStart;

	int mRows, mColumns;

	// indices for sky cubemap faces
	int mFaceStart[7];
	int mSideStart;

	void SkyVertex(int r, int c, bool yflip);
	void CreateSkyHemisphere(int hemi);
	void CreateDome();
	void RenderRow(int prim, int row);

public:

	FSkyVertexBuffer();
	virtual ~FSkyVertexBuffer();
	void RenderDome(FMaterial *tex, int mode);
	void BindVBO();
	int FaceStart(int i)
	{
		if (i >= 0 && i < 7) return mFaceStart[i];
		else return mSideStart;
	}

};

class FModelVertexBuffer : public FVertexBuffer, public IModelVertexBuffer
{
	int mIndexFrame[2];
	FModelVertex *vbo_ptr;
	uint32_t ibo_id;

public:

	FModelVertexBuffer(bool needindex, bool singleframe);
	~FModelVertexBuffer();

	FModelVertex *LockVertexBuffer(unsigned int size) override;
	void UnlockVertexBuffer() override;

	unsigned int *LockIndexBuffer(unsigned int size) override;
	void UnlockIndexBuffer() override;

	void SetupFrame(FModelRenderer *renderer, unsigned int frame1, unsigned int frame2, unsigned int size) override;
	void BindVBO() override;
};

#define VSO ((FSkyVertex*)NULL)

#endif