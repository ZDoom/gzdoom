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
// DESCRIPTION:
//		Handle Sector base lighting effects.
//
//-----------------------------------------------------------------------------



#include "z_zone.h"
#include "m_random.h"

#include "doomdef.h"
#include "p_local.h"

#include "p_lnspec.h"

// State.
#include "r_state.h"

// [RH] Make sure the light level is in bounds.
#define CLIPLIGHT(l)	(((l) < 0) ? 0 : (((l) > 255) ? 255 : (l)))

IMPLEMENT_SERIAL (DLighting, DSectorEffect)

DLighting::DLighting ()
{
}

void DLighting::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
}

DLighting::DLighting (sector_t *sector)
	: DSectorEffect (sector)
{
}

//
// FIRELIGHT FLICKER
//

IMPLEMENT_SERIAL (DFireFlicker, DLighting)

DFireFlicker::DFireFlicker ()
{
}

void DFireFlicker::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (arc.IsStoring ())
		arc << m_Count << m_MaxLight << m_MinLight;
	else
		arc >> m_Count >> m_MaxLight >> m_MinLight;
}


//
// T_FireFlicker
//
void DFireFlicker::RunThink ()
{
	int amount;
		
	if (--m_Count == 0)
	{
		amount = (P_Random (pr_fireflicker) & 3) << 4;
		
		if (m_Sector->lightlevel - amount < m_MinLight)
			m_Sector->lightlevel = m_MinLight;
		else
			m_Sector->lightlevel = m_MaxLight - amount;

		m_Count = 4;
	}
}

//
// P_SpawnFireFlicker
//
DFireFlicker::DFireFlicker (sector_t *sector)
	: DLighting (sector)
{
	m_MaxLight = sector->lightlevel;
	m_MinLight = P_FindMinSurroundingLight(sector,sector->lightlevel)+16;
	m_Count = 4;
}

DFireFlicker::DFireFlicker (sector_t *sector, int upper, int lower)
	: DLighting (sector)
{
	m_MaxLight = CLIPLIGHT(upper);
	m_MinLight = CLIPLIGHT(lower);
	m_Count = 4;
}

//
// [RH] flickering light like Hexen's
//
IMPLEMENT_SERIAL (DFlicker, DLighting)

DFlicker::DFlicker ()
{
}

void DFlicker::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (arc.IsStoring ())
		arc << m_Count << m_MaxLight << m_MinLight;
	else
		arc >> m_Count >> m_MaxLight >> m_MinLight;
}


void DFlicker::RunThink ()
{
	if (m_Count)
	{
		m_Count--;	
	}
	else if (m_Sector->lightlevel == m_MaxLight)
	{
		m_Sector->lightlevel = m_MinLight;
		m_Count = (P_Random(pr_misc)&7)+1;
	}
	else
	{
		m_Sector->lightlevel = m_MaxLight;
		m_Count = (P_Random(pr_misc)&31)+1;
	}
}

DFlicker::DFlicker (sector_t *sector, int upper, int lower)
	: DLighting (sector)
{
	m_MaxLight = upper;
	m_MinLight = lower;
	sector->lightlevel = upper;
	m_Count = (P_Random(pr_misc)&64)+1;
}

void EV_StartLightFlickering (int tag, int upper, int lower)
{
	int secnum;
		
	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag,secnum)) >= 0)
	{
		new DFlicker (&sectors[secnum], upper, lower);
	}
}


//
// BROKEN LIGHT FLASHING
//

IMPLEMENT_SERIAL (DLightFlash, DLighting)

DLightFlash::DLightFlash ()
{
}

void DLightFlash::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (arc.IsStoring ())
		arc << m_Count << m_MaxLight << m_MaxTime << m_MinLight << m_MinTime;
	else
		arc >> m_Count >> m_MaxLight >> m_MaxTime >> m_MinLight >> m_MinTime;
}

//
// T_LightFlash
// Do flashing lights.
//
void DLightFlash::RunThink ()
{
	if (--m_Count == 0)
	{
		if (m_Sector->lightlevel == m_MaxLight)
		{
			m_Sector->lightlevel = m_MinLight;
			m_Count = (P_Random (pr_lightflash) & m_MinTime) + 1;
		}
		else
		{
			m_Sector->lightlevel = m_MaxLight;
			m_Count = (P_Random (pr_lightflash) & m_MaxTime) + 1;
		}
	}
}

//
// P_SpawnLightFlash
//
DLightFlash::DLightFlash (sector_t *sector)
	: DLighting (sector)
{
	// Find light levels like Doom.
	m_MaxLight = sector->lightlevel;
	m_MinLight = P_FindMinSurroundingLight (sector,sector->lightlevel);
	m_MaxTime = 64;
	m_MinTime = 7;
	m_Count = (P_Random (pr_spawnlightflash) & m_MaxTime) + 1;
}
	
DLightFlash::DLightFlash (sector_t *sector, int min, int max)
	: DLighting (sector)
{
	// Use specified light levels.
	m_MaxLight = CLIPLIGHT(max);
	m_MinLight = CLIPLIGHT(min);
	m_MaxTime = 64;
	m_MinTime = 7;
	m_Count = (P_Random (pr_spawnlightflash) & m_MaxTime) + 1;
}


