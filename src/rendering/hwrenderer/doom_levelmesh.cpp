
#include "templates.h"
#include "doom_levelmesh.h"
#include "g_levellocals.h"
#include "texturemanager.h"
#include "playsim/p_lnspec.h"


#include "c_dispatch.h"
#include "g_levellocals.h"

#include "common/rendering/vulkan/accelstructs/vk_lightmap.h"
#include <vulkan/accelstructs/halffloat.h>

CCMD(dumplevelmesh)
{
	if (level.levelMesh)
	{
		level.levelMesh->DumpMesh(FString("levelmesh.obj"));
		Printf("Level mesh exported.");
	}
	else
	{
		Printf("No level mesh. Perhaps your level has no lightmap loaded?");
	}
}

DoomLevelMesh::DoomLevelMesh(FLevelLocals &doomMap)
{
	for (unsigned int i = 0; i < doomMap.sides.Size(); i++)
	{
		CreateSideSurfaces(doomMap, &doomMap.sides[i]);
	}

	CreateSubsectorSurfaces(doomMap);

	for (size_t i = 0; i < Surfaces.Size(); i++)
	{
		const Surface &s = Surfaces[i];
		int numVerts = s.numVerts;
		unsigned int pos = s.startVertIndex;
		FVector3* verts = &MeshVertices[pos];

		for (int j = 0; j < numVerts; j++)
		{
			MeshUVIndex.Push(j);
		}

		if (s.type == ST_FLOOR || s.type == ST_CEILING)
		{
			for (int j = 2; j < numVerts; j++)
			{
				if (!IsDegenerate(verts[0], verts[j - 1], verts[j]))
				{
					MeshElements.Push(pos);
					MeshElements.Push(pos + j - 1);
					MeshElements.Push(pos + j);
					MeshSurfaces.Push((int)i);
				}
			}
		}
		else if (s.type == ST_MIDDLEWALL || s.type == ST_UPPERWALL || s.type == ST_LOWERWALL)
		{
			if (!IsDegenerate(verts[0], verts[1], verts[2]))
			{
				MeshElements.Push(pos + 0);
				MeshElements.Push(pos + 1);
				MeshElements.Push(pos + 2);
				MeshSurfaces.Push((int)i);
			}
			if (!IsDegenerate(verts[1], verts[2], verts[3]))
			{
				MeshElements.Push(pos + 3);
				MeshElements.Push(pos + 2);
				MeshElements.Push(pos + 1);
				MeshSurfaces.Push((int)i);
			}
		}
	}

	Collision = std::make_unique<TriangleMeshShape>(MeshVertices.Data(), MeshVertices.Size(), MeshElements.Data(), MeshElements.Size());
}

