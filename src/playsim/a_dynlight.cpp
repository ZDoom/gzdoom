// 
//---------------------------------------------------------------------------
//
// Copyright(C) 2004-2016 Christoph Oelckers
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
** a_dynlight.cpp
** Implements actors representing dynamic lights (hardware independent)
**
**
** all functions marked with [TS] are licensed under
**---------------------------------------------------------------------------
** Copyright 2003 Timothy Stump
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

#include "c_dispatch.h"
#include "thingdef.h"
#include "r_utility.h"
#include "doomstat.h"
#include "serializer.h"
#include "g_levellocals.h"
#include "a_dynlight.h"
#include "actorinlines.h"
#include "memarena.h"

static FMemArena DynLightArena(sizeof(FDynamicLight) * 200);
static TArray<FDynamicLight*> FreeList;
static FRandom randLight;

extern TArray<FLightDefaults *> StateLights;


//==========================================================================
//
//
//
//==========================================================================

static FDynamicLight *GetLight(FLevelLocals *Level)
{
	FDynamicLight *ret;
	if (FreeList.Size())
	{
		FreeList.Pop(ret);
	}
	else ret = (FDynamicLight*)DynLightArena.Alloc(sizeof(FDynamicLight));
	memset(ret, 0, sizeof(*ret));
	ret->m_cycler.m_increment = true;
	ret->next = Level->lights;
	Level->lights = ret;
	if (ret->next) ret->next->prev = ret;
	ret->visibletoplayer = true;
	ret->mShadowmapIndex = 1024;
	ret->Level = Level;
	ret->Pos.X = -10000000;	// not a valid coordinate.
	return ret;
}


//==========================================================================
//
// Attaches a dynamic light descriptor to a dynamic light actor.
// Owned lights do not use this function.
//
//==========================================================================

void AttachLight(AActor *self)
{
	auto light = GetLight(self->Level);

	light->pSpotInnerAngle = &self->AngleVar(NAME_SpotInnerAngle);
	light->pSpotOuterAngle = &self->AngleVar(NAME_SpotOuterAngle);
	light->pPitch = &self->Angles.Pitch;
	light->pLightFlags = (LightFlags*)&self->IntVar(NAME_lightflags);
	light->pArgs = self->args;
	light->specialf1 = DAngle(double(self->SpawnAngle)).Normalized360().Degrees;
	light->Sector = self->Sector;
	light->target = self;
	light->mShadowmapIndex = 1024;
	light->m_active = false;
	light->visibletoplayer = true;
	light->lighttype = (uint8_t)self->IntVar(NAME_lighttype);
	self->AttachedLights.Push(light);

	// Disable postponed processing of dynamic light because its setup has been completed by this function
	self->flags8 &= ~MF8_RECREATELIGHTS;
}

DEFINE_ACTION_FUNCTION_NATIVE(ADynamicLight, AttachLight, AttachLight)
{
	PARAM_SELF_PROLOGUE(AActor);
	AttachLight(self);
	return 0;
}

//==========================================================================
//
//
//
//==========================================================================

void ActivateLight(AActor *self)
{
	for (auto l : self->AttachedLights) l->Activate();
}

DEFINE_ACTION_FUNCTION_NATIVE(ADynamicLight, ActivateLight, ActivateLight)
{
	PARAM_SELF_PROLOGUE(AActor);
	ActivateLight(self);
	return 0;
}


//==========================================================================
//
//
//
//==========================================================================

void DeactivateLight(AActor *self)
{
	for (auto l : self->AttachedLights) l->Deactivate();
}

DEFINE_ACTION_FUNCTION_NATIVE(ADynamicLight, DeactivateLight, DeactivateLight)
{
	PARAM_SELF_PROLOGUE(AActor);
	DeactivateLight(self);
	return 0;
}

//==========================================================================
//
//
//
//==========================================================================

static void SetOffset(AActor *self, double x, double y, double z)
{
	for (auto l : self->AttachedLights)
	{
		l->SetOffset(DVector3(x, y, z));
	}
}

DEFINE_ACTION_FUNCTION_NATIVE(ADynamicLight, SetOffset, SetOffset)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_FLOAT(x);
	PARAM_FLOAT(y);
	PARAM_FLOAT(z);
	SetOffset(self, x, y, z);
	return 0;
}

//==========================================================================
//
//
//
//==========================================================================

