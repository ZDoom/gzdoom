
#pragma once

#include "tarray.h"
#include "vectors.h"
#include "hw_collision.h"
#include "bounds.h"
#include "common/utility/matrix.h"
#include <memory>

#include <dp_rect_pack.h>

typedef dp::rect_pack::RectPacker<int> RectPacker;

class LevelMeshLight
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

enum LevelMeshSurfaceType
{
	ST_UNKNOWN,
	ST_MIDDLESIDE,
	ST_UPPERSIDE,
	ST_LOWERSIDE,
	ST_CEILING,
	ST_FLOOR
};

struct LevelMeshSurface
{
	LevelMeshSurfaceType Type = ST_UNKNOWN;
	int typeIndex;
	int numVerts;
	unsigned int startVertIndex;
	unsigned int startUvIndex;
	FVector4 plane;
	bool bSky;

	// Lightmap UV information in pixel size
	int atlasPageIndex = 0;
	int atlasX = 0;
	int atlasY = 0;
	int texWidth = 0;
	int texHeight = 0;

	//
	// Required for internal lightmapper:
	//
	
	// int portalDestinationIndex = -1; // line or sector index
	int portalIndex = 0;
	int sectorGroup = 0;

	BBox bounds;
	uint16_t sampleDimension = 0;

	// Lightmap world coordinates for the texture
	FVector3 worldOrigin = { 0.f, 0.f, 0.f };
	FVector3 worldStepX = { 0.f, 0.f, 0.f };
	FVector3 worldStepY = { 0.f, 0.f, 0.f };

	// Calculate world coordinates to UV coordinates
	FVector3 translateWorldToLocal = { 0.f, 0.f, 0.f };
	FVector3 projLocalToU = { 0.f, 0.f, 0.f };
	FVector3 projLocalToV = { 0.f, 0.f, 0.f };

	// Smoothing group surface is to be rendered with
	int smoothingGroupIndex = -1;

	//
	// VkLightmap extra stuff that I dislike:
	//
	TArray<FVector3> verts;
	TArray<FVector2> uvs;

	// Touching light sources
	std::vector<LevelMeshLight*> LightList;
	std::vector<LevelMeshLight> LightListBuffer;

	// Lightmapper has a lot of additional padding around the borders
	int lightmapperAtlasPage = -1;
	int lightmapperAtlasX = -1;
	int lightmapperAtlasY = -1;
};


struct LevelMeshSmoothingGroup
{
	FVector4 plane = FVector4(0, 0, 1, 0);
	int sectorGroup = 0;
	std::vector<LevelMeshSurface*> surfaces;
};

struct LevelMeshPortal
{
	LevelMeshPortal() { transformation.loadIdentity(); }

	VSMatrix transformation;

	int sourceSectorGroup = 0;
	int targetSectorGroup = 0;

	inline FVector3 TransformPosition(const FVector3& pos) const
	{
		auto v = transformation * FVector4(pos, 1.0);
		return FVector3(v.X, v.Y, v.Z);
	}

	inline FVector3 TransformRotation(const FVector3& dir) const
	{
		auto v = transformation * FVector4(dir, 0.0);
		return FVector3(v.X, v.Y, v.Z);
	}

	// Checks only transformation
	inline bool IsInverseTransformationPortal(const LevelMeshPortal& portal) const
	{
		auto diff = portal.TransformPosition(TransformPosition(FVector3(0, 0, 0)));
		return abs(diff.X) < 0.001 && abs(diff.Y) < 0.001 && abs(diff.Z) < 0.001;
	}

	// Checks only transformation
	inline bool IsEqualTransformationPortal(const LevelMeshPortal& portal) const
	{
		auto diff = portal.TransformPosition(FVector3(0, 0, 0)) - TransformPosition(FVector3(0, 0, 0));
		return (abs(diff.X) < 0.001 && abs(diff.Y) < 0.001 && abs(diff.Z) < 0.001);
	}