void DoomLevelMesh::CreateSideSurfaces(FLevelLocals &doomMap, side_t *side)
{
	sector_t *front;
	sector_t *back;

	front = side->sector;
	back = (side->linedef->frontsector == front) ? side->linedef->backsector : side->linedef->frontsector;

	if (IsControlSector(front))
		return;

	FVector2 v1 = ToFVector2(side->V1()->fPos());
	FVector2 v2 = ToFVector2(side->V2()->fPos());

	float v1Top = (float)front->ceilingplane.ZatPoint(v1);
	float v1Bottom = (float)front->floorplane.ZatPoint(v1);
	float v2Top = (float)front->ceilingplane.ZatPoint(v2);
	float v2Bottom = (float)front->floorplane.ZatPoint(v2);

	int typeIndex = side->Index();

	FVector2 dx(v2.X - v1.X, v2.Y - v1.Y);
	float distance = dx.Length();

	// line_horizont consumes everything
	if (side->linedef->special == Line_Horizon && front != back)
	{
		Surface surf;
		surf.type = ST_MIDDLEWALL;
		surf.typeIndex = typeIndex;
		surf.bSky = front->GetTexture(sector_t::floor) == skyflatnum || front->GetTexture(sector_t::ceiling) == skyflatnum;

		FVector3 verts[4];
		verts[0].X = verts[2].X = v1.X;
		verts[0].Y = verts[2].Y = v1.Y;
		verts[1].X = verts[3].X = v2.X;
		verts[1].Y = verts[3].Y = v2.Y;
		verts[0].Z = v1Bottom;
		verts[1].Z = v2Bottom;
		verts[2].Z = v1Top;
		verts[3].Z = v2Top;

		surf.startVertIndex = MeshVertices.Size();
		surf.numVerts = 4;
		MeshVertices.Push(verts[0]);
		MeshVertices.Push(verts[1]);
		MeshVertices.Push(verts[2]);
		MeshVertices.Push(verts[3]);

		surf.plane = ToPlane(verts[0], verts[1], verts[2]);
		Surfaces.Push(surf);
		return;
	}

	if (back)
	{
		for (unsigned int j = 0; j < front->e->XFloor.ffloors.Size(); j++)
		{
			F3DFloor *xfloor = front->e->XFloor.ffloors[j];

			// Don't create a line when both sectors have the same 3d floor
			bool bothSides = false;
			for (unsigned int k = 0; k < back->e->XFloor.ffloors.Size(); k++)
			{
				if (back->e->XFloor.ffloors[k] == xfloor)
				{
					bothSides = true;
					break;
				}
			}
			if (bothSides)
				continue;

			Surface surf;
			surf.type = ST_MIDDLEWALL;
			surf.typeIndex = typeIndex;
			surf.controlSector = xfloor->model;
			surf.bSky = false;

			FVector3 verts[4];
			verts[0].X = verts[2].X = v2.X;
			verts[0].Y = verts[2].Y = v2.Y;
			verts[1].X = verts[3].X = v1.X;
			verts[1].Y = verts[3].Y = v1.Y;
			verts[0].Z = (float)xfloor->model->floorplane.ZatPoint(v2);
			verts[1].Z = (float)xfloor->model->floorplane.ZatPoint(v1);
			verts[2].Z = (float)xfloor->model->ceilingplane.ZatPoint(v2);
			verts[3].Z = (float)xfloor->model->ceilingplane.ZatPoint(v1);

			surf.startVertIndex = MeshVertices.Size();
			surf.numVerts = 4;
			MeshVertices.Push(verts[0]);
			MeshVertices.Push(verts[1]);
			MeshVertices.Push(verts[2]);
			MeshVertices.Push(verts[3]);

			surf.plane = ToPlane(verts[0], verts[1], verts[2]);
			Surfaces.Push(surf);
		}

		float v1TopBack = (float)back->ceilingplane.ZatPoint(v1);
		float v1BottomBack = (float)back->floorplane.ZatPoint(v1);
		float v2TopBack = (float)back->ceilingplane.ZatPoint(v2);
		float v2BottomBack = (float)back->floorplane.ZatPoint(v2);

		if (v1Top == v1TopBack && v1Bottom == v1BottomBack && v2Top == v2TopBack && v2Bottom == v2BottomBack)
		{
			return;
		}

		// bottom seg
		if (v1Bottom < v1BottomBack || v2Bottom < v2BottomBack)
		{
			if (IsBottomSideVisible(side))
			{
				Surface surf;

				FVector3 verts[4];
				verts[0].X = verts[2].X = v1.X;
				verts[0].Y = verts[2].Y = v1.Y;
				verts[1].X = verts[3].X = v2.X;
				verts[1].Y = verts[3].Y = v2.Y;
				verts[0].Z = v1Bottom;
				verts[1].Z = v2Bottom;
				verts[2].Z = v1BottomBack;
				verts[3].Z = v2BottomBack;

				surf.startVertIndex = MeshVertices.Size();
				surf.numVerts = 4;
				MeshVertices.Push(verts[0]);
				MeshVertices.Push(verts[1]);
				MeshVertices.Push(verts[2]);
				MeshVertices.Push(verts[3]);

				surf.plane = ToPlane(verts[0], verts[1], verts[2]);
				surf.type = ST_LOWERWALL;
				surf.typeIndex = typeIndex;
				surf.bSky = false;
				surf.controlSector = nullptr;

				Surfaces.Push(surf);
			}

			v1Bottom = v1BottomBack;
			v2Bottom = v2BottomBack;
		}

		// top seg
		if (v1Top > v1TopBack || v2Top > v2TopBack)
		{
			bool bSky = IsTopSideSky(front, back, side);
			if (bSky || IsTopSideVisible(side))
			{
				Surface surf;

				FVector3 verts[4];
				verts[0].X = verts[2].X = v1.X;
				verts[0].Y = verts[2].Y = v1.Y;
				verts[1].X = verts[3].X = v2.X;
				verts[1].Y = verts[3].Y = v2.Y;
				verts[0].Z = v1TopBack;
				verts[1].Z = v2TopBack;
				verts[2].Z = v1Top;
				verts[3].Z = v2Top;

				surf.startVertIndex = MeshVertices.Size();
				surf.numVerts = 4;
				MeshVertices.Push(verts[0]);
				MeshVertices.Push(verts[1]);
				MeshVertices.Push(verts[2]);
				MeshVertices.Push(verts[3]);

				surf.plane = ToPlane(verts[0], verts[1], verts[2]);
				surf.type = ST_UPPERWALL;
				surf.typeIndex = typeIndex;
				surf.bSky = bSky;
				surf.controlSector = nullptr;

				Surfaces.Push(surf);
			}

			v1Top = v1TopBack;
			v2Top = v2TopBack;
		}
	}

	// middle seg
	if (back == nullptr)
	{
		Surface surf;
		surf.bSky = false;

		FVector3 verts[4];
		verts[0].X = verts[2].X = v1.X;
		verts[0].Y = verts[2].Y = v1.Y;
		verts[1].X = verts[3].X = v2.X;
		verts[1].Y = verts[3].Y = v2.Y;
		verts[0].Z = v1Bottom;
		verts[1].Z = v2Bottom;
		verts[2].Z = v1Top;
		verts[3].Z = v2Top;

		surf.startVertIndex = MeshVertices.Size();
		surf.numVerts = 4;
		surf.bSky = false;
		MeshVertices.Push(verts[0]);
		MeshVertices.Push(verts[1]);
		MeshVertices.Push(verts[2]);
		MeshVertices.Push(verts[3]);

		surf.plane = ToPlane(verts[0], verts[1], verts[2]);
		surf.type = ST_MIDDLEWALL;
		surf.typeIndex = typeIndex;
		surf.controlSector = nullptr;

		Surfaces.Push(surf);
	}
}