void FDynamicLight::ReleaseLight()
{
	assert(prev != nullptr || this == Level->lights);
	if (prev != nullptr) prev->next = next;
	else Level->lights = next;
	if (next != nullptr) next->prev = prev;
	next = prev = nullptr;
	FreeList.Push(this);
}


//==========================================================================
//
// [TS]
//
//==========================================================================
void FDynamicLight::Activate()
{
	m_active = true;
	m_currentRadius = float(GetIntensity());
	m_tickCount = 0;

	if (lighttype == PulseLight)
	{
		float pulseTime = float(specialf1 / TICRATE);

		m_lastUpdate = Level->maptime;
		if (!swapped) m_cycler.SetParams(float(GetSecondaryIntensity()), float(GetIntensity()), pulseTime);
		else m_cycler.SetParams(float(GetIntensity()), float(GetSecondaryIntensity()), pulseTime);
		m_cycler.ShouldCycle(true);
		m_cycler.SetCycleType(CYCLE_Sin);
		m_currentRadius = float(m_cycler.GetVal());
	}
	if (m_currentRadius <= 0) m_currentRadius = 1;
}


//==========================================================================
//
// [TS]
//
//==========================================================================
void FDynamicLight::Tick()
{
	if (!target)
	{
		// How did we get here? :?
		ReleaseLight();
		return;
	}

	if (owned)
	{
		if (!target->state)
		{
			Deactivate();
			return;
		}
		if (target->flags & MF_UNMORPHED)
		{
			m_active = false;
			return;
		}
		visibletoplayer = target->IsVisibleToPlayer();	// cache this value for the renderer to speed up calculations.
	}

	// Don't bother if the light won't be shown
	if (!IsActive()) return;

	// I am doing this with a type field so that I can dynamically alter the type of light
	// without having to create or maintain multiple objects.
	switch(lighttype)
	{
	case PulseLight:
	{
		float diff = (Level->maptime - m_lastUpdate) / (float)TICRATE;
		
		m_lastUpdate = Level->maptime;
		m_cycler.Update(diff);
		m_currentRadius = float(m_cycler.GetVal());
		break;
	}

	case FlickerLight:
	{
		int rnd = randLight(360);
		m_currentRadius = float((rnd < int(specialf1))? GetIntensity() : GetSecondaryIntensity());
		break;
	}

	case RandomFlickerLight:
	{
		int flickerRange = GetSecondaryIntensity() - GetIntensity();
		float amt = randLight() / 255.f;
		
		if (m_tickCount > specialf1)
		{
			m_tickCount = 0;
		}
		if (m_tickCount++ == 0 || m_currentRadius > GetSecondaryIntensity())
		{
			m_currentRadius = float(GetIntensity() + (amt * flickerRange));
		}
		break;
	}

#if 0
	// These need some more work elsewhere
	case ColorFlickerLight:
	{
		int rnd = randLight();
		float pct = specialf1/360.f;
		
		m_currentRadius = m_Radius[rnd >= pct * 255];
		break;
	}

	case RandomColorFlickerLight:
	{
		int flickerRange = GetSecondaryIntensity() - GetIntensity();
		float amt = randLight() / 255.f;
		
		m_tickCount++;
		
		if (m_tickCount > specialf1)
		{
			m_currentRadius = GetIntensity() + (amt * flickerRange);
			m_tickCount = 0;
		}
		break;
	}
#endif

	case SectorLight:
	{
		float intensity;
		float scale = GetIntensity() / 8.f;
		
		if (scale == 0.f) scale = 1.f;
		
		intensity = Sector? Sector->lightlevel * scale : 0;
		intensity = clamp<float>(intensity, 0.f, 255.f);
		
		m_currentRadius = intensity;
		break;
	}

	case PointLight:
		m_currentRadius = float(GetIntensity());
		break;
	}
	if (m_currentRadius <= 0) m_currentRadius = 1;
	UpdateLocation();
}




