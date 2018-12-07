// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2000-2016 Christoph Oelckers
// All rights reserved.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//--------------------------------------------------------------------------
//
/*
** gl_renderhacks.cpp
** Handles missing upper and lower textures and self referencing sector hacks
**
*/

#include "a_sharedglobal.h"
#include "r_utility.h"
#include "r_sky.h"
#include "g_levellocals.h"
#include "a_dynlight.h"

#include "hw_drawinfo.h"
#include "hw_drawstructs.h"
#include "hwrenderer/utility/hw_clock.h"
#include "hwrenderer/dynlights/hw_dynlightdata.h"
#include "hwrenderer/data/flatvertices.h"
#include "hwrenderer/dynlights/hw_lightbuffer.h"
#include "hwrenderer/scene/hw_portal.h"
#include "hw_fakeflat.h"

//==========================================================================
//
// Create render list entries from the data generated below
//
//==========================================================================

void HWDrawInfo::DispatchRenderHacks()
{
	TMap<int, gl_subsectorrendernode*>::Pair *pair;
	TMap<int, gl_floodrendernode*>::Pair *fpair;
	TMap<int, gl_subsectorrendernode*>::Iterator ofi(otherFloorPlanes);
	GLFlat glflat;
	glflat.section = nullptr;
	while (ofi.NextPair(pair))
	{
		auto sec = hw_FakeFlat(&level.sectors[pair->Key], in_area, false);
		glflat.ProcessSector(this, sec, SSRF_RENDERFLOOR | SSRF_PLANEHACK);
	}

	TMap<int, gl_subsectorrendernode*>::Iterator oci(otherCeilingPlanes);
	while (oci.NextPair(pair))
	{
		auto sec = hw_FakeFlat(&level.sectors[pair->Key], in_area, false);
		glflat.ProcessSector(this, sec, SSRF_RENDERCEILING | SSRF_PLANEHACK);
	}

	TMap<int, gl_floodrendernode*>::Iterator ffi(floodFloorSegs);
	while (ffi.NextPair(fpair))
	{
		auto sec = hw_FakeFlat(&level.sectors[fpair->Key], in_area, false);
		glflat.ProcessSector(this, sec, SSRF_RENDERFLOOR | SSRF_FLOODHACK);
	}

	TMap<int, gl_floodrendernode*>::Iterator fci(floodCeilingSegs);
	while (fci.NextPair(fpair))
	{
		auto sec = hw_FakeFlat(&level.sectors[fpair->Key], in_area, false);
		glflat.ProcessSector(this, sec, SSRF_RENDERCEILING | SSRF_FLOODHACK);
	}
}


//==========================================================================
//
// Get the nodes from the render data allocator so we don't have to keep track of them ourselves.
//
//==========================================================================

static gl_subsectorrendernode *NewSubsectorRenderNode()
{
    return (gl_subsectorrendernode*)RenderDataAllocator.Alloc(sizeof(gl_subsectorrendernode));
}

static gl_floodrendernode *NewFloodRenderNode()
{
    return (gl_floodrendernode*)RenderDataAllocator.Alloc(sizeof(gl_floodrendernode));
}

//==========================================================================
//
// light setup for render hacks.
// This can ignore many of the special checks because
// this will never be translucent and never be part of a portal or a 3D floor
//
//==========================================================================

int HWDrawInfo::SetupLightsForOtherPlane(subsector_t * sub, FDynLightData &lightdata, const secplane_t *plane)
{
	if (level.HasDynamicLights && !isFullbrightScene())
	{
		Plane p;

		FLightNode * node = sub->section->lighthead;

		lightdata.Clear();
		while (node)
		{
			ADynamicLight * light = node->lightsource;

			if (light->flags2&MF2_DORMANT)
			{
				node = node->nextLight;
				continue;
			}
			iter_dlightf++;

			p.Set(plane->Normal(), plane->fD());
			draw_dlightf += lightdata.GetLight(sub->sector->PortalGroup, p, light, true);
			node = node->nextLight;
		}

		return screen->mLights->UploadLights(lightdata);
	}
	else return -1;
}

//==========================================================================
//
// Vertex setup for extra planes
//
//==========================================================================

int HWDrawInfo::CreateOtherPlaneVertices(subsector_t *sub, const secplane_t *plane)
{
	auto alloc = screen->mVertexData->AllocVertices(sub->numlines);
	auto ptr = alloc.first;
	for (unsigned int k = 0; k < sub->numlines; k++)
	{
		vertex_t *vt = sub->firstline[k].v1;
		ptr->x = vt->fX();
		ptr->z = plane->ZatPoint(vt);
		ptr->y = vt->fY();
		ptr->u = vt->fX() / 64.f;
		ptr->v = -vt->fY() / 64.f;
		ptr++;
	}
	return alloc.second;
}

//==========================================================================
//
// Adds a subsector plane to a sector's render list
//
//==========================================================================

void HWDrawInfo::AddOtherFloorPlane(int sector, gl_subsectorrendernode * node)
{
    auto pNode = otherFloorPlanes.CheckKey(sector);
    
	node->next = pNode? *pNode : nullptr;
	node->lightindex = SetupLightsForOtherPlane(node->sub, lightdata, &level.sectors[sector].floorplane);
	node->vertexindex = CreateOtherPlaneVertices(node->sub, &level.sectors[sector].floorplane);
	otherFloorPlanes[sector] = node;
}

void HWDrawInfo::AddOtherCeilingPlane(int sector, gl_subsectorrendernode * node)
{
    auto pNode = otherCeilingPlanes.CheckKey(sector);
    
    node->next = pNode? *pNode : nullptr;
	node->lightindex = SetupLightsForOtherPlane(node->sub, lightdata, &level.sectors[sector].ceilingplane);
	node->vertexindex = CreateOtherPlaneVertices(node->sub, &level.sectors[sector].ceilingplane);
	otherCeilingPlanes[sector] = node;
}