void DoomLevelMesh::CreateFloorSurface(FLevelLocals &doomMap, subsector_t *sub, sector_t *sector, int typeIndex, bool is3DFloor)
{
	Surface surf;
	surf.bSky = IsSkySector(sector, sector_t::floor);

	if (!is3DFloor)
	{
		surf.plane = sector->floorplane;
	}
	else
	{
		surf.plane = sector->ceilingplane;
		surf.plane.FlipVert();
	}

	surf.numVerts = sub->numlines;
	surf.startVertIndex = MeshVertices.Size();
	MeshVertices.Resize(surf.startVertIndex + surf.numVerts);
	FVector3* verts = &MeshVertices[surf.startVertIndex];

	for (int j = 0; j < surf.numVerts; j++)
	{
		seg_t *seg = &sub->firstline[(surf.numVerts - 1) - j];
		FVector2 v1 = ToFVector2(seg->v1->fPos());

		verts[j].X = v1.X;
		verts[j].Y = v1.Y;
		verts[j].Z = (float)surf.plane.ZatPoint(verts[j]);
	}

	surf.type = ST_FLOOR;
	surf.typeIndex = typeIndex;
	surf.controlSector = is3DFloor ? sector : nullptr;

	Surfaces.Push(surf);
}

void DoomLevelMesh::CreateCeilingSurface(FLevelLocals &doomMap, subsector_t *sub, sector_t *sector, int typeIndex, bool is3DFloor)
{
	Surface surf;
	surf.bSky = IsSkySector(sector, sector_t::ceiling);

	if (!is3DFloor)
	{
		surf.plane = sector->ceilingplane;
	}
	else
	{
		surf.plane = sector->floorplane;
		surf.plane.FlipVert();
	}

	surf.numVerts = sub->numlines;
	surf.startVertIndex = MeshVertices.Size();
	MeshVertices.Resize(surf.startVertIndex + surf.numVerts);
	FVector3* verts = &MeshVertices[surf.startVertIndex];

	for (int j = 0; j < surf.numVerts; j++)
	{
		seg_t *seg = &sub->firstline[j];
		FVector2 v1 = ToFVector2(seg->v1->fPos());

		verts[j].X = v1.X;
		verts[j].Y = v1.Y;
		verts[j].Z = (float)surf.plane.ZatPoint(verts[j]);
	}

	surf.type = ST_CEILING;
	surf.typeIndex = typeIndex;
	surf.controlSector = is3DFloor ? sector : nullptr;

	Surfaces.Push(surf);
}