//==========================================================================
//
//
//
//==========================================================================
void FDynamicLight::UpdateLocation()
{
	double oldx= X();
	double oldy= Y();
	float oldradius = radius;

	if (IsActive())
	{
		AActor *target = this->target;	// perform the read barrier only once.

		// Offset is calculated in relation to the owning actor.
		DAngle angle = target->Angles.Yaw;
		double s = angle.Sin();
		double c = angle.Cos();

		Pos = target->Vec3Offset(m_off.X * c + m_off.Y * s, m_off.X * s - m_off.Y * c, m_off.Z + target->GetBobOffset());
		Sector = target->subsector->sector;	// Get the render sector. target->Sector is the sector according to play logic.

		// Some z-coordinate fudging to prevent the light from getting too close to the floor or ceiling planes. With proper attenuation this would render them invisible.
		// A distance of 5 is needed so that the light's effect doesn't become too small.
		if (Z() < target->floorz + 5.) Pos.Z = target->floorz + 5.;
		else if (Z() > target->ceilingz - 5.) Pos.Z = target->ceilingz - 5.;

		// The radius being used here is always the maximum possible with the
		// current settings. This avoids constant relinking of flickering lights

		float intensity;

		if (lighttype == FlickerLight || lighttype == RandomFlickerLight || lighttype == PulseLight)
		{
			intensity = float(MAX(GetIntensity(), GetSecondaryIntensity()));
		}
		else
		{
			intensity = m_currentRadius;
		}
		radius = intensity * 2.0f;
		if (radius < m_currentRadius * 2) radius = m_currentRadius * 2;

		if (X() != oldx || Y() != oldy || radius != oldradius)
		{
			//Update the light lists
			LinkLight();
		}
	}
}

//=============================================================================
//
// These have been copied from the secnode code and modified for the light links
//
// P_AddSecnode() searches the current list to see if this sector is
// already there. If not, it adds a sector node at the head of the list of
// sectors this object appears in. This is called when creating a list of
// nodes that will get linked in later. Returns a pointer to the new node.
//
//=============================================================================

FLightNode * AddLightNode(FLightNode ** thread, void * linkto, FDynamicLight * light, FLightNode *& nextnode)
{
	FLightNode * node;

	node = nextnode;
	while (node)
    {
		if (node->targ==linkto)   // Already have a node for this sector?
		{
			node->lightsource = light; // Yes. Setting m_thing says 'keep it'.
			return(nextnode);
		}
		node = node->nextTarget;
    }

	// Couldn't find an existing node for this sector. Add one at the head
	// of the list.
	
	node = new FLightNode;
	
	node->targ = linkto;
	node->lightsource = light; 

	node->prevTarget = &nextnode; 
	node->nextTarget = nextnode;

	if (nextnode) nextnode->prevTarget = &node->nextTarget;
	
	// Add new node at head of sector thread starting at s->touching_thinglist
	
	node->prevLight = thread;  	
	node->nextLight = *thread; 
	if (node->nextLight) node->nextLight->prevLight=&node->nextLight;
	*thread = node;
	return(node);
}


//=============================================================================
//
// P_DelSecnode() deletes a sector node from the list of
// sectors this object appears in. Returns a pointer to the next node
// on the linked list, or nullptr.
//
//=============================================================================

static FLightNode * DeleteLightNode(FLightNode * node)
{
	FLightNode * tn;  // next node on thing thread
	
	if (node)
    {
		
		*node->prevTarget = node->nextTarget;
		if (node->nextTarget) node->nextTarget->prevTarget=node->prevTarget;

		*node->prevLight = node->nextLight;
		if (node->nextLight) node->nextLight->prevLight=node->prevLight;
		
		// Return this node to the freelist
		tn=node->nextTarget;
		delete node;
		return(tn);
	}
	return(nullptr);
}



//==========================================================================
//
// Gets the light's distance to a line
//
//==========================================================================

double FDynamicLight::DistToSeg(const DVector3 &pos, vertex_t *start, vertex_t *end)
{
	double u, px, py;

	double seg_dx = end->fX() - start->fX();
	double seg_dy = end->fY() - start->fY();
	double seg_length_sq = seg_dx * seg_dx + seg_dy * seg_dy;

	u = (((pos.X - start->fX()) * seg_dx) + (pos.Y - start->fY()) * seg_dy) / seg_length_sq;
	if (u < 0.) u = 0.; // clamp the test point to the line segment
	else if (u > 1.) u = 1.;

	px = start->fX() + (u * seg_dx);
	py = start->fY() + (u * seg_dy);

	px -= pos.X;
	py -= pos.Y;

	return (px*px) + (py*py);
}


//==========================================================================
//
// Collect all touched sidedefs and subsectors
// to sidedefs and sector parts.
//
//==========================================================================
struct LightLinkEntry
{
	FSection *sect;
	DVector3 pos;
};
static TArray<LightLinkEntry> collected_ss;