//==========================================================================
//
// Collects all sectors that might need a fake ceiling
//
//==========================================================================
void HWDrawInfo::AddUpperMissingTexture(side_t * side, subsector_t *sub, float Backheight)
{
	if (!side->segs[0]->backsector) return;

	for (int i = 0; i < side->numsegs; i++)
	{
		seg_t *seg = side->segs[i];

		// we need find the seg belonging to the passed subsector
		if (seg->Subsector == sub)
		{
			MissingTextureInfo mti = {};
			MissingSegInfo msi;


			if (sub->render_sector != sub->sector || seg->frontsector != sub->sector)
			{
				return;
			}

			for (unsigned int i = 0; i < MissingUpperTextures.Size(); i++)
			{
				if (MissingUpperTextures[i].sub == sub)
				{
					// Use the lowest adjoining height to draw a fake ceiling if necessary
					if (Backheight < MissingUpperTextures[i].Planez)
					{
						MissingUpperTextures[i].Planez = Backheight;
						MissingUpperTextures[i].seg = seg;
					}

					msi.MTI_Index = i;
					msi.seg = seg;
					MissingUpperSegs.Push(msi);
					return;
				}
			}
			mti.seg = seg;
			mti.sub = sub;
			mti.Planez = Backheight;
			msi.MTI_Index = MissingUpperTextures.Push(mti);
			msi.seg = seg;
			MissingUpperSegs.Push(msi);
		}
	}
}

//==========================================================================
//
// Collects all sectors that might need a fake floor
//
//==========================================================================
void HWDrawInfo::AddLowerMissingTexture(side_t * side, subsector_t *sub, float Backheight)
{
	sector_t *backsec = side->segs[0]->backsector;
	if (!backsec) return;
	if (backsec->transdoor)
	{
		// Transparent door hacks alter the backsector's floor height so we should not
		// process the missing texture for them.
		if (backsec->transdoorheight == backsec->GetPlaneTexZ(sector_t::floor)) return;
	}

	// we need to check all segs of this sidedef
	for (int i = 0; i < side->numsegs; i++)
	{
		seg_t *seg = side->segs[i];

		// we need find the seg belonging to the passed subsector
		if (seg->Subsector == sub)
		{
			MissingTextureInfo mti = {};
			MissingSegInfo msi;

			subsector_t * sub = seg->Subsector;

			if (sub->render_sector != sub->sector || seg->frontsector != sub->sector)
			{
				return;
			}

			// Ignore FF_FIX's because they are designed to abuse missing textures
			if (seg->backsector->e->XFloor.ffloors.Size() && (seg->backsector->e->XFloor.ffloors[0]->flags&(FF_FIX | FF_SEETHROUGH)) == FF_FIX)
			{
				return;
			}

			for (unsigned int i = 0; i < MissingLowerTextures.Size(); i++)
			{
				if (MissingLowerTextures[i].sub == sub)
				{
					// Use the highest adjoining height to draw a fake floor if necessary
					if (Backheight > MissingLowerTextures[i].Planez)
					{
						MissingLowerTextures[i].Planez = Backheight;
						MissingLowerTextures[i].seg = seg;
					}

					msi.MTI_Index = i;
					msi.seg = seg;
					MissingLowerSegs.Push(msi);
					return;
				}
			}
			mti.seg = seg;
			mti.sub = sub;
			mti.Planez = Backheight;
			msi.MTI_Index = MissingLowerTextures.Push(mti);
			msi.seg = seg;
			MissingLowerSegs.Push(msi);
		}
	}
}


//==========================================================================
//
// 
//
//==========================================================================
bool HWDrawInfo::DoOneSectorUpper(subsector_t * subsec, float Planez, area_t in_area)
{
	// Is there a one-sided wall in this sector?
	// Do this first to avoid unnecessary recursion
	for (uint32_t i = 0; i < subsec->numlines; i++)
	{
		if (subsec->firstline[i].backsector == NULL) return false;
		if (subsec->firstline[i].PartnerSeg == NULL) return false;
	}

	for (uint32_t i = 0; i < subsec->numlines; i++)
	{
		seg_t * seg = subsec->firstline + i;
		subsector_t * backsub = seg->PartnerSeg->Subsector;

		// already checked?
		if (backsub->validcount == validcount) continue;
		backsub->validcount = validcount;

		if (seg->frontsector != seg->backsector && seg->linedef)
		{
			// Note: if this is a real line between sectors
			// we can be sure that render_sector is the real sector!

			sector_t * sec = hw_FakeFlat(seg->backsector, in_area, true);

			// Don't bother with slopes
			if (sec->ceilingplane.isSlope())  return false;

			// Is the neighboring ceiling lower than the desired height?
			if (sec->GetPlaneTexZ(sector_t::ceiling) < Planez)
			{
				// todo: check for missing textures.
				return false;
			}

			// This is an exact height match which means we don't have to do any further checks for this sector
			if (sec->GetPlaneTexZ(sector_t::ceiling) == Planez)
			{
				// If there's a texture abort
				FTexture * tex = TexMan.GetTexture(seg->sidedef->GetTexture(side_t::top));
				if (!tex || !tex->isValid()) continue;
				else return false;
			}
		}
		if (!DoOneSectorUpper(backsub, Planez, in_area)) return false;
	}
	// all checked ok. This subsector is part of the current fake plane

	HandledSubsectors.Push(subsec);
	return 1;
}

