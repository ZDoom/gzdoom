//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
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
//		Handle Sector base lighting effects.
//
//-----------------------------------------------------------------------------


#include "templates.h"
#include "m_random.h"

#include "doomdef.h"
#include "p_local.h"
#include "p_spec.h"

#include "p_lnspec.h"
#include "doomstat.h"
#include "p_maputl.h"
#include "g_levellocals.h"
#include "maploader/maploader.h"
#include "p_spec_thinkers.h"

// State.
#include "serializer.h"

static FRandom pr_flicker ("Flicker");
static FRandom pr_lightflash ("LightFlash");
static FRandom pr_strobeflash ("StrobeFlash");
static FRandom pr_fireflicker ("FireFlicker");


//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS(DLighting, false, false)

//-----------------------------------------------------------------------------
//
// FIRELIGHT FLICKER
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS(DFireFlicker, false, false)

void DFireFlicker::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("count", m_Count)
		("maxlight", m_MaxLight)
		("minlight", m_MinLight);
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

void DFireFlicker::Construct(sector_t *sector)
{
	Super::Construct(sector);
	m_MaxLight = sector->lightlevel;
	m_MinLight = sector_t::ClampLight(FindMinSurroundingLight(sector, sector->lightlevel) + 16);
	m_Count = 4;
}

void DFireFlicker::Construct(sector_t *sector, int upper, int lower)
{
	Super::Construct(sector);
	m_MaxLight = sector_t::ClampLight(upper);
	m_MinLight = sector_t::ClampLight(lower);
	m_Count = 4;
}

//-----------------------------------------------------------------------------
//
// [RH] flickering light like Hexen's
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS(DFlicker, false, false)

void DFlicker::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("count", m_Count)
		("maxlight", m_MaxLight)
		("minlight", m_MinLight);
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

void DFlicker::Construct(sector_t *sector, int upper, int lower)
{
	Super::Construct(sector);
	m_MaxLight = sector_t::ClampLight(upper);
	m_MinLight = sector_t::ClampLight(lower);
	sector->lightlevel = m_MaxLight;
	m_Count = (pr_flicker()&64)+1;
}

//-----------------------------------------------------------------------------
//
// BROKEN LIGHT FLASHING
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS(DLightFlash, false, false)

void DLightFlash::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("count", m_Count)
		("maxlight", m_MaxLight)
		("minlight", m_MinLight)
		("maxtime", m_MaxTime)
		("mintime", m_MinTime);
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

void DLightFlash::Construct(sector_t *sector)
{
	Super::Construct(sector);
	// Find light levels like Doom.
	m_MaxLight = sector->lightlevel;
	m_MinLight = FindMinSurroundingLight (sector, sector->lightlevel);
	m_MaxTime = 64;
	m_MinTime = 7;
	m_Count = (pr_lightflash() & m_MaxTime) + 1;
}
	