//
// STROBE LIGHT FLASHING
//

IMPLEMENT_SERIAL (DStrobe, DLighting)

DStrobe::DStrobe ()
{
}

void DStrobe::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (arc.IsStoring ())
		arc << m_Count << m_MaxLight << m_MinLight << m_DarkTime << m_BrightTime;
	else
		arc >> m_Count >> m_MaxLight >> m_MinLight >> m_DarkTime >> m_BrightTime;
}

//
// T_StrobeFlash
//
void DStrobe::RunThink ()
{
	if (--m_Count == 0)
	{
		if (m_Sector->lightlevel == m_MinLight)
		{
			m_Sector->lightlevel = m_MaxLight;
			m_Count = m_BrightTime;
		}
		else
		{
			m_Sector->lightlevel = m_MinLight;
			m_Count = m_DarkTime;
		}
	}
}

//
// Hexen-style constructor
//
DStrobe::DStrobe (sector_t *sector, int upper, int lower, int utics, int ltics)
	: DLighting (sector)
{
	m_DarkTime = ltics;
	m_BrightTime = utics;
	m_MaxLight = CLIPLIGHT(upper);
	m_MinLight = CLIPLIGHT(lower);
	m_Count = 1;	// Hexen-style is always in sync
}

//
// Doom-style constructor
//
DStrobe::DStrobe (sector_t *sector, int utics, int ltics, bool inSync)
	: DLighting (sector)
{
	m_DarkTime = ltics;
	m_BrightTime = utics;

	m_MaxLight = sector->lightlevel;
	m_MinLight = P_FindMinSurroundingLight (sector, sector->lightlevel);

	if (m_MinLight == m_MaxLight)
		m_MinLight = 0;

	m_Count = inSync ? 1 : (P_Random (pr_spawnstrobeflash) & 7) + 1;
}



//
// Start strobing lights (usually from a trigger)
// [RH] Made it more configurable.
//
void EV_StartLightStrobing (int tag, int upper, int lower, int utics, int ltics)
{
	int secnum;
		
	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag,secnum)) >= 0)
	{
		sector_t *sec = &sectors[secnum];
		if (sec->lightingdata)
			continue;
		
		new DStrobe (sec, upper, lower, utics, ltics);
	}
}

void EV_StartLightStrobing (int tag, int utics, int ltics)
{
	int secnum;
		
	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag,secnum)) >= 0)
	{
		sector_t *sec = &sectors[secnum];
		if (sec->lightingdata)
			continue;
		
		new DStrobe (sec, utics, ltics, false);
	}
}


//
// TURN LINE'S TAG LIGHTS OFF
// [RH] Takes a tag instead of a line
//
void EV_TurnTagLightsOff (int tag)
{
	int i;
	int secnum;

	// [RH] Don't do a linear search
	for (secnum = -1; (secnum = P_FindSectorFromTag (tag, secnum)) >= 0; ) 
	{
		sector_t *sector = sectors + secnum;
		int min = sector->lightlevel;

		for (i = 0; i < sector->linecount; i++)
		{
			sector_t *tsec = getNextSector (sector->lines[i],sector);
			if (!tsec)
				continue;
			if (tsec->lightlevel < min)
				min = tsec->lightlevel;
		}
		sector->lightlevel = min;
	}
}


//
// TURN LINE'S TAG LIGHTS ON
// [RH] Takes a tag instead of a line
//
void EV_LightTurnOn (int tag, int bright)
{
	int secnum = -1;

	// [RH] Don't do a linear search
	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0) 
	{
		sector_t *sector = sectors + secnum;

		// bright = -1 means to search ([RH] Not 0)
		// for highest light level
		// surrounding sector
		if (bright < 0)
		{
			int j;

			bright = 0;
			for (j = 0; j < sector->linecount; j++)
			{
				sector_t *temp = getNextSector (sector->lines[j], sector);

				if (!temp)
					continue;

				if (temp->lightlevel > bright)
					bright = temp->lightlevel;
			}
		}
		sector->lightlevel = CLIPLIGHT(bright);
	}
}


//
// [RH] New function to adjust tagged sectors' light levels
//		by a relative amount. Light levels are clipped to
//		within the range 0-255 inclusive.
//
void EV_LightChange (int tag, int value)
{
	int secnum = -1;

	while ((secnum = P_FindSectorFromTag (tag, secnum)) >= 0) {
		int newlight = sectors[secnum].lightlevel + value;
		sectors[secnum].lightlevel = CLIPLIGHT(newlight);
	}
}

	
//
// Spawn glowing light
//
IMPLEMENT_SERIAL (DGlow, DLighting)

DGlow::DGlow ()
{
}

void DGlow::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (arc.IsStoring ())
		arc << m_Direction << m_MaxLight << m_MinLight;
	else
		arc >> m_Direction >> m_MaxLight >> m_MinLight;
}