void DoomLevelMesh::CreateSubsectorSurfaces(FLevelLocals &doomMap)
{
	for (unsigned int i = 0; i < doomMap.subsectors.Size(); i++)
	{
		subsector_t *sub = &doomMap.subsectors[i];

		if (sub->numlines < 3)
		{
			continue;
		}

		sector_t *sector = sub->sector;
		if (!sector || IsControlSector(sector))
			continue;

		CreateFloorSurface(doomMap, sub, sector, i, false);
		CreateCeilingSurface(doomMap, sub, sector, i, false);

		for (unsigned int j = 0; j < sector->e->XFloor.ffloors.Size(); j++)
		{
			CreateFloorSurface(doomMap, sub, sector->e->XFloor.ffloors[j]->model, i, true);
			CreateCeilingSurface(doomMap, sub, sector->e->XFloor.ffloors[j]->model, i, true);
		}
	}
}

bool DoomLevelMesh::IsTopSideSky(sector_t* frontsector, sector_t* backsector, side_t* side)
{
	return IsSkySector(frontsector, sector_t::ceiling) && IsSkySector(backsector, sector_t::ceiling);
}

bool DoomLevelMesh::IsTopSideVisible(side_t* side)
{
	auto tex = TexMan.GetGameTexture(side->GetTexture(side_t::top), true);
	return tex && tex->isValid();
}

bool DoomLevelMesh::IsBottomSideVisible(side_t* side)
{
	auto tex = TexMan.GetGameTexture(side->GetTexture(side_t::bottom), true);
	return tex && tex->isValid();
}

bool DoomLevelMesh::IsSkySector(sector_t* sector, int plane)
{
	// plane is either sector_t::ceiling or sector_t::floor
	return sector->GetTexture(plane) == skyflatnum;
}

bool DoomLevelMesh::IsControlSector(sector_t* sector)
{
	//return sector->controlsector;
	return false;
}

bool DoomLevelMesh::IsDegenerate(const FVector3 &v0, const FVector3 &v1, const FVector3 &v2)
{
	// A degenerate triangle has a zero cross product for two of its sides.
	float ax = v1.X - v0.X;
	float ay = v1.Y - v0.Y;
	float az = v1.Z - v0.Z;
	float bx = v2.X - v0.X;
	float by = v2.Y - v0.Y;
	float bz = v2.Z - v0.Z;
	float crossx = ay * bz - az * by;
	float crossy = az * bx - ax * bz;
	float crossz = ax * by - ay * bx;
	float crosslengthsqr = crossx * crossx + crossy * crossy + crossz * crossz;
	return crosslengthsqr <= 1.e-6f;
}

