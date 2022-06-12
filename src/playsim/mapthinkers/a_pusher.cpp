//-----------------------------------------------------------------------------
//
// Copyright 1998-1998 Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Initializes and implements BOOM linedef triggers for
//			Wind/Current
//
//-----------------------------------------------------------------------------


#include <stdlib.h>
#include "actor.h"
#include "p_spec.h"
#include "serializer.h"
#include "serialize_obj.h"
#include "p_lnspec.h"
#include "p_maputl.h"
#include "p_local.h"
#include "d_player.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "p_spec_thinkers.h"
#include "maploader/maploader.h"

EXTERN_CVAR(Bool, var_pushers);

IMPLEMENT_CLASS(DPusher, false, true)

IMPLEMENT_POINTERS_START(DPusher)
	IMPLEMENT_POINTER(m_Source)
IMPLEMENT_POINTERS_END

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void DPusher::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc.Enum("type", m_Type)
		("source", m_Source)
		("pushvec", m_PushVec)
		("magnitude", m_Magnitude)
		("radius", m_Radius)
		("affectee", m_Affectee);
}


//-----------------------------------------------------------------------------
//
// PUSH/PULL EFFECT
//
// phares 3/20/98: Start of push/pull effects
//
// This is where push/pull effects are applied to objects in the sectors.
//
// There are four kinds of push effects
//
// 1) Pushing Away
//
//    Pushes you away from a point source defined by the location of an
//    MT_PUSH Thing. The force decreases linearly with distance from the
//    source. This force crosses sector boundaries and is felt w/in a circle
//    whose center is at the MT_PUSH. The force is felt only if the point
//    MT_PUSH can see the target object.
//
// 2) Pulling toward
//
//    Same as Pushing Away except you're pulled toward an MT_PULL point
//    source. This force crosses sector boundaries and is felt w/in a circle
//    whose center is at the MT_PULL. The force is felt only if the point
//    MT_PULL can see the target object.
//
// 3) Wind
//
//    Pushes you in a constant direction. Full force above ground, half
//    force on the ground, nothing if you're below it (water).
//
// 4) Current
//
//    Pushes you in a constant direction. No force above ground, full
//    force if on the ground or below it (water).
//
// The magnitude of the force is controlled by the length of a controlling
// linedef. The force vector for types 3 & 4 is determined by the angle
// of the linedef, and is constant.
//
// For each sector where these effects occur, the sector special type has
// to have the PUSH_MASK bit set. If this bit is turned off by a switch
// at run-time, the effect will not occur. The controlling sector for
// types 1 & 2 is the sector containing the MT_PUSH/MT_PULL Thing.
//
//-----------------------------------------------------------------------------

//-----------------------------------------------------------------------------
//
// Add a push thinker to the thinker list
//
//-----------------------------------------------------------------------------

void DPusher::Construct (DPusher::EPusher type, line_t *l, int magnitude, int angle,
				  AActor *source, int affectee)
{
	m_Source = source;
	m_Type = type;
	if (l)
	{
		m_PushVec = l->Delta();
		m_Magnitude = m_PushVec.Length();
	}
	else
	{ // [RH] Allow setting magnitude and angle with parameters
		ChangeValues (magnitude, angle);
	}
	if (source) // point source exist?
	{
		m_Radius = m_Magnitude * 2; // where force goes to zero
	}
	m_Affectee = affectee;
}

int DPusher::CheckForSectorMatch (EPusher type, int tag)
{
	if (m_Type == type && Level->SectorHasTag(m_Affectee, tag))
		return m_Affectee;
	else
		return -1;
}


//-----------------------------------------------------------------------------
//
// T_Pusher looks for all objects that are inside the radius of
// the effect.
//
//-----------------------------------------------------------------------------

