#ifndef __VERTEXBUFFER_H
#define __VERTEXBUFFER_H

#include "tarray.h"

struct vertex_t;
struct secplane_t;
struct subsector_t;
struct sector_t;


class FVertexBuffer
{
protected:
	unsigned int vbo_id;

public:
	FVertexBuffer();
	virtual ~FVertexBuffer();
	virtual void BindVBO() = 0;
};

struct FFlatVertex
{
	float x,z,y;	// world position
	float u,v;		// texture coordinates

	void SetFlatVertex(vertex_t *vt, const secplane_t &plane);
};

#define VTO ((FFlatVertex*)NULL)


class FFlatVertexBuffer : public FVertexBuffer
{
	FFlatVertex *map;
	unsigned int mIndex;
	unsigned int mCurIndex;

	void CheckPlanes(sector_t *sector);

public:
	int vbo_arg;
	TArray<FFlatVertex> vbo_shadowdata;	// this is kept around for updating the actual (non-readable) buffer

	FFlatVertexBuffer();
	~FFlatVertexBuffer();

	void CreateVBO();
	void BindVBO();
	void CheckUpdate(sector_t *sector);

	FFlatVertex *GetBuffer()
	{
		return &map[mCurIndex];
	}
	unsigned int GetCount(FFlatVertex *newptr, unsigned int *poffset)
	{
		unsigned int newofs = unsigned int(newptr - map);
		unsigned int diff = newofs - mCurIndex;
		*poffset = mCurIndex;
		mCurIndex = newofs;
		return diff;
	}
	void Reset()
	{
		mCurIndex = mIndex;
	}

private:
	int CreateSubsectorVertices(subsector_t *sub, const secplane_t &plane, int floor);
	int CreateSectorVertices(sector_t *sec, const secplane_t &plane, int floor);
	int CreateVertices(int h, sector_t *sec, const secplane_t &plane, int floor);
	void CreateFlatVBO();
	void UpdatePlaneVertices(sector_t *sec, int plane);

};

#endif