void DoomLevelMesh::DumpMesh(const FString& filename) const
{
	auto f = fopen(filename.GetChars(), "w");

	fprintf(f, "# DoomLevelMesh debug export\n");
	fprintf(f, "# MeshVertices: %d, MeshElements: %d\n", MeshVertices.Size(), MeshElements.Size());

	double scale = 1 / 10.0;

	for (const auto& v : MeshVertices)
	{
		fprintf(f, "v %f %f %f\n", v.X * scale, v.Y * scale, v.Z * scale);
	}

	{
		const auto s = LightmapUvs.Size();
		for (unsigned i = 0; i + 1 < s; i += 2)
		{
			fprintf(f, "vt %f %f\n", LightmapUvs[i], LightmapUvs[i + 1]);
		}
	}

	const auto s = MeshElements.Size();
	for (unsigned i = 0; i + 2 < s; i += 3)
	{
		// fprintf(f, "f %d %d %d\n", MeshElements[i] + 1, MeshElements[i + 1] + 1, MeshElements[i + 2] + 1);
		fprintf(f, "f %d/%d %d/%d %d/%d\n",
			MeshElements[i + 0] + 1, MeshElements[i + 0] + 1,
			MeshElements[i + 1] + 1, MeshElements[i + 1] + 1,
			MeshElements[i + 2] + 1, MeshElements[i + 2] + 1);

	}

	fclose(f);
}

int DoomLevelMesh::SetupLightmapUvs(int lightmapSize)
{
	std::vector<Surface*> sortedSurfaces;
	sortedSurfaces.reserve(Surfaces.Size());

	for (auto& surface : Surfaces)
	{
		BuildSurfaceParams(lightmapSize, lightmapSize, surface);
		sortedSurfaces.push_back(&surface);
	}


	for (const auto& surface : Surfaces)
	{
		auto hwSurface = std::make_unique<hwrenderer::Surface>();

		for (int i = 0; i < surface.numVerts; ++i)
		{
			hwSurface->verts.Push(MeshVertices[surface.startVertIndex + i]);
		}

		for (int i = 0; i < surface.numVerts; ++i)
		{
			hwSurface->uvs.Push(
				FVector2(LightmapUvs[surface.startUvIndex + i * 2], LightmapUvs[surface.startUvIndex + i * 2 + 1]));
		}

		hwSurface->boundsMax = surface.bounds.max;
		hwSurface->boundsMin = surface.bounds.min;

		hwSurface->projLocalToU = surface.projLocalToU;
		hwSurface->projLocalToV = surface.projLocalToV;
		hwSurface->smoothingGroupIndex = 0;
		hwSurface->texHeight = surface.texHeight;
		hwSurface->texWidth = surface.texWidth;

		hwSurface->translateWorldToLocal = surface.translateWorldToLocal;
		hwSurface->type = hwrenderer::SurfaceType(surface.type);

		hwSurface->texPixels.resize(surface.texWidth * surface.texHeight);

		for (auto& pixel : hwSurface->texPixels)
		{
			pixel.X = 0.0;
			pixel.Y = 1.0;
			pixel.Z = 0.0;
		}

		// TODO push
		surfaces.push_back(std::move(hwSurface));

		SurfaceInfo info;
		info.Normal = FVector3(surface.plane.Normal());
		info.PortalIndex = 0;
		info.SamplingDistance = surface.sampleDimension;
		info.Sky = surface.bSky;

		surfaceInfo.Push(info);


	}

	{
		hwrenderer::SmoothingGroup smoothing;

		for (auto& surface : surfaces)
		{
			smoothing.surfaces.push_back(surface.get());
		}
		smoothingGroups.Push(std::move(smoothing));
	}

	std::sort(sortedSurfaces.begin(), sortedSurfaces.end(), [](Surface* a, Surface* b) { return a->texHeight != b->texHeight ? a->texHeight > b->texHeight : a->texWidth > b->texWidth; });

	RectPacker packer(lightmapSize, lightmapSize, RectPacker::Spacing(0));

	for (Surface* surf : sortedSurfaces)
	{
		FinishSurface(lightmapSize, lightmapSize, packer, *surf);
	}

	// You have no idea how long this took me to figure out...

	// Reorder vertices into renderer format
	for (Surface& surface : Surfaces)
	{
		if (surface.type == ST_FLOOR)
		{
			// reverse vertices on floor
			for (int j = surface.startUvIndex + surface.numVerts * 2 - 2, k = surface.startUvIndex; j > k; j-=2, k+=2)
			{
				std::swap(LightmapUvs[k], LightmapUvs[j]);
				std::swap(LightmapUvs[k + 1], LightmapUvs[j + 1]);
			}
		}
		else if (surface.type != ST_CEILING) // walls
		{
			// from 0 1 2 3
			// to   0 2 1 3
			std::swap(LightmapUvs[surface.startUvIndex + 2 * 1], LightmapUvs[surface.startUvIndex + 2 * 2]);
			std::swap(LightmapUvs[surface.startUvIndex + 2 * 2], LightmapUvs[surface.startUvIndex + 2 * 3]);

			std::swap(LightmapUvs[surface.startUvIndex + 2 * 1 + 1], LightmapUvs[surface.startUvIndex + 2 * 2 + 1]);
			std::swap(LightmapUvs[surface.startUvIndex + 2 * 2 + 1], LightmapUvs[surface.startUvIndex + 2 * 3 + 1]);
		}
	}


	return packer.getNumPages();
}

