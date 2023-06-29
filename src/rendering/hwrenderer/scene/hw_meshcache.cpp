
#include "hw_meshcache.h"
#include "hw_drawinfo.h"
#include "hw_drawstructs.h"
#include "hw_mesh.h"
#include "hw_fakeflat.h"
#include "hw_vertexbuilder.h"
#include "g_levellocals.h"
#include <unordered_set>

EXTERN_CVAR(Bool, gl_texture)
EXTERN_CVAR(Float, gl_mask_threshold)
EXTERN_CVAR(Bool, gl_meshcache)

HWMeshCache meshcache;

void HWMeshCache::Clear()
{
#if 0
	Sectors.Reset();
#endif
	Opaque.reset();
	Translucent.reset();
	TranslucentDepthBiased.reset();
}

void HWMeshCache::Update(HWDrawContext* drawctx, FRenderViewpoint& vp)
{
	if (!gl_meshcache)
		return;

	auto level = vp.ViewLevel;

#if 0

	unsigned int count = level->sectors.Size();
	Sectors.Resize(count);

	// Look for changes
	for (unsigned int i = 0; i < count; i++)
	{
		auto sector = &level->sectors[i];
		auto cacheitem = &Sectors[i];
		if (cacheitem->Floorplane != sector->floorplane || cacheitem->Ceilingplane != sector->ceilingplane) // Sector height changes
		{
			cacheitem->NeedsUpdate = true;
			for (line_t* line : sector->Lines)
			{
				sector_t* backsector = (line->frontsector == sector) ? line->backsector : line->frontsector;
				if (backsector)
				{
					Sectors[backsector->Index()].NeedsUpdate = true;
				}
			}
		}
	}

	// Update changed sectors
	for (unsigned int i = 0; i < count; i++)
	{
		auto sector = &level->sectors[i];
		auto cacheitem = &Sectors[i];
		if (cacheitem->NeedsUpdate)
		{
			cacheitem->NeedsUpdate = false;
			cacheitem->Sector = sector;
			cacheitem->Floorplane = sector->floorplane;
			cacheitem->Ceilingplane = sector->ceilingplane;
			cacheitem->Update(vp);
		}
	}

#endif

	if (!Opaque)
	{
		HWDrawInfo* di = HWDrawInfo::StartDrawInfo(drawctx, vp.ViewLevel, nullptr, vp, nullptr);
		di->MeshBuilding = true;

		MeshBuilder state;
		state.SetShadowData(sector_vertices, sector_indexes);

		// Add to the draw lists

		unsigned int count = level->sectors.Size();
		for (unsigned int i = 0; i < count; i++)
		{
			auto sector = &level->sectors[i];

			// CheckUpdate(state, sector);
			std::unordered_set<FSection*> seenSections;
			for (int i = 0, count = sector->subsectorcount; i < count; i++)
			{
				subsector_t* subsector = sector->subsectors[i];
				if (seenSections.find(subsector->section) == seenSections.end())
				{
					seenSections.insert(subsector->section);

					HWFlat flat;
					flat.section = subsector->section;
					sector_t* front = hw_FakeFlat(di->drawctx, subsector->render_sector, area_default, false);
					flat.ProcessSector(di, state, front);
				}
			}

			for (line_t* line : sector->Lines)
			{
				side_t* side = (line->sidedef[0]->sector == sector) ? line->sidedef[0] : line->sidedef[1];

				HWWall wall;
				wall.sub = sector->subsectors[0];
				wall.Process(di, state, side->segs[0], sector, (line->sidedef[0]->sector == sector) ? line->backsector : line->frontsector);
			}
		}

		// Convert draw lists to meshes

		state.SetDepthMask(true);
		state.EnableFog(true);
		state.SetRenderStyle(STYLE_Source);

		di->drawlists[GLDL_PLAINWALLS].SortWalls();
		di->drawlists[GLDL_PLAINFLATS].SortFlats();
		di->drawlists[GLDL_MASKEDWALLS].SortWalls();
		di->drawlists[GLDL_MASKEDFLATS].SortFlats();
		di->drawlists[GLDL_MASKEDWALLSOFS].SortWalls();

		// Part 1: solid geometry. This is set up so that there are no transparent parts
		state.SetDepthFunc(DF_Less);
		state.AlphaFunc(Alpha_GEqual, 0.f);
		state.ClearDepthBias();
		state.EnableTexture(gl_texture);
		state.EnableBrightmap(true);
		di->drawlists[GLDL_PLAINWALLS].DrawWalls(di, state, false);
		di->drawlists[GLDL_PLAINFLATS].DrawFlats(di, state, false);
		Opaque = state.Create();

		// Part 2: masked geometry. This is set up so that only pixels with alpha>gl_mask_threshold will show
		state.AlphaFunc(Alpha_GEqual, gl_mask_threshold);
		di->drawlists[GLDL_MASKEDWALLS].DrawWalls(di, state, false);
		di->drawlists[GLDL_MASKEDFLATS].DrawFlats(di, state, false);
		Translucent = state.Create();

		// Part 3: masked geometry with polygon offset. This list is empty most of the time so only waste time on it when in use.
		if (di->drawlists[GLDL_MASKEDWALLSOFS].Size() > 0)
		{
			state.SetDepthBias(-1, -128);
			di->drawlists[GLDL_MASKEDWALLSOFS].DrawWalls(di, state, false);
			state.ClearDepthBias();
			TranslucentDepthBiased = state.Create();
		}
		else
		{
			TranslucentDepthBiased.reset();
		}

		di->MeshBuilding = false;
		di->EndDrawInfo();
	}
}
