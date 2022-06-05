
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
	TArray<unsigned int> MeshElements;
	TArray<int> MeshSurfaces;
};

} // namespace