//==========================================================================
//
// 
//
//==========================================================================
bool HWDrawInfo::DoOneSectorLower(subsector_t * subsec, float Planez, area_t in_area)
{
	// Is there a one-sided wall in this subsector?
	// Do this first to avoid unnecessary recursion
	for (uint32_t i = 0; i < subsec->numlines; i++)
	{
		if (subsec->firstline[i].backsector == NULL) return false;
		if (subsec->firstline[i].PartnerSeg == NULL) return false;
	}

	for (uint32_t i = 0; i < subsec->numlines; i++)
	{
		seg_t * seg = subsec->firstline + i;
		subsector_t * backsub = seg->PartnerSeg->Subsector;

		// already checked?
		if (backsub->validcount == validcount) continue;
		backsub->validcount = validcount;

		if (seg->frontsector != seg->backsector && seg->linedef)
		{
			// Note: if this is a real line between sectors
			// we can be sure that render_sector is the real sector!

			sector_t * sec = hw_FakeFlat(seg->backsector, in_area, true);

			// Don't bother with slopes
			if (sec->floorplane.isSlope())  return false;

			// Is the neighboring floor higher than the desired height?
			if (sec->GetPlaneTexZ(sector_t::floor) > Planez)
			{
				// todo: check for missing textures.
				return false;
			}

			// This is an exact height match which means we don't have to do any further checks for this sector
			if (sec->GetPlaneTexZ(sector_t::floor) == Planez)
			{
				// If there's a texture abort
				FTexture * tex = TexMan.GetTexture(seg->sidedef->GetTexture(side_t::bottom));
				if (!tex || !tex->isValid()) continue;
				else return false;
			}
		}
		if (!DoOneSectorLower(backsub, Planez, in_area)) return false;
	}
	// all checked ok. This sector is part of the current fake plane

	HandledSubsectors.Push(subsec);
	return 1;
}


//==========================================================================
//
//
//
//==========================================================================
bool HWDrawInfo::DoFakeBridge(subsector_t * subsec, float Planez, area_t in_area)
{
	// Is there a one-sided wall in this sector?
	// Do this first to avoid unnecessary recursion
	for (uint32_t i = 0; i < subsec->numlines; i++)
	{
		if (subsec->firstline[i].backsector == NULL) return false;
		if (subsec->firstline[i].PartnerSeg == NULL) return false;
	}

	for (uint32_t i = 0; i < subsec->numlines; i++)
	{
		seg_t * seg = subsec->firstline + i;
		subsector_t * backsub = seg->PartnerSeg->Subsector;

		// already checked?
		if (backsub->validcount == validcount) continue;
		backsub->validcount = validcount;

		if (seg->frontsector != seg->backsector && seg->linedef)
		{
			// Note: if this is a real line between sectors
			// we can be sure that render_sector is the real sector!

			sector_t * sec = hw_FakeFlat(seg->backsector, in_area, true);

			// Don't bother with slopes
			if (sec->floorplane.isSlope())  return false;

			// Is the neighboring floor higher than the desired height?
			if (sec->GetPlaneTexZ(sector_t::floor) < Planez)
			{
				// todo: check for missing textures.
				return false;
			}

			// This is an exact height match which means we don't have to do any further checks for this sector
			// No texture checks though! 
			if (sec->GetPlaneTexZ(sector_t::floor) == Planez) continue;
		}
		if (!DoFakeBridge(backsub, Planez, in_area)) return false;
	}
	// all checked ok. This sector is part of the current fake plane

	HandledSubsectors.Push(subsec);
	return 1;
}

//==========================================================================
//
//
//
//==========================================================================
bool HWDrawInfo::DoFakeCeilingBridge(subsector_t * subsec, float Planez, area_t in_area)
{
	// Is there a one-sided wall in this sector?
	// Do this first to avoid unnecessary recursion
	for (uint32_t i = 0; i < subsec->numlines; i++)
	{
		if (subsec->firstline[i].backsector == NULL) return false;
		if (subsec->firstline[i].PartnerSeg == NULL) return false;
	}

	for (uint32_t i = 0; i < subsec->numlines; i++)
	{
		seg_t * seg = subsec->firstline + i;
		subsector_t * backsub = seg->PartnerSeg->Subsector;

		// already checked?
		if (backsub->validcount == validcount) continue;
		backsub->validcount = validcount;

		if (seg->frontsector != seg->backsector && seg->linedef)
		{
			// Note: if this is a real line between sectors
			// we can be sure that render_sector is the real sector!

			sector_t * sec = hw_FakeFlat(seg->backsector, in_area, true);

			// Don't bother with slopes
			if (sec->ceilingplane.isSlope())  return false;

			// Is the neighboring ceiling higher than the desired height?
			if (sec->GetPlaneTexZ(sector_t::ceiling) > Planez)
			{
				// todo: check for missing textures.
				return false;
			}

			// This is an exact height match which means we don't have to do any further checks for this sector
			// No texture checks though! 
			if (sec->GetPlaneTexZ(sector_t::ceiling) == Planez) continue;
		}
		if (!DoFakeCeilingBridge(backsub, Planez, in_area)) return false;
	}
	// all checked ok. This sector is part of the current fake plane

	HandledSubsectors.Push(subsec);
	return 1;
}