void DLightFlash::Construct (sector_t *sector, int min, int max)
{
	Super::Construct(sector);
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

IMPLEMENT_CLASS(DStrobe, false, false)

void DStrobe::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("count", m_Count)
		("maxlight", m_MaxLight)
		("minlight", m_MinLight)
		("darktime", m_DarkTime)
		("brighttime", m_BrightTime);
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

void DStrobe::Construct(sector_t *sector, int upper, int lower, int utics, int ltics)
{
	Super::Construct(sector);
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

void DStrobe::Construct(sector_t *sector, int utics, int ltics, bool inSync)
{
	Super::Construct(sector);
	m_DarkTime = ltics;
	m_BrightTime = utics;

	m_MaxLight = sector->lightlevel;
	m_MinLight = FindMinSurroundingLight (sector, sector->lightlevel);

	if (m_MinLight == m_MaxLight)
		m_MinLight = 0;

	m_Count = inSync ? 1 : (pr_strobeflash() & 7) + 1;
}




//-----------------------------------------------------------------------------
//
// Spawn glowing light
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS(DGlow, false, false)

void DGlow::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("direction", m_Direction)
		("maxlight", m_MaxLight)
		("minlight", m_MinLight);
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void DGlow::Tick ()
{
	const int GLOWSPEED = 8;
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

void DGlow::Construct(sector_t *sector)
{
	Super::Construct(sector);
	m_MinLight = FindMinSurroundingLight (sector, sector->lightlevel);
	m_MaxLight = sector->lightlevel;
	m_Direction = -1;
}

//-----------------------------------------------------------------------------
//
// [RH] More glowing light, this time appropriate for Hexen-ish uses.
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS(DGlow2, false, false)

void DGlow2::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("end", m_End)
		("maxtics", m_MaxTics)
		("oneshot", m_OneShot)
		("start", m_Start)
		("tics", m_Tics);
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

void DGlow2::Construct(sector_t *sector, int start, int end, int tics, bool oneshot)
{
	Super::Construct(sector);
	m_Start = sector_t::ClampLight(start);
	m_End = sector_t::ClampLight(end);
	m_MaxTics = tics;
	m_Tics = -1;
	m_OneShot = oneshot;
}

//-----------------------------------------------------------------------------
//
// [RH] Phased lighting ala Hexen, but implemented without the help of the Hexen source
// The effect is a little different, but close enough, I feel.
//
//-----------------------------------------------------------------------------

IMPLEMENT_CLASS(DPhased, false, false)

void DPhased::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("baselevel", m_BaseLevel)
		("phase", m_Phase);
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
	if (!sector || sector->validcount == validcount)
	{
		return index;
	}
	else
	{
		DPhased *l;
		int baselevel = sector->lightlevel ? sector->lightlevel : light;
		sector->validcount = validcount;

		if (index == 0)
		{
			l = this;
			m_BaseLevel = baselevel;
		}
		else
			l = Level->CreateThinker<DPhased> (sector, baselevel);

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

void DPhased::Propagate()
{
	validcount++;
	PhaseHelper (m_Sector, 0, 0, nullptr);
}

void DPhased::Construct (sector_t *sector, int baselevel, int phase)
{
	Super::Construct(sector);
	m_BaseLevel = baselevel;
	m_Phase = phase;
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FLevelLocals::EV_StartLightFlickering(int tag, int upper, int lower)
{
	int secnum;
	auto it = GetSectorTagIterator(tag);
	while ((secnum = it.Next()) >= 0)
	{
		CreateThinker<DFlicker>(&sectors[secnum], upper, lower);
	}
}


//-----------------------------------------------------------------------------
//
// Start strobing lights (usually from a trigger)
// [RH] Made it more configurable.
//
//-----------------------------------------------------------------------------

void FLevelLocals::EV_StartLightStrobing(int tag, int upper, int lower, int utics, int ltics)
{
	int secnum;
	auto it = GetSectorTagIterator(tag);
	while ((secnum = it.Next()) >= 0)
	{
		sector_t *sec = &sectors[secnum];
		if (sec->lightingdata)
			continue;

		CreateThinker<DStrobe>(sec, upper, lower, utics, ltics);
	}
}

void FLevelLocals::EV_StartLightStrobing(int tag, int utics, int ltics)
{
	int secnum;
	auto it = GetSectorTagIterator(tag);
	while ((secnum = it.Next()) >= 0)
	{
		sector_t *sec = &sectors[secnum];
		if (sec->lightingdata)
			continue;

		CreateThinker<DStrobe>(sec, utics, ltics, false);
	}
}


//-----------------------------------------------------------------------------
//
// TURN LINE'S TAG LIGHTS OFF
// [RH] Takes a tag instead of a line
//
//-----------------------------------------------------------------------------

void FLevelLocals::EV_TurnTagLightsOff(int tag)
{
	int secnum;
	auto it = GetSectorTagIterator(tag);
	while ((secnum = it.Next()) >= 0)
	{
		sector_t *sector = &sectors[secnum];
		int min = sector->lightlevel;

		for (auto ln : sector->Lines)
		{
			sector_t *tsec = getNextSector(ln, sector);
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

void FLevelLocals::EV_LightTurnOn(int tag, int bright)
{
	int secnum;
	auto it = GetSectorTagIterator(tag);
	while ((secnum = it.Next()) >= 0)
	{
		sector_t *sector = &sectors[secnum];
		int tbright = bright; //jff 5/17/98 search for maximum PER sector

		// bright = -1 means to search ([RH] Not 0)
		// for highest light level
		// surrounding sector
		if (bright < 0)
		{
			for (auto ln : sector->Lines)
			{
				sector_t *temp = getNextSector(ln, sector);

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

void FLevelLocals::EV_LightTurnOnPartway(int tag, double frac)
{
	frac = clamp(frac, 0., 1.);

	// Search all sectors for ones with same tag as activating line
	int secnum;
	auto it = GetSectorTagIterator(tag);
	while ((secnum = it.Next()) >= 0)
	{
		sector_t *temp, *sector = &sectors[secnum];
		int bright = 0, min = sector->lightlevel;

		for (auto ln : sector->Lines)
		{
			if ((temp = getNextSector(ln, sector)) != nullptr)
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
		sector->SetLightLevel(int(frac * bright + (1 - frac) * min));
	}
}


//-----------------------------------------------------------------------------
//
// [RH] New function to adjust tagged sectors' light levels
//		by a relative amount. Light levels are clipped to
//		be within range for sector_t::light
//
//-----------------------------------------------------------------------------

void FLevelLocals::EV_LightChange(int tag, int value)
{
	int secnum;
	auto it = GetSectorTagIterator(tag);
	while ((secnum = it.Next()) >= 0)
	{
		sectors[secnum].SetLightLevel(sectors[secnum].lightlevel + value);
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FLevelLocals::EV_StartLightGlowing(int tag, int upper, int lower, int tics)
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

	auto it = GetSectorTagIterator(tag);
	while ((secnum = it.Next()) >= 0)
	{
		sector_t *sec = &sectors[secnum];
		if (sec->lightingdata)
			continue;

		CreateThinker<DGlow2>(sec, upper, lower, tics, false);
	}
}

//-----------------------------------------------------------------------------
//
//
//
//-----------------------------------------------------------------------------

void FLevelLocals::EV_StartLightFading(int tag, int value, int tics)
{
	int secnum;
	auto it = GetSectorTagIterator(tag);
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

			CreateThinker<DGlow2>(sec, sec->lightlevel, value, tics, true);
		}
	}
}


//============================================================================
//
// EV_StopLightEffect
//
// Stops a lighting effect that is currently running in a sector.
//
//============================================================================

void FLevelLocals::EV_StopLightEffect (int tag)
{
	auto iterator = GetThinkerIterator<DLighting>(NAME_None, STAT_LIGHT);
	DLighting *effect;

	while ((effect = iterator.Next()) != nullptr)
	{
		if (SectorHasTag(effect->GetSector(), tag))
		{
			effect->Destroy();
		}
	}
}


