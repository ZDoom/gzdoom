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


CUSTOM_CVAR (Bool, gl_lights, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG | CVAR_NOINITCALL)
{
	if (self) AActor::RecreateAllAttachedLights();
	else AActor::DeleteAllAttachedLights();
}

CVAR (Bool, gl_attachedlights, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG);

//==========================================================================
//
//==========================================================================
DEFINE_CLASS_PROPERTY(type, S, DynamicLight)
{
	PROP_STRING_PARM(str, 0);
	static const char * ltype_names[]={
		"Point","Pulse","Flicker","Sector","RandomFlicker", "ColorPulse", "ColorFlicker", "RandomColorFlicker", nullptr};

	static const int ltype_values[]={
		PointLight, PulseLight, FlickerLight, SectorLight, RandomFlickerLight, ColorPulseLight, ColorFlickerLight, RandomColorFlickerLight };

	int style = MatchString(str, ltype_names);
	if (style < 0) I_Error("Unknown light type '%s'", str);
	defaults->lighttype = ltype_values[style];
}

//==========================================================================
//
// Actor classes
//
// For flexibility all functionality has been packed into a single class
// which is controlled by flags
//
//==========================================================================
IMPLEMENT_CLASS(ADynamicLight, false, false)

DEFINE_FIELD(ADynamicLight, SpotInnerAngle)
DEFINE_FIELD(ADynamicLight, SpotOuterAngle)

static FRandom randLight;

//==========================================================================
//
// Base class
//
//==========================================================================

//==========================================================================
//
//
//
//==========================================================================
void ADynamicLight::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	auto def = static_cast<ADynamicLight*>(GetDefault());
	arc("lightflags", lightflags, def->lightflags)
		("lighttype", lighttype, def->lighttype)
		("tickcount", m_tickCount, def->m_tickCount)
		("currentradius", m_currentRadius, def->m_currentRadius)
		("spotinnerangle", SpotInnerAngle, def->SpotInnerAngle)
		("spotouterangle", SpotOuterAngle, def->SpotOuterAngle);

	if (lighttype == PulseLight)
		arc("lastupdate", m_lastUpdate, def->m_lastUpdate)
			("cycler", m_cycler, def->m_cycler);

	// Remap the old flags.
	if (SaveVersion < 4552)
	{
		lightflags = 0;
		if (flags4 & MF4_MISSILEEVENMORE) lightflags |= LF_SUBTRACTIVE;
		if (flags4 & MF4_MISSILEMORE) lightflags |= LF_ADDITIVE;
		if (flags4 & MF4_SEESDAGGERS) lightflags |= LF_DONTLIGHTSELF;
		if (flags4 & MF4_INCOMBAT) lightflags |= LF_ATTENUATE;
		if (flags4 & MF4_STANDSTILL) lightflags |= LF_NOSHADOWMAP;
		if (flags4 & MF4_EXTREMEDEATH) lightflags |= LF_DONTLIGHTACTORS;
		flags4 &= ~(MF4_SEESDAGGERS);	// this flag is dangerous and must be cleared. The others do not matter.
	}
}


void ADynamicLight::PostSerialize()
{
	Super::PostSerialize();
	// The default constructor which is used for creating objects before deserialization will not set this variable.
	// It needs to be true for all placed lights.
	visibletoplayer = true;
	mShadowmapIndex = 1024;
	LinkLight();
}

//==========================================================================
//
// [TS]
//
//==========================================================================
void ADynamicLight::BeginPlay()
{
	//Super::BeginPlay();
	ChangeStatNum(STAT_DLIGHT);

	specialf1 = DAngle(double(SpawnAngle)).Normalized360().Degrees;
	visibletoplayer = true;
	mShadowmapIndex = 1024;
}

//==========================================================================
//
// [TS]
//
//==========================================================================
void ADynamicLight::PostBeginPlay()
{
	Super::PostBeginPlay();
	
	if (!(SpawnFlags & MTF_DORMANT))
	{
		Activate (nullptr);
	}

	subsector = R_PointInSubsector(Pos());
}


