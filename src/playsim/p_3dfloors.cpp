/*
** p_3dfloor.cpp
**
** 3D-floor handling
**
**---------------------------------------------------------------------------
** Copyright 2005-2008 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
** 4. Full disclosure of the entire project's source code, except for third
**    party libraries is mandatory. (NOTE: This clause is non-negotiable!)
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include "templates.h"
#include "p_local.h"
#include "p_lnspec.h"
#include "p_maputl.h"
#include "w_wad.h"
#include "g_level.h"
#include "p_terrain.h"
#include "d_player.h"
#include "p_spec.h"
#include "g_levellocals.h"
#include "actorinlines.h"

//==========================================================================
//
//  3D Floors
//
//==========================================================================

//==========================================================================
//
// Wrappers to access the colormap information
//
//==========================================================================

FColormap F3DFloor::GetColormap()
{
	// If there's no fog in either model or target sector this is easy and fast.
	if ((target->Colormap.FadeColor.isBlack() && model->Colormap.FadeColor.isBlack()) || (flags & (FF_FADEWALLS|FF_FOG)))
	{
		return model->Colormap;
	}
	else
	{
		// We must create a new colormap combining the properties we need
		return{ model->Colormap.LightColor, target->Colormap.FadeColor, model->Colormap.Desaturation, model->Colormap.BlendFactor, target->Colormap.FogDensity };
	}
}

PalEntry F3DFloor::GetBlend()
{
	// The model sector's fog is used as blend unless FF_FADEWALLS is set.
	if (!(flags & FF_FADEWALLS) && target->Colormap.FadeColor != model->Colormap.FadeColor)
	{
		return model->Colormap.FadeColor;
	}
	else
	{
		return 0;
	}
}

void F3DFloor::UpdateColormap(FColormap &map)
{
	// Note that this is a bit different from GetColormap.
	if ((target->Colormap.FadeColor.isBlack() && model->Colormap.FadeColor.isBlack()) || (flags & FF_FADEWALLS) ||
		target->Colormap.FadeColor == model->Colormap.FadeColor)
	{
		map = model->Colormap;
	}
	else
	{
		map = { model->Colormap.LightColor, target->Colormap.FadeColor, model->Colormap.Desaturation, model->Colormap.BlendFactor, target->Colormap.FogDensity };
	}
}

//==========================================================================
//
// Add one 3D floor to the sector
//
//==========================================================================
void P_Add3DFloor(sector_t* sec, sector_t* sec2, line_t* master, int flags, int alpha)
{
	F3DFloor*      ffloor;
	unsigned  i;

	if(!(flags & FF_THISINSIDE)) {
		for(i = 0; i < sec2->e->XFloor.attached.Size(); i++) if(sec2->e->XFloor.attached[i] == sec) return;
		sec2->e->XFloor.attached.Push(sec);
	}
	
	//Add the floor
	ffloor = new F3DFloor;
	ffloor->top.copied = ffloor->bottom.copied = false;
	ffloor->top.model = ffloor->bottom.model = ffloor->model = sec2;
	ffloor->target = sec;
	ffloor->top.vindex = ffloor->bottom.vindex = -1;

	if (!(flags&FF_THINFLOOR))
	{
		ffloor->bottom.plane = &sec2->floorplane;
		ffloor->bottom.texture = &sec2->planes[sector_t::floor].Texture;
		ffloor->bottom.isceiling = sector_t::floor;
	}
	else 
	{
		ffloor->bottom.plane = &sec2->ceilingplane;
		ffloor->bottom.texture = &sec2->planes[sector_t::ceiling].Texture;
		ffloor->bottom.isceiling = sector_t::ceiling;
	}
	
	if (!(flags&FF_FIX))
	{
		ffloor->top.plane = &sec2->ceilingplane;
		ffloor->top.texture = &sec2->planes[sector_t::ceiling].Texture;
		ffloor->toplightlevel = &sec2->lightlevel;
		ffloor->top.isceiling = sector_t::ceiling;
	}
	else	// FF_FIX is a special case to patch rendering holes
	{
		ffloor->top.plane = &sec->floorplane;
		ffloor->top.texture = &sec2->planes[sector_t::floor].Texture;
		ffloor->toplightlevel = &sec->lightlevel;
		ffloor->top.isceiling = sector_t::floor;
		ffloor->top.model = sec;
	}

	// Hacks for Vavoom's idiotic implementation
	if (flags&FF_INVERTSECTOR)
	{
		// switch the planes
		std::swap(ffloor->top, ffloor->bottom);

		if (flags&FF_SWIMMABLE)
		{
			// Vavoom floods the lower part if it is swimmable.
			// fortunately this plane won't be rendered - otherwise this wouldn't work...
			ffloor->bottom.plane=&sec->floorplane;
			ffloor->bottom.model=sec;
			ffloor->bottom.isceiling = sector_t::floor;
		}
	}

	ffloor->flags  = flags;
	ffloor->master = master;
	ffloor->alpha  = clamp(alpha, 0, 255);
	ffloor->top.vindex = ffloor->bottom.vindex = -1;

	// The engine cannot handle sloped translucent floors. Sorry
	if (ffloor->top.plane->isSlope() || ffloor->bottom.plane->isSlope())
	{
		ffloor->alpha = 255;
		ffloor->flags &= ~FF_ADDITIVETRANS;
	}

	if(flags & FF_THISINSIDE) 
	{
		// switch the planes
		std::swap(ffloor->top, ffloor->bottom);
	}

	sec->e->XFloor.ffloors.Push(ffloor);

	// kg3D - software renderer only hack
	// this is really required because of ceilingclip and floorclip
	if(flags & FF_BOTHPLANES)
	{
		P_Add3DFloor(sec, sec2, master, FF_EXISTS | FF_THISINSIDE | FF_RENDERPLANES | FF_NOSHADE | FF_SEETHROUGH | FF_SHOOTTHROUGH |
			(flags & (FF_INVERTSECTOR | FF_TRANSLUCENT | FF_ADDITIVETRANS)), alpha);
	}
}

//==========================================================================
//
// P_PlayerOnSpecial3DFloor
// Checks to see if a player is standing on or is inside a 3D floor (water)
// and applies any specials..
//
//==========================================================================

void P_PlayerOnSpecial3DFloor(player_t* player)
{
	for(auto rover : player->mo->Sector->e->XFloor.ffloors)
	{
		if (!(rover->flags & FF_EXISTS)) continue;
		if (rover->flags & FF_FIX) continue;

		// Check the 3D floor's type...
		if(rover->flags & FF_SOLID)
		{
			// Player must be on top of the floor to be affected...
			if(player->mo->Z() != rover->top.plane->ZatPoint(player->mo)) continue;
		}
		else
		{
			//Water and DEATH FOG!!! heh
			if (player->mo->Z() > rover->top.plane->ZatPoint(player->mo) || 
				player->mo->Top() < rover->bottom.plane->ZatPoint(player->mo))
				continue;
		}

		// Apply sector specials
		P_PlayerInSpecialSector(player, rover->model);

		// Apply flat specials (using the ceiling!)
		P_PlayerOnSpecialFlat(player, rover->model->GetTerrain(rover->top.isceiling));

		break;
	}
}

//==========================================================================
//
// P_CheckFor3DFloorHit
// Checks whether the player's feet touch a solid 3D floor in the sector
//
//==========================================================================
bool P_CheckFor3DFloorHit(AActor * mo, double z, bool trigger)
{
	if ((mo->player && (mo->player->cheats & CF_PREDICTING))) return false;

	for (auto rover : mo->Sector->e->XFloor.ffloors)
	{
		if (!(rover->flags & FF_EXISTS)) continue;

		if(rover->flags & FF_SOLID && rover->model->SecActTarget)
		{
			if (fabs(z - rover->top.plane->ZatPoint(mo)) < EQUAL_EPSILON) 
			{
				mo->BlockingFloor = rover->model;
				mo->Blocking3DFloor = rover->model;
				if (trigger) rover->model->TriggerSectorActions (mo, SECSPAC_HitFloor);
				return true;
			}
		}
	}
	return false;
}

//==========================================================================
//
// P_CheckFor3DCeilingHit
// Checks whether the player's head touches a solid 3D floor in the sector
//
//==========================================================================
bool P_CheckFor3DCeilingHit(AActor * mo, double z, bool trigger)
{
	if ((mo->player && (mo->player->cheats & CF_PREDICTING))) return false;

	for (auto rover : mo->Sector->e->XFloor.ffloors)
	{
		if (!(rover->flags & FF_EXISTS)) continue;

		if(rover->flags & FF_SOLID && rover->model->SecActTarget)
		{
			if(fabs(z - rover->bottom.plane->ZatPoint(mo)) < EQUAL_EPSILON)
			{
				mo->BlockingCeiling = rover->model;
				mo->Blocking3DFloor = rover->model;
				if (trigger) rover->model->TriggerSectorActions (mo, SECSPAC_HitCeiling);
				return true;
			}
		}
	}
	return false;
}

//==========================================================================
//
// P_Recalculate3DFloors
//
// This function sorts the ffloors by height and creates the lightlists 
// that the given sector uses to light floors/ceilings/walls according to the 3D floors.
//
//==========================================================================

void P_Recalculate3DFloors(sector_t * sector)
{
	F3DFloor *		rover;
	F3DFloor *		pick;
	unsigned		pickindex;
	F3DFloor *		clipped=NULL;
	F3DFloor *		solid=NULL;
	double			solid_bottom=0;
	double			clipped_top;
	double			clipped_bottom=0;
	double			maxheight, minheight;
	unsigned		i, j;
	lightlist_t newlight;
	lightlist_t resetlight;	// what it goes back to after FF_DOUBLESHADOW

	TArray<F3DFloor*> & ffloors=sector->e->XFloor.ffloors;
	TArray<lightlist_t> & lightlist = sector->e->XFloor.lightlist;

	// Sort the floors top to bottom for quicker access here and later
	// Translucent and swimmable floors are split if they overlap with solid ones.
	if (ffloors.Size()>1)
	{
		TArray<F3DFloor*> oldlist = std::move(ffloors);

		// first delete the old dynamic stuff
		for(i=0;i<oldlist.Size();i++)
		{
			F3DFloor * rover=oldlist[i];

			if (rover->flags&FF_DYNAMIC)
			{
				delete rover;
				oldlist.Delete(i);
				i--;
				continue;
			}
			if (rover->flags&FF_CLIPPED)
			{
				rover->flags&=~FF_CLIPPED;
				rover->flags|=FF_EXISTS;
			}
		}

		while (oldlist.Size())
		{
			pick=oldlist[0];
			double height=pick->top.plane->ZatPoint(sector->centerspot);

			// find highest starting ffloor - intersections are not supported!
			pickindex=0;
			for (j=1;j<oldlist.Size();j++)
			{
				double h2=oldlist[j]->top.plane->ZatPoint(sector->centerspot);

				if (h2>height)
				{
					pick=oldlist[j];
					pickindex=j;
					height=h2;
				}
			}

			oldlist.Delete(pickindex);
			double pick_bottom=pick->bottom.plane->ZatPoint(sector->centerspot);

			if (pick->flags & FF_THISINSIDE)
			{
				// These have the floor higher than the ceiling and cannot be processed
				// by the clipping code below.
				ffloors.Push(pick);
			}
			else if ((pick->flags&(FF_SWIMMABLE|FF_TRANSLUCENT) || (!(pick->flags&FF_RENDERALL))) && pick->flags&FF_EXISTS)
			{
				// We must check if this nonsolid segment gets clipped from the top by another 3D floor
				if (solid != NULL && solid_bottom < height)
				{
					ffloors.Push(pick);
					if (solid_bottom < pick_bottom)
					{
						// this one is fully covered
						pick->flags|=FF_CLIPPED;
						pick->flags&=~FF_EXISTS;
					}
					else
					{
						F3DFloor * dyn=new F3DFloor;
						*dyn=*pick;
						pick->flags|=FF_CLIPPED;
						pick->flags&=~FF_EXISTS;
						dyn->flags|=FF_DYNAMIC;
						dyn->top.copyPlane(&solid->bottom);
						ffloors.Push(dyn);

						clipped = dyn;
						clipped_top = solid_bottom;
						clipped_bottom = pick_bottom;
					}
				}
				else if (pick_bottom > height)	// do not allow inverted planes
				{
					F3DFloor * dyn = new F3DFloor;
					*dyn = *pick;
					pick->flags |= FF_CLIPPED;
					pick->flags &= ~FF_EXISTS;
					dyn->flags |= FF_DYNAMIC;
					dyn->bottom.copyPlane(&pick->top);
					ffloors.Push(pick);
					ffloors.Push(dyn);
				}
				else
				{
					clipped = pick;
					clipped_top = height;
					clipped_bottom = pick_bottom;
					ffloors.Push(pick);
				}
			}
			else if (clipped && clipped_bottom<height)
			{
				// translucent floor above must be clipped to this one!
				F3DFloor * dyn=new F3DFloor;
				*dyn=*clipped;
				clipped->flags|=FF_CLIPPED;
				clipped->flags&=~FF_EXISTS;
				dyn->flags|=FF_DYNAMIC;
				dyn->bottom.copyPlane(&pick->top);
				ffloors.Push(dyn);
				ffloors.Push(pick);

				if (pick_bottom<=clipped_bottom)
				{
					clipped=NULL;
				}
				else
				{
					// the translucent part extends below the clipper
					dyn=new F3DFloor;
					*dyn=*clipped;
					dyn->flags|=FF_DYNAMIC|FF_EXISTS;
					dyn->top.copyPlane(&pick->bottom);
					ffloors.Push(dyn);
					clipped = dyn;
					clipped_top = pick_bottom;
				}
				solid = pick;
				solid_bottom = pick_bottom;
			}
			else
			{
				clipped = NULL;
				if (solid == NULL || solid_bottom > pick_bottom)
				{
					// only if this one is lower
					solid = pick;
					solid_bottom = pick_bottom;
				}
				ffloors.Push(pick);

			}

		}
	}

	// having the floors sorted makes this routine significantly simpler
	// Only some overlapping cases with FF_DOUBLESHADOW might create anomalies
	// but these are clearly undefined.
	if(ffloors.Size())
	{
		lightlist.Resize(1);
		lightlist[0].plane = sector->ceilingplane;
		lightlist[0].p_lightlevel = &sector->lightlevel;
		lightlist[0].caster = NULL;
		lightlist[0].lightsource = NULL;
		lightlist[0].extra_colormap = sector->Colormap;
		lightlist[0].blend = 0;
		lightlist[0].flags = 0;

		resetlight = lightlist[0];

		maxheight = sector->ceilingplane.ZatPoint(sector->centerspot);
		minheight = sector->floorplane.ZatPoint(sector->centerspot);
		for(i = 0; i < ffloors.Size(); i++)
		{
			rover=ffloors[i];

			if ( !(rover->flags & FF_EXISTS) || rover->flags & FF_NOSHADE )
				continue;
				
			double ff_top=rover->top.plane->ZatPoint(sector->centerspot);
			if (ff_top < minheight) break;	// reached the floor
			if (ff_top < maxheight)
			{
				newlight.plane = *rover->top.plane;
				newlight.p_lightlevel = rover->toplightlevel;
				newlight.caster = rover;
				newlight.lightsource = rover;
				newlight.extra_colormap = rover->GetColormap();
				newlight.blend = rover->GetBlend();
				newlight.flags = rover->flags;
				lightlist.Push(newlight);
			}
			else
			{
				double ff_bottom=rover->bottom.plane->ZatPoint(sector->centerspot);
				if (ff_bottom<maxheight)
				{
					// this segment begins over the ceiling and extends beyond it
					lightlist[0].p_lightlevel = rover->toplightlevel;
					lightlist[0].caster = rover;
					lightlist[0].lightsource = rover;
					lightlist[0].extra_colormap = rover->GetColormap();
					lightlist[0].blend = rover->GetBlend();
					lightlist[0].flags = rover->flags;
				}
			}
			if (!(rover->flags & (FF_DOUBLESHADOW | FF_RESET)))
			{
				resetlight = lightlist.Last();
			}
			else if (rover->flags & FF_RESET)
			{
				resetlight.p_lightlevel = &sector->lightlevel;
				resetlight.lightsource = NULL;
				resetlight.extra_colormap = sector->Colormap;
				resetlight.blend = 0;
			}

			if (rover->flags&FF_DOUBLESHADOW)
			{
				double ff_bottom=rover->bottom.plane->ZatPoint(sector->centerspot);
				if(ff_bottom < maxheight && ff_bottom>minheight)
				{
					newlight.caster = rover;
					newlight.plane = *rover->bottom.plane;
					newlight.lightsource = resetlight.lightsource;
					newlight.p_lightlevel = resetlight.p_lightlevel;
					newlight.extra_colormap = resetlight.extra_colormap;
					newlight.blend = resetlight.blend;
					newlight.flags = rover->flags;
					lightlist.Push(newlight);
				}
			}
		}
	}
}

//==========================================================================
//
// removes all dynamic data. This needs to be done once before creating
// the vertex buffer.
//
//==========================================================================

void FLevelLocals::ClearDynamic3DFloorData()
{
	for (auto &sec : sectors)
	{
		TArray<F3DFloor*> & ffloors = sec.e->XFloor.ffloors;

		// delete the dynamic stuff
		for (unsigned i = 0; i < ffloors.Size(); i++)
		{
			F3DFloor * rover = ffloors[i];

			if (rover->flags&FF_DYNAMIC)
			{
				delete rover;
				ffloors.Delete(i);
				i--;
				continue;
			}
			if (rover->flags&FF_CLIPPED)
			{
				rover->flags &= ~FF_CLIPPED;
				rover->flags |= FF_EXISTS;
			}
		}
	}
}

//==========================================================================
//
// recalculates 3D floors for all attached sectors
//
//==========================================================================

void P_RecalculateAttached3DFloors(sector_t * sec)
{
	extsector_t::xfloor &x = sec->e->XFloor;

	for(unsigned int i=0; i<x.attached.Size(); i++)
	{
		P_Recalculate3DFloors(x.attached[i]);
	}
	P_Recalculate3DFloors(sec);
}

//==========================================================================
//
// recalculates light lists for this sector
//
//==========================================================================

void P_RecalculateLights(sector_t *sector)
{
	TArray<lightlist_t> &lightlist = sector->e->XFloor.lightlist;

	for(unsigned i = 0; i < lightlist.Size(); i++)
	{
		lightlist_t *ll = &lightlist[i];
		if (ll->lightsource != NULL)
		{
			ll->lightsource->UpdateColormap(ll->extra_colormap);
			ll->blend = ll->lightsource->GetBlend();
		}
		else
		{
			ll->extra_colormap = sector->Colormap;
			ll->blend = 0;
		}
	}
}

//==========================================================================
//
// recalculates light lists for all attached sectors
//
//==========================================================================

void P_RecalculateAttachedLights(sector_t *sector)
{
	extsector_t::xfloor &x = sector->e->XFloor;

	for(unsigned int i=0; i<x.attached.Size(); i++)
	{
		P_RecalculateLights(x.attached[i]);
	}
	P_RecalculateLights(sector);
}

//==========================================================================
//
//
//
//==========================================================================

lightlist_t * P_GetPlaneLight(sector_t * sector, secplane_t * plane, bool underside)
{
	unsigned   i;
	TArray<lightlist_t> &lightlist = sector->e->XFloor.lightlist;

	double planeheight=plane->ZatPoint(sector->centerspot);
	if(underside) planeheight-= EQUAL_EPSILON;
	
	for(i = 1; i < lightlist.Size(); i++)
		if (lightlist[i].plane.ZatPoint(sector->centerspot) <= planeheight) 
			return &lightlist[i - 1];
		
	return &lightlist[lightlist.Size() - 1];
}

//==========================================================================
//
// Extended P_LineOpening
//
//==========================================================================

void P_LineOpening_XFloors (FLineOpening &open, AActor * thing, const line_t *linedef, 
	double x, double y, bool restrict)
{
    if(thing)
    {
		double thingbot, thingtop;
		
		thingbot = thing->Z();
		thingtop = thing->Top();
		

		extsector_t::xfloor *xf[2] = {&linedef->frontsector->e->XFloor, &linedef->backsector->e->XFloor};

		// Check for 3D-floors in the sector (mostly identical to what Legacy does here)
		if(xf[0]->ffloors.Size() || xf[1]->ffloors.Size())
		{
			double    lowestceiling = open.top;
			double    highestfloor = open.bottom;
			double    lowestfloor[2] = {
				linedef->frontsector->floorplane.ZatPoint(x, y), 
				linedef->backsector->floorplane.ZatPoint(x, y) };
			bool lowestfloorset[2] = { false, false };
			FTextureID highestfloorpic;
			int highestfloorterrain = -1;
			FTextureID lowestceilingpic;
			sector_t *lowestceilingsec = nullptr, *highestfloorsec = nullptr;
			secplane_t *highestfloorplanes[2] = { &open.frontfloorplane, &open.backfloorplane };
			F3DFloor *lowestceilingffloor = nullptr;
			F3DFloor *highestfloorffloor = nullptr;
			
			highestfloorpic.SetInvalid();
			lowestceilingpic.SetInvalid();
			
			for(int j=0;j<2;j++)
			{
				for(unsigned i=0;i<xf[j]->ffloors.Size();i++)
				{
					F3DFloor *rover = xf[j]->ffloors[i];

					if (!(rover->flags & FF_EXISTS)) continue;
					if (!(rover->flags & FF_SOLID)) continue;
					
					double ff_bottom=rover->bottom.plane->ZatPoint(x, y);
					double ff_top=rover->top.plane->ZatPoint(x, y);
					
					double delta1 = fabs(thingbot - ((ff_bottom + ff_top) / 2));
					double delta2 = fabs(thingtop - ((ff_bottom + ff_top) / 2));
					
					if(ff_bottom < lowestceiling && delta1 > delta2) 
					{
						lowestceiling = ff_bottom;
						lowestceilingpic = *rover->bottom.texture;
						lowestceilingsec = j == 0 ? linedef->frontsector : linedef->backsector;
						lowestceilingffloor = rover;
					}
					
					if(delta1 <= delta2 && (!restrict || thing->Z() >= ff_top))
					{
						if (ff_top > highestfloor)
						{
							highestfloor = ff_top;
							highestfloorpic = *rover->top.texture;
							highestfloorterrain = rover->model->GetTerrain(rover->top.isceiling);
							highestfloorsec = j == 0 ? linedef->frontsector : linedef->backsector;
							highestfloorffloor = rover;
						}
						if (ff_top > highestfloorplanes[j]->ZatPoint(x, y))
						{
							highestfloorplanes[j] = rover->top.plane;
						}
					}
					if (ff_top > lowestfloor[j] && ff_top <= thing->Z() + thing->MaxStepHeight)
					{
						lowestfloor[j] = ff_top;
						lowestfloorset[j] = true;
					}
				}
			}
			
			if(highestfloor > open.bottom)
			{
				open.bottom = highestfloor;
				open.floorpic = highestfloorpic;
				open.floorterrain = highestfloorterrain;
				open.bottomsec = highestfloorsec;
				open.bottomffloor = highestfloorffloor;
			}
			if (highestfloorplanes[0] != &open.frontfloorplane)
			{
				open.frontfloorplane = *highestfloorplanes[0];
				if (open.frontfloorplane.fC() < 0) open.frontfloorplane.FlipVert();
			}
			if (highestfloorplanes[1] != &open.backfloorplane)
			{
				open.backfloorplane = *highestfloorplanes[1];
				if (open.backfloorplane.fC() < 0) open.backfloorplane.FlipVert();
			}

			if(lowestceiling < open.top) 
			{
				open.top = lowestceiling;
				open.ceilingpic = lowestceilingpic;
				open.topsec = lowestceilingsec;
				open.topffloor = lowestceilingffloor;
			}

			// Don't overwrite still valid info from portals here.
			if ((open.lowfloorthroughportal & 1) && lowestfloorset[0]) open.lowfloorthroughportal &= ~1;
			if ((open.lowfloorthroughportal & 2) && lowestfloorset[1]) open.lowfloorthroughportal &= ~2;

			double low1 = (open.lowfloorthroughportal & 1) ? open.lowfloor : lowestfloor[0];
			double low2 = (open.lowfloorthroughportal & 2) ? open.lowfloor : lowestfloor[1];
			open.lowfloor = MIN(low1, low2);
		}
    }
}


//==========================================================================
//
// Returns a 3D floorplane appropriate for the given coordinates
//
//==========================================================================

secplane_t P_FindFloorPlane(sector_t * sector, const DVector3 &pos)
{
	secplane_t retplane = sector->floorplane;
	if (sector->e)	// apparently this can be called when the data is already gone
	{
		for(auto rover : sector->e->XFloor.ffloors)
		{
			if(!(rover->flags & FF_SOLID) || !(rover->flags & FF_EXISTS)) continue;

			if (rover->top.plane->ZatPoint(pos) == pos.Z)
			{
				retplane = *rover->top.plane;
				if (retplane.fC() < 0) retplane.FlipVert();
				break;
			}
		}
	}
	return retplane;
}

//==========================================================================
//
// Gives the index to an extra floor above or below the given location.
// -1 means normal floor or ceiling
//
//==========================================================================

int	P_Find3DFloor(sector_t * sec, const DVector3 &pos, bool above, bool floor, double &cmpz)
{
	// sector must be given
	if (sec == nullptr) return -1;

	// Above normal ceiling
	cmpz = sec->ceilingplane.ZatPoint(pos);
	if (pos.Z >= cmpz)
		return -1;

	// Below normal floor
	cmpz = sec->floorplane.ZatPoint(pos);
	if (pos.Z <= cmpz)
		return -1;

	// Looking through planes from top to bottom
	for (int i = 0; i < (signed)sec->e->XFloor.ffloors.Size(); ++i)
	{
		F3DFloor *rover = sec->e->XFloor.ffloors[i];

		// We are only interested in solid 3D floors here
		if(!(rover->flags & FF_SOLID) || !(rover->flags & FF_EXISTS)) continue;

		if (above)
		{
			// z is above that floor
			if (floor && (pos.Z >= (cmpz = rover->top.plane->ZatPoint(pos))))
				return i - 1;
			// z is above that ceiling
			if (pos.Z >= (cmpz = rover->bottom.plane->ZatPoint(pos)))
				return i - 1;
		}
		else // below
		{
			// z is below that ceiling
			if (!floor && (pos.Z <= (cmpz = rover->bottom.plane->ZatPoint(pos))))
				return i;
			// z is below that floor
			if (pos.Z <= (cmpz = rover->top.plane->ZatPoint(pos)))
				return i;
		}
	}

	// Failsafe
	return -1;
}

