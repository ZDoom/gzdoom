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
						  int damrad, int tremrad, FSoundID quakesound, int flags, 
						  fixed_t waveSpeedX, fixed_t waveSpeedY, fixed_t waveSpeedZ)
						  : DThinker(STAT_EARTHQUAKE)
{
	m_QuakeSFX = quakesound;
	m_Spot = center;
	// Radii are specified in tile units (64 pixels)
	m_DamageRadius = damrad << (FRACBITS);
	m_TremorRadius = tremrad << (FRACBITS);
	m_IntensityX = intensityX;
	m_IntensityY = intensityY;
	m_IntensityZ = intensityZ;
	m_CountdownStart = duration;
	m_Countdown = duration;
	m_Flags = flags;
	m_WaveSpeedX = waveSpeedX;
	m_WaveSpeedY = waveSpeedY;
	m_WaveSpeedZ = waveSpeedZ;
}

//==========================================================================
//
// DEarthquake :: Serialize
//
//==========================================================================

void DEarthquake::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Spot << m_IntensityX << m_Countdown
		<< m_TremorRadius << m_DamageRadius
		<< m_QuakeSFX;
	if (SaveVersion < 4519)
	{
		m_IntensityY = m_IntensityX;
		m_IntensityZ = 0;
		m_Flags = 0;
	}
	else
	{
		arc << m_IntensityY << m_IntensityZ << m_Flags;
	}
	if (SaveVersion < 4520)
	{
		m_CountdownStart = 0;
	}
	else
	{
		arc << m_CountdownStart;
	}
	if (SaveVersion < 4521)
	{
		m_WaveSpeedX = m_WaveSpeedY = m_WaveSpeedZ = 0;
	}
	else
	{
		arc << m_WaveSpeedX << m_WaveSpeedY << m_WaveSpeedZ;
	}
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
					if (m_IntensityX == m_IntensityY)
					{ // Thrust in a circle
						P_ThrustMobj (victim, an, m_IntensityX << (FRACBITS-1));
					}
					else
					{ // Thrust in an ellipse
						an >>= ANGLETOFINESHIFT;
						// So this is actually completely wrong, but it ought to be good
						// enough. Otherwise, I'd have to use tangents and square roots.
						victim->velx += FixedMul(m_IntensityX << (FRACBITS-1), finecosine[an]);
						victim->vely += FixedMul(m_IntensityY << (FRACBITS-1), finesine[an]);
					}
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

fixed_t DEarthquake::GetModWave(fixed_t waveMultiplier) const
{
	//QF_WAVE converts intensity into amplitude and unlocks a new property, the wave length.
	//This is, in short, waves per second (full cycles, mind you, from 0 to 360.)
	//Named waveMultiplier because that's as the name implies: adds more waves per second.

	fixed_t wavesPerSecond = (waveMultiplier >> 15) * m_Countdown % (TICRATE * 2);
	fixed_t index = ((wavesPerSecond * (FINEANGLES / 2)) / (TICRATE));
	return finesine[index & FINEMASK];
}

//==========================================================================
//
// DEarthquake :: GetModIntensity
//
// Given a base intensity, modify it according to the quake's flags.
//
//==========================================================================