void DGlow::RunThink ()
{
	switch (m_Direction)
	{
	case -1:
		// DOWN
		m_Sector->lightlevel -= GLOWSPEED;
		if (m_Sector->lightlevel <= m_MinLight)
		{
			m_Sector->lightlevel += GLOWSPEED;
			m_Direction = 1;
		}
		break;
		
	case 1:
		// UP
		m_Sector->lightlevel += GLOWSPEED;
		if (m_Sector->lightlevel >= m_MaxLight)
		{
			m_Sector->lightlevel -= GLOWSPEED;
			m_Direction = -1;
		}
		break;
	}
}


DGlow::DGlow (sector_t *sector)
	: DLighting (sector)
{
	m_MinLight = P_FindMinSurroundingLight (sector, sector->lightlevel);
	m_MaxLight = sector->lightlevel;
	m_Direction = -1;
}

//
// [RH] More glowing light, this time appropriate for Hexen-ish uses.
//

IMPLEMENT_SERIAL (DGlow2, DLighting)

DGlow2::DGlow2 ()
{
}

void DGlow2::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (arc.IsStoring ())
		arc << m_End << m_MaxTics << m_OneShot << m_Start << m_Tics;
	else
		arc >> m_End >> m_MaxTics >> m_OneShot >> m_Start >> m_Tics;
}

void DGlow2::RunThink ()
{
	if (m_Tics++ >= m_MaxTics)
	{
		if (m_OneShot)
		{
			m_Sector->lightlevel = m_End;
			Destroy ();
			return;
		}
		else
		{
			int temp = m_Start;
			m_Start = m_End;
			m_End = temp;
			m_Tics -= m_MaxTics;
		}
	}

	m_Sector->lightlevel = ((m_End - m_Start) * m_Tics) / m_MaxTics + m_Start;
}

DGlow2::DGlow2 (sector_t *sector, int start, int end, int tics, bool oneshot)
	: DLighting (sector)
{
	m_Start = CLIPLIGHT(start);
	m_End = CLIPLIGHT(end);
	m_MaxTics = tics;
	m_Tics = -1;
	m_OneShot = oneshot;
}

void EV_StartLightGlowing (int tag, int upper, int lower, int tics)
{
	int secnum;

	if (upper < lower) {
		int temp = upper;
		upper = lower;
		lower = temp;
	}

	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag,secnum)) >= 0)
	{
		sector_t *sec = &sectors[secnum];
		if (sec->lightingdata)
			continue;
		
		new DGlow2 (sec, upper, lower, tics, false);
	}
}

void EV_StartLightFading (int tag, int value, int tics)
{
	int secnum;
		
	secnum = -1;
	while ((secnum = P_FindSectorFromTag (tag,secnum)) >= 0)
	{
		sector_t *sec = &sectors[secnum];
		if (sec->lightingdata)
			continue;

		// No need to fade if lightlevel is already at desired value.
		if (sec->lightlevel == value)
			continue;

		new DGlow2 (sec, sec->lightlevel, value, tics, true);
	}
}


// [RH] Phased lighting ala Hexen

IMPLEMENT_SERIAL (DPhased, DLighting)

DPhased::DPhased ()
{
}

void DPhased::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	if (arc.IsStoring ())
		arc << m_BaseLevel << m_Phase;
	else
		arc >> m_BaseLevel >> m_Phase;
}

void DPhased::RunThink ()
{
	const int steps = 12;

	if (m_Phase < steps)
		m_Sector->lightlevel = ((255 - m_BaseLevel) * m_Phase) / steps + m_BaseLevel;
	else if (m_Phase < 2*steps)
		m_Sector->lightlevel = ((255 - m_BaseLevel) * (2*steps - m_Phase - 1) / steps
								+ m_BaseLevel);
	else
		m_Sector->lightlevel = m_BaseLevel;

	if (m_Phase == 0)
		m_Phase = 63;
	else
		m_Phase--;
}

int DPhased::PhaseHelper (sector_t *sector, int index, int light, sector_t *prev)
{
	if (!sector)
	{
		return index;
	}
	else
	{
		DPhased *l;
		int baselevel = sector->lightlevel ? sector->lightlevel : light;

		if (index == 0)
		{
			l = this;
			m_BaseLevel = baselevel;
		}
		else
			l = new DPhased (sector, baselevel);

		int numsteps = PhaseHelper (P_NextSpecialSector (sector,
				(sector->special & 0x00ff) == LightSequenceSpecial1 ?
					LightSequenceSpecial2 : LightSequenceSpecial1, prev),
				index + 1, l->m_BaseLevel, sector);
		l->m_Phase = ((numsteps - index - 1) * 64) / numsteps;

		sector->special &= 0xff00;

		return numsteps;
	}
}

DPhased::DPhased (sector_t *sector, int baselevel)
	: DLighting (sector)
{
	m_BaseLevel = baselevel;
}

DPhased::DPhased (sector_t *sector)
	: DLighting (sector)
{
	PhaseHelper (sector, 0, 0, NULL);
}

DPhased::DPhased (sector_t *sector, int baselevel, int phase)
	: DLighting (sector)
{
	m_BaseLevel = baselevel;
	m_Phase = phase;
	sector->special &= 0xff00;
}