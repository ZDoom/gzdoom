// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION: Door animation code (opening/closing)
//		[RH] Removed sliding door code and simplified for Hexen-ish specials
//
//-----------------------------------------------------------------------------



#include "doomdef.h"
#include "p_local.h"
#include "s_sound.h"
#include "s_sndseq.h"
#include "doomstat.h"
#include "r_state.h"
#include "c_console.h"
#include "gi.h"

IMPLEMENT_CLASS (DDoor)

DDoor::DDoor ()
{
}

void DDoor::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Type
		<< m_TopDist
		<< m_BotSpot << m_BotDist << m_OldFloorDist
		<< m_Speed
		<< m_Direction
		<< m_TopWait
		<< m_TopCountdown;
}


//
// VERTICAL DOORS
//

//
// T_VerticalDoor
//
void DDoor::Tick ()
{
	EResult res;

	if (m_Sector->floorplane.d != m_OldFloorDist)
	{
		m_OldFloorDist = m_Sector->floorplane.d;
		m_BotDist = m_Sector->ceilingplane.PointToDist (m_BotSpot,
			m_Sector->floorplane.ZatPoint (m_BotSpot));
	}

	switch (m_Direction)
	{
	case 0:
		// WAITING
		if (!--m_TopCountdown)
		{
			switch (m_Type)
			{
			case doorRaise:
				m_Direction = -1; // time to go back down
				DoorSound (false);
				break;
				
			case doorCloseWaitOpen:
				m_Direction = 1;
				DoorSound (true);
				break;
				
			default:
				break;
			}
		}
		break;
		
	case 2:
		//	INITIAL WAIT
		if (!--m_TopCountdown)
		{
			switch (m_Type)
			{
			case doorRaiseIn5Mins:
				m_Direction = 1;
				m_Type = doorRaise;
				DoorSound (true);
				break;
				
			default:
				break;
			}
		}
		break;
		
	case -1:
		// DOWN
		res = MoveCeiling (m_Speed, m_BotDist, -1, m_Direction);
		if (res == pastdest)
		{
			SN_StopSequence (m_Sector);
			switch (m_Type)
			{
			case doorRaise:
			case doorClose:
				m_Sector->ceilingdata = NULL;	//jff 2/22/98
				stopinterpolation (&m_Sector->ceilingplane.d);
				stopinterpolation (&m_Sector->ceilingtexz);
				Destroy ();						// unlink and free
				break;
				
			case doorCloseWaitOpen:
				m_Direction = 0;
				m_TopCountdown = m_TopWait;
				break;
				
			default:
				break;
			}
		}
		else if (res == crushed)
		{
			switch (m_Type)
			{
			case doorClose:				// DO NOT GO BACK UP!
				break;
				
			default:
				m_Direction = 1;
				DoorSound (true);
				break;
			}
		}
		break;
		
	case 1:
		// UP
		res = MoveCeiling (m_Speed, m_TopDist, -1, m_Direction);
		
		if (res == pastdest)
		{
			SN_StopSequence (m_Sector);
			switch (m_Type)
			{
			case doorRaise:
				m_Direction = 0; // wait at top
				m_TopCountdown = m_TopWait;
				break;
				
			case doorCloseWaitOpen:
			case doorOpen:
				m_Sector->ceilingdata = NULL;	//jff 2/22/98
				stopinterpolation (&m_Sector->ceilingplane.d);
				stopinterpolation (&m_Sector->ceilingtexz);
				Destroy ();						// unlink and free
				break;
				
			default:
				break;
			}
		}
		break;
	}
}

// [RH] DoorSound: Plays door sound depending on direction and speed
void DDoor::DoorSound (bool raise) const
{
	const char *snd;

	if (m_Sector->seqType >= 0)
	{
		SN_StartSequence (m_Sector, m_Sector->seqType, SEQ_DOOR);
	}
	else
	{
		if (gameinfo.gametype == GAME_Heretic)
		{
			if (raise)
				snd = "HereticDoorOpen";
			else
				snd = "HereticDoorClose";
		}
		else
		{
			if (raise)
				snd = (m_Speed >= FRACUNIT*8) ? "DoorOpenBlazing" : "DoorOpenNormal";
			else
				snd = (m_Speed >= FRACUNIT*8) ? "DoorCloseBlazing" : "DoorCloseNormal";
		}
		SN_StartSequence (m_Sector, snd);
	}
}

DDoor::DDoor (sector_t *sector)
	: DMovingCeiling (sector)
{
}

// [RH] Merged EV_VerticalDoor and EV_DoLockedDoor into EV_DoDoor
//		and made them more general to support the new specials.