void DoomLevelMesh::FinishSurface(int lightmapTextureWidth, int lightmapTextureHeight, RectPacker& packer, Surface& surface)
{
	int sampleWidth = surface.texWidth;
	int sampleHeight = surface.texHeight;

	auto result = packer.insert(sampleWidth, sampleHeight);
	int x = result.pos.x, y = result.pos.y;
	surface.atlasPageIndex = (int)result.pageIndex;

	// calculate final texture coordinates
	auto uvIndex = surface.startUvIndex;
	for (int i = 0; i < (int)surface.numVerts; i++)
	{
		auto& u = LightmapUvs[uvIndex++];
		auto& v = LightmapUvs[uvIndex++];
		u = (u + x) / (float)lightmapTextureWidth;
		v = (v + y) / (float)lightmapTextureHeight;
	}
	
	surface.atlasX = x;
	surface.atlasY = y;

#if 0
	while (result.pageIndex >= textures.size())
	{
		textures.push_back(std::make_unique<LightmapTexture>(textureWidth, textureHeight));
	}

	uint16_t* currentTexture = textures[surface->atlasPageIndex]->Pixels();

	FVector3* colorSamples = surface->texPixels.data();
	// store results to lightmap texture
	for (int i = 0; i < sampleHeight; i++)
	{
		for (int j = 0; j < sampleWidth; j++)
		{
			// get texture offset
			int offs = ((textureWidth * (i + surface->atlasY)) + surface->atlasX) * 3;

			// convert RGB to bytes
			currentTexture[offs + j * 3 + 0] = floatToHalf(clamp(colorSamples[i * sampleWidth + j].x, 0.0f, 65000.0f));
			currentTexture[offs + j * 3 + 1] = floatToHalf(clamp(colorSamples[i * sampleWidth + j].y, 0.0f, 65000.0f));
			currentTexture[offs + j * 3 + 2] = floatToHalf(clamp(colorSamples[i * sampleWidth + j].z, 0.0f, 65000.0f));
		}
	}
#endif
}

BBox DoomLevelMesh::GetBoundsFromSurface(const Surface& surface) const
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

DoomLevelMesh::PlaneAxis DoomLevelMesh::BestAxis(const secplane_t& p)
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

