//-----------------------------------------------------------------------------
//
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
// Hexen's lightning system
//

#include "a_lightning.h"
#include "doomstat.h"
#include "p_lnspec.h"
#include "m_random.h"
#include "templates.h"
#include "s_sound.h"
#include "p_acs.h"
#include "r_sky.h"
#include "g_level.h"
#include "r_state.h"
#include "serializer.h"
#include "g_levellocals.h"
#include "events.h"
#include "gi.h"

static FRandom pr_lightning ("Lightning");

IMPLEMENT_CLASS(DLightningThinker, false, false)

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DLightningThinker::Construct()
{
	Stopped = false;
	LightningFlashCount = 0;
	NextLightningFlash = ((pr_lightning()&15)+5)*35; // don't flash at level start

	LightningLightLevels.Resize(Level->sectors.Size());
	fillshort(&LightningLightLevels[0], LightningLightLevels.Size(), SHRT_MAX);
}

DLightningThinker::~DLightningThinker ()
{
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DLightningThinker::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("stopped", Stopped)
		("next", NextLightningFlash)
		("count", LightningFlashCount)
		("levels", LightningLightLevels);
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DLightningThinker::Tick ()
{
	if (!NextLightningFlash || LightningFlashCount)
	{
		LightningFlash();
	}
	else
	{
		--NextLightningFlash;
		if (Stopped) Destroy();
	}
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DLightningThinker::LightningFlash ()
{
	int i, j;
	sector_t *tempSec;
	uint8_t flashLight;

	if (LightningFlashCount)
	{
		LightningFlashCount--;
		if (LightningFlashCount)
		{ // reduce the brightness of the flash
			tempSec = &Level->sectors[0];
			for (i = Level->sectors.Size(), j = 0; i > 0; ++j, --i, ++tempSec)
			{
				// [RH] Checking this sector's applicability to lightning now
				// is not enough to know if we should lower its light level,
				// because it might have changed since the lightning flashed.
				// Instead, change the light if this sector was effected by
				// the last flash.
				if (LightningLightLevels[j] < tempSec->lightlevel-4)
				{
					tempSec->ChangeLightLevel(-4);
				}
			}
		}					
		else
		{ // remove the alternate lightning flash special
			tempSec = &Level->sectors[0];
			for (i = Level->sectors.Size(), j = 0; i > 0; ++j, --i, ++tempSec)
			{
				if (LightningLightLevels[j] != SHRT_MAX)
				{
					tempSec->SetLightLevel(LightningLightLevels[j]);
				}
			}
			fillshort(&LightningLightLevels[0], Level->sectors.Size(), SHRT_MAX);
			Level->flags &= ~LEVEL_SWAPSKIES;
		}
		return;
	}

	LightningFlashCount = (pr_lightning()&7)+8;
	flashLight = 200+(pr_lightning()&31);
	tempSec = &Level->sectors[0];
	for (i = Level->sectors.Size(), j = 0; i > 0; ++j, --i, ++tempSec)
	{
		// allow combination of the lightning sector specials with bit masks
		int special = tempSec->special;
		if (tempSec->GetTexture(sector_t::ceiling) == skyflatnum
			|| special == Light_IndoorLightning1
			|| special == Light_IndoorLightning2
			|| special == Light_OutdoorLightning)
		{
			LightningLightLevels[j] = tempSec->lightlevel;
			if (special == Light_IndoorLightning1)
			{
				tempSec->SetLightLevel(MIN<int> (tempSec->lightlevel+64, flashLight));
			}
			else if (special == Light_IndoorLightning2)
			{
				tempSec->SetLightLevel(MIN<int> (tempSec->lightlevel+32, flashLight));
			}
			else
			{
				tempSec->SetLightLevel(flashLight);
			}
			if (tempSec->lightlevel < LightningLightLevels[j])
			{ // The lightning is darker than this sector already is, so no lightning here.
				tempSec->SetLightLevel(LightningLightLevels[j]);
				LightningLightLevels[j] = SHRT_MAX;
			}
		}
	}

	Level->flags |= LEVEL_SWAPSKIES;	// set alternate sky
	S_Sound (CHAN_AUTO, "world/thunder", 1.0, ATTN_NONE);
	// [ZZ] just in case
	Level->localEventManager->WorldLightning();
	// start LIGHTNING scripts
	Level->Behaviors.StartTypedScripts (SCRIPT_Lightning, NULL, false);	// [RH] Run lightning scripts

	// Calculate the next lighting flash
	if (!NextLightningFlash)
	{
		if (pr_lightning() < 50)
		{ // Immediate Quick flash
			NextLightningFlash = (pr_lightning()&15)+16;
		}
		else
		{
			if (pr_lightning() < 128 && !(Level->time&32))
			{
				NextLightningFlash = ((pr_lightning()&7)+2)*35;
			}
			else
			{
				NextLightningFlash = ((pr_lightning()&15)+5)*35;
			}
		}
	}
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void DLightningThinker::ForceLightning (int mode)
{
	switch (mode)
	{
	default:
		NextLightningFlash = 0;
		break;

	case 1:
		NextLightningFlash = 0;
		// Fall through
	case 2:
		Stopped = true;
		break;
	}
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

static DLightningThinker *LocateLightning (FLevelLocals *Level)
{
	auto iterator = Level->GetThinkerIterator<DLightningThinker>(NAME_None, STAT_LIGHTNING);
	return iterator.Next ();
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void FLevelLocals::StartLightning ()
{
	const bool isOriginalHexen = (gameinfo.gametype == GAME_Hexen)
		&& (flags2 & LEVEL2_HEXENHACK);

	if (isOriginalHexen)
	{
		bool hasLightning = false;

		for (const sector_t &sector : sectors)
		{
			hasLightning = sector.GetTexture(sector_t::ceiling) == skyflatnum
				|| sector.special == Light_IndoorLightning1
				|| sector.special == Light_IndoorLightning2;

			if (hasLightning)
			{
				break;
			}
		}

		if (!hasLightning)
		{
			flags &= ~LEVEL_STARTLIGHTNING;
			return;
		}
	}

	DLightningThinker *lightning = LocateLightning (this);
	if (lightning == nullptr)
	{
		CreateThinker<DLightningThinker>();
	}
}

//----------------------------------------------------------------------------
//
//
//
//----------------------------------------------------------------------------

void FLevelLocals::ForceLightning (int mode)
{
	DLightningThinker *lightning = LocateLightning (this);
	if (lightning == nullptr)
	{
		lightning = CreateThinker<DLightningThinker>();
	}
	if (lightning != nullptr)
	{
		lightning->ForceLightning (mode);
	}
}
