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


#include "templates.h"
#include "m_random.h"

#include "doomdef.h"
#include "p_local.h"

#include "p_lnspec.h"

// State.
#include "r_state.h"
#include "statnums.h"
#include "farchive.h"

static FRandom pr_flicker ("Flicker");
static FRandom pr_lightflash ("LightFlash");
static FRandom pr_strobeflash ("StrobeFlash");
static FRandom pr_fireflicker ("FireFlicker");

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS (DLighting)

DLighting::DLighting ()
{
}

DLighting::DLighting (sector_t *sector)
	: DSectorEffect (sector)
{
	ChangeStatNum (STAT_LIGHT);
}

//-----------------------------------------------------------------------------
//
// FIRELIGHT FLICKER
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS (DFireFlicker)

DFireFlicker::DFireFlicker ()
{
}

void DFireFlicker::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Count << m_MaxLight << m_MinLight;
}


//-----------------------------------------------------------------------------
//
// T_FireFlicker
//
//-----------------------------------------------------------------------------

void DFireFlicker::Tick ()
{
	int amount;

	if (--m_Count == 0)
	{
		amount = (pr_fireflicker() & 3) << 4;

		// [RH] Shouldn't this be (m_MaxLight - amount < m_MinLight)?
		if (m_Sector->lightlevel - amount < m_MinLight)
			m_Sector->SetLightLevel(m_MinLight);
		else
			m_Sector->SetLightLevel(m_MaxLight - amount);

		m_Count = 4;
	}
}

//-----------------------------------------------------------------------------
//
// P_SpawnFireFlicker
//
//-----------------------------------------------------------------------------

DFireFlicker::DFireFlicker (sector_t *sector)
	: DLighting (sector)
{
	m_MaxLight = sector->lightlevel;
	m_MinLight = sector_t::ClampLight(sector->FindMinSurroundingLight(sector->lightlevel) + 16);
	m_Count = 4;
}

DFireFlicker::DFireFlicker (sector_t *sector, int upper, int lower)
	: DLighting (sector)
{
	m_MaxLight = sector_t::ClampLight(upper);
	m_MinLight = sector_t::ClampLight(lower);
	m_Count = 4;
}

//-----------------------------------------------------------------------------
//
// [RH] flickering light like Hexen's
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS (DFlicker)

DFlicker::DFlicker ()
{
}

