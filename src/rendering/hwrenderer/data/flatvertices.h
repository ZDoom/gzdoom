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

#ifndef _HW__VERTEXBUFFER_H
#define _HW__VERTEXBUFFER_H

#include "tarray.h"
#include "hwrenderer/data/buffers.h"
#include "hw_vertexbuilder.h"
#include <atomic>
#include <mutex>

class FRenderState;
struct secplane_t;
struct subsector_t;

struct FFlatVertex
{
	float x, z, y;	// world position
	float u, v;		// texture coordinates

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

class FFlatVertexBuffer
{
	TArray<FFlatVertex> vbo_shadowdata;
	TArray<uint32_t> ibo_data;

	IVertexBuffer *mVertexBuffer;
	IIndexBuffer *mIndexBuffer;

	unsigned int mIndex;
	std::atomic<unsigned int> mCurIndex;
	unsigned int mNumReserved;


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

	FFlatVertexBuffer(int width, int height);
	~FFlatVertexBuffer();

	void OutputResized(int width, int height);
	std::pair<IVertexBuffer *, IIndexBuffer *> GetBufferObjects() const 
	{
		return std::make_pair(mVertexBuffer, mIndexBuffer);
	}

	void CreateVBO(TArray<sector_t> &sectors);
	void Copy(int start, int count);

	FFlatVertex *GetBuffer(int index) const
	{
		FFlatVertex *ff = (FFlatVertex*)mVertexBuffer->Memory();
		return &ff[index];
	}

	FFlatVertex *GetBuffer() const
	{
		return GetBuffer(mCurIndex);
	}

	std::pair<FFlatVertex *, unsigned int> AllocVertices(unsigned int count);

	void Reset()
	{
		mCurIndex = mIndex;
	}

	void Map()
	{
		mVertexBuffer->Map();
	}

	void Unmap()
	{
		mVertexBuffer->Unmap();
	}

private:
	int CreateIndexedSectionVertices(subsector_t *sub, const secplane_t &plane, int floor, VertexContainer &cont);
	int CreateIndexedSectorVertices(sector_t *sec, const secplane_t &plane, int floor, VertexContainer &cont);
	int CreateIndexedVertices(int h, sector_t *sec, const secplane_t &plane, int floor, VertexContainers &cont);
	void CreateIndexedFlatVertices(TArray<sector_t> &sectors);

	void UpdatePlaneVertices(sector_t *sec, int plane);
protected:
	void CreateVertices(TArray<sector_t> &sectors);
	void CheckPlanes(sector_t *sector);
public:
	void CheckUpdate(sector_t *sector);

};

#endif