//==========================================================================
//
// [TS]
//
//==========================================================================
void ADynamicLight::Activate(AActor *activator)
{
	//Super::Activate(activator);
	flags2&=~MF2_DORMANT;	

	m_currentRadius = float(args[LIGHT_INTENSITY]);
	m_tickCount = 0;

	if (lighttype == PulseLight)
	{
		float pulseTime = float(specialf1 / TICRATE);
		
		m_lastUpdate = level.maptime;
		if (!swapped) m_cycler.SetParams(float(args[LIGHT_SECONDARY_INTENSITY]), float(args[LIGHT_INTENSITY]), pulseTime);
		else m_cycler.SetParams(float(args[LIGHT_INTENSITY]), float(args[LIGHT_SECONDARY_INTENSITY]), pulseTime);
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
void ADynamicLight::Deactivate(AActor *activator)
{
	//Super::Deactivate(activator);
	flags2|=MF2_DORMANT;	
}


//==========================================================================
//
// [TS]
//
//==========================================================================
void ADynamicLight::Tick()
{
	if (IsOwned())
	{
		if (!target || !target->state)
		{
			this->Destroy();
			return;
		}
		if (target->flags & MF_UNMORPHED) return;
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
		float diff = (level.maptime - m_lastUpdate) / (float)TICRATE;
		
		m_lastUpdate = level.maptime;
		m_cycler.Update(diff);
		m_currentRadius = float(m_cycler.GetVal());
		break;
	}

	case FlickerLight:
	{
		int rnd = randLight();
		float pct = float(specialf1 / 360.f);
		
		m_currentRadius = float(args[LIGHT_INTENSITY + (rnd >= pct * 255)]);
		break;
	}

	case RandomFlickerLight:
	{
		int flickerRange = args[LIGHT_SECONDARY_INTENSITY] - args[LIGHT_INTENSITY];
		float amt = randLight() / 255.f;
		
		if (m_tickCount > specialf1)
		{
			m_tickCount = 0;
		}
		if (m_tickCount++ == 0 || m_currentRadius > args[LIGHT_SECONDARY_INTENSITY])
		{
			m_currentRadius = float(args[LIGHT_INTENSITY] + (amt * flickerRange));
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
		int flickerRange = args[LIGHT_SECONDARY_INTENSITY] - args[LIGHT_INTENSITY];
		float amt = randLight() / 255.f;
		
		m_tickCount++;
		
		if (m_tickCount > specialf1)
		{
			m_currentRadius = args[LIGHT_INTENSITY] + (amt * flickerRange);
			m_tickCount = 0;
		}
		break;
	}
#endif

	case SectorLight:
	{
		float intensity;
		float scale = args[LIGHT_SCALE] / 8.f;
		
		if (scale == 0.f) scale = 1.f;
		
		intensity = Sector->lightlevel * scale;
		intensity = clamp<float>(intensity, 0.f, 255.f);
		
		m_currentRadius = intensity;
		break;
	}

	case PointLight:
		m_currentRadius = float(args[LIGHT_INTENSITY]);
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
void ADynamicLight::UpdateLocation()
{
	double oldx= X();
	double oldy= Y();
	double oldradius= radius;
	float intensity;

	if (IsActive())
	{
		if (target)
		{
			DAngle angle = target->Angles.Yaw;
			double s = angle.Sin();
			double c = angle.Cos();

			DVector3 pos = target->Vec3Offset(m_off.X * c + m_off.Y * s, m_off.X * s - m_off.Y * c, m_off.Z + target->GetBobOffset());
			SetXYZ(pos); // attached lights do not need to go into the regular blockmap
			Prev = target->Pos();
			subsector = R_PointInSubsector(Prev);
			Sector = subsector->sector;

			// Some z-coordinate fudging to prevent the light from getting too close to the floor or ceiling planes. With proper attenuation this would render them invisible.
			// A distance of 5 is needed so that the light's effect doesn't become too small.
			if (Z() < target->floorz + 5.) 	SetZ(target->floorz + 5.);
			else if (Z() > target->ceilingz - 5.) 	SetZ(target->ceilingz - 5.);
		}
		else
		{
			if (Z() < floorz + 5.) 	SetZ(floorz + 5.);
			else if (Z() > ceilingz - 5.) 	SetZ(ceilingz - 5.);
		}


		// The radius being used here is always the maximum possible with the
		// current settings. This avoids constant relinking of flickering lights

		if (lighttype == FlickerLight || lighttype == RandomFlickerLight || lighttype == PulseLight)
		{
			intensity = float(MAX(args[LIGHT_INTENSITY], args[LIGHT_SECONDARY_INTENSITY]));
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


//==========================================================================
//
//
//
//==========================================================================

void ADynamicLight::SetOrigin(double x, double y, double z, bool moving)
{
	Super::SetOrigin(x, y, z, moving);
	LinkLight();
}

//==========================================================================
//
//
//
//==========================================================================

void ADynamicLight::SetOffset(const DVector3 &pos)
{
	m_off = pos;
	UpdateLocation();
}

static void SetOffset(ADynamicLight *self, const DVector3 &pos)
{
	self->SetOffset(pos);
}

DEFINE_ACTION_FUNCTION_NATIVE(ADynamicLight, SetOffset, SetOffset)
{
	PARAM_SELF_PROLOGUE(ADynamicLight)
	PARAM_FLOAT(x)
	PARAM_FLOAT(y)
	PARAM_FLOAT(z)
	self->SetOffset(DVector3(x, y, z));
	return 0;
}

//==========================================================================
//
// The target pointer in dynamic lights should never be substituted unless 
// notOld is nullptr (which indicates that the object was destroyed by force.)
//
//==========================================================================
size_t ADynamicLight::PointerSubstitution (DObject *old, DObject *notOld)
{
	AActor *saved_target = target;
	size_t ret = Super::PointerSubstitution(old, notOld);
	if (notOld != nullptr) target = saved_target;
	return ret;
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

FLightNode * AddLightNode(FLightNode ** thread, void * linkto, ADynamicLight * light, FLightNode *& nextnode)
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
}                             // phares 3/13/98



//==========================================================================
//
// Gets the light's distance to a line
//
//==========================================================================

double ADynamicLight::DistToSeg(const DVector3 &pos, vertex_t *start, vertex_t *end)
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

void ADynamicLight::CollectWithinRadius(const DVector3 &opos, FSection *section, float radius)
{
	if (!section) return;
	collected_ss.Clear();
	collected_ss.Push({ section, opos });
	section->validcount = dl_validcount;

	bool hitonesidedback = false;
	for (unsigned i = 0; i < collected_ss.Size(); i++)
	{
		auto &pos = collected_ss[i].pos;
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
						subsector_t *othersub = R_PointInSubsector(other->v1->fPos() + other->Delta() / 2);
						FSection *othersect = othersub->section;
						if (othersect->validcount != ::validcount)
						{
							othersect->validcount = ::validcount;
							collected_ss.Push({ othersect, PosRelative(other) });
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
				subsector_t *othersub = R_PointInSubsector(refpos);
				FSection *othersect = othersub->section;
				if (othersect->validcount != dl_validcount)
				{
					othersect->validcount = dl_validcount;
					collected_ss.Push({ othersect, PosRelative(othersub->sector) });
				}
			}
		}
		if (!sec->PortalBlocksSight(sector_t::floor))
		{
			line_t *other = section->segments[0].sidedef->linedef;
			if (sec->GetPortalPlaneZ(sector_t::floor) > Z() - radius)
			{
				DVector2 refpos = other->v1->fPos() + other->Delta() / 2 + sec->GetPortalDisplacement(sector_t::floor);
				subsector_t *othersub = R_PointInSubsector(refpos);
				FSection *othersect = othersub->section;
				if (othersect->validcount != dl_validcount)
				{
					othersect->validcount = dl_validcount;
					collected_ss.Push({ othersect, PosRelative(othersub->sector) });
				}
			}
		}
	}
	shadowmapped = hitonesidedback && !(lightflags & LF_NOSHADOWMAP);
}

//==========================================================================
//
// Link the light into the world
//
//==========================================================================

void ADynamicLight::LinkLight()
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
		FSection *sect = R_PointInSubsector(Pos())->section;

		dl_validcount++;
		::validcount++;
		CollectWithinRadius(Pos(), sect, float(radius*radius));

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
void ADynamicLight::UnlinkLight ()
{
	if (owned && target != nullptr)
	{
		// Delete reference in owning actor
		for(int c=target->AttachedLights.Size()-1; c>=0; c--)
		{
			if (target->AttachedLights[c] == this)
			{
				target->AttachedLights.Delete(c);
				break;
			}
		}
	}
	while (touching_sides) touching_sides = DeleteLightNode(touching_sides);
	while (touching_sector) touching_sector = DeleteLightNode(touching_sector);
	shadowmapped = false;
}

void ADynamicLight::OnDestroy()
{
	UnlinkLight();
	Super::OnDestroy();
}


//==========================================================================
//
//
//
//==========================================================================

void AActor::AttachLight(unsigned int count, const FLightDefaults *lightdef)
{
	ADynamicLight *light;

	if (count < AttachedLights.Size()) 
	{
		light = barrier_cast<ADynamicLight*>(AttachedLights[count]);
		assert(light != nullptr);
	}
	else
	{
		light = Spawn<ADynamicLight>(Pos(), NO_REPLACE);
		light->target = this;
		light->owned = true;
		light->ObjectFlags |= OF_Transient;
		//light->lightflags |= LF_ATTENUATE;
		AttachedLights.Push(light);
	}
	light->flags2&=~MF2_DORMANT;
	lightdef->ApplyProperties(light);
}

//==========================================================================
//
// per-state light adjustment
//
//==========================================================================
extern TArray<FLightDefaults *> StateLights;

void AActor::SetDynamicLights()
{
	TArray<FInternalLightAssociation *> & LightAssociations = GetInfo()->LightAssociations;
	unsigned int count = 0;

	if (state == nullptr) return;
	if (LightAssociations.Size() > 0)
	{
		ADynamicLight *lights, *tmpLight;
		unsigned int i;

		lights = tmpLight = nullptr;

		for (i = 0; i < LightAssociations.Size(); i++)
		{
			if (LightAssociations[i]->Sprite() == sprite &&
				(LightAssociations[i]->Frame()==frame || LightAssociations[i]->Frame()==-1))
			{
				AttachLight(count++, LightAssociations[i]->Light());
			}
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
		AttachedLights[count]->flags2 |= MF2_DORMANT;
		memset(AttachedLights[count]->args, 0, 3*sizeof(args[0]));
	}
}

//==========================================================================
//
// Needed for garbage collection
//
//==========================================================================

size_t AActor::PropagateMark()
{
	for (unsigned i = 0; i<AttachedLights.Size(); i++)
	{
		GC::Mark(AttachedLights[i]);
	}
	return Super::PropagateMark();
}

//==========================================================================
//
// This is called before saving the game
//
//==========================================================================

void AActor::DeleteAllAttachedLights()
{
	TThinkerIterator<AActor> it;
	AActor * a;
	ADynamicLight * l;

	while ((a=it.Next())) 
	{
		a->AttachedLights.Clear();
	}

	TThinkerIterator<ADynamicLight> it2;

	l=it2.Next();
	while (l) 
	{
		ADynamicLight * ll = it2.Next();
		if (l->owned) l->Destroy();
		l=ll;
	}
}

//==========================================================================
//
//
//
//==========================================================================

void AActor::RecreateAllAttachedLights()
{
	TThinkerIterator<AActor> it;
	AActor * a;

	while ((a=it.Next())) 
	{
		a->SetDynamicLights();
	}
}

//==========================================================================
//
// CCMDs
//
//==========================================================================

CCMD(listlights)
{
	int walls, sectors;
	int allwalls=0, allsectors=0, allsubsecs = 0;
	int i=0, shadowcount = 0;
	ADynamicLight * dl;
	TThinkerIterator<ADynamicLight> it;

	while ((dl=it.Next()))
	{
		walls=0;
		sectors=0;
		Printf("%s at (%f, %f, %f), color = 0x%02x%02x%02x, radius = %f %s %s",
			dl->target? dl->target->GetClass()->TypeName.GetChars() : dl->GetClass()->TypeName.GetChars(),
			dl->X(), dl->Y(), dl->Z(), dl->args[LIGHT_RED], 
			dl->args[LIGHT_GREEN], dl->args[LIGHT_BLUE], dl->radius, (dl->lightflags & LF_ATTENUATE)? "attenuated" : "", dl->shadowmapped? "shadowmapped" : "");
		i++;
		shadowcount += dl->shadowmapped;

		if (dl->target)
		{
			FTextureID spr = sprites[dl->target->sprite].GetSpriteFrame(dl->target->frame, 0, 0., nullptr);
			Printf(", frame = %s ", TexMan.GetTexture(spr)->GetName().GetChars());
		}


		FLightNode * node;

		node=dl->touching_sides;

		while (node)
		{
			walls++;
			allwalls++;
			node = node->nextTarget;
		}


		node = dl->touching_sector;

		while (node)
		{
			allsectors++;
			sectors++;
			node = node->nextTarget;
		}
		Printf("- %d walls, %d sectors\n", walls, sectors);

	}
	Printf("%i dynamic lights, %d shadowmapped, %d walls, %d sectors\n\n\n", i, shadowcount, allwalls, allsectors);
}