void FDynamicLight::CollectWithinRadius(const DVector3 &opos, FSection *section, float radius)
{
	if (!section) return;
	collected_ss.Clear();
	collected_ss.Push({ section, opos });
	section->validcount = dl_validcount;

	bool hitonesidedback = false;
	for (unsigned i = 0; i < collected_ss.Size(); i++)
	{
		auto pos = collected_ss[i].pos;
		section = collected_ss[i].sect;

		touching_sector = AddLightNode(&section->lighthead, section, this, touching_sector);


		auto processSide = [&](side_t *sidedef, const vertex_t *v1, const vertex_t *v2)
		{
			auto linedef = sidedef->linedef;
			if (linedef && linedef->validcount != ::validcount)
			{
				// light is in front of the seg
				if ((pos.Y - v1->fY()) * (v2->fX() - v1->fX()) + (v1->fX() - pos.X) * (v2->fY() - v1->fY()) <= 0)
				{
					linedef->validcount = ::validcount;
					touching_sides = AddLightNode(&sidedef->lighthead, sidedef, this, touching_sides);
				}
				else if (linedef->sidedef[0] == sidedef && linedef->sidedef[1] == nullptr)
				{
					hitonesidedback = true;
				}
			}
			if (linedef)
			{
				FLinePortal *port = linedef->getPortal();
				if (port && port->mType == PORTT_LINKED)
				{
					line_t *other = port->mDestination;
					if (other->validcount != ::validcount)
					{
						subsector_t *othersub = Level->PointInRenderSubsector(other->v1->fPos() + other->Delta() / 2);
						FSection *othersect = othersub->section;
						if (othersect->validcount != ::validcount)
						{
							othersect->validcount = ::validcount;
							collected_ss.Push({ othersect, PosRelative(other->frontsector->PortalGroup) });
						}
					}
				}
			}
		};

		for (auto &segment : section->segments)
		{
			// check distance from x/y to seg and if within radius add this seg and, if present the opposing subsector (lather/rinse/repeat)
			// If out of range we do not need to bother with this seg.
			if (DistToSeg(pos, segment.start, segment.end) <= radius)
			{
				auto sidedef = segment.sidedef;
				if (sidedef)
				{
					processSide(sidedef, segment.start, segment.end);
				}

				auto partner = segment.partner;
				if (partner)
				{
					FSection *sect = partner->section;
					if (sect != nullptr && sect->validcount != dl_validcount)
					{
						sect->validcount = dl_validcount;
						collected_ss.Push({ sect, pos });
					}
				}
			}
		}
		for (auto side : section->sides)
		{
			auto v1 = side->V1(), v2 = side->V2();
			if (DistToSeg(pos, v1, v2) <= radius)
			{
				processSide(side, v1, v2);
			}
		}
		sector_t *sec = section->sector;
		if (!sec->PortalBlocksSight(sector_t::ceiling))
		{
			line_t *other = section->segments[0].sidedef->linedef;
			if (sec->GetPortalPlaneZ(sector_t::ceiling) < Z() + radius)
			{
				DVector2 refpos = other->v1->fPos() + other->Delta() / 2 + sec->GetPortalDisplacement(sector_t::ceiling);
				subsector_t *othersub = Level->PointInRenderSubsector(refpos);
				FSection *othersect = othersub->section;
				if (othersect->validcount != dl_validcount)
				{
					othersect->validcount = dl_validcount;
					collected_ss.Push({ othersect, PosRelative(othersub->sector->PortalGroup) });
				}
			}
		}
		if (!sec->PortalBlocksSight(sector_t::floor))
		{
			line_t *other = section->segments[0].sidedef->linedef;
			if (sec->GetPortalPlaneZ(sector_t::floor) > Z() - radius)
			{
				DVector2 refpos = other->v1->fPos() + other->Delta() / 2 + sec->GetPortalDisplacement(sector_t::floor);
				subsector_t *othersub = Level->PointInRenderSubsector(refpos);
				FSection *othersect = othersub->section;
				if (othersect->validcount != dl_validcount)
				{
					othersect->validcount = dl_validcount;
					collected_ss.Push({ othersect, PosRelative(othersub->sector->PortalGroup) });
				}
			}
		}
	}
	shadowmapped = hitonesidedback && !DontShadowmap();
}