void DoomLevelMesh::BuildSurfaceParams(int lightMapTextureWidth, int lightMapTextureHeight, Surface& surface)
{
	secplane_t* plane;
	BBox bounds;
	FVector3 roundedSize;
	FVector3 tOrigin;
	int width;
	int height;
	float d;

	plane = &surface.plane;
	bounds = GetBoundsFromSurface(surface);
	surface.bounds = bounds;

	if (surface.sampleDimension <= 0)
	{
		surface.sampleDimension = 16;
	}
	//surface->sampleDimension = Math::RoundPowerOfTwo(surface->sampleDimension);

	// round off dimensions
	for (int i = 0; i < 3; i++)
	{
		bounds.min[i] = surface.sampleDimension * (floor(bounds.min[i] / surface.sampleDimension) - 1);
		bounds.max[i] = surface.sampleDimension * (ceil(bounds.max[i] / surface.sampleDimension) + 1);

		roundedSize[i] = (bounds.max[i] - bounds.min[i]) / surface.sampleDimension;
	}

	FVector3 tCoords[2] = { FVector3(0.0f, 0.0f, 0.0f), FVector3(0.0f, 0.0f, 0.0f) };

	PlaneAxis axis = BestAxis(*plane);

	switch (axis)
	{
	case AXIS_YZ:
		width = (int)roundedSize.Y;
		height = (int)roundedSize.Z;
		tCoords[0].Y = 1.0f / surface.sampleDimension;
		tCoords[1].Z = 1.0f / surface.sampleDimension;
		break;

	case AXIS_XZ:
		width = (int)roundedSize.X;
		height = (int)roundedSize.Z;
		tCoords[0].X = 1.0f / surface.sampleDimension;
		tCoords[1].Z = 1.0f / surface.sampleDimension;
		break;

	case AXIS_XY:
		width = (int)roundedSize.X;
		height = (int)roundedSize.Y;
		tCoords[0].X = 1.0f / surface.sampleDimension;
		tCoords[1].Y = 1.0f / surface.sampleDimension;
		break;
	}

	// clamp width
	if (width > lightMapTextureWidth - 2)
	{
		tCoords[0] *= ((float)(lightMapTextureWidth - 2) / (float)width);
		width = (lightMapTextureWidth - 2);
	}

	// clamp height
	if (height > lightMapTextureHeight - 2)
	{
		tCoords[1] *= ((float)(lightMapTextureHeight - 2) / (float)height);
		height = (lightMapTextureHeight - 2);
	}

	surface.translateWorldToLocal = bounds.min;
	surface.projLocalToU = tCoords[0];
	surface.projLocalToV = tCoords[1];

	surface.startUvIndex = AllocUvs(surface.numVerts);
	auto uv = surface.startUvIndex;
	for (int i = 0; i < surface.numVerts; i++)
	{
		FVector3 tDelta = MeshVertices[surface.startVertIndex + i] - surface.translateWorldToLocal;

		LightmapUvs[uv++] = (tDelta | surface.projLocalToU);
		LightmapUvs[uv++] = (tDelta | surface.projLocalToV);
	}


	tOrigin = bounds.min;

	// project tOrigin and tCoords so they lie on the plane
	d = float(((bounds.min | FVector3(plane->Normal())) - plane->D) / plane->Normal()[axis]); //d = (plane->PointToDist(bounds.min)) / plane->Normal()[axis];
	tOrigin[axis] -= d;

	for (int i = 0; i < 2; i++)
	{
		tCoords[i].MakeUnit();
		d = (tCoords[i] | FVector3(plane->Normal())) / plane->Normal()[axis]; //d = dot(tCoords[i], plane->Normal()) / plane->Normal()[axis];
		tCoords[i][axis] -= d;
	}

	surface.texWidth = width;
	surface.texHeight = height;
	//surface->texPixels.resize(width * height);
	surface.worldOrigin = tOrigin;
	surface.worldStepX = tCoords[0] * (float)surface.sampleDimension;
	surface.worldStepY = tCoords[1] * (float)surface.sampleDimension;
}
