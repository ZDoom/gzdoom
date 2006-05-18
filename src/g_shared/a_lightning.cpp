#include "a_lightning.h"
#include "p_lnspec.h"
#include "statnums.h"
#include "r_data.h"
#include "m_random.h"
#include "templates.h"
#include "s_sound.h"
#include "p_acs.h"

static FRandom pr_lightning ("Lightning");

IMPLEMENT_CLASS (DLightningThinker)

DLightningThinker::DLightningThinker ()
	: DThinker (STAT_LIGHTNING)
{
	LightningLightLevels = NULL;
	LightningFlashCount = 0;
	NextLightningFlash = ((pr_lightning()&15)+5)*35; // don't flash at level start

	LightningLightLevels = new BYTE[numsectors + (numsectors+7)/8];
	memset (LightningLightLevels, 0, numsectors + (numsectors+7)/8);
}

DLightningThinker::~DLightningThinker ()
{
	if (LightningLightLevels != NULL)
	{
		delete[] LightningLightLevels;
	}
}

void DLightningThinker::Serialize (FArchive &arc)
{
	int i;
	BYTE *lights;

	Super::Serialize (arc);

	arc << NextLightningFlash << LightningFlashCount;

	if (arc.IsLoading ())
	{
		if (LightningLightLevels != NULL)
		{
			delete[] LightningLightLevels;
		}
		LightningLightLevels = new BYTE[numsectors + (numsectors+7)/8];
	}
	lights = LightningLightLevels;
	for (i = (numsectors + (numsectors+7)/8); i > 0; ++lights, --i)
	{
		arc << *lights;
	}
}

void DLightningThinker::Tick ()
{
	if (!NextLightningFlash || LightningFlashCount)
	{
		LightningFlash();
	}
	else
	{
		--NextLightningFlash;
	}
}

void DLightningThinker::LightningFlash ()
{
	int i, j;
	sector_t *tempSec;
	BYTE flashLight;

	if (LightningFlashCount)
	{
		LightningFlashCount--;
		if (LightningFlashCount)
		{ // reduce the brightness of the flash
			tempSec = sectors;
			for (i = numsectors, j = 0; i > 0; ++j, --i, ++tempSec)
			{
				// [RH] Checking this sector's applicability to lightning now
				// is not enough to know if we should lower its light level,
				// because it might have changed since the lightning flashed.
				// Instead, change the light if this sector was effected by
				// the last flash.
				if (LightningLightLevels[numsectors+(j>>3)] & (1<<(j&7)) &&
					LightningLightLevels[j] < tempSec->lightlevel-4)
				{
					tempSec->lightlevel -= 4;
				}
			}
		}					
		else
		{ // remove the alternate lightning flash special
			tempSec = sectors;
			for (i = numsectors, j = 0; i > 0; ++j, --i, ++tempSec)
			{
				if (LightningLightLevels[numsectors+(j>>3)] & (1<<(j&7)))
				{
					tempSec->lightlevel = LightningLightLevels[j];
				}
			}
			memset (&LightningLightLevels[numsectors], 0, (numsectors+7)/8);
			level.flags &= ~LEVEL_SWAPSKIES;
		}
		return;
	}

	LightningFlashCount = (pr_lightning()&7)+8;
	flashLight = 200+(pr_lightning()&31);
	tempSec = sectors;
	for (i = numsectors, j = 0; i > 0; --i, ++j, ++tempSec)
	{
		// allow combination of the lightning sector specials with bit masks
		int special = (tempSec->special&0xff);
		if (tempSec->ceilingpic == skyflatnum
			|| special == Light_IndoorLightning1
			|| special == Light_IndoorLightning2
			|| special == Light_OutdoorLightning)
		{
			LightningLightLevels[j] = tempSec->lightlevel;
			LightningLightLevels[numsectors+(j>>3)] |= 1<<(j&7);
			if (special == Light_IndoorLightning1)
			{
				tempSec->lightlevel = MIN<int> (tempSec->lightlevel+64, flashLight);
			}
			else if (special == Light_IndoorLightning2)
			{
				tempSec->lightlevel = MIN<int> (tempSec->lightlevel+32, flashLight);
			}
			else
			{
				tempSec->lightlevel = flashLight;
			}
			if (tempSec->lightlevel < LightningLightLevels[j])
			{
				tempSec->lightlevel = LightningLightLevels[j];
			}
		}
	}

	level.flags |= LEVEL_SWAPSKIES;	// set alternate sky
	S_Sound (CHAN_AUTO, "world/thunder", 1.0, ATTN_NONE);
	FBehavior::StaticStartTypedScripts (SCRIPT_Lightning, NULL, false);	// [RH] Run lightning scripts

	// Calculate the next lighting flash
	if (!NextLightningFlash)
	{
		if (pr_lightning() < 50)
		{ // Immediate Quick flash
			NextLightningFlash = (pr_lightning()&15)+16;
		}
		else
		{
			if (pr_lightning() < 128 && !(level.time&32))
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

void DLightningThinker::ForceLightning ()
{
	NextLightningFlash = 0;
}

static DLightningThinker *LocateLightning ()
{
	TThinkerIterator<DLightningThinker> iterator (STAT_LIGHTNING);
	return iterator.Next ();
}

void P_StartLightning ()
{
	DLightningThinker *lightning = LocateLightning ();
	if (lightning == NULL)
	{
		new DLightningThinker ();
	}
}

void P_StopLightning ()
{
	DLightningThinker *lightning = LocateLightning ();
	if (lightning != NULL)
	{
		lightning->Destroy ();
	}
}

void P_ForceLightning ()
{
	DLightningThinker *lightning = LocateLightning ();
	if (lightning != NULL)
	{
		lightning->ForceLightning ();
	}
}
