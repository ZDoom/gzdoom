
#pragma once

#include "tarray.h"
#include "vectors.h"

namespace hwrenderer
{

class LevelMesh
{
public:
	virtual ~LevelMesh() = default;

	TArray<FVector3> MeshVertices;
	TArray<int> MeshUVIndex;
	TArray<uint32_t> MeshElements;
	TArray<int> MeshSurfaces;
};

} // namespace
