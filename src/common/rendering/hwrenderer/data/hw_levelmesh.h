
#pragma once

#include "tarray.h"
#include "vectors.h"
#include "hw_collision.h"

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

	std::unique_ptr<TriangleMeshShape> Collision;

	bool Trace(const FVector3& start, FVector3 direction, float maxDist)
	{
		FVector3 end = start + direction * std::max(maxDist - 10.0f, 0.0f);
		return !TriangleMeshShape::find_any_hit(Collision.get(), start, end);
	}
};

} // namespace
