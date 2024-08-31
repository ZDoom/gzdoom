#pragma once
#include "tarray.h"
#include "r_defs.h"
struct vertex_t;

struct FQualifiedVertex
{
	vertex_t *vertex;       // index into vertex array
	int qualifier;          // some index to prevent vertices in different groups from being merged together.
};

template<> struct THashTraits<FQualifiedVertex>
{
	hash_t Hash(const FQualifiedVertex &key)
	{
		return (int)(((intptr_t)key.vertex) >> 4) ^ (key.qualifier << 16);
	}
	int Compare(const FQualifiedVertex &left, const FQualifiedVertex &right) { return left.vertex != right.vertex || left.qualifier != right.qualifier; }
};

//=============================================================================
//
// One sector's vertex data.
//
//=============================================================================

struct VertexContainer
{
	TArray<FQualifiedVertex> vertices;
	TMap<FQualifiedVertex, uint32_t> vertexmap;
	bool perSubsector = false;
	
	TArray<uint32_t> indices;
	
	uint32_t AddVertex(FQualifiedVertex *vert)
	{
		auto check = vertexmap.CheckKey(*vert);
		if (check != nullptr) return *check;
		auto index = vertices.Push(*vert);
		vertexmap[*vert] = index;
		return index;
	}
	
	uint32_t AddVertex(vertex_t *vert, int qualifier)
	{
		FQualifiedVertex vertx = { vert, qualifier};
		return AddVertex(&vertx);
	}
	
	uint32_t AddIndexForVertex(FQualifiedVertex *vert)
	{
		return indices.Push(AddVertex(vert));
	}
	
	uint32_t AddIndexForVertex(vertex_t *vert, int qualifier)
	{
		return indices.Push(AddVertex(vert, qualifier));
	}
	
	uint32_t AddIndex(uint32_t indx)
	{
		return indices.Push(indx);
	}
};

using VertexContainers = TArray<VertexContainer>;

VertexContainers BuildVertices(TArray<sector_t> &sectors);

class FFlatVertexBuffer;
void CheckUpdate(FFlatVertexBuffer* fvb, sector_t* sector);
void CreateVBO(FFlatVertexBuffer* fvb, TArray<sector_t>& sectors);