	// Checks transformation, source and destiantion sector groups
	inline bool IsEqualPortal(const LevelMeshPortal& portal) const
	{
		return sourceSectorGroup == portal.sourceSectorGroup && targetSectorGroup == portal.targetSectorGroup && IsEqualTransformationPortal(portal);
	}

	// Checks transformation, source and destiantion sector groups
	inline bool IsInversePortal(const LevelMeshPortal& portal) const
	{
		return sourceSectorGroup == portal.targetSectorGroup && targetSectorGroup == portal.sourceSectorGroup && IsInverseTransformationPortal(portal);
	}
};

// for use with std::set to recursively go through portals and skip returning portals
struct RecursivePortalComparator
{
	bool operator()(const LevelMeshPortal& a, const LevelMeshPortal& b) const
	{
		return !a.IsInversePortal(b) && std::memcmp(&a.transformation, &b.transformation, sizeof(VSMatrix)) < 0;
	}
};

// for use with std::map to reject portals which have the same effect for light rays
struct IdenticalPortalComparator
{
	bool operator()(const LevelMeshPortal& a, const LevelMeshPortal& b) const
	{
		return !a.IsEqualPortal(b) && std::memcmp(&a.transformation, &b.transformation, sizeof(VSMatrix)) < 0;
	}
};


class LevelMesh
{
public:
	LevelMesh()
	{
		// Default portal
		LevelMeshPortal portal;
		Portals.Push(portal);
	}

	virtual ~LevelMesh() = default;

	TArray<FVector3> MeshVertices;
	TArray<int> MeshUVIndex;
	TArray<uint32_t> MeshElements;
	TArray<int> MeshSurfaceIndexes;

	std::unique_ptr<TriangleMeshShape> Collision;

	virtual LevelMeshSurface* GetSurface(int index) { return nullptr; }
	virtual int GetSurfaceCount() { return 0; }

	virtual void UpdateLightLists() { }

	TArray<LevelMeshSmoothingGroup> SmoothingGroups; // TODO fill
	TArray<LevelMeshPortal> Portals; // TODO fill

	int LMTextureCount = 0;
	int LMTextureSize = 0;

	FVector3 SunDirection = FVector3(0.0f, 0.0f, -1.0f);
	FVector3 SunColor = FVector3(0.0f, 0.0f, 0.0f);
	uint16_t LightmapSampleDistance = 16;

	bool Trace(const FVector3& start, FVector3 direction, float maxDist)
	{
		FVector3 end = start + direction * std::max(maxDist - 10.0f, 0.0f);
		return !TriangleMeshShape::find_any_hit(Collision.get(), start, end);
	}

	void BuildSmoothingGroups()
	{
		for (int i = 0, count = GetSurfaceCount(); i < count; i++)
		{
			auto surface = GetSurface(i);

			// Is this surface in the same plane as an existing smoothing group?
			int smoothingGroupIndex = -1;

			for (size_t j = 0; j < SmoothingGroups.Size(); j++)
			{
				if (surface->sectorGroup == SmoothingGroups[j].sectorGroup)
				{
					float direction = std::abs((SmoothingGroups[j].plane.XYZ() | surface->plane.XYZ()));
					if (direction >= 0.9999f && direction <= 1.001f)
					{
						auto point = (surface->plane.XYZ() * surface->plane.W);
						auto planeDistance = (SmoothingGroups[j].plane.XYZ() | point) - SmoothingGroups[j].plane.W;

						float dist = std::abs(planeDistance);
						if (dist <= 0.01f)
						{
							smoothingGroupIndex = (int)j;
							break;
						}
					}
				}
			}

			// Surface is in a new plane. Create a smoothing group for it
			if (smoothingGroupIndex == -1)
			{
				smoothingGroupIndex = SmoothingGroups.Size();

				LevelMeshSmoothingGroup group;
				group.plane = surface->plane;
				group.sectorGroup = surface->sectorGroup;
				SmoothingGroups.Push(group);
			}

			SmoothingGroups[smoothingGroupIndex].surfaces.push_back(surface);
			surface->smoothingGroupIndex = smoothingGroupIndex;
		}
	}
};
