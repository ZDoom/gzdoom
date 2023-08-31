
#pragma once

#include "tarray.h"
#include "vectors.h"
#include "hw_collision.h"
#include <memory>

namespace hwrenderer
{

// Note: this is just the current layout needed to get VkLightmap/GPURaytracer from zdray to compile within this project.
//
// The surface actually class needs to be moved up DoomLevelMesh, since this can't otherwise be accessed in the common part of the codebase shared with Raze.
// Ideally, we'd undoomify it as much as possible, so Raze in theory also would be able to declare an raytracing acceleration structure for dynlights and lightmaps

class ThingLight
{
public:
	FVector3 Origin;
	FVector3 RelativeOrigin;
	float Radius;
	float Intensity;
	float InnerAngleCos;
	float OuterAngleCos;
	FVector3 SpotDir;
	FVector3 Color;
};

enum SurfaceType
{
	ST_UNKNOWN,
	ST_MIDDLESIDE,
	ST_UPPERSIDE,
	ST_LOWERSIDE,
	ST_CEILING,
	ST_FLOOR
};

class Surface
{
public:
	// Surface geometry
	SurfaceType type = ST_UNKNOWN;
	TArray<FVector3> verts;
	//Plane plane;
	FVector3 boundsMin, boundsMax;

	// Touching light sources
	std::vector<ThingLight*> LightList;

	// Lightmap world coordinates for the texture
	FVector3 worldOrigin = { 0.0f, 0.0f, 0.0f };
	FVector3 worldStepX = { 0.0f, 0.0f, 0.0f };
	FVector3 worldStepY = { 0.0f, 0.0f, 0.0f };

	// Calculate world coordinates to UV coordinates
	FVector3 translateWorldToLocal = { 0.0f, 0.0f, 0.0f };
	FVector3 projLocalToU = { 0.0f, 0.0f, 0.0f };
	FVector3 projLocalToV = { 0.0f, 0.0f, 0.0f };

	// Output lightmap for the surface
	int texWidth = 0;
	int texHeight = 0;
	std::vector<FVector3> texPixels;

	// Placement in final texture atlas
	int atlasPageIndex = -1;
	int atlasX = 0;
	int atlasY = 0;

	// Smoothing group surface is to be rendered with
	int smoothingGroupIndex = -1;
};

struct SmoothingGroup
{
	FVector4 plane = FVector4(0, 0, 1, 0);
	int sectorGroup = 0;
	std::vector<Surface*> surfaces;
};

class LevelMesh
{
public:
	virtual ~LevelMesh() = default;

	TArray<FVector3> MeshVertices;
	TArray<int> MeshUVIndex;
	TArray<uint32_t> MeshElements;
	TArray<int> MeshSurfaces;

	std::unique_ptr<TriangleMeshShape> Collision;

	// To do: these are currently not filled
	TArray<std::unique_ptr<Surface>> surfaces;
	TArray<SmoothingGroup> smoothingGroups;
	FVector3 SunDirection;
	FVector3 SunColor;

	bool Trace(const FVector3& start, FVector3 direction, float maxDist)
	{
		FVector3 end = start + direction * std::max(maxDist - 10.0f, 0.0f);
		return !TriangleMeshShape::find_any_hit(Collision.get(), start, end);
	}
};

} // namespace