// [RH] SpawnDoor: Helper function for EV_DoDoor
DDoor::DDoor (sector_t *sec, EVlDoor type, fixed_t speed, int delay)
	: DMovingCeiling (sec)
{
	vertex_t *spot;
	fixed_t height;

	m_Type = type;
	m_TopWait = delay;
	m_Speed = speed;

	switch (type)
	{
	case doorClose:
		m_Direction = -1;
		height = sec->FindLowestCeilingSurrounding (&spot);
		m_TopDist = sec->ceilingplane.PointToDist (spot, height - 4*FRACUNIT);
		DoorSound (false);
		break;

	case doorOpen:
	case doorRaise:
		m_Direction = 1;
		height = sec->FindLowestCeilingSurrounding (&spot);
		m_TopDist = sec->ceilingplane.PointToDist (spot, height - 4*FRACUNIT);
		if (m_TopDist != sec->ceilingplane.d)
			DoorSound (true);
		break;

	case doorCloseWaitOpen:
		m_TopDist = sec->ceilingplane.d;
		m_Direction = -1;
		DoorSound (false);
		break;

	case doorRaiseIn5Mins:
		m_Direction = 2;
		height = sec->FindLowestCeilingSurrounding (&spot);
		m_TopDist = sec->ceilingplane.PointToDist (spot, height - 4*FRACUNIT);
		m_TopCountdown = 5 * 60 * TICRATE;
		break;
	}

	height = sec->FindHighestFloorPoint (&m_BotSpot);
	m_BotDist = sec->ceilingplane.PointToDist (m_BotSpot, height);
	m_OldFloorDist = sec->floorplane.d;
}

bool EV_DoDoor (DDoor::EVlDoor type, line_t *line, AActor *thing,
				int tag, int speed, int delay, keyspecialtype_t lock)
{
	bool		rtn = false;
	int 		secnum;
	sector_t*	sec;

	if (lock && !P_CheckKeys (thing->player, lock, tag))
		return false;

	if (tag == 0)
	{		// [RH] manual door
		if (!line)
			return false;

		// if the wrong side of door is pushed, give oof sound
		if (line->sidenum[1] == NO_INDEX)			// killough
		{
			S_Sound (thing, CHAN_VOICE, "*usefail", 1, ATTN_NORM);
			return false;
		}

		// get the sector on the second side of activating linedef
		sec = sides[line->sidenum[1]].sector;
		secnum = sec-sectors;

		// if door already has a thinker, use it
		if (sec->ceilingdata)
		{
			if (sec->ceilingdata->IsKindOf (RUNTIME_CLASS(DDoor)))
			{
				DDoor *door = static_cast<DDoor *>(sec->ceilingdata);

				// ONLY FOR "RAISE" DOORS, NOT "OPEN"s
				if (door->m_Type == DDoor::doorRaise && type == DDoor::doorRaise)
				{
					if (door->m_Direction == -1)
					{
						door->m_Direction = 1;	// go back up
					}
					else if (GET_SPAC(line->flags) != SPAC_PUSH)
						// [RH] activate push doors don't go back down when you
						//		run into them (otherwise opening them would be
						//		a real pain).
					{
						if (!thing->player || thing->player->isbot)
							return false;	// JDC: bad guys never close doors
											//Added by MC: Neither do bots.

						door->m_Direction = -1;	// start going down immediately

						// [RH] If this sector doesn't have a specific sound
						// attached to it, start the door close sequence.
						// Otherwise, just let the current one continue.
						if (sec->seqType == -1)
						{
							door->DoorSound (false);
						}
						return true;
					}
					else
					{
						return false;
					}
				}
			}
			return false;
		}
		if (new DDoor (sec, type, speed, delay))
			rtn = true;
	}
	else
	{	// [RH] Remote door

		secnum = -1;
		while ((secnum = P_FindSectorFromTag (tag,secnum)) >= 0)
		{
			sec = &sectors[secnum];
			// if the ceiling already moving, don't start the door action
			if (sec->ceilingdata)
				continue;

			if (new DDoor (sec, type, speed, delay))
				rtn = true;
		}
				
	}
	return rtn;
}


//
// Spawn a door that closes after 30 seconds
//
void P_SpawnDoorCloseIn30 (sector_t *sec)
{
	fixed_t height;
	DDoor *door = new DDoor (sec);

	sec->special = 0;

	door->m_Sector = sec;
	door->m_Direction = 0;
	door->m_Type = DDoor::doorRaise;
	door->m_Speed = FRACUNIT*2;
	door->m_TopCountdown = 30 * TICRATE;
	height = sec->FindHighestFloorPoint (&door->m_BotSpot);
	door->m_BotDist = sec->ceilingplane.PointToDist (door->m_BotSpot, height);
	door->m_OldFloorDist = sec->floorplane.d;
	door->m_TopDist = sec->ceilingplane.d;
}

//
// Spawn a door that opens after 5 minutes
//
void P_SpawnDoorRaiseIn5Mins (sector_t *sec)
{
	sec->special = 0;
	new DDoor (sec, DDoor::doorRaiseIn5Mins, 2*FRACUNIT, TICRATE*30/7);
}
