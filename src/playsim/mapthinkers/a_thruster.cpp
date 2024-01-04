//-----------------------------------------------------------------------------
//
// Copyright 2023 Ryan Krafnick
// Copyright 2023 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		UDMF-style thruster
//
//-----------------------------------------------------------------------------


#include <stdlib.h>
#include "actor.h"
#include "p_spec.h"
#include "serializer.h"
#include "serializer_doom.h"
#include "p_spec_thinkers.h"

EXTERN_CVAR(Bool, var_pushers);

IMPLEMENT_CLASS(DThruster, false, false)

enum
{
    THRUST_STATIC     = 0x01,
    THRUST_PLAYER     = 0x02,
    THRUST_MONSTER    = 0x04,
    THRUST_PROJECTILE = 0x08,
    THRUST_WINDTHRUST = 0x10,
    
    THRUST_GROUNDED = 1,
    THRUST_AIRBORNE = 2,
    THRUST_CEILING  = 4
};


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void DThruster::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("type", m_Type)
		("location", m_Location)
		("pushvec", m_PushVec)
		("sector", m_Sector);
}


//-----------------------------------------------------------------------------
//
// Add a thrust thinker to the thinker list
//
//-----------------------------------------------------------------------------

void DThruster::Construct(sector_t* sec, double dx, double dy, int type, int location)
{
	m_Type = type;
	m_Location = location;
	m_Sector = sec;
	m_PushVec = { dx, dy };
}

//-----------------------------------------------------------------------------
//
// 
//
//-----------------------------------------------------------------------------

void DThruster::Tick ()
{
    sector_t* sec = m_Sector;

    if (m_PushVec.isZero())
        return;

    for (auto node = sec->touching_thinglist; node; node = node->m_snext) 
    {
        bool thrust_it = false;
        AActor* thing = node->m_thing;

        if (thing->flags & MF_NOCLIP)
            continue;

        if (!(thing->flags & MF_NOGRAVITY) && thing->Z() <= thing->floorz) 
        {
            if (m_Location & THRUST_GROUNDED)
                thrust_it = true;
        }
        else if (
            thing->flags & MF_SPAWNCEILING &&
            thing->flags & MF_NOGRAVITY &&
            thing->Top() == thing->ceilingz
            ) 
        {
            if (m_Location & THRUST_CEILING)
                thrust_it = true;
        }
        else if (thing->flags & MF_NOGRAVITY || thing->Z() > thing->floorz)
        {
            if (m_Location & THRUST_AIRBORNE)
                thrust_it = true;
        }

        if (thrust_it) 
        {
            thrust_it = false;

            if (thing->flags2 & MF2_WINDTHRUST && m_Type & THRUST_WINDTHRUST)
                thrust_it = true;
            else if (thing->flags3 & MF3_ISMONSTER) 
            {
                if (m_Type & THRUST_MONSTER)
                    thrust_it = true;
            }
            else if (thing->player) 
            {
                if (m_Type & THRUST_PLAYER)
                    thrust_it = true;
            }
            else if (thing->flags & MF_MISSILE) 
            {
                if (m_Type & THRUST_PROJECTILE)
                    thrust_it = true;
            }
            else 
            {
                if (m_Type & THRUST_STATIC)
                    thrust_it = true;
            }

            if (thrust_it) 
            {
                thing->Vel += m_PushVec;
            }
        }
    }
}

