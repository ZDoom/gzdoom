
#include "templates.h"
#include "doom_levelmesh.h"
#include "g_levellocals.h"
#include "texturemanager.h"
#include "playsim/p_lnspec.h"
#include "c_dispatch.h"
#include "g_levellocals.h"
#include "a_dynlight.h"
#include "common/rendering/vulkan/accelstructs/vk_lightmap.h"
#include <vulkan/accelstructs/halffloat.h>

CCMD(dumplevelmesh)
{
	if (level.levelMesh)
	{
		level.levelMesh->DumpMesh(FString("levelmesh.obj"), FString("levelmesh.mtl"));
		Printf("Level mesh exported.");
	}
	else
	{
		Printf("No level mesh. Perhaps your level has no lightmap loaded?");
	}
}

CCMD(invalidatelightmap)
{
	int count = 0;
	for (auto& surface : level.levelMesh->Surfaces)
	{
		if (!surface.needsUpdate)
			++count;
		surface.needsUpdate = true;
	}
	Printf("Marked %d out of %d surfaces for update.\n", count, level.levelMesh->Surfaces.Size());
}

EXTERN_CVAR(Float, lm_scale);

DoomLevelMesh::DoomLevelMesh(FLevelLocals &doomMap)
{
	SunColor = doomMap.SunColor; // TODO keep only one copy?
	SunDirection = doomMap.SunDirection;
	LightmapSampleDistance = doomMap.LightmapSampleDistance;

	for (unsigned int i = 0; i < doomMap.sides.Size(); i++)
	{
		CreateSideSurfaces(doomMap, &doomMap.sides[i]);
	}

	CreateSubsectorSurfaces(doomMap);

	for (size_t i = 0; i < Surfaces.Size(); i++)
	{
		const auto &s = Surfaces[i];
		int numVerts = s.numVerts;
		unsigned int pos = s.startVertIndex;
		FVector3* verts = &MeshVertices[pos];

		for (int j = 0; j < numVerts; j++)
		{
			MeshUVIndex.Push(j);
		}

		if (s.Type == ST_FLOOR || s.Type == ST_CEILING)
		{
			for (int j = 2; j < numVerts; j++)
			{
				if (!IsDegenerate(verts[0], verts[j - 1], verts[j]))
				{
					MeshElements.Push(pos);
					MeshElements.Push(pos + j - 1);
					MeshElements.Push(pos + j);
					MeshSurfaceIndexes.Push((int)i);
				}
			}
		}
		else if (s.Type == ST_MIDDLESIDE || s.Type == ST_UPPERSIDE || s.Type == ST_LOWERSIDE)
		{
			if (!IsDegenerate(verts[0], verts[1], verts[2]))
			{
				MeshElements.Push(pos + 0);
				MeshElements.Push(pos + 1);
				MeshElements.Push(pos + 2);
				MeshSurfaceIndexes.Push((int)i);
			}
			if (!IsDegenerate(verts[1], verts[2], verts[3]))
			{
				MeshElements.Push(pos + 3);
				MeshElements.Push(pos + 2);
				MeshElements.Push(pos + 1);
				MeshSurfaceIndexes.Push((int)i);
			}
		}
	}

	SetupLightmapUvs();

	Collision = std::make_unique<TriangleMeshShape>(MeshVertices.Data(), MeshVertices.Size(), MeshElements.Data(), MeshElements.Size());
}

