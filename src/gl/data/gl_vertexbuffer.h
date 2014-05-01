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
	unsigned int vao_id;

public:
	FVertexBuffer();
	virtual ~FVertexBuffer();
	void BindVAO();
	unsigned int GetVAO() { return vao_id; }
};

struct FFlatVertex
{
	float x,z,y;
	float u,v;
	int index;

	void SetFlatVertex(vertex_t *vt, const secplane_t &plane, int aindex);
};

#define VTO ((FFlatVertex*)NULL)


class FFlatVertexBuffer : public FVertexBuffer
{
	FFlatVertex *map;

	void MapVBO();
	void CheckPlanes(sector_t *sector);
	void BindVBO();

public:
	int vbo_arg;
	TArray<FFlatVertex> vbo_shadowdata;	// this is kept around for non-VBO rendering

public:
	FFlatVertexBuffer();
	~FFlatVertexBuffer();

	int CreateSubsectorVertices(subsector_t *sub, const secplane_t &plane, int floor, int attribindex);
	int CreateSectorVertices(sector_t *sec, const secplane_t &plane, int floor);
	int CreateVertices(int h, sector_t *sec, const secplane_t &plane, int floor);
	void CreateFlatVBO();
	void CreateVBO();
	void UpdatePlaneVertices(sector_t *sec, int plane);
	void CheckUpdate(sector_t *sector);
	void UnmapVBO();

};

#endif