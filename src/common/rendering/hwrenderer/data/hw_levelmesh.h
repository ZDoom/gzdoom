
#pragma once

#include "tarray.h"
#include "vectors.h"
#include "hw_collision.h"
#include "bounds.h"
#include <memory>

#include <dp_rect_pack.h>

typedef dp::rect_pack::RectPacker<int> RectPacker;

struct SurfaceInfo // This is the structure of the buffer inside the shader
{
	FVector3 Normal;
	float Sky;
	float SamplingDistance;
	uint32_t PortalIndex;
	float Padding1, Padding2;
};

struct PortalInfo  // This is the structure of the buffer inside the shader
{
	float transformation[16]; // mat4 
};

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
	TArray<FVector3> uvs;
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

struct Portal
{
	float transformation[4][4] = {
		{1, 0, 0, 0},
		{0, 1, 0, 0},
		{0, 0, 1, 0},
		{0, 0, 0, 1}
	};

	int sourceSectorGroup = 0;
	int targetSectorGroup = 0;

	inline FVector4 Mat4Vec4Mul(const FVector4& vec) const
	{
		FVector4 result(0.0f, 0.0f, 0.0f, 0.0f);
		for (int i = 0; i < 4; ++i) {
			for (int j = 0; j < 4; ++j) {
				result[i] += transformation[i][j] * vec[j];
			}
		}
		return result;
	}

	inline FVector3 TransformPosition(const FVector3& pos) const
	{
		auto v = Mat4Vec4Mul(FVector4(pos, 1.0));
		return FVector3(v.X, v.Y, v.Z);
	}

	inline FVector3 TransformRotation(const FVector3& dir) const
	{
		auto v = Mat4Vec4Mul(FVector4(dir, 0.0));
		return FVector3(v.X, v.Y, v.Z);
	}

	// Checks only transformation
	inline bool IsInverseTransformationPortal(const Portal& portal) const
	{
		auto diff = portal.TransformPosition(TransformPosition(FVector3(0, 0, 0)));
		return abs(diff.X) < 0.001 && abs(diff.Y) < 0.001 && abs(diff.Z) < 0.001;
	}

	// Checks only transformation
	inline bool IsEqualTransformationPortal(const Portal& portal) const
	{
		auto diff = portal.TransformPosition(FVector3(0, 0, 0)) - TransformPosition(FVector3(0, 0, 0));
		return (abs(diff.X) < 0.001 && abs(diff.Y) < 0.001 && abs(diff.Z) < 0.001);
	}

	// Checks transformation, source and destiantion sector groups
	inline bool IsEqualPortal(const Portal& portal) const
	{
		return sourceSectorGroup == portal.sourceSectorGroup && targetSectorGroup == portal.targetSectorGroup && IsEqualTransformationPortal(portal);
	}

	// Checks transformation, source and destiantion sector groups
	inline bool IsInversePortal(const Portal& portal) const
	{
		return sourceSectorGroup == portal.targetSectorGroup && targetSectorGroup == portal.sourceSectorGroup && IsInverseTransformationPortal(portal);
	}
};

class LevelMesh
{
public:
	virtual ~LevelMesh() = default;

	TArray<FVector3> MeshVertices;
	TArray<int> MeshUVIndex;
	TArray<uint32_t> MeshElements;
	TArray<int> MeshSurfaces;

	TArray<SurfaceInfo> surfaceInfo;
	TArray<PortalInfo> portalInfo;

	std::unique_ptr<TriangleMeshShape> Collision;

	std::vector<std::unique_ptr<Surface>> surfaces;
	TArray<SmoothingGroup> smoothingGroups; // TODO fill
	TArray<Portal> portals; // TODO fill

	FVector3 SunDirection;
	FVector3 SunColor;

	bool Trace(const FVector3& start, FVector3 direction, float maxDist)
	{
		FVector3 end = start + direction * std::max(maxDist - 10.0f, 0.0f);
		return !TriangleMeshShape::find_any_hit(Collision.get(), start, end);
	}

	inline void FinishSurface(int lightmapTextureWidth, int lightmapTextureHeight, RectPacker& packer, Surface& surface)
	{
		int sampleWidth = surface.texWidth;
		int sampleHeight = surface.texHeight;

		auto result = packer.insert(sampleWidth, sampleHeight);
		int x = result.pos.x, y = result.pos.y;
		surface.atlasPageIndex = (int)result.pageIndex;

		// calculate final texture coordinates
		for (unsigned i = 0; i < surface.uvs.Size(); i++)
		{
			auto& u = surface.uvs[i].X;
			auto& v = surface.uvs[i].Y;
			u = (u + x) / (float)lightmapTextureWidth;
			v = (v + y) / (float)lightmapTextureHeight;
		}

		surface.atlasX = x;
		surface.atlasY = y;

	}
};

} // namespace