void DoomLevelMesh::CreatePortals()
{
	std::map<LevelMeshPortal, int, IdenticalPortalComparator> transformationIndices; // TODO use the list of portals from the level to avoids duplicates?
	transformationIndices.emplace(LevelMeshPortal{}, 0); // first portal is an identity matrix

	for (auto& surface : Surfaces)
	{
		bool hasPortal = [&]() {
			if (surface.Type == ST_FLOOR || surface.Type == ST_CEILING)
			{
				return !surface.Subsector->sector->GetPortalDisplacement(surface.Type == ST_FLOOR ? sector_t::floor : sector_t::ceiling).isZero();
			}
			else if (surface.Type == ST_MIDDLESIDE)
			{
				return surface.Side->linedef->isLinePortal();
			}
			return false; // It'll take eternity to get lower/upper side portals into the ZDoom family.
		}();

		if (hasPortal)
		{
			auto transformation = [&]() {
				VSMatrix matrix;
				matrix.loadIdentity();

				if (surface.Type == ST_FLOOR || surface.Type == ST_CEILING)
				{
					auto d = surface.Subsector->sector->GetPortalDisplacement(surface.Type == ST_FLOOR ? sector_t::floor : sector_t::ceiling);
					matrix.translate((float)d.X, (float)d.Y, 0.0f);
				}
				else if(surface.Type == ST_MIDDLESIDE)
				{
					auto sourceLine = surface.Side->linedef;

					if (sourceLine->isLinePortal())
					{
						auto targetLine = sourceLine->getPortalDestination();
						if (targetLine && sourceLine->frontsector && targetLine->frontsector)
						{
							double z = 0;

							// auto xy = surface.Side->linedef->getPortalDisplacement(); // Works only for static portals... ugh
							auto sourceXYZ = DVector2((sourceLine->v1->fX() + sourceLine->v2->fX()) / 2, (sourceLine->v2->fY() + sourceLine->v1->fY()) / 2);
							auto targetXYZ = DVector2((targetLine->v1->fX() + targetLine->v2->fX()) / 2, (targetLine->v2->fY() + targetLine->v1->fY()) / 2);

							// floor or ceiling alignment
							auto alignment = surface.Side->linedef->GetLevel()->linePortals[surface.Side->linedef->portalindex].mAlign;
							if (alignment != PORG_ABSOLUTE)
							{
								int plane = alignment == PORG_FLOOR ? 1 : 0;

								auto& sourcePlane = plane ? sourceLine->frontsector->floorplane : sourceLine->frontsector->ceilingplane;
								auto& targetPlane = plane ? targetLine->frontsector->floorplane : targetLine->frontsector->ceilingplane;

								auto tz = targetPlane.ZatPoint(targetXYZ);
								auto sz = sourcePlane.ZatPoint(sourceXYZ);

								z = tz - sz;
							}

							matrix.rotate((float)sourceLine->getPortalAngleDiff().Degrees(), 0.0f, 0.0f, 1.0f);
							matrix.translate((float)(targetXYZ.X - sourceXYZ.X), (float)(targetXYZ.Y - sourceXYZ.Y), (float)z);
						}
					}
				}
				return matrix;
			}();

			LevelMeshPortal portal;
			portal.transformation = transformation;

			auto& index = transformationIndices[portal];

			if (index == 0) // new transformation was created
			{
				index = Portals.Size();
				Portals.Push(portal);
			}

			surface.portalIndex = index;
		}
		else
		{
			surface.portalIndex = 0;
		}
	}
}