//==========================================================================
//
// Link the light into the world
//
//==========================================================================

void FDynamicLight::LinkLight()
{
	// mark the old light nodes
	FLightNode * node;
	
	node = touching_sides;
	while (node)
    {
		node->lightsource = nullptr;
		node = node->nextTarget;
    }
	node = touching_sector;
	while (node)
	{
		node->lightsource = nullptr;
		node = node->nextTarget;
	}

	if (radius>0)
	{
		// passing in radius*radius allows us to do a distance check without any calls to sqrt
		FSection *sect = Level->PointInRenderSubsector(Pos)->section;

		dl_validcount++;
		::validcount++;
		CollectWithinRadius(Pos, sect, float(radius*radius));

	}
		
	// Now delete any nodes that won't be used. These are the ones where
	// m_thing is still nullptr.
	
	node = touching_sides;
	while (node)
	{
		if (node->lightsource == nullptr)
		{
			node = DeleteLightNode(node);
		}
		else
			node = node->nextTarget;
	}

	node = touching_sector;
	while (node)
	{
		if (node->lightsource == nullptr)
		{
			node = DeleteLightNode(node);
		}
		else
			node = node->nextTarget;
	}
}


//==========================================================================
//
// Deletes the link lists
//
//==========================================================================
void FDynamicLight::UnlinkLight ()
{
	while (touching_sides) touching_sides = DeleteLightNode(touching_sides);
	while (touching_sector) touching_sector = DeleteLightNode(touching_sector);
	shadowmapped = false;
}

//==========================================================================
//
//
//
//==========================================================================

void AActor::AttachLight(unsigned int count, const FLightDefaults *lightdef)
{
	FDynamicLight *light;

	if (count < AttachedLights.Size()) 
	{
		light = AttachedLights[count];
		assert(light != nullptr);
	}
	else
	{
		light = GetLight(Level);
		light->SetActor(this, true);
		AttachedLights.Push(light);
	}
	lightdef->ApplyProperties(light);
	light->UpdateLocation();
}

//==========================================================================
//
// per-state light adjustment
//
//==========================================================================

void AActor::SetDynamicLights()
{
	TArray<FInternalLightAssociation *> & LightAssociations = GetInfo()->LightAssociations;
	unsigned int count = 0;

	if (state == nullptr) return;

	for (const auto def : UserLights)
	{
		AttachLight(count++, def);
	}

	for (const auto asso : LightAssociations)
	{
		if (asso->Sprite() == sprite && (asso->Frame() == frame || asso->Frame() == -1))
		{
			AttachLight(count++, asso->Light());
		}
	}
	if (count == 0 && state->Light > 0)
	{
		for(int i= state->Light; StateLights[i] != nullptr; i++)
		{
			if (StateLights[i] != (FLightDefaults*)-1)
			{
				AttachLight(count++, StateLights[i]);
			}
		}
	}

	for(;count<AttachedLights.Size();count++)
	{
		AttachedLights[count]->Deactivate();
	}
}

//==========================================================================
//
//
//
//==========================================================================

void AActor::DeleteAttachedLights()
{
	for (auto l : AttachedLights)
	{
		l->UnlinkLight();
		l->ReleaseLight();
	}
	AttachedLights.Clear();
}

//==========================================================================
//
//
//
//==========================================================================
extern TDeletingArray<FLightDefaults *> LightDefaults;

unsigned FindUserLight(AActor *self, FName id, bool create = false)
{
	for (unsigned i = 0; i < self->UserLights.Size(); i++ )
	{
		if (self->UserLights[i]->GetName() == id) return i;
	}
	if (create)
	{
		auto li = new FLightDefaults(id);
		return self->UserLights.Push(li);
	}
	return ~0u;
}

//==========================================================================
//
//
//
//==========================================================================

