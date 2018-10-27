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

#include <atomic>
#include <thread>
#include <mutex>
#include "tarray.h"
#include "hwrenderer/utility/hw_clock.h"
#include "gl_load/gl_interface.h"
#include "r_data/models/models.h"
#include "hwrenderer/data/flatvertices.h"
#include "hwrenderer/scene/hw_skydome.h"
#include "hwrenderer/data/vertexbuffer.h"

struct vertex_t;
struct secplane_t;
struct subsector_t;
struct sector_t;
class FMaterial;
class FRenderState;



class FVertexBuffer
{
protected:
	unsigned int vbo_id;

public:
	FVertexBuffer(bool wantbuffer = true);
	virtual ~FVertexBuffer();
	virtual void BindVBO() = 0;
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

class FSkyVertexBuffer : public FVertexBuffer, public FSkyDomeCreator
{

public:

	FSkyVertexBuffer();
	void BindVBO();
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