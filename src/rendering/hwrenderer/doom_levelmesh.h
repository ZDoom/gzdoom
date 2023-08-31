
#pragma once

#include "hw_levelmesh.h"
#include "tarray.h"
#include "vectors.h"
#include "r_defs.h"
#include "bounds.h"

struct FLevelLocals;

struct Surface
{
	SurfaceType type;
	int typeIndex;
	int numVerts;
	unsigned int startVertIndex;
	secplane_t plane;
	sector_t *controlSector;
	bool bSky;

	//
	// Required for internal lightmapper:

	int sampleDimension = 0;

	// Lightmap world coordinates for the texture
	FVector3 worldOrigin = { 0.f, 0.f, 0.f };
	FVector3 worldStepX = { 0.f, 0.f, 0.f };
	FVector3 worldStepY = { 0.f, 0.f, 0.f };

	// Calculate world coordinates to UV coordinates
	FVector3 translateWorldToLocal = { 0.f, 0.f, 0.f };
	FVector3 projLocalToU = { 0.f, 0.f, 0.f };
	FVector3 projLocalToV = { 0.f, 0.f, 0.f };

	// Output lightmap for the surface
	int texWidth = 0;
	int texHeight = 0;

	// UV coordinates for the vertices
	int startUvIndex = -666;
};

class DoomLevelMesh : public hwrenderer::LevelMesh
{
public:
	DoomLevelMesh(FLevelLocals &doomMap);

	bool TraceSky(const FVector3& start, FVector3 direction, float dist)
	{
		FVector3 end = start + direction * dist;
		TraceHit hit = TriangleMeshShape::find_first_hit(Collision.get(), start, end);
		if (hit.fraction == 1.0f)
			return true;

		int surfaceIndex = MeshSurfaces[hit.triangle];
		const Surface& surface = Surfaces[surfaceIndex];
		return surface.bSky;
	}

	TArray<Surface> Surfaces;
	TArray<float> LightmapUvs;

	void DumpMesh(const FString& filename) const;

private:
	void CreateSubsectorSurfaces(FLevelLocals &doomMap);
	void CreateCeilingSurface(FLevelLocals &doomMap, subsector_t *sub, sector_t *sector, int typeIndex, bool is3DFloor);
	void CreateFloorSurface(FLevelLocals &doomMap, subsector_t *sub, sector_t *sector, int typeIndex, bool is3DFloor);
	void CreateSideSurfaces(FLevelLocals &doomMap, side_t *side);

	static bool IsTopSideSky(sector_t* frontsector, sector_t* backsector, side_t* side);
	static bool IsTopSideVisible(side_t* side);
	static bool IsBottomSideVisible(side_t* side);
	static bool IsSkySector(sector_t* sector, int plane);
	static bool IsControlSector(sector_t* sector);

	static secplane_t ToPlane(const FVector3& pt1, const FVector3& pt2, const FVector3& pt3)
	{
		FVector3 n = ((pt2 - pt1) ^ (pt3 - pt2)).Unit();
		float d = pt1 | n;
		secplane_t p;
		p.set(n.X, n.Y, n.Z, d);
		return p;
	}

	static FVector2 ToFVector2(const DVector2& v) { return FVector2((float)v.X, (float)v.Y); }
	static FVector3 ToFVector3(const DVector3& v) { return FVector3((float)v.X, (float)v.Y, (float)v.Z); }
	static FVector4 ToFVector4(const DVector4& v) { return FVector4((float)v.X, (float)v.Y, (float)v.Z, (float)v.W); }

	static bool IsDegenerate(const FVector3 &v0, const FVector3 &v1, const FVector3 &v2);

	// WIP internal lightmapper

	BBox GetBoundsFromSurface(const Surface& surface) const
	{
		constexpr float M_INFINITY = 1e30; // TODO cleanup

		FVector3 low(M_INFINITY, M_INFINITY, M_INFINITY);
		FVector3 hi(-M_INFINITY, -M_INFINITY, -M_INFINITY);

		for (int i = int(surface.startVertIndex); i < int(surface.startVertIndex) + surface.numVerts; i++)
		{
			for (int j = 0; j < 3; j++)
			{
				if (MeshVertices[i][j] < low[j])
				{
					low[j] = MeshVertices[i][j];
				}
				if (MeshVertices[i][j] > hi[j])
				{
					hi[j] = MeshVertices[i][j];
				}
			}
		}

		BBox bounds;
		bounds.Clear();
		bounds.min = low;
		bounds.max = hi;
		return bounds;
	}

	enum PlaneAxis
	{
		AXIS_YZ = 0,
		AXIS_XZ,
		AXIS_XY
	};

	inline static PlaneAxis BestAxis(const secplane_t& p)
	{
		float na = fabs(float(p.Normal().X));
		float nb = fabs(float(p.Normal().Y));
		float nc = fabs(float(p.Normal().Z));

		// figure out what axis the plane lies on
		if (na >= nb && na >= nc)
		{
			return AXIS_YZ;
		}
		else if (nb >= na && nb >= nc)
		{
			return AXIS_XZ;
		}

		return AXIS_XY;
	}

	int AllocUvs(int amount)
	{
		return LightmapUvs.Reserve(amount * 2);
	}

	public:
		void BuildSurfaceParams(int lightMapTextureWidth, int lightMapTextureHeight, Surface& surface);
};