void DoomLevelMesh::PropagateLight(const LevelMeshLight* light, std::set<LevelMeshPortal, RecursivePortalComparator>& touchedPortals, int lightPropagationRecursiveDepth)
{
	if (++lightPropagationRecursiveDepth > 32) // TODO is this too much?
	{
		return;
	}

	SphereShape sphere;
	sphere.center = FVector3(light->RelativeOrigin);
	sphere.radius = light->Radius;
	std::set<LevelMeshPortal, RecursivePortalComparator> portalsToErase;
	for (int triangleIndex : TriangleMeshShape::find_all_hits(Collision.get(), &sphere))
	{
		auto& surface = Surfaces[MeshSurfaceIndexes[triangleIndex]];

		// TODO skip any surface which isn't physically connected to the sector group in which the light resides
		//if (light-> == surface.sectorGroup)
		{
			if (surface.portalIndex >= 0)
			{
				auto& portal = Portals[surface.portalIndex];

				if (touchedPortals.insert(portal).second)
				{
					auto newLight = std::make_unique<LevelMeshLight>(*light);
					auto fakeLight = newLight.get();
					Lights.push_back(std::move(newLight));

					fakeLight->RelativeOrigin = portal.TransformPosition(light->RelativeOrigin);
					//fakeLight->sectorGroup = portal->targetSectorGroup;

					PropagateLight(fakeLight, touchedPortals, lightPropagationRecursiveDepth);
					portalsToErase.insert(portal);
				}
			}

			// Add light to the list if it isn't already there
			// TODO in order for this to work the light list be fed from global light buffer? Or just somehow de-duplicate portals?
			bool found = false;
			for (const LevelMeshLight* light2 : surface.LightList)
			{
				if (light2 == light)
				{
					found = true;
					break;
				}
			}

			if (!found)
				surface.LightList.push_back(light);
		}
	}

	for (auto& portal : portalsToErase)
	{
		touchedPortals.erase(portal); // Dear me: I wonder what was the reason for all of this.
	}
}

void DoomLevelMesh::CreateLightList()
{
	std::set<FDynamicLight*> lightList; // bit silly ain't it?

	Lights.clear();
	for (auto& surface : Surfaces)
	{
		surface.LightList.clear();

		if (surface.Type == ST_FLOOR || surface.Type == ST_CEILING)
		{
			auto node = surface.Subsector->section->lighthead;

			while (node)
			{
				FDynamicLight* light = node->lightsource;

				if (light->Trace())
				{
					if (lightList.insert(light).second)
					{
						DVector3 pos = light->Pos; //light->PosRelative(portalgroup);

						LevelMeshLight meshlight;
						meshlight.Origin = { (float)pos.X, (float)pos.Y, (float)pos.Z };
						meshlight.RelativeOrigin = meshlight.Origin;
						meshlight.Radius = (float)light->GetRadius();
						meshlight.Intensity = (float)light->target->Alpha;
						if (light->IsSpot())
						{
							meshlight.InnerAngleCos = (float)light->pSpotInnerAngle->Cos();
							meshlight.OuterAngleCos = (float)light->pSpotOuterAngle->Cos();

							DAngle negPitch = -*light->pPitch;
							DAngle Angle = light->target->Angles.Yaw;
							double xzLen = negPitch.Cos();
							meshlight.SpotDir.X = float(-Angle.Cos() * xzLen);
							meshlight.SpotDir.Y = float(-Angle.Sin() * xzLen);
							meshlight.SpotDir.Z = float(-negPitch.Sin());
						}
						else
						{
							meshlight.InnerAngleCos = -1.0f;
							meshlight.OuterAngleCos = -1.0f;
							meshlight.SpotDir.X = 0.0f;
							meshlight.SpotDir.Y = 0.0f;
							meshlight.SpotDir.Z = 0.0f;
						}
						meshlight.Color.X = light->GetRed() * (1.0f / 255.0f);
						meshlight.Color.Y = light->GetGreen() * (1.0f / 255.0f);
						meshlight.Color.Z = light->GetBlue() * (1.0f / 255.0f);

						Lights.push_back(std::make_unique<LevelMeshLight>(meshlight));
					}
				}

				node = node->nextLight;
			}
		}
	}

	std::set<LevelMeshPortal, RecursivePortalComparator> touchedPortals;
	touchedPortals.insert(Portals[0]);

	for (int i = 0, count = (int)Lights.size(); i < count; ++i) // The array expands as the lights are duplicated/propagated
	{
		PropagateLight(Lights[i].get(), touchedPortals, 0);
	}
}

void DoomLevelMesh::UpdateLightLists()
{
	CreateLightList(); // full recreation
}