void DPusher::Tick ()
{
	sector_t *sec;
	AActor *thing;
	msecnode_t *node;
	double ht;

	if (!var_pushers)
		return;

	sec = &Level->sectors[m_Affectee];

	// Be sure the special sector type is still turned on. If so, proceed.
	// Else, bail out; the sector type has been changed on us.

	if (!(sec->Flags & SECF_PUSH))
		return;

	// For constant pushers (wind/current) there are 3 situations:
	//
	// 1) Affected Thing is above the floor.
	//
	//    Apply the full force if wind, no force if current.
	//
	// 2) Affected Thing is on the ground.
	//
	//    Apply half force if wind, full force if current.
	//
	// 3) Affected Thing is below the ground (underwater effect).
	//
	//    Apply no force if wind, full force if current.
	//
	// Apply the effect to clipped players only for now.
	//
	// In Phase II, you can apply these effects to Things other than players.
	// [RH] No Phase II, but it works with anything having MF2_WINDTHRUST now.

	if (m_Type == p_push)
	{
		if (m_Source == nullptr)
		{
			Destroy();
			return;
		}
		// Seek out all pushable things within the force radius of this
		// point pusher. Crosses sectors, so use blockmap.

		FPortalGroupArray check(FPortalGroupArray::PGA_NoSectorPortals);	// no sector portals because this thing is utterly z-unaware.
		FMultiBlockThingsIterator it(check, m_Source, m_Radius);
		FMultiBlockThingsIterator::CheckResult cres;


		while (it.Next(&cres))
		{
			AActor *thing = cres.thing;
			// Normal ZDoom is based only on the WINDTHRUST flag, with the noclip cheat as an exemption.
			bool pusharound = ((thing->flags2 & MF2_WINDTHRUST) && !(thing->flags & MF_NOCLIP));
					
			// MBF allows any sentient or shootable thing to be affected, but players with a fly cheat aren't.
			if (Level->i_compatflags & COMPATF_MBFMONSTERMOVE)
			{
				pusharound = ((pusharound || (thing->IsSentient()) || (thing->flags & MF_SHOOTABLE)) // Add categories here
					&& (!(thing->player && (thing->flags & (MF_NOGRAVITY))))); // Exclude flying players here
			}

			if ((pusharound) )
			{
				DVector2 pos = m_Source->Vec2To(thing);
				double dist = pos.Length();
				double speed = (m_Magnitude - (dist/2)) / (PUSH_FACTOR * 2);

				// If speed <= 0, you're outside the effective radius. You also have
				// to be able to see the push/pull source point.

				if ((speed > 0) && (P_CheckSight (thing, m_Source, SF_IGNOREVISIBILITY)))
				{
					DAngle pushangle = pos.Angle();
					if (m_Source->IsKindOf(NAME_PointPuller)) pushangle += 180;
					thing->Thrust(pushangle, speed);
				}
			}
		}
		return;
	}

	// constant pushers p_wind and p_current

	node = sec->touching_thinglist; // things touching this sector
	for ( ; node ; node = node->m_snext)
	{
		thing = node->m_thing;
		if (!(thing->flags2 & MF2_WINDTHRUST) || (thing->flags & MF_NOCLIP))
			continue;

		sector_t *hsec = sec->GetHeightSec();
		DVector3 pos = thing->PosRelative(sec);
		DVector2 pushvel;
		if (m_Type == p_wind)
		{
			if (hsec == NULL)
			{ // NOT special water sector
				if (thing->Z() > thing->floorz) // above ground
				{
					pushvel = m_PushVec; // full force
				}
				else // on ground
				{
					pushvel = m_PushVec / 2; // half force
				}
			}
			else // special water sector
			{
				ht = hsec->floorplane.ZatPoint(pos);
				if (thing->Z() > ht) // above ground
				{
					pushvel = m_PushVec; // full force
				}
				else if (thing->player && thing->player->viewz < ht) // underwater
				{
					pushvel.Zero(); // no force
				}
				else // wading in water
				{
					pushvel = m_PushVec / 2; // half force
				}
			}
		}
		else // p_current
		{
			const secplane_t *floor;

			if (hsec == NULL)
			{ // NOT special water sector
				floor = &sec->floorplane;
			}
			else
			{ // special water sector
				floor = &hsec->floorplane;
			}
			if (thing->Z() > floor->ZatPoint(pos))
			{ // above ground
				pushvel.Zero(); // no force
			}
			else
			{ // on ground/underwater
				pushvel = m_PushVec; // full force
			}
		}
		thing->Vel += pushvel / PUSH_FACTOR;
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FLevelLocals::AdjustPusher(int tag, int magnitude, int angle, bool wind)
{
	struct FThinkerCollection
	{
		int RefNum;
		DThinker *Obj;
	};

	DPusher::EPusher type = wind? DPusher::p_wind : DPusher::p_current;
	
	// Find pushers already attached to the sector, and change their parameters.
	TArray<FThinkerCollection> Collection;
	{
		auto iterator = GetThinkerIterator<DPusher>();
		FThinkerCollection collect;

		while ((collect.Obj = iterator.Next()))
		{
			if ((collect.RefNum = ((DPusher *)collect.Obj)->CheckForSectorMatch(type, tag)) >= 0)
			{
				((DPusher *)collect.Obj)->ChangeValues(magnitude, angle);
				Collection.Push(collect);
			}
		}
	}

	size_t numcollected = Collection.Size();
	int secnum;

	// Now create pushers for any sectors that don't already have them.
	auto itr = GetSectorTagIterator(tag);
	while ((secnum = itr.Next()) >= 0)
	{
		unsigned int i;
		for (i = 0; i < numcollected; i++)
		{
			if (Collection[i].RefNum == secnum)
				break;
		}
		if (i == numcollected)
		{
			CreateThinker<DPusher>(type, nullptr, magnitude, angle, nullptr, secnum);
		}
	}
}
