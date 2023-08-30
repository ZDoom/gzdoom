
#include "templates.h"
#include "doom_levelmesh.h"
#include "g_levellocals.h"
#include "texturemanager.h"
#include "playsim/p_lnspec.h"

#include "c_dispatch.h"
#include "g_levellocals.h"

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
		surf.type = ST_MIDDLEWALL;
		surf.typeIndex = typeIndex;
		surf.controlSector = nullptr;

		Surfaces.Push(surf);
	}
}

void DoomLevelMesh::CreateFloorSurface(FLevelLocals &doomMap, subsector_t *sub, sector_t *sector, int typeIndex, bool is3DFloor)
{
	Surface surf;

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
	surf.bSky = IsSkySector(sector);

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
	return IsSkySector(frontsector) && IsSkySector(backsector);
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

bool DoomLevelMesh::IsSkySector(sector_t* sector)
{
	return sector->GetTexture(sector_t::ceiling) == skyflatnum;
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

	double scale = 1 / 100.0;

	for (const auto& v : MeshVertices)
	{
		fprintf(f, "v %f %f %f\n", v.X * scale, v.Y * scale, v.Z * scale);
	}

	const auto s = MeshElements.Size();
	for (auto i = 0; i + 2 < s; i += 3)
	{
		fprintf(f, "f %d %d %d\n", MeshElements[i] + 1, MeshElements[i + 1] + 1, MeshElements[i + 2] + 1);
	}

	fclose(f);
}