void DoomLevelMesh::BindLightmapSurfacesToGeometry(FLevelLocals& doomMap)
{
	// Allocate room for all surfaces

	unsigned int allSurfaces = 0;

	for (unsigned int i = 0; i < doomMap.sides.Size(); i++)
		allSurfaces += 4 + doomMap.sides[i].sector->e->XFloor.ffloors.Size();

	for (unsigned int i = 0; i < doomMap.subsectors.Size(); i++)
		allSurfaces += 2 + doomMap.subsectors[i].sector->e->XFloor.ffloors.Size() * 2;

	doomMap.LMSurfaces.Resize(allSurfaces);
	memset(&doomMap.LMSurfaces[0], 0, sizeof(DoomLevelMeshSurface*) * allSurfaces);

	// Link the surfaces to sectors, sides and their 3D floors

	unsigned int offset = 0;
	for (unsigned int i = 0; i < doomMap.sides.Size(); i++)
	{
		auto& side = doomMap.sides[i];
		side.lightmap = &doomMap.LMSurfaces[offset];
		offset += 4 + side.sector->e->XFloor.ffloors.Size();
	}
	for (unsigned int i = 0; i < doomMap.subsectors.Size(); i++)
	{
		auto& subsector = doomMap.subsectors[i];
		unsigned int count = 1 + subsector.sector->e->XFloor.ffloors.Size();
		subsector.lightmap[0] = &doomMap.LMSurfaces[offset];
		subsector.lightmap[1] = &doomMap.LMSurfaces[offset + count];
		offset += count * 2;
	}

	// Copy and build properties
	for (auto& surface : Surfaces)
	{
		surface.TexCoords = (float*)&LightmapUvs[surface.startUvIndex];

		if (surface.Type == ST_FLOOR || surface.Type == ST_CEILING)
		{
			surface.Subsector = &doomMap.subsectors[surface.typeIndex];
			if (surface.Subsector->firstline && surface.Subsector->firstline->sidedef)
				surface.Subsector->firstline->sidedef->sector->HasLightmaps = true;
			SetSubsectorLightmap(&surface);
		}
		else
		{
			surface.Side = &doomMap.sides[surface.typeIndex];
			SetSideLightmap(&surface);
		}
	}

	// Runtime helper
	for (auto& surface : Surfaces)
	{
		if ((surface.Type == ST_FLOOR || surface.Type == ST_CEILING) && surface.ControlSector)
		{
			XFloorToSurface[surface.Subsector->sector].Push(&surface);
		}
	}
}

void DoomLevelMesh::SetSubsectorLightmap(DoomLevelMeshSurface* surface)
{
	if (!surface->ControlSector)
	{
		int index = surface->Type == ST_CEILING ? 1 : 0;
		surface->Subsector->lightmap[index][0] = surface;
	}
	else
	{
		int index = surface->Type == ST_CEILING ? 0 : 1;
		const auto& ffloors = surface->Subsector->sector->e->XFloor.ffloors;
		for (unsigned int i = 0; i < ffloors.Size(); i++)
		{
			if (ffloors[i]->model == surface->ControlSector)
			{
				surface->Subsector->lightmap[index][i + 1] = surface;
			}
		}
	}
}