void DFlicker::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Count << m_MaxLight << m_MinLight;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void DFlicker::Tick ()
{
	if (m_Count)
	{
		m_Count--;	
	}
	else if (m_Sector->lightlevel == m_MaxLight)
	{
		m_Sector->SetLightLevel(m_MinLight);
		m_Count = (pr_flicker()&7)+1;
	}
	else
	{
		m_Sector->SetLightLevel(m_MaxLight);
		m_Count = (pr_flicker()&31)+1;
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

DFlicker::DFlicker (sector_t *sector, int upper, int lower)
	: DLighting (sector)
{
	m_MaxLight = upper;
	m_MinLight = lower;
	sector->lightlevel = upper;
	m_Count = (pr_flicker()&64)+1;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void EV_StartLightFlickering (int tag, int upper, int lower)
{
	int secnum;
	FSectorTagIterator it(tag);
	while ((secnum = it.Next()) >= 0)
	{
		new DFlicker (&sectors[secnum], upper, lower);
	}
}


//-----------------------------------------------------------------------------
//
// BROKEN LIGHT FLASHING
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS (DLightFlash)

DLightFlash::DLightFlash ()
{
}

void DLightFlash::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Count << m_MaxLight << m_MaxTime << m_MinLight << m_MinTime;
}

//-----------------------------------------------------------------------------
//
// T_LightFlash
// Do flashing lights.
//
//-----------------------------------------------------------------------------

void DLightFlash::Tick ()
{
	if (--m_Count == 0)
	{
		if (m_Sector->lightlevel == m_MaxLight)
		{
			m_Sector->SetLightLevel(m_MinLight);
			m_Count = (pr_lightflash() & m_MinTime) + 1;
		}
		else
		{
			m_Sector->SetLightLevel(m_MaxLight);
			m_Count = (pr_lightflash() & m_MaxTime) + 1;
		}
	}
}

//-----------------------------------------------------------------------------
//
// P_SpawnLightFlash
//
//-----------------------------------------------------------------------------

DLightFlash::DLightFlash (sector_t *sector)
	: DLighting (sector)
{
	// Find light levels like Doom.
	m_MaxLight = sector->lightlevel;
	m_MinLight = sector->FindMinSurroundingLight (sector->lightlevel);
	m_MaxTime = 64;
	m_MinTime = 7;
	m_Count = (pr_lightflash() & m_MaxTime) + 1;
}
	
DLightFlash::DLightFlash (sector_t *sector, int min, int max)
	: DLighting (sector)
{
	// Use specified light levels.
	m_MaxLight = sector_t::ClampLight(max);
	m_MinLight = sector_t::ClampLight(min);
	m_MaxTime = 64;
	m_MinTime = 7;
	m_Count = (pr_lightflash() & m_MaxTime) + 1;
}


//-----------------------------------------------------------------------------
//
// STROBE LIGHT FLASHING
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS (DStrobe)

DStrobe::DStrobe ()
{
}

void DStrobe::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Count << m_MaxLight << m_MinLight << m_DarkTime << m_BrightTime;
}

//-----------------------------------------------------------------------------
//
// T_StrobeFlash
//
//-----------------------------------------------------------------------------

void DStrobe::Tick ()
{
	if (--m_Count == 0)
	{
		if (m_Sector->lightlevel == m_MinLight)
		{
			m_Sector->SetLightLevel(m_MaxLight);
			m_Count = m_BrightTime;
		}
		else
		{
			m_Sector->SetLightLevel(m_MinLight);
			m_Count = m_DarkTime;
		}
	}
}

//-----------------------------------------------------------------------------
//
// Hexen-style constructor
//
//-----------------------------------------------------------------------------

DStrobe::DStrobe (sector_t *sector, int upper, int lower, int utics, int ltics)
	: DLighting (sector)
{
	m_DarkTime = ltics;
	m_BrightTime = utics;
	m_MaxLight = sector_t::ClampLight(upper);
	m_MinLight = sector_t::ClampLight(lower);
	m_Count = 1;	// Hexen-style is always in sync
}

//-----------------------------------------------------------------------------
//
// Doom-style constructor
//
//-----------------------------------------------------------------------------

DStrobe::DStrobe (sector_t *sector, int utics, int ltics, bool inSync)
	: DLighting (sector)
{
	m_DarkTime = ltics;
	m_BrightTime = utics;

	m_MaxLight = sector->lightlevel;
	m_MinLight = sector->FindMinSurroundingLight (sector->lightlevel);

	if (m_MinLight == m_MaxLight)
		m_MinLight = 0;

	m_Count = inSync ? 1 : (pr_strobeflash() & 7) + 1;
}



//-----------------------------------------------------------------------------
//
// Start strobing lights (usually from a trigger)
// [RH] Made it more configurable.
//
//-----------------------------------------------------------------------------

void EV_StartLightStrobing (int tag, int upper, int lower, int utics, int ltics)
{
	int secnum;
	FSectorTagIterator it(tag);
	while ((secnum = it.Next()) >= 0)
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
	FSectorTagIterator it(tag);
	while ((secnum = it.Next()) >= 0)
	{
		sector_t *sec = &sectors[secnum];
		if (sec->lightingdata)
			continue;
		
		new DStrobe (sec, utics, ltics, false);
	}
}


//-----------------------------------------------------------------------------
//
// TURN LINE'S TAG LIGHTS OFF
// [RH] Takes a tag instead of a line
//
//-----------------------------------------------------------------------------

void EV_TurnTagLightsOff (int tag)
{
	int secnum;
	FSectorTagIterator it(tag);
	while ((secnum = it.Next()) >= 0)
	{
		sector_t *sector = sectors + secnum;
		int min = sector->lightlevel;

		for (int i = 0; i < sector->linecount; i++)
		{
			sector_t *tsec = getNextSector (sector->lines[i],sector);
			if (!tsec)
				continue;
			if (tsec->lightlevel < min)
				min = tsec->lightlevel;
		}
		sector->SetLightLevel(min);
	}
}


//-----------------------------------------------------------------------------
//
// TURN LINE'S TAG LIGHTS ON
// [RH] Takes a tag instead of a line
//
//-----------------------------------------------------------------------------

void EV_LightTurnOn (int tag, int bright)
{
	int secnum;
	FSectorTagIterator it(tag);
	while ((secnum = it.Next()) >= 0)
	{
		sector_t *sector = sectors + secnum;
		int tbright = bright; //jff 5/17/98 search for maximum PER sector

		// bright = -1 means to search ([RH] Not 0)
		// for highest light level
		// surrounding sector
		if (bright < 0)
		{
			int j;

			for (j = 0; j < sector->linecount; j++)
			{
				sector_t *temp = getNextSector (sector->lines[j], sector);

				if (!temp)
					continue;

				if (temp->lightlevel > tbright)
					tbright = temp->lightlevel;
			}
		}
		sector->SetLightLevel(tbright);

		//jff 5/17/98 unless compatibility optioned
		//then maximum near ANY tagged sector
		if (i_compatflags & COMPATF_LIGHT)
		{
			bright = tbright;
		}
	}
}

//-----------------------------------------------------------------------------
//
// killough 10/98
//
// EV_LightTurnOnPartway
//
// Turn sectors tagged to line lights on to specified or max neighbor level
//
// Passed the tag of sector(s) to light and a light level fraction between 0 and 1.
// Sets the light to min on 0, max on 1, and interpolates in-between.
// Used for doors with gradual lighting effects.
//
//-----------------------------------------------------------------------------

void EV_LightTurnOnPartway (int tag, fixed_t frac)
{
	frac = clamp<fixed_t> (frac, 0, FRACUNIT);

	// Search all sectors for ones with same tag as activating line
	int secnum;
	FSectorTagIterator it(tag);
	while ((secnum = it.Next()) >= 0)
	{
		sector_t *temp, *sector = &sectors[secnum];
		int j, bright = 0, min = sector->lightlevel;

		for (j = 0; j < sector->linecount; ++j)
		{
			if ((temp = getNextSector (sector->lines[j], sector)) != NULL)
			{
				if (temp->lightlevel > bright)
				{
					bright = temp->lightlevel;
				}
				if (temp->lightlevel < min)
				{
					min = temp->lightlevel;
				}
			}
		}
		sector->SetLightLevel(DMulScale16 (frac, bright, FRACUNIT-frac, min));
	}
}


//-----------------------------------------------------------------------------
//
// [RH] New function to adjust tagged sectors' light levels
//		by a relative amount. Light levels are clipped to
//		be within range for sector_t::lightlevel.
//
//-----------------------------------------------------------------------------

void EV_LightChange (int tag, int value)
{
	int secnum;
	FSectorTagIterator it(tag);
	while ((secnum = it.Next()) >= 0)
	{
		sectors[secnum].SetLightLevel(sectors[secnum].lightlevel + value);
	}
}

	
//-----------------------------------------------------------------------------
//
// Spawn glowing light
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS (DGlow)

DGlow::DGlow ()
{
}

void DGlow::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Direction << m_MaxLight << m_MinLight;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void DGlow::Tick ()
{
	int newlight = m_Sector->lightlevel;

	switch (m_Direction)
	{
	case -1:
		// DOWN
		newlight -= GLOWSPEED;
		if (newlight <= m_MinLight)
		{
			newlight += GLOWSPEED;
			m_Direction = 1;
		}
		break;
		
	case 1:
		// UP
		newlight += GLOWSPEED;
		if (newlight >= m_MaxLight)
		{
			newlight -= GLOWSPEED;
			m_Direction = -1;
		}
		break;
	}
	m_Sector->SetLightLevel(newlight);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

DGlow::DGlow (sector_t *sector)
	: DLighting (sector)
{
	m_MinLight = sector->FindMinSurroundingLight (sector->lightlevel);
	m_MaxLight = sector->lightlevel;
	m_Direction = -1;
}

//-----------------------------------------------------------------------------
//
// [RH] More glowing light, this time appropriate for Hexen-ish uses.
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS (DGlow2)

DGlow2::DGlow2 ()
{
}

void DGlow2::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_End << m_MaxTics << m_OneShot << m_Start << m_Tics;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void DGlow2::Tick ()
{
	if (m_Tics++ >= m_MaxTics)
	{
		if (m_OneShot)
		{
			m_Sector->SetLightLevel(m_End);
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

	m_Sector->SetLightLevel(((m_End - m_Start) * m_Tics) / m_MaxTics + m_Start);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

DGlow2::DGlow2 (sector_t *sector, int start, int end, int tics, bool oneshot)
	: DLighting (sector)
{
	m_Start = sector_t::ClampLight(start);
	m_End = sector_t::ClampLight(end);
	m_MaxTics = tics;
	m_Tics = -1;
	m_OneShot = oneshot;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void EV_StartLightGlowing (int tag, int upper, int lower, int tics)
{
	int secnum;

	// If tics is non-positive, then we can't really do anything.
	if (tics <= 0)
	{
		return;
	}

	if (upper < lower)
	{
		int temp = upper;
		upper = lower;
		lower = temp;
	}

	FSectorTagIterator it(tag);
	while ((secnum = it.Next()) >= 0)
	{
		sector_t *sec = &sectors[secnum];
		if (sec->lightingdata)
			continue;
		
		new DGlow2 (sec, upper, lower, tics, false);
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void EV_StartLightFading (int tag, int value, int tics)
{
	int secnum;
	FSectorTagIterator it(tag);
	while ((secnum = it.Next()) >= 0)
	{
		sector_t *sec = &sectors[secnum];
		if (sec->lightingdata)
			continue;

		if (tics <= 0)
		{
			sec->SetLightLevel(value);
		}
		else
		{
			// No need to fade if lightlevel is already at desired value.
			if (sec->lightlevel == value)
				continue;

			new DGlow2 (sec, sec->lightlevel, value, tics, true);
		}
	}
}


//-----------------------------------------------------------------------------
//
// [RH] Phased lighting ala Hexen, but implemented without the help of the Hexen source
// The effect is a little different, but close enough, I feel.
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS (DPhased)

DPhased::DPhased ()
{
}

void DPhased::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_BaseLevel << m_Phase;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void DPhased::Tick ()
{
	const int steps = 12;

	if (m_Phase < steps)
		m_Sector->SetLightLevel( ((255 - m_BaseLevel) * m_Phase) / steps + m_BaseLevel);
	else if (m_Phase < 2*steps)
		m_Sector->SetLightLevel( ((255 - m_BaseLevel) * (2*steps - m_Phase - 1) / steps
								+ m_BaseLevel));
	else
		m_Sector->SetLightLevel(m_BaseLevel);

	if (m_Phase == 0)
		m_Phase = 63;
	else
		m_Phase--;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

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

		int numsteps = PhaseHelper (sector->NextSpecialSector (
				sector->special == LightSequenceSpecial1 ?
					LightSequenceSpecial2 : LightSequenceSpecial1, prev),
				index + 1, l->m_BaseLevel, sector);
		l->m_Phase = ((numsteps - index - 1) * 64) / numsteps;

		sector->special = 0;

		return numsteps;
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

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
}

//============================================================================
//
// EV_StopLightEffect
//
// Stops a lighting effect that is currently running in a sector.
//
//============================================================================

void EV_StopLightEffect (int tag)
{
	TThinkerIterator<DLighting> iterator;
	DLighting *effect;

	while ((effect = iterator.Next()) != NULL)
	{
		if (tagManager.SectorHasTag(effect->GetSector(), tag))
		{
			effect->Destroy();
		}
	}
}
