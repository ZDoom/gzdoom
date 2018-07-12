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

class FFlatVertexGenerator
{
protected:
	TArray<FFlatVertex> vbo_shadowdata;
	TArray<uint32_t> ibo_data;
	FFlatVertex *mMap;

	// Temporary data for creating an indexed buffer
	struct FIndexGenerationInfo
	{
		TArray<vertex_t *> vertices;
		TMap<vertex_t*, uint32_t> vertexmap;

		uint32_t AddVertex(vertex_t *vert)
		{
			auto check = vertexmap.CheckKey(vert);
			if (check != nullptr) return *check;
			auto index = vertices.Push(vert);
			vertexmap[vert] = index;
			return index;
		}

		uint32_t GetIndex(vertex_t *vert)
		{
			auto check = vertexmap.CheckKey(vert);
			if (check != nullptr) return *check;
			return ~0;
		}
	};


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

	FFlatVertexGenerator(int width, int height);

	void OutputResized(int width, int height);

private:
	int CreateIndexedSubsectorVertices(subsector_t *sub, const secplane_t &plane, int floor, int vi, FIndexGenerationInfo &gen);
	int CreateIndexedSectorVertices(sector_t *sec, const secplane_t &plane, int floor, FIndexGenerationInfo &gen);
	int CreateIndexedVertices(int h, sector_t *sec, const secplane_t &plane, int floor, TArray<FIndexGenerationInfo> &gen);
	void CreateIndexedFlatVertices();

	void UpdatePlaneVertices(sector_t *sec, int plane);
protected:
	void CreateVertices();
	void CheckPlanes(sector_t *sector);
public:
	void CheckUpdate(sector_t *sector);
};

#endif