void DoomLevelMesh::SetSideLightmap(DoomLevelMeshSurface* surface)
{
	if (!surface->ControlSector)
	{
		if (surface->Type == ST_UPPERSIDE)
		{
			surface->Side->lightmap[0] = surface;
		}
		else if (surface->Type == ST_MIDDLESIDE)
		{
			surface->Side->lightmap[1] = surface;
			surface->Side->lightmap[2] = surface;
		}
		else if (surface->Type == ST_LOWERSIDE)
		{
			surface->Side->lightmap[3] = surface;
		}
	}
	else
	{
		const auto& ffloors = surface->Side->sector->e->XFloor.ffloors;
		for (unsigned int i = 0; i < ffloors.Size(); i++)
		{
			if (ffloors[i]->model == surface->ControlSector)
			{
				surface->Side->lightmap[4 + i] = surface;
			}
		}
	}
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


	// line portal
	if (side->linedef->getPortal() && side->linedef->frontsector == front)
	{
		float texWidth = 128.0f;
		float texHeight = 128.0f;

		DoomLevelMeshSurface surf;
		surf.Type = ST_MIDDLESIDE;
		surf.typeIndex = typeIndex;
		surf.bSky = front->GetTexture(sector_t::floor) == skyflatnum || front->GetTexture(sector_t::ceiling) == skyflatnum;
		surf.sampleDimension = side->textures[side_t::mid].LightmapSampleDistance;

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

		surf.plane = ToPlane(verts[0], verts[1], verts[2], verts[3]);
		Surfaces.Push(surf);
		return;
	}

	// line_horizont consumes everything
	if (side->linedef->special == Line_Horizon && front != back)
	{
		DoomLevelMeshSurface surf;
		surf.Type = ST_MIDDLESIDE;
		surf.typeIndex = typeIndex;
		surf.bSky = front->GetTexture(sector_t::floor) == skyflatnum || front->GetTexture(sector_t::ceiling) == skyflatnum;
		surf.sampleDimension = side->textures[side_t::mid].LightmapSampleDistance;

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

		surf.plane = ToPlane(verts[0], verts[1], verts[2], verts[3]);
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

			DoomLevelMeshSurface surf;
			surf.Type = ST_MIDDLESIDE;
			surf.typeIndex = typeIndex;
			surf.ControlSector = xfloor->model;
			surf.bSky = false;
			surf.sampleDimension = side->textures[side_t::mid].LightmapSampleDistance;

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

			surf.plane = ToPlane(verts[0], verts[1], verts[2], verts[3]);
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
				DoomLevelMeshSurface surf;

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

				surf.plane = ToPlane(verts[0], verts[1], verts[2], verts[3]);
				surf.Type = ST_LOWERSIDE;
				surf.typeIndex = typeIndex;
				surf.bSky = false;
				surf.sampleDimension = side->textures[side_t::bottom].LightmapSampleDistance;
				surf.ControlSector = nullptr;

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
				DoomLevelMeshSurface surf;

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

				surf.plane = ToPlane(verts[0], verts[1], verts[2], verts[3]);
				surf.Type = ST_UPPERSIDE;
				surf.typeIndex = typeIndex;
				surf.bSky = bSky;
				surf.sampleDimension = side->textures[side_t::top].LightmapSampleDistance;
				surf.ControlSector = nullptr;

				Surfaces.Push(surf);
			}

			v1Top = v1TopBack;
			v2Top = v2TopBack;
		}
	}

	// middle seg
	if (back == nullptr)
	{
		DoomLevelMeshSurface surf;
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

		surf.plane = ToPlane(verts[0], verts[1], verts[2], verts[3]);
		surf.Type = ST_MIDDLESIDE;
		surf.typeIndex = typeIndex;
		surf.sampleDimension = side->textures[side_t::mid].LightmapSampleDistance;
		surf.ControlSector = nullptr;

		Surfaces.Push(surf);
	}
}

void DoomLevelMesh::CreateFloorSurface(FLevelLocals &doomMap, subsector_t *sub, sector_t *sector, sector_t *controlSector, int typeIndex)
{
	DoomLevelMeshSurface surf;
	surf.bSky = IsSkySector(sector, sector_t::floor);

	secplane_t plane;
	if (!controlSector)
	{
		plane = sector->floorplane;
	}
	else
	{
		plane = controlSector->ceilingplane;
		plane.FlipVert();
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
		verts[j].Z = (float)plane.ZatPoint(verts[j]);
	}

	surf.Type = ST_FLOOR;
	surf.typeIndex = typeIndex;
	surf.sampleDimension = (controlSector ? controlSector : sector)->planes[sector_t::floor].LightmapSampleDistance;
	surf.ControlSector = controlSector;
	surf.plane = FVector4((float)plane.Normal().X, (float)plane.Normal().Y, (float)plane.Normal().Z, -(float)plane.D);

	Surfaces.Push(surf);
}