int AttachLightDef(AActor *self, int _lightid, int _lightname)
{
	FName lightid = FName(ENamedName(_lightid));
	FName lightname = FName(ENamedName(_lightname));
	
	// Todo: Optimize. This may be too slow.
	auto lightdef = LightDefaults.FindEx([=](const auto &a) {
		return a->GetName() == lightname;
	});
	if (lightdef < LightDefaults.Size())
	{
		auto userlight = self->UserLights[FindUserLight(self, lightid, true)];
		userlight->CopyFrom(*LightDefaults[lightdef]);
		self->flags8 |= MF8_RECREATELIGHTS;
		self->Level->flags3 |= LEVEL3_LIGHTCREATED;
		return 1;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(AActor, A_AttachLightDef, AttachLightDef)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_NAME(lightid);
	PARAM_NAME(lightname);
	ACTION_RETURN_BOOL(AttachLightDef(self, lightid.GetIndex(), lightname.GetIndex()));
}

//==========================================================================
//
//
//
//==========================================================================

int AttachLightDirect(AActor *self, int _lightid, int type, int color, int radius1, int radius2, int flags, double ofs_x, double ofs_y, double ofs_z, double param, double spoti, double spoto, double spotp)
{
	FName lightid = FName(ENamedName(_lightid));
	auto userlight = self->UserLights[FindUserLight(self, lightid, true)];
	userlight->SetType(ELightType(type));
	userlight->SetArg(LIGHT_RED, RPART(color));
	userlight->SetArg(LIGHT_GREEN, GPART(color));
	userlight->SetArg(LIGHT_BLUE, BPART(color));
	userlight->SetArg(LIGHT_INTENSITY, radius1);
	userlight->SetArg(LIGHT_SECONDARY_INTENSITY, radius2);
	userlight->SetFlags(LightFlags::FromInt(flags));
	float of[] = { float(ofs_x), float(ofs_z), float(ofs_y)};
	userlight->SetOffset(of);
	userlight->SetParameter(type == PulseLight? param*TICRATE : param*360.);
	userlight->SetSpotInnerAngle(spoti);
	userlight->SetSpotOuterAngle(spoto);
	if (spotp >= -90. && spotp <= 90.)
	{
		userlight->SetSpotPitch(spotp);
	}
	else
	{
		userlight->UnsetSpotPitch();
	}
	self->flags8 |= MF8_RECREATELIGHTS;
	self->Level->flags3 |= LEVEL3_LIGHTCREATED;
	return 1;
}

DEFINE_ACTION_FUNCTION_NATIVE(AActor, A_AttachLight, AttachLightDirect)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_NAME(lightid);
	PARAM_INT(type);
	PARAM_INT(color);
	PARAM_INT(radius1);
	PARAM_INT(radius2);
	PARAM_INT(flags);
	PARAM_FLOAT(ofs_x);
	PARAM_FLOAT(ofs_y);
	PARAM_FLOAT(ofs_z);
	PARAM_FLOAT(parami);
	PARAM_FLOAT(spoti);
	PARAM_FLOAT(spoto);
	PARAM_FLOAT(spotp);
	ACTION_RETURN_BOOL(AttachLightDirect(self, lightid.GetIndex(), type, color, radius1, radius2, flags, ofs_x, ofs_y, ofs_z, parami, spoti, spoto, spotp));
}

//==========================================================================
//
//
//
//==========================================================================

int RemoveLight(AActor *self, int _lightid)
{
	FName lightid = FName(ENamedName(_lightid));
	auto userlight = FindUserLight(self, lightid, false);
	if (userlight < self->UserLights.Size())
	{
		delete self->UserLights[userlight];
		self->UserLights.Delete(userlight);
		self->flags8 |= MF8_RECREATELIGHTS;
		self->Level->flags3 |= LEVEL3_LIGHTCREATED;
		return 1;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION_NATIVE(AActor, A_RemoveLight, RemoveLight)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_NAME(lightid);
	ACTION_RETURN_BOOL(RemoveLight(self, lightid.GetIndex()));
}


//==========================================================================
//
//
//
//==========================================================================

//==========================================================================
//
// This is called before saving the game
//
//==========================================================================

void FLevelLocals::DeleteAllAttachedLights()
{
	auto it = GetThinkerIterator<AActor>();
	AActor * a;

	while ((a=it.Next())) 
	{
		a->DeleteAttachedLights();
	}
}

//==========================================================================
//
//
//
//==========================================================================

void FLevelLocals::RecreateAllAttachedLights()
{
	auto it = GetThinkerIterator<AActor>();
	AActor * a;

	while ((a=it.Next())) 
	{
		if (!a->IsKindOf(NAME_DynamicLight))
		{
			a->SetDynamicLights();
		}
		else if (a->AttachedLights.Size() == 0)
		{
			::AttachLight(a);
			if (!(a->flags2 & MF2_DORMANT))
			{
				::ActivateLight(a);
			}
		}
	}
}
