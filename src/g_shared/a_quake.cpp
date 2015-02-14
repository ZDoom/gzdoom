#include "templates.h"
#include "doomtype.h"
#include "doomstat.h"
#include "p_local.h"
#include "actor.h"
#include "m_bbox.h"
#include "m_random.h"
#include "s_sound.h"
#include "a_sharedglobal.h"
#include "statnums.h"
#include "farchive.h"

static FRandom pr_quake ("Quake");

IMPLEMENT_POINTY_CLASS (DEarthquake)
 DECLARE_POINTER (m_Spot)
END_POINTERS

//==========================================================================
//
// DEarthquake :: DEarthquake private constructor
//
//==========================================================================

DEarthquake::DEarthquake()
: DThinker(STAT_EARTHQUAKE)
{
}

//==========================================================================
//
// DEarthquake :: DEarthquake public constructor
//
//==========================================================================

DEarthquake::DEarthquake (AActor *center, int intensityX, int intensityY, int intensityZ, int duration,
						  int damrad, int tremrad, FSoundID quakesound, int flags)
						  : DThinker(STAT_EARTHQUAKE)
{
	m_QuakeSFX = quakesound;
	m_Spot = center;
	// Radii are specified in tile units (64 pixels)
	m_DamageRadius = damrad << (FRACBITS);
	m_TremorRadius = tremrad << (FRACBITS);
	m_Intensity = intensityX;
	m_Countdown = duration;
	m_Flags = flags;
	m_iX = intensityX;
	m_iY = intensityY;
	m_iZ = intensityZ;
}

//==========================================================================
//
// DEarthquake :: Serialize
//
//==========================================================================

void DEarthquake::Serialize (FArchive &arc)
{
	Super::Serialize (arc); //[MC] m_Intensity is unused now but I don't want to break compatibility...
	arc << m_Spot << m_Intensity << m_Countdown
		<< m_TremorRadius << m_DamageRadius
		<< m_QuakeSFX << m_Flags << m_iX << m_iY << m_iZ;
}

//==========================================================================
//
// DEarthquake :: Tick
//
// Deals damage to any players near the earthquake and makes sure it's
// making noise.
//
//==========================================================================

void DEarthquake::Tick ()
{
	int i;

	if (m_Spot == NULL)
	{
		Destroy ();
		return;
	}

	if (!S_IsActorPlayingSomething (m_Spot, CHAN_BODY, m_QuakeSFX))
	{
		S_Sound (m_Spot, CHAN_BODY | CHAN_LOOP, m_QuakeSFX, 1, ATTN_NORM);
	}
	if (m_DamageRadius > 0)
	{
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i] && !(players[i].cheats & CF_NOCLIP))
			{
				AActor *victim = players[i].mo;
				fixed_t dist;

				dist = P_AproxDistance (victim->x - m_Spot->x, victim->y - m_Spot->y);
				// Check if in damage radius
				if (dist < m_DamageRadius && victim->z <= victim->floorz)
				{
					if (pr_quake() < 50)
					{
						P_DamageMobj (victim, NULL, NULL, pr_quake.HitDice (1), NAME_None);
					}
					// Thrust player around
					angle_t an = victim->angle + ANGLE_1*pr_quake();
					P_ThrustMobj (victim, an, m_Intensity << (FRACBITS-1));
				}
			}
		}
	}
	if (--m_Countdown == 0)
	{
		if (S_IsActorPlayingSomething(m_Spot, CHAN_BODY, m_QuakeSFX))
		{
			S_StopSound(m_Spot, CHAN_BODY);
		}
		Destroy();
	}
}

//==========================================================================
//
// DEarthquake::StaticGetQuakeIntensity
//
// Searches for all quakes near the victim and returns their combined
// intensity.
//
//==========================================================================

int DEarthquake::StaticGetQuakeIntensity (AActor *victim, int selector)
{
	int intensity = 0;
	int quakeIntensity = 0;
	TThinkerIterator<DEarthquake> iterator (STAT_EARTHQUAKE);
	DEarthquake *quake;

	if (victim->player != NULL && (victim->player->cheats & CF_NOCLIP))
	{
		return 0;
	}

	while ( (quake = iterator.Next()) != NULL)
	{
		if (quake->m_Spot != NULL)
		{
			fixed_t dist = P_AproxDistance (victim->x - quake->m_Spot->x,
				victim->y - quake->m_Spot->y);
			if (dist < quake->m_TremorRadius)
			{
				switch (selector)
				{
				default:
				case 0:				
					quakeIntensity = quake->m_iX;
					break;
				case 1:
					quakeIntensity = quake->m_iY;
					break;
				case 2:
					quakeIntensity = quake->m_iZ;
					break;
				
				}

				if (intensity < quakeIntensity)
					intensity = quakeIntensity;
			}
		}
	}
	return intensity;
}

//==========================================================================
//
// DEarthquake::StaticGetQuakeIntensity
//
// Searches for all quakes near the victim and returns their combined
// intensity.
//
//==========================================================================

int DEarthquake::StaticGetQuakeFlags(AActor *victim)
{
	int flags = 0;
	TThinkerIterator<DEarthquake> iterator(STAT_EARTHQUAKE);
	DEarthquake *quake;

	if (victim->player != NULL && (victim->player->cheats & CF_NOCLIP))
	{
		return 0;
	}

	while ((quake = iterator.Next()) != NULL)
	{
		if (quake->m_Spot != NULL)
		{
			fixed_t dist = P_AproxDistance(victim->x - quake->m_Spot->x,
				victim->y - quake->m_Spot->y);
			if (dist < quake->m_TremorRadius)
			{
				if (!(flags & QF_RELATIVE) && (quake->m_Flags & QF_RELATIVE))
					flags += QF_RELATIVE;
			}
		}
	}
	return flags;
}
//==========================================================================
//
// P_StartQuake
//
//==========================================================================

bool P_StartQuakeXYZ(AActor *activator, int tid, int intensityX, int intensityY, int intensityZ, int duration, int damrad, int tremrad, FSoundID quakesfx, int flags)
{
	AActor *center;
	bool res = false;

	if (intensityX)		intensityX = clamp(intensityX, 1, 9);
	if (intensityY)		intensityY = clamp(intensityY, 1, 9);
	if (intensityZ)		intensityZ = clamp(intensityZ, 1, 9);

	if (tid == 0)
	{
		if (activator != NULL)
		{
			new DEarthquake(activator, intensityX, intensityY, intensityZ, duration, damrad, tremrad, quakesfx, flags);
			return true;
		}
	}
	else
	{
		FActorIterator iterator (tid);
		while ( (center = iterator.Next ()) )
		{
			res = true;
			new DEarthquake(center, intensityX, intensityY, intensityZ, duration, damrad, tremrad, quakesfx, flags);
		}
	}
	
	return res;
}

bool P_StartQuake(AActor *activator, int tid, int intensity, int duration, int damrad, int tremrad, FSoundID quakesfx)
{	//Maintains original behavior by passing 0 to intensityZ, and flags.
	return P_StartQuakeXYZ(activator, tid, intensity, intensity, 0, duration, damrad, tremrad, quakesfx, 0);
}
