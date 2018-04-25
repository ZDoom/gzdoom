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
#include "p_lnspec.h"
#include "p_maputl.h"
#include "p_local.h"
#include "d_player.h"
#include "g_levellocals.h"
#include "actorinlines.h"

CVAR(Bool, var_pushers, true, CVAR_SERVERINFO);

// phares 3/20/98: added new model of Pushers for push/pull effects

class DPusher : public DThinker
{
	DECLARE_CLASS (DPusher, DThinker)
	HAS_OBJECT_POINTERS
public:
	enum EPusher
	{
		p_push,
		p_pull,
		p_wind,
		p_current
	};

	DPusher ();
	DPusher (EPusher type, line_t *l, int magnitude, int angle, AActor *source, int affectee);
	void Serialize(FSerializer &arc);
	int CheckForSectorMatch (EPusher type, int tag);
	void ChangeValues (int magnitude, int angle)
	{
		DAngle ang = angle * (360. / 256.);
		m_PushVec = ang.ToVector(magnitude);
		m_Magnitude = magnitude;
	}

	void Tick ();

protected:
	EPusher m_Type;
	TObjPtr<AActor*> m_Source;// Point source if point pusher
	DVector2 m_PushVec;
	double m_Magnitude;		// Vector strength for point pusher
	double m_Radius;		// Effective radius for point pusher
	int m_Affectee;			// Number of affected sector

	friend bool PIT_PushThing (AActor *thing);
};

IMPLEMENT_CLASS(DPusher, false, true)

IMPLEMENT_POINTERS_START(DPusher)
	IMPLEMENT_POINTER(m_Source)
IMPLEMENT_POINTERS_END

DPusher::DPusher ()
{
}

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


////////////////////////////////////////////////////////////////////////////
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


#define PUSH_FACTOR 128

/////////////////////////////
//
// Add a push thinker to the thinker list

DPusher::DPusher (DPusher::EPusher type, line_t *l, int magnitude, int angle,
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
	if (m_Type == type && tagManager.SectorHasTag(m_Affectee, tag))
		return m_Affectee;
	else
		return -1;
}


/////////////////////////////
//
// T_Pusher looks for all objects that are inside the radius of
// the effect.
//
void DPusher::Tick ()
{
	sector_t *sec;
	AActor *thing;
	msecnode_t *node;
	double ht;

	if (!var_pushers)
		return;

	sec = &level.sectors[m_Affectee];

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
			if (compatflags & COMPATF_MBFMONSTERMOVE)
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
					if (m_Source->GetClass()->TypeName == NAME_PointPuller) pushangle += 180;  
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
				else if (thing->player->viewz < ht) // underwater
				{
					pushvel.Zero(); // no force
				}
				else // wading in water
				{
					pushvel = m_PushVec / 2; // full force
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

/////////////////////////////
//
// P_GetPushThing() returns a pointer to an MT_PUSH or MT_PULL thing,
// NULL otherwise.

AActor *P_GetPushThing (int s)
{
	AActor* thing;
	sector_t* sec;

	sec = &level.sectors[s];
	thing = sec->thinglist;

	while (thing &&
		thing->GetClass()->TypeName != NAME_PointPusher &&
		thing->GetClass()->TypeName != NAME_PointPuller)
	{
		thing = thing->snext;
	}
	return thing;
}

/////////////////////////////
//
// Initialize the sectors where pushers are present
//

void P_SpawnPushers ()
{
	line_t *l = &level.lines[0];
	int s;

	for (unsigned i = 0; i < level.lines.Size(); i++, l++)
	{
		switch (l->special)
		{
		case Sector_SetWind: // wind
		{
			FSectorTagIterator itr(l->args[0]);
			while ((s = itr.Next()) >= 0)
				Create<DPusher>(DPusher::p_wind, l->args[3] ? l : nullptr, l->args[1], l->args[2], nullptr, s);
			l->special = 0;
			break;
		}

		case Sector_SetCurrent: // current
		{
			FSectorTagIterator itr(l->args[0]);
			while ((s = itr.Next()) >= 0)
				Create<DPusher>(DPusher::p_current, l->args[3] ? l : nullptr, l->args[1], l->args[2], nullptr, s);
			l->special = 0;
			break;
		}

		case PointPush_SetForce: // push/pull
			if (l->args[0]) {	// [RH] Find thing by sector
				FSectorTagIterator itr(l->args[0]);
				while ((s = itr.Next()) >= 0)
				{
					AActor *thing = P_GetPushThing (s);
					if (thing) {	// No MT_P* means no effect
						// [RH] Allow narrowing it down by tid
						if (!l->args[1] || l->args[1] == thing->tid)
							Create<DPusher> (DPusher::p_push, l->args[3] ? l : NULL, l->args[2],
										 0, thing, s);
					}
				}
			} else {	// [RH] Find thing by tid
				AActor *thing;
				FActorIterator iterator (l->args[1]);

				while ( (thing = iterator.Next ()) )
				{
					if (thing->GetClass()->TypeName == NAME_PointPusher ||
						thing->GetClass()->TypeName == NAME_PointPuller)
					{
						Create<DPusher> (DPusher::p_push, l->args[3] ? l : NULL, l->args[2], 0, thing, thing->Sector->Index());
					}
				}
			}
			l->special = 0;
			break;
		}
	}
}

void AdjustPusher (int tag, int magnitude, int angle, bool wind)
{
	DPusher::EPusher type = wind? DPusher::p_wind : DPusher::p_current;
	
	// Find pushers already attached to the sector, and change their parameters.
	TArray<FThinkerCollection> Collection;
	{
		TThinkerIterator<DPusher> iterator;
		FThinkerCollection collect;

		while ( (collect.Obj = iterator.Next ()) )
		{
			if ((collect.RefNum = ((DPusher *)collect.Obj)->CheckForSectorMatch (type, tag)) >= 0)
			{
				((DPusher *)collect.Obj)->ChangeValues (magnitude, angle);
				Collection.Push (collect);
			}
		}
	}

	size_t numcollected = Collection.Size ();
	int secnum;

	// Now create pushers for any sectors that don't already have them.
	FSectorTagIterator itr(tag);
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
			Create<DPusher> (type, nullptr, magnitude, angle, nullptr, secnum);
		}
	}
}