void DoomLevelMesh::CreateCeilingSurface(FLevelLocals& doomMap, subsector_t* sub, sector_t* sector, sector_t* controlSector, int typeIndex)
{
	DoomLevelMeshSurface surf;
	surf.bSky = IsSkySector(sector, sector_t::ceiling);

	secplane_t plane;
	if (!controlSector)
	{
		plane = sector->ceilingplane;
	}
	else
	{
		plane = controlSector->floorplane;
		plane.FlipVert();
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
		verts[j].Z = (float)plane.ZatPoint(verts[j]);
	}

	surf.Type = ST_CEILING;
	surf.typeIndex = typeIndex;
	surf.sampleDimension = (controlSector ? controlSector : sector)->planes[sector_t::ceiling].LightmapSampleDistance;
	surf.ControlSector = controlSector;
	surf.plane = FVector4((float)plane.Normal().X, (float)plane.Normal().Y, (float)plane.Normal().Z, -(float)plane.D);

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

		CreateFloorSurface(doomMap, sub, sector, nullptr, i);
		CreateCeilingSurface(doomMap, sub, sector, nullptr, i);

		for (unsigned int j = 0; j < sector->e->XFloor.ffloors.Size(); j++)
		{
			CreateFloorSurface(doomMap, sub, sector, sector->e->XFloor.ffloors[j]->model, i);
			CreateCeilingSurface(doomMap, sub, sector, sector->e->XFloor.ffloors[j]->model, i);
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

void DoomLevelMesh::DumpMesh(const FString& objFilename, const FString& mtlFilename) const
{
	auto f = fopen(objFilename.GetChars(), "w");

	fprintf(f, "# DoomLevelMesh debug export\n");
	fprintf(f, "# MeshVertices: %u, MeshElements: %u, Surfaces: %u\n", MeshVertices.Size(), MeshElements.Size(), Surfaces.Size());
	fprintf(f, "mtllib %s\n", mtlFilename.GetChars());

	double scale = 1 / 10.0;

	for (const auto& v : MeshVertices)
	{
		fprintf(f, "v %f %f %f\n", v.X * scale, v.Y * scale, v.Z * scale);
	}

	{
		for (const auto& uv : LightmapUvs)
		{
			fprintf(f, "vt %f %f\n", uv.X, uv.Y);
		}
	}

	auto name = [](LevelMeshSurfaceType type) -> const char* {
		switch (type)
		{
		case ST_CEILING:
			return "ceiling";
		case ST_FLOOR:
			return "floor";
		case ST_LOWERSIDE:
			return "lowerside";
		case ST_UPPERSIDE:
			return "upperside";
		case ST_MIDDLESIDE:
			return "middleside";
		case ST_UNKNOWN:
			return "unknown";
		default:
			break;
		}
		return "error";
	};


	uint32_t lastSurfaceIndex = -1;


	bool useErrorMaterial = false;
	int highestUsedAtlasPage = -1;

	for (unsigned i = 0, count = MeshElements.Size(); i + 2 < count; i += 3)
	{
		auto index = MeshSurfaceIndexes[i / 3];

		if(index != lastSurfaceIndex)
		{
			lastSurfaceIndex = index;

			if (unsigned(index) >= Surfaces.Size())
			{
				fprintf(f, "o Surface[%d] (bad index)\n", index);
				fprintf(f, "usemtl error\n");

				useErrorMaterial = true;
			}
			else
			{
				const auto& surface = Surfaces[index];
				fprintf(f, "o Surface[%d] %s %d%s\n", index, name(surface.Type), surface.typeIndex, surface.bSky ? " sky" : "");
				fprintf(f, "usemtl lightmap%d\n", surface.atlasPageIndex);

				if (surface.atlasPageIndex > highestUsedAtlasPage)
				{
					highestUsedAtlasPage = surface.atlasPageIndex;
				}
			}
		}

		// fprintf(f, "f %d %d %d\n", MeshElements[i] + 1, MeshElements[i + 1] + 1, MeshElements[i + 2] + 1);
		fprintf(f, "f %d/%d %d/%d %d/%d\n",
			MeshElements[i + 0] + 1, MeshElements[i + 0] + 1,
			MeshElements[i + 1] + 1, MeshElements[i + 1] + 1,
			MeshElements[i + 2] + 1, MeshElements[i + 2] + 1);

	}

	fclose(f);

	// material

	f = fopen(mtlFilename.GetChars(), "w");

	fprintf(f, "# DoomLevelMesh debug export\n");

	if (useErrorMaterial)
	{
		fprintf(f, "# Surface indices that are referenced, but do not exists in the 'Surface' array\n");
		fprintf(f, "newmtl error\nKa 1 0 0\nKd 1 0 0\nKs 1 0 0\n");
	}

	for (int page = 0; page <= highestUsedAtlasPage; ++page)
	{
		fprintf(f, "newmtl lightmap%d\n", page);
		fprintf(f, "Ka 1 1 1\nKd 1 1 1\nKs 0 0 0\n");
		fprintf(f, "map_Ka lightmap%d.png\n", page);
		fprintf(f, "map_Kd lightmap%d.png\n", page);
	}

	fclose(f);
}

void DoomLevelMesh::SetupLightmapUvs()
{
	LMTextureSize = 1024; // TODO cvar

	std::vector<LevelMeshSurface*> sortedSurfaces;
	sortedSurfaces.reserve(Surfaces.Size());

	for (auto& surface : Surfaces)
	{
		BuildSurfaceParams(LMTextureSize, LMTextureSize, surface);
		sortedSurfaces.push_back(&surface);
	}

	// VkLightmapper old ZDRay properties
	for (auto& surface : Surfaces)
	{
		for (int i = 0; i < surface.numVerts; ++i)
		{
			surface.verts.Push(MeshVertices[surface.startVertIndex + i]);
		}

		for (int i = 0; i < surface.numVerts; ++i)
		{
			surface.uvs.Push(LightmapUvs[surface.startUvIndex + i]);
		}
	}

	BuildSmoothingGroups();

	std::sort(sortedSurfaces.begin(), sortedSurfaces.end(), [](LevelMeshSurface* a, LevelMeshSurface* b) { return a->texHeight != b->texHeight ? a->texHeight > b->texHeight : a->texWidth > b->texWidth; });

	RectPacker packer(LMTextureSize, LMTextureSize, RectPacker::Spacing(0));

	for (LevelMeshSurface* surf : sortedSurfaces)
	{
		FinishSurface(LMTextureSize, LMTextureSize, packer, *surf);
	}

	// You have no idea how long this took me to figure out...

	// Reorder vertices into renderer format
	for (LevelMeshSurface& surface : Surfaces)
	{
		if (surface.Type == ST_FLOOR)
		{
			// reverse vertices on floor
			for (int j = surface.startUvIndex + surface.numVerts - 1, k = surface.startUvIndex; j > k; j--, k++)
			{
				std::swap(LightmapUvs[k], LightmapUvs[j]);
			}
		}
		else if (surface.Type != ST_CEILING) // walls
		{
			// from 0 1 2 3
			// to   0 2 1 3
			std::swap(LightmapUvs[surface.startUvIndex + 1], LightmapUvs[surface.startUvIndex + 2]);
			std::swap(LightmapUvs[surface.startUvIndex + 2], LightmapUvs[surface.startUvIndex + 3]);
		}
	}

	LMTextureCount = (int)packer.getNumPages();
}

void DoomLevelMesh::FinishSurface(int lightmapTextureWidth, int lightmapTextureHeight, RectPacker& packer, LevelMeshSurface& surface)
{
	int sampleWidth = surface.texWidth;
	int sampleHeight = surface.texHeight;

	auto result = packer.insert(sampleWidth, sampleHeight);
	int x = result.pos.x, y = result.pos.y;
	surface.atlasPageIndex = (int)result.pageIndex;

	// calculate final texture coordinates
	for (int i = 0; i < (int)surface.numVerts; i++)
	{
		auto& u = LightmapUvs[surface.startUvIndex + i].X;
		auto& v = LightmapUvs[surface.startUvIndex + i].Y;
		u = (u + x) / (float)lightmapTextureWidth;
		v = (v + y) / (float)lightmapTextureHeight;
	}
	
	surface.atlasX = x;
	surface.atlasY = y;
}

BBox DoomLevelMesh::GetBoundsFromSurface(const LevelMeshSurface& surface) const
{
	constexpr float M_INFINITY = 1e30f; // TODO cleanup

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

DoomLevelMesh::PlaneAxis DoomLevelMesh::BestAxis(const FVector4& p)
{
	float na = fabs(float(p.X));
	float nb = fabs(float(p.Y));
	float nc = fabs(float(p.Z));

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

void DoomLevelMesh::BuildSurfaceParams(int lightMapTextureWidth, int lightMapTextureHeight, LevelMeshSurface& surface)
{
	BBox bounds;
	FVector3 roundedSize;
	FVector3 tOrigin;
	int width;
	int height;
	float d;

	const FVector4& plane = surface.plane;
	bounds = GetBoundsFromSurface(surface);
	surface.bounds = bounds;

	if (surface.sampleDimension <= 0)
	{
		surface.sampleDimension = LightmapSampleDistance;
	}

	surface.sampleDimension = uint16_t(max(int(roundf(float(surface.sampleDimension) / max(1.0f / 4, float(lm_scale)))), 1));

	{
		// Round to nearest power of two
		uint32_t n = uint16_t(surface.sampleDimension);
		n |= n >> 1;
		n |= n >> 2;
		n |= n >> 4;
		n |= n >> 8;
		n = (n + 1) >> 1;
		surface.sampleDimension = uint16_t(n) ? uint16_t(n) : uint16_t(0xFFFF);
	}

	// round off dimensions
	for (int i = 0; i < 3; i++)
	{
		bounds.min[i] = surface.sampleDimension * (floor(bounds.min[i] / surface.sampleDimension) - 1);
		bounds.max[i] = surface.sampleDimension * (ceil(bounds.max[i] / surface.sampleDimension) + 1);

		roundedSize[i] = (bounds.max[i] - bounds.min[i]) / surface.sampleDimension;
	}

	FVector3 tCoords[2] = { FVector3(0.0f, 0.0f, 0.0f), FVector3(0.0f, 0.0f, 0.0f) };

	PlaneAxis axis = BestAxis(plane);

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

	for (int i = 0; i < surface.numVerts; i++)
	{
		FVector3 tDelta = MeshVertices[surface.startVertIndex + i] - surface.translateWorldToLocal;

		LightmapUvs[surface.startUvIndex + i].X = (tDelta | surface.projLocalToU);
		LightmapUvs[surface.startUvIndex + i].Y = (tDelta | surface.projLocalToV);
	}


	tOrigin = bounds.min;

	// project tOrigin and tCoords so they lie on the plane
	d = ((bounds.min | FVector3(plane.X, plane.Y, plane.Z)) - plane.W) / plane[axis]; //d = (plane->PointToDist(bounds.min)) / plane->Normal()[axis];
	tOrigin[axis] -= d;

	for (int i = 0; i < 2; i++)
	{
		tCoords[i].MakeUnit();
		d = (tCoords[i] | FVector3(plane.X, plane.Y, plane.Z)) / plane[axis]; //d = dot(tCoords[i], plane->Normal()) / plane->Normal()[axis];
		tCoords[i][axis] -= d;
	}

	surface.texWidth = width;
	surface.texHeight = height;
	surface.worldOrigin = tOrigin;
	surface.worldStepX = tCoords[0] * (float)surface.sampleDimension;
	surface.worldStepY = tCoords[1] * (float)surface.sampleDimension;
}