//==========================================================================
//
// Draws the fake planes
//
//==========================================================================
void HWDrawInfo::HandleMissingTextures(area_t in_area)
{
	for (unsigned int i = 0; i < MissingUpperTextures.Size(); i++)
	{
		if (!MissingUpperTextures[i].seg) continue;
		HandledSubsectors.Clear();
		validcount++;

		if (MissingUpperTextures[i].Planez > Viewpoint.Pos.Z)
		{
			// close the hole only if all neighboring sectors are an exact height match
			// Otherwise just fill in the missing textures.
			MissingUpperTextures[i].sub->validcount = validcount;
			if (DoOneSectorUpper(MissingUpperTextures[i].sub, MissingUpperTextures[i].Planez, in_area))
			{
				sector_t * sec = MissingUpperTextures[i].seg->backsector;
				for (unsigned int j = 0; j < HandledSubsectors.Size(); j++)
				{
                    gl_subsectorrendernode * node = NewSubsectorRenderNode();
					node->sub = HandledSubsectors[j];

					AddOtherCeilingPlane(sec->sectornum, node);
				}

				if (HandledSubsectors.Size() != 1)
				{
					// mark all subsectors in the missing list that got processed by this
					for (unsigned int j = 0; j < HandledSubsectors.Size(); j++)
					{
						for (unsigned int k = 0; k < MissingUpperTextures.Size(); k++)
						{
							if (MissingUpperTextures[k].sub == HandledSubsectors[j])
							{
								MissingUpperTextures[k].seg = NULL;
							}
						}
					}
				}
				else MissingUpperTextures[i].seg = NULL;
				continue;
			}
		}

		if (!MissingUpperTextures[i].seg->PartnerSeg) continue;
		subsector_t *backsub = MissingUpperTextures[i].seg->PartnerSeg->Subsector;
		if (!backsub) continue;
		validcount++;
		HandledSubsectors.Clear();

		{
			// It isn't a hole. Now check whether it might be a fake bridge
			sector_t * fakesector = hw_FakeFlat(MissingUpperTextures[i].seg->frontsector, in_area, false);
			float planez = (float)fakesector->GetPlaneTexZ(sector_t::ceiling);

			backsub->validcount = validcount;
			if (DoFakeCeilingBridge(backsub, planez, in_area))
			{
				for (unsigned int j = 0; j < HandledSubsectors.Size(); j++)
				{
                    gl_subsectorrendernode * node = NewSubsectorRenderNode();
					node->sub = HandledSubsectors[j];
					AddOtherCeilingPlane(fakesector->sectornum, node);
				}
			}
			continue;
		}
	}

	for (unsigned int i = 0; i < MissingLowerTextures.Size(); i++)
	{
		if (!MissingLowerTextures[i].seg) continue;
		HandledSubsectors.Clear();
		validcount++;

		if (MissingLowerTextures[i].Planez < Viewpoint.Pos.Z)
		{
			// close the hole only if all neighboring sectors are an exact height match
			// Otherwise just fill in the missing textures.
			MissingLowerTextures[i].sub->validcount = validcount;
			if (DoOneSectorLower(MissingLowerTextures[i].sub, MissingLowerTextures[i].Planez, in_area))
			{
				sector_t * sec = MissingLowerTextures[i].seg->backsector;

				for (unsigned int j = 0; j < HandledSubsectors.Size(); j++)
				{
                    gl_subsectorrendernode * node = NewSubsectorRenderNode();
					node->sub = HandledSubsectors[j];
					AddOtherFloorPlane(sec->sectornum, node);
				}

				if (HandledSubsectors.Size() != 1)
				{
					// mark all subsectors in the missing list that got processed by this
					for (unsigned int j = 0; j < HandledSubsectors.Size(); j++)
					{
						for (unsigned int k = 0; k < MissingLowerTextures.Size(); k++)
						{
							if (MissingLowerTextures[k].sub == HandledSubsectors[j])
							{
								MissingLowerTextures[k].seg = NULL;
							}
						}
					}
				}
				else MissingLowerTextures[i].seg = NULL;
				continue;
			}
		}

		if (!MissingLowerTextures[i].seg->PartnerSeg) continue;
		subsector_t *backsub = MissingLowerTextures[i].seg->PartnerSeg->Subsector;
		if (!backsub) continue;
		validcount++;
		HandledSubsectors.Clear();

		{
			// It isn't a hole. Now check whether it might be a fake bridge
			sector_t * fakesector = hw_FakeFlat(MissingLowerTextures[i].seg->frontsector, in_area, false);
			float planez = (float)fakesector->GetPlaneTexZ(sector_t::floor);

			backsub->validcount = validcount;
			if (DoFakeBridge(backsub, planez, in_area))
			{
				for (unsigned int j = 0; j < HandledSubsectors.Size(); j++)
				{
                    gl_subsectorrendernode * node = NewSubsectorRenderNode();
					node->sub = HandledSubsectors[j];
					AddOtherFloorPlane(fakesector->sectornum, node);
				}
			}
			continue;
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void HWDrawInfo::CreateFloodStencilPoly(wallseg * ws, FFlatVertex *vertices)
{
	vertices[0].Set(ws->x1, ws->z1, ws->y1, 0, 0);
	vertices[1].Set(ws->x1, ws->z2, ws->y1, 0, 0);
	vertices[2].Set(ws->x2, ws->z2, ws->y2, 0, 0);
	vertices[3].Set(ws->x2, ws->z1, ws->y2, 0, 0);
}

//==========================================================================
//
// 
//
//==========================================================================

void HWDrawInfo::CreateFloodPoly(wallseg * ws, FFlatVertex *vertices, float planez, sector_t * sec, bool ceiling)
{
	float fviewx = Viewpoint.Pos.X;
	float fviewy = Viewpoint.Pos.Y;
	float fviewz = Viewpoint.Pos.Z;

	float prj_fac1 = (planez - fviewz) / (ws->z1 - fviewz);
	float prj_fac2 = (planez - fviewz) / (ws->z2 - fviewz);

	float px1 = fviewx + prj_fac1 * (ws->x1 - fviewx);
	float py1 = fviewy + prj_fac1 * (ws->y1 - fviewy);

	float px2 = fviewx + prj_fac2 * (ws->x1 - fviewx);
	float py2 = fviewy + prj_fac2 * (ws->y1 - fviewy);

	float px3 = fviewx + prj_fac2 * (ws->x2 - fviewx);
	float py3 = fviewy + prj_fac2 * (ws->y2 - fviewy);

	float px4 = fviewx + prj_fac1 * (ws->x2 - fviewx);
	float py4 = fviewy + prj_fac1 * (ws->y2 - fviewy);

	vertices[0].Set(px1, planez, py1, px1 / 64, -py1 / 64);
	vertices[1].Set(px2, planez, py2, px2 / 64, -py2 / 64);
	vertices[2].Set(px3, planez, py3, px3 / 64, -py3 / 64);
	vertices[3].Set(px4, planez, py4, px4 / 64, -py4 / 64);
}

//==========================================================================
//
//
//
//==========================================================================

void HWDrawInfo::PrepareUpperGap(seg_t * seg)
{
	wallseg ws;
	sector_t * fakefsector = hw_FakeFlat(seg->frontsector, in_area, false);
	sector_t * fakebsector = hw_FakeFlat(seg->backsector, in_area, true);

	vertex_t * v1, *v2;

	// Although the plane can be sloped this code will only be called
	// when the edge itself is not.
	double backz = fakebsector->ceilingplane.ZatPoint(seg->v1);
	double frontz = fakefsector->ceilingplane.ZatPoint(seg->v1);

	if (fakebsector->GetTexture(sector_t::ceiling) == skyflatnum) return;
	if (backz < Viewpoint.Pos.Z) return;

	if (seg->sidedef == seg->linedef->sidedef[0])
	{
		v1 = seg->linedef->v1;
		v2 = seg->linedef->v2;
	}
	else
	{
		v1 = seg->linedef->v2;
		v2 = seg->linedef->v1;
	}

	ws.x1 = v1->fX();
	ws.y1 = v1->fY();
	ws.x2 = v2->fX();
	ws.y2 = v2->fY();

	ws.z1 = frontz;
	ws.z2 = backz;

	auto vertices = screen->mVertexData->AllocVertices(8);

	CreateFloodStencilPoly(&ws, vertices.first);
	CreateFloodPoly(&ws, vertices.first+4, ws.z2, fakebsector, true);

	gl_floodrendernode *node = NewFloodRenderNode();
    auto pNode = floodCeilingSegs.CheckKey(fakebsector->sectornum);

    node->next = pNode? *pNode : nullptr;
	node->seg = seg;
	node->vertexindex = vertices.second;
	floodCeilingSegs[fakebsector->sectornum] = node;
}


//==========================================================================
//
//
//
//==========================================================================

void HWDrawInfo::PrepareLowerGap(seg_t * seg)
{
	wallseg ws;
	sector_t * fakefsector = hw_FakeFlat(seg->frontsector, in_area, false);
	sector_t * fakebsector = hw_FakeFlat(seg->backsector, in_area, true);

	vertex_t * v1, *v2;

	// Although the plane can be sloped this code will only be called
	// when the edge itself is not.
	double backz = fakebsector->floorplane.ZatPoint(seg->v1);
	double frontz = fakefsector->floorplane.ZatPoint(seg->v1);


	if (fakebsector->GetTexture(sector_t::floor) == skyflatnum) return;
	if (fakebsector->GetPlaneTexZ(sector_t::floor) > Viewpoint.Pos.Z) return;

	if (seg->sidedef == seg->linedef->sidedef[0])
	{
		v1 = seg->linedef->v1;
		v2 = seg->linedef->v2;
	}
	else
	{
		v1 = seg->linedef->v2;
		v2 = seg->linedef->v1;
	}

	ws.x1 = v1->fX();
	ws.y1 = v1->fY();
	ws.x2 = v2->fX();
	ws.y2 = v2->fY();

	ws.z2 = frontz;
	ws.z1 = backz;

	auto vertices = screen->mVertexData->AllocVertices(8);

	CreateFloodStencilPoly(&ws, vertices.first);
	CreateFloodPoly(&ws, vertices.first+4, ws.z1, fakebsector, false);

	gl_floodrendernode *node = NewFloodRenderNode();
    auto pNode = floodFloorSegs.CheckKey(fakebsector->sectornum);
    
    node->next = pNode? *pNode : nullptr;

	node->seg = seg;
	node->vertexindex = vertices.second;
	floodFloorSegs[fakebsector->sectornum] = node;
}

//==========================================================================
//
//
//
//==========================================================================

void HWDrawInfo::PrepareUnhandledMissingTextures()
{
	if (!level.notexturefill)
	{
		validcount++;
		for (int i = MissingUpperSegs.Size() - 1; i >= 0; i--)
		{
			int index = MissingUpperSegs[i].MTI_Index;
			if (index >= 0 && MissingUpperTextures[index].seg == NULL) continue;

			seg_t * seg = MissingUpperSegs[i].seg;

			// already done!
			if (seg->linedef->validcount == validcount) continue;		// already done
			seg->linedef->validcount = validcount;
			if (seg->frontsector->GetPlaneTexZ(sector_t::ceiling) < Viewpoint.Pos.Z) continue;	// out of sight

			// FIXME: The check for degenerate subsectors should be more precise
			if (seg->PartnerSeg && (seg->PartnerSeg->Subsector->flags & SSECF_DEGENERATE)) continue;
			if (seg->backsector->transdoor) continue;
			if (seg->backsector->GetTexture(sector_t::ceiling) == skyflatnum) continue;
			if (seg->backsector->ValidatePortal(sector_t::ceiling) != NULL) continue;

			PrepareUpperGap(seg);
		}

		validcount++;
		for (int i = MissingLowerSegs.Size() - 1; i >= 0; i--)
		{
			int index = MissingLowerSegs[i].MTI_Index;
			if (index >= 0 && MissingLowerTextures[index].seg == NULL) continue;

			seg_t * seg = MissingLowerSegs[i].seg;

			if (seg->linedef->validcount == validcount) continue;		// already done
			seg->linedef->validcount = validcount;
			if (seg->frontsector->GetPlaneTexZ(sector_t::floor) > Viewpoint.Pos.Z) continue;	// out of sight
			if (seg->backsector->transdoor) continue;
			if (seg->backsector->GetTexture(sector_t::floor) == skyflatnum) continue;
			if (seg->backsector->ValidatePortal(sector_t::floor) != NULL) continue;

			PrepareLowerGap(seg);
		}
	}
	MissingUpperTextures.Clear();
	MissingLowerTextures.Clear();
	MissingUpperSegs.Clear();
	MissingLowerSegs.Clear();

}

//==========================================================================
//
// Multi-sector deep water hacks
//
//==========================================================================

void HWDrawInfo::AddHackedSubsector(subsector_t * sub)
{
	if (!(level.maptype == MAPTYPE_HEXEN))
	{
		SubsectorHackInfo sh={sub, 0};
		SubsectorHacks.Push (sh);
	}
}

//==========================================================================
//
// Finds a subsector whose plane can be used for rendering
//
//==========================================================================

bool HWDrawInfo::CheckAnchorFloor(subsector_t * sub)
{
	// This subsector has a one sided wall and can be used.
	if (sub->hacked==3) return true;
	if (sub->flags & SSECF_DEGENERATE) return false;

	for(uint32_t j=0;j<sub->numlines;j++)
	{
		seg_t * seg = sub->firstline + j;
		if (!seg->PartnerSeg) return true;

		subsector_t * backsub = seg->PartnerSeg->Subsector;

		// Find a linedef with a different visplane on the other side.
		if (!(backsub->flags & SSECF_DEGENERATE) && seg->linedef && 
			(sub->render_sector != backsub->render_sector && sub->sector != backsub->sector))
		{
			// I'm ignoring slopes, scaling and rotation here. The likelihood of ZDoom maps
			// using such crap hacks is simply too small
			if (sub->render_sector->GetTexture(sector_t::floor) == backsub->render_sector->GetTexture(sector_t::floor) &&
				sub->render_sector->GetPlaneTexZ(sector_t::floor) == backsub->render_sector->GetPlaneTexZ(sector_t::floor) &&
				sub->render_sector->GetFloorLight() == backsub->render_sector->GetFloorLight())
			{
				continue;
			}
			// This means we found an adjoining subsector that clearly would go into another
			// visplane. That means that this subsector can be used as an anchor.
			return true;
		}
	}
	return false;
}

//==========================================================================
//
// Collect connected subsectors that have to be rendered with the same plane
//
//==========================================================================

bool HWDrawInfo::CollectSubsectorsFloor(subsector_t * sub, sector_t * anchor)
{

	// mark it checked
	sub->validcount=validcount;


	// We must collect any subsector that either is connected to this one with a miniseg
	// or has the same visplane.
	// We must not collect any subsector that  has the anchor's visplane!
	if (!(sub->flags & SSECF_DEGENERATE))
	{
		// Is not being rendered so don't bother.
		if (!(ss_renderflags[sub->Index()] & SSRF_PROCESSED)) return true;

		if (sub->render_sector->GetTexture(sector_t::floor) != anchor->GetTexture(sector_t::floor) ||
			sub->render_sector->GetPlaneTexZ(sector_t::floor) != anchor->GetPlaneTexZ(sector_t::floor) ||
			sub->render_sector->GetFloorLight() != anchor->GetFloorLight())
		{
			if (sub == viewsubsector && Viewpoint.Pos.Z < anchor->GetPlaneTexZ(sector_t::floor)) inview = true;
			HandledSubsectors.Push(sub);
		}
	}

	// We can assume that all segs in this subsector are connected to a subsector that has
	// to be checked as well
	for(uint32_t j=0;j<sub->numlines;j++)
	{
		seg_t * seg = sub->firstline + j;
		if (seg->PartnerSeg)
		{
			subsector_t * backsub = seg->PartnerSeg->Subsector;

			// could be an anchor itself.
			if (!CheckAnchorFloor (backsub)) // must not be an anchor itself!
			{
				if (backsub->validcount!=validcount) 
				{
					if (!CollectSubsectorsFloor (backsub, anchor)) return false;
				}
			}
			else if (sub->render_sector == backsub->render_sector)
			{
				// Any anchor not within the original anchor's visplane terminates the processing.
				if (sub->render_sector->GetTexture(sector_t::floor) != anchor->GetTexture(sector_t::floor) ||
					sub->render_sector->GetPlaneTexZ(sector_t::floor) != anchor->GetPlaneTexZ(sector_t::floor) ||
					sub->render_sector->GetFloorLight() != anchor->GetFloorLight())
				{
					return false;
				}
			}
			if (!seg->linedef || (seg->frontsector==seg->backsector && sub->render_sector!=backsub->render_sector))
				lowersegs.Push(seg);
		}
	}
	return true;
}

//==========================================================================
//
// Finds a subsector whose plane can be used for rendering
//
//==========================================================================

bool HWDrawInfo::CheckAnchorCeiling(subsector_t * sub)
{
	// This subsector has a one sided wall and can be used.
	if (sub->hacked==3) return true;
	if (sub->flags & SSECF_DEGENERATE) return false;

	for(uint32_t j=0;j<sub->numlines;j++)
	{
		seg_t * seg = sub->firstline + j;
		if (!seg->PartnerSeg) return true;

		subsector_t * backsub = seg->PartnerSeg->Subsector;

		// Find a linedef with a different visplane on the other side.
		if (!(backsub->flags & SSECF_DEGENERATE) && seg->linedef && 
			(sub->render_sector != backsub->render_sector && sub->sector != backsub->sector))
		{
			// I'm ignoring slopes, scaling and rotation here. The likelihood of ZDoom maps
			// using such crap hacks is simply too small
			if (sub->render_sector->GetTexture(sector_t::ceiling) == backsub->render_sector->GetTexture(sector_t::ceiling) &&
				sub->render_sector->GetPlaneTexZ(sector_t::ceiling) == backsub->render_sector->GetPlaneTexZ(sector_t::ceiling) &&
				sub->render_sector->GetCeilingLight() == backsub->render_sector->GetCeilingLight())
			{
				continue;
			}
			// This means we found an adjoining subsector that clearly would go into another
			// visplane. That means that this subsector can be used as an anchor.
			return true;
		}
	}
	return false;
}

//==========================================================================
//
// Collect connected subsectors that have to be rendered with the same plane
//
//==========================================================================

bool HWDrawInfo::CollectSubsectorsCeiling(subsector_t * sub, sector_t * anchor)
{
	// mark it checked
	sub->validcount=validcount;


	// We must collect any subsector that either is connected to this one with a miniseg
	// or has the same visplane.
	// We must not collect any subsector that  has the anchor's visplane!
	if (!(sub->flags & SSECF_DEGENERATE)) 
	{
		// Is not being rendererd so don't bother.
		if (!(ss_renderflags[sub->Index()]&SSRF_PROCESSED)) return true;

		if (sub->render_sector->GetTexture(sector_t::ceiling) != anchor->GetTexture(sector_t::ceiling) ||
			sub->render_sector->GetPlaneTexZ(sector_t::ceiling) != anchor->GetPlaneTexZ(sector_t::ceiling) ||
			sub->render_sector->GetCeilingLight() != anchor->GetCeilingLight())
		{
			HandledSubsectors.Push(sub);
		}
	}

	// We can assume that all segs in this subsector are connected to a subsector that has
	// to be checked as well
	for(uint32_t j=0;j<sub->numlines;j++)
	{
		seg_t * seg = sub->firstline + j;
		if (seg->PartnerSeg)
		{
			subsector_t * backsub = seg->PartnerSeg->Subsector;

			// could be an anchor itself.
			if (!CheckAnchorCeiling (backsub)) // must not be an anchor itself!
			{
				if (backsub->validcount!=validcount) 
				{
					if (!CollectSubsectorsCeiling (backsub, anchor)) return false;
				}
			}
			else if (sub->render_sector == backsub->render_sector)
			{
				// Any anchor not within the original anchor's visplane terminates the processing.
				if (sub->render_sector->GetTexture(sector_t::ceiling) != anchor->GetTexture(sector_t::ceiling) ||
					sub->render_sector->GetPlaneTexZ(sector_t::ceiling) != anchor->GetPlaneTexZ(sector_t::ceiling) ||
					sub->render_sector->GetCeilingLight() != anchor->GetCeilingLight())
				{
					return false;
				}
			}
		}
	}
	return true;
}

//==========================================================================
//
// Process the subsectors that have been marked as hacked
//
//==========================================================================

void HWDrawInfo::ProcessLowerMinisegs(TArray<seg_t *> &lowersegs)
{
    for(unsigned int j=0;j<lowersegs.Size();j++)
    {
        seg_t * seg=lowersegs[j];
        GLWall wall;
        wall.ProcessLowerMiniseg(this, seg, seg->Subsector->render_sector, seg->PartnerSeg->Subsector->render_sector);
        rendered_lines++;
    }
}


void HWDrawInfo::HandleHackedSubsectors()
{
	viewsubsector = R_PointInSubsector(Viewpoint.Pos);

	// Each subsector may only be processed once in this loop!
	validcount++;
	for(unsigned int i=0;i<SubsectorHacks.Size();i++)
	{
		subsector_t * sub = SubsectorHacks[i].sub;
		if (sub->validcount!=validcount && CheckAnchorFloor(sub))
		{
			// Now collect everything that is connected with this subsector.
			HandledSubsectors.Clear();
			inview=false;
			lowersegs.Clear();
			if (CollectSubsectorsFloor(sub, sub->render_sector))
			{
				for(unsigned int j=0;j<HandledSubsectors.Size();j++)
				{				
                    gl_subsectorrendernode * node = NewSubsectorRenderNode();
					node->sub = HandledSubsectors[j];
					AddOtherFloorPlane(sub->render_sector->sectornum, node);
				}
                if (inview) ProcessLowerMinisegs(lowersegs);
			}
		}
	}

	// Each subsector may only be processed once in this loop!
	validcount++;
	for(unsigned int i=0;i<SubsectorHacks.Size();i++)
	{
		subsector_t * sub = SubsectorHacks[i].sub;
		if (sub->validcount!=validcount && CheckAnchorCeiling(sub))
		{
			// Now collect everything that is connected with this subsector.
			HandledSubsectors.Clear();
			if (CollectSubsectorsCeiling(sub, sub->render_sector))
			{
				for(unsigned int j=0;j<HandledSubsectors.Size();j++)
				{				
                    gl_subsectorrendernode * node = NewSubsectorRenderNode();
					node->sub = HandledSubsectors[j];
					AddOtherCeilingPlane(sub->render_sector->sectornum, node);
				}
			}
		}
	}


	SubsectorHacks.Clear();
}

// This code was meant to fix the portal in KDIZD's Z1M1, but later versions of that mod do not need it anymore.
// I am not aware of other portals ever having been set up so badly as this one so it probably is not needed anymore.
// Even if needed this must be done differently because this code depends on hacking the render data in a bad way.
#if 0	
		
//==========================================================================
//
// This merges visplanes that lie inside a sector stack together
// to avoid rendering these unneeded flats
//
//==========================================================================

void HWDrawInfo::AddFloorStack(sector_t * sec)
{
	FloorStacks.Push(sec);
}

void HWDrawInfo::AddCeilingStack(sector_t * sec)
{
	CeilingStacks.Push(sec);
}

//==========================================================================
//
//
//
//==========================================================================

void HWDrawInfo::CollectSectorStacksCeiling(subsector_t * sub, sector_t * anchor, area_t in_area)
{
	// mark it checked
	sub->validcount=validcount;

	// Has a sector stack or skybox itself!
	if (sub->render_sector->GetPortalGroup(sector_t::ceiling) != nullptr) return;

	// Don't bother processing unrendered subsectors
	if (sub->numlines>2 && !(ss_renderflags[sub->Index()]&SSRF_PROCESSED)) return;

	// Must be the exact same visplane
	sector_t * me = hw_FakeFlat(sub->render_sector, in_area, false);
	if (me->GetTexture(sector_t::ceiling) != anchor->GetTexture(sector_t::ceiling) ||
		me->ceilingplane != anchor->ceilingplane ||
		me->GetCeilingLight() != anchor->GetCeilingLight() ||
		me->Colormap != anchor->Colormap ||
		me->SpecialColors[sector_t::ceiling] != anchor->SpecialColors[sector_t::ceiling] ||
		me->planes[sector_t::ceiling].xform != anchor->planes[sector_t::ceiling].xform)
	{
		// different visplane so it can't belong to this stack
		return;
	}

	HandledSubsectors.Push (sub);

	for(uint32_t j=0;j<sub->numlines;j++)
	{
		seg_t * seg = sub->firstline + j;
		if (seg->PartnerSeg)
		{
			subsector_t * backsub = seg->PartnerSeg->Subsector;

			if (backsub->validcount!=validcount) CollectSectorStacksCeiling (backsub, anchor, in_area);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void HWDrawInfo::CollectSectorStacksFloor(subsector_t * sub, sector_t * anchor, area_t in_area)
{
	// mark it checked
	sub->validcount=validcount;

	// Has a sector stack or skybox itself!
	if (sub->render_sector->GetPortalGroup(sector_t::floor) != nullptr) return;

	// Don't bother processing unrendered subsectors
	if (sub->numlines>2 && !(ss_renderflags[sub->Index()]&SSRF_PROCESSED)) return;

	// Must be the exact same visplane
	sector_t * me = hw_FakeFlat(sub->render_sector, in_area, false);
	if (me->GetTexture(sector_t::floor) != anchor->GetTexture(sector_t::floor) ||
		me->floorplane != anchor->floorplane ||
		me->GetFloorLight() != anchor->GetFloorLight() ||
		me->Colormap != anchor->Colormap ||
		me->SpecialColors[sector_t::floor] != anchor->SpecialColors[sector_t::floor] ||
		me->planes[sector_t::floor].xform != anchor->planes[sector_t::floor].xform)
	{
		// different visplane so it can't belong to this stack
		return;
	}

	HandledSubsectors.Push (sub);

	for(uint32_t j=0;j<sub->numlines;j++)
	{
		seg_t * seg = sub->firstline + j;
		if (seg->PartnerSeg)
		{
			subsector_t * backsub = seg->PartnerSeg->Subsector;

			if (backsub->validcount!=validcount) CollectSectorStacksFloor (backsub, anchor, in_area);
		}
	}
}

//==========================================================================
//
//
//
//==========================================================================

void HWDrawInfo::ProcessSectorStacks(area_t in_area)
{
	unsigned int i;

	validcount++;
	for (i=0;i<CeilingStacks.Size (); i++)
	{
		sector_t *sec = hw_FakeFlat(CeilingStacks[i], in_area, false);
		auto portal = sec->GetPortalGroup(sector_t::ceiling);
		if (portal != NULL) for(int k=0;k<sec->subsectorcount;k++)
		{
			subsector_t * sub = sec->subsectors[k];
			if (ss_renderflags[sub->Index()] & SSRF_PROCESSED)
			{
				HandledSubsectors.Clear();
				for(uint32_t j=0;j<sub->numlines;j++)
				{
					seg_t * seg = sub->firstline + j;
					if (seg->PartnerSeg)
					{
						subsector_t * backsub = seg->PartnerSeg->Subsector;

						if (backsub->validcount!=validcount) CollectSectorStacksCeiling (backsub, sec, in_area);
					}
				}
				for(unsigned int j=0;j<HandledSubsectors.Size();j++)
				{				
					subsector_t *sub = HandledSubsectors[j];
					ss_renderflags[sub->Index()] &= ~SSRF_RENDERCEILING;

					if (sub->portalcoverage[sector_t::ceiling].subsectors == NULL)
					{
						BuildPortalCoverage(&sub->portalcoverage[sector_t::ceiling], sub, portal->mDisplacement);
					}

					AddSubsectorToPortal(portal, sub);

					if (sec->GetAlpha(sector_t::ceiling) != 0 && sec->GetTexture(sector_t::ceiling) != skyflatnum)
					{
                        gl_subsectorrendernode * node = NewSubsectorRenderNode();
						node->sub = sub;
						AddOtherCeilingPlane(sec->sectornum, node);
					}
				}
			}
		}
	}

	validcount++;
	for (i=0;i<FloorStacks.Size (); i++)
	{
		sector_t *sec = hw_FakeFlat(FloorStacks[i], in_area, false);
		auto portal = sec->GetPortalGroup(sector_t::floor);
		if (portal != NULL) for(int k=0;k<sec->subsectorcount;k++)
		{
			subsector_t * sub = sec->subsectors[k];
			if (ss_renderflags[sub->Index()] & SSRF_PROCESSED)
			{
				HandledSubsectors.Clear();
				for(uint32_t j=0;j<sub->numlines;j++)
				{
					seg_t * seg = sub->firstline + j;
					if (seg->PartnerSeg)
					{
						subsector_t	* backsub = seg->PartnerSeg->Subsector;

						if (backsub->validcount!=validcount) CollectSectorStacksFloor (backsub, sec, in_area);
					}
				}

				for(unsigned int j=0;j<HandledSubsectors.Size();j++)
				{				
					subsector_t *sub = HandledSubsectors[j];
					ss_renderflags[sub->Index()] &= ~SSRF_RENDERFLOOR;

					if (sub->portalcoverage[sector_t::floor].subsectors == NULL)
					{
						BuildPortalCoverage(&sub->portalcoverage[sector_t::floor], sub, portal->mDisplacement);
					}

					AddSubsectorToPortal(portal, sub);

					if (sec->GetAlpha(sector_t::floor) != 0 && sec->GetTexture(sector_t::floor) != skyflatnum)
					{
                        gl_subsectorrendernode * node = NewSubsectorRenderNode();
						node->sub = sub;
						AddOtherFloorPlane(sec->sectornum, node);
					}
				}
			}
		}
	}

	FloorStacks.Clear();
	CeilingStacks.Clear();
}

#endif
