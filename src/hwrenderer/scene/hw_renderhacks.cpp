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

#include "hw_drawinfo.h"
#include "hw_drawstructs.h"
#include "hwrenderer/utility/hw_clock.h"

sector_t * hw_FakeFlat(sector_t * sec, sector_t * dest, area_t in_area, bool back);

//==========================================================================
//
// Adds a subsector plane to a sector's render list
//
//==========================================================================

void HWDrawInfo::AddOtherFloorPlane(int sector, gl_subsectorrendernode * node)
{
	int oldcnt = otherfloorplanes.Size();

	if (oldcnt<=sector)
	{
		otherfloorplanes.Resize(sector+1);
		for(int i=oldcnt;i<=sector;i++) otherfloorplanes[i]=NULL;
	}
	node->next = otherfloorplanes[sector];
	otherfloorplanes[sector] = node;
}

void HWDrawInfo::AddOtherCeilingPlane(int sector, gl_subsectorrendernode * node)
{
	int oldcnt = otherceilingplanes.Size();

	if (oldcnt<=sector)
	{
		otherceilingplanes.Resize(sector+1);
		for(int i=oldcnt;i<=sector;i++) otherceilingplanes[i]=NULL;
	}
	node->next = otherceilingplanes[sector];
	otherceilingplanes[sector] = node;
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

			sector_t * sec = hw_FakeFlat(seg->backsector, &fakesec, in_area, true);

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
				FTexture * tex = TexMan[seg->sidedef->GetTexture(side_t::top)];
				if (!tex || tex->UseType == ETextureType::Null) continue;
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

			sector_t * sec = hw_FakeFlat(seg->backsector, &fakesec, in_area, true);

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
				FTexture * tex = TexMan[seg->sidedef->GetTexture(side_t::bottom)];
				if (!tex || tex->UseType == ETextureType::Null) continue;
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

			sector_t * sec = hw_FakeFlat(seg->backsector, &fakesec, in_area, true);

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

			sector_t * sec = hw_FakeFlat(seg->backsector, &fakesec, in_area, true);

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
	sector_t fake;

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
				// The mere fact that this seg has been added to the list means that the back sector
				// will be rendered so we can safely assume that it is already in the render list

				for (unsigned int j = 0; j < HandledSubsectors.Size(); j++)
				{
					gl_subsectorrendernode * node = new gl_subsectorrendernode;
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
			sector_t * fakesector = hw_FakeFlat(MissingUpperTextures[i].seg->frontsector, &fake, in_area, false);
			float planez = (float)fakesector->GetPlaneTexZ(sector_t::ceiling);

			backsub->validcount = validcount;
			if (DoFakeCeilingBridge(backsub, planez, in_area))
			{
				// The mere fact that this seg has been added to the list means that the back sector
				// will be rendered so we can safely assume that it is already in the render list

				for (unsigned int j = 0; j < HandledSubsectors.Size(); j++)
				{
					gl_subsectorrendernode * node = new gl_subsectorrendernode;
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
				// The mere fact that this seg has been added to the list means that the back sector
				// will be rendered so we can safely assume that it is already in the render list

				for (unsigned int j = 0; j < HandledSubsectors.Size(); j++)
				{
					gl_subsectorrendernode * node = new gl_subsectorrendernode;
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
			sector_t * fakesector = hw_FakeFlat(MissingLowerTextures[i].seg->frontsector, &fake, in_area, false);
			float planez = (float)fakesector->GetPlaneTexZ(sector_t::floor);

			backsub->validcount = validcount;
			if (DoFakeBridge(backsub, planez, in_area))
			{
				// The mere fact that this seg has been added to the list means that the back sector
				// will be rendered so we can safely assume that it is already in the render list

				for (unsigned int j = 0; j < HandledSubsectors.Size(); j++)
				{
					gl_subsectorrendernode * node = new gl_subsectorrendernode;
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

void HWDrawInfo::DrawUnhandledMissingTextures()
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

		if (!level.notexturefill) FloodUpperGap(seg);
	}

	validcount++;
	for (int i = MissingLowerSegs.Size() - 1; i >= 0; i--)
	{
		int index = MissingLowerSegs[i].MTI_Index;
		if (index >= 0 && MissingLowerTextures[index].seg == NULL) continue;

		seg_t * seg = MissingLowerSegs[i].seg;

		if (seg->linedef->validcount == validcount) continue;		// already done
		seg->linedef->validcount = validcount;
		if (!(sectorrenderflags[seg->backsector->sectornum] & SSRF_RENDERFLOOR)) continue;
		if (seg->frontsector->GetPlaneTexZ(sector_t::floor) > Viewpoint.Pos.Z) continue;	// out of sight
		if (seg->backsector->transdoor) continue;
		if (seg->backsector->GetTexture(sector_t::floor) == skyflatnum) continue;
		if (seg->backsector->ValidatePortal(sector_t::floor) != NULL) continue;

		if (!level.notexturefill) FloodLowerGap(seg);
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
					gl_subsectorrendernode * node = new gl_subsectorrendernode;

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
					gl_subsectorrendernode * node = new gl_subsectorrendernode;

					node->sub = HandledSubsectors[j];
					AddOtherCeilingPlane(sub->render_sector->sectornum, node);
				}
			}
		}
	}


	SubsectorHacks.Clear();
}

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
	sector_t * me = hw_FakeFlat(sub->render_sector, &fakesec, in_area, false);
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
	sector_t * me = hw_FakeFlat(sub->render_sector, &fakesec, in_area, false);
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
		sector_t *sec = hw_FakeFlat(CeilingStacks[i], &fakesec, in_area, false);
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
					sub->sector->ibocount = -1;	// cannot render this sector in one go.

					if (sub->portalcoverage[sector_t::ceiling].subsectors == NULL)
					{
						BuildPortalCoverage(&sub->portalcoverage[sector_t::ceiling], sub, portal->mDisplacement);
					}

					AddSubsectorToPortal(portal, sub);

					if (sec->GetAlpha(sector_t::ceiling) != 0 && sec->GetTexture(sector_t::ceiling) != skyflatnum)
					{
						gl_subsectorrendernode * node = new gl_subsectorrendernode;
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
		sector_t *sec = hw_FakeFlat(FloorStacks[i], &fakesec, in_area, false);
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
					sub->sector->ibocount = -1;	// cannot render this sector in one go.

					if (sub->portalcoverage[sector_t::floor].subsectors == NULL)
					{
						BuildPortalCoverage(&sub->portalcoverage[sector_t::floor], sub, portal->mDisplacement);
					}

					AddSubsectorToPortal(portal, sub);

					if (sec->GetAlpha(sector_t::floor) != 0 && sec->GetTexture(sector_t::floor) != skyflatnum)
					{
						gl_subsectorrendernode * node = new gl_subsectorrendernode;
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