fixed_t DEarthquake::GetModIntensity(int intensity) const
{
	assert(m_CountdownStart >= m_Countdown);
	intensity += intensity;		// always doubled

	if (m_Flags & (QF_SCALEDOWN | QF_SCALEUP))
	{
		int scalar;
		if ((m_Flags & (QF_SCALEDOWN | QF_SCALEUP)) == (QF_SCALEDOWN | QF_SCALEUP))
		{
			scalar = (m_Flags & QF_MAX) ? MAX(m_Countdown, m_CountdownStart - m_Countdown)
				: MIN(m_Countdown, m_CountdownStart - m_Countdown);

			if (m_Flags & QF_FULLINTENSITY)
			{
				scalar *= 2;
			}
		}
		else if (m_Flags & QF_SCALEDOWN)
		{
			scalar = m_Countdown;
		}
		else			// QF_SCALEUP
		{
			scalar = m_CountdownStart - m_Countdown;
		}
		assert(m_CountdownStart > 0);
		intensity = intensity * (scalar << FRACBITS) / m_CountdownStart;
	}
	else
	{
		intensity <<= FRACBITS;
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
// Pre: jiggers was pre-zeroed by the caller.
//
//==========================================================================

int DEarthquake::StaticGetQuakeIntensities(AActor *victim, FQuakeJiggers &jiggers)
{
	if (victim->player != NULL && (victim->player->cheats & CF_NOCLIP))
	{
		return 0;
	}

	TThinkerIterator<DEarthquake> iterator(STAT_EARTHQUAKE);
	DEarthquake *quake;
	int count = 0;

	while ( (quake = iterator.Next()) != NULL)
	{
		if (quake->m_Spot != NULL)
		{
			fixed_t dist = P_AproxDistance (victim->x - quake->m_Spot->x,
				victim->y - quake->m_Spot->y);
			if (dist < quake->m_TremorRadius)
			{
				++count;
				fixed_t x = quake->GetModIntensity(quake->m_IntensityX);
				fixed_t y = quake->GetModIntensity(quake->m_IntensityY);
				fixed_t z = quake->GetModIntensity(quake->m_IntensityZ);
				if (!(quake->m_Flags & QF_WAVE))
				{
					if (quake->m_Flags & QF_RELATIVE)
					{
						jiggers.RelIntensityX = MAX(x, jiggers.RelIntensityX);
						jiggers.RelIntensityY = MAX(y, jiggers.RelIntensityY);
						jiggers.RelIntensityZ = MAX(z, jiggers.RelIntensityZ);
					}
					else
					{
						jiggers.IntensityX = MAX(x, jiggers.IntensityX);
						jiggers.IntensityY = MAX(y, jiggers.IntensityY);
						jiggers.IntensityZ = MAX(z, jiggers.IntensityZ);
					}
				}
				else
				{
					fixed_t mx = FixedMul(x, quake->GetModWave(quake->m_WaveSpeedX));
					fixed_t my = FixedMul(y, quake->GetModWave(quake->m_WaveSpeedY));
					fixed_t mz = FixedMul(z, quake->GetModWave(quake->m_WaveSpeedZ));

					// [RH] This only gives effect to the last sine quake. I would
					// prefer if some way was found to make multiples coexist
					// peacefully, but just summing them together is undesirable
					// because they could cancel each other out depending on their
					// relative phases.
					if (quake->m_Flags & QF_RELATIVE)
					{
						jiggers.RelOffsetX = mx;
						jiggers.RelOffsetY = my;
						jiggers.RelOffsetZ = mz;
					}
					else
					{
						jiggers.OffsetX = mx;
						jiggers.OffsetY = my;
						jiggers.OffsetZ = mz;
					}
				}
			}
		}
	}
	return count;
}

//==========================================================================
//
// P_StartQuake
//
//==========================================================================

bool P_StartQuakeXYZ(AActor *activator, int tid, int intensityX, int intensityY, int intensityZ, int duration,
	int damrad, int tremrad, FSoundID quakesfx, int flags,
	fixed_t waveSpeedX, fixed_t waveSpeedY, fixed_t waveSpeedZ)
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
			new DEarthquake(activator, intensityX, intensityY, intensityZ, duration, damrad, tremrad,
				quakesfx, flags, waveSpeedX, waveSpeedY, waveSpeedZ);
			return true;
		}
	}
	else
	{
		FActorIterator iterator (tid);
		while ( (center = iterator.Next ()) )
		{
			res = true;
			new DEarthquake(center, intensityX, intensityY, intensityZ, duration, damrad, tremrad,
				quakesfx, flags, waveSpeedX, waveSpeedY, waveSpeedZ);
		}
	}
	
	return res;
}

bool P_StartQuake(AActor *activator, int tid, int intensity, int duration, int damrad, int tremrad, FSoundID quakesfx)
{	//Maintains original behavior by passing 0 to intensityZ, and flags.
	return P_StartQuakeXYZ(activator, tid, intensity, intensity, 0, duration, damrad, tremrad, quakesfx, 0, 0, 0, 0);
}
