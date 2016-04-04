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
#include "d_player.h"
#include "r_utility.h"

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
						  double waveSpeedX, double waveSpeedY, double waveSpeedZ, int falloff, int highpoint)
						  : DThinker(STAT_EARTHQUAKE)
{
	m_QuakeSFX = quakesound;
	m_Spot = center;
	// Radii are specified in tile units (64 pixels)
	m_DamageRadius = damrad << FRACBITS;
	m_TremorRadius = tremrad << FRACBITS;
	m_IntensityX = intensityX << FRACBITS;
	m_IntensityY = intensityY << FRACBITS;
	m_IntensityZ = intensityZ << FRACBITS;
	m_CountdownStart = duration;
	m_Countdown = duration;
	m_Flags = flags;
	m_WaveSpeedX = (float)waveSpeedX;
	m_WaveSpeedY = (float)waveSpeedY;
	m_WaveSpeedZ = (float)waveSpeedZ;
	m_Falloff = falloff << FRACBITS;
	m_Highpoint = highpoint;
	m_MiniCount = highpoint;
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
		m_IntensityX <<= FRACBITS;
		m_IntensityY <<= FRACBITS;
		m_IntensityZ <<= FRACBITS;
	}
	else
	{
		arc << m_WaveSpeedX << m_WaveSpeedY << m_WaveSpeedZ;
	}
	if (SaveVersion < 4534)
	{
		m_Falloff = m_Highpoint = m_MiniCount = 0;
	}
	else
	{
		arc << m_Falloff << m_Highpoint << m_MiniCount;
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

				dist = m_Spot->AproxDistance (victim, true);
				// Check if in damage radius
				if (dist < m_DamageRadius && victim->Z() <= victim->floorz)
				{
					if (pr_quake() < 50)
					{
						P_DamageMobj (victim, NULL, NULL, pr_quake.HitDice (1), NAME_None);
					}
					// Thrust player around
					angle_t an = victim->angle + ANGLE_1*pr_quake();
					if (m_IntensityX == m_IntensityY)
					{ // Thrust in a circle
						P_ThrustMobj (victim, an, m_IntensityX/2);
					}
					else
					{ // Thrust in an ellipse
						an >>= ANGLETOFINESHIFT;
						// So this is actually completely wrong, but it ought to be good
						// enough. Otherwise, I'd have to use tangents and square roots.
						victim->vel.x += FixedMul(m_IntensityX/2, finecosine[an]);
						victim->vel.y += FixedMul(m_IntensityY/2, finesine[an]);
					}
				}
			}
		}
	}
	
	if (m_MiniCount > 0)
		m_MiniCount--;
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
// DEarthquake :: GetModWave
//
// QF_WAVE converts intensity into amplitude and unlocks a new property, the 
// wave length. This is, in short, waves per second. Named waveMultiplier 
// because that's as the name implies: adds more waves per second.
//
//==========================================================================

fixed_t DEarthquake::GetModWave(double waveMultiplier) const
{
	double time = m_Countdown - FIXED2DBL(r_TicFrac);
	return FLOAT2FIXED(sin(waveMultiplier * time * (M_PI * 2 / TICRATE)));
}

//==========================================================================
//
// DEarthquake :: GetModIntensity
//
// Given a base intensity, modify it according to the quake's flags.
//
//==========================================================================

fixed_t DEarthquake::GetModIntensity(fixed_t intensity) const
{
	assert(m_CountdownStart >= m_Countdown);

	intensity += intensity;		// always doubled

	if (m_Flags & (QF_SCALEDOWN | QF_SCALEUP))
	{
		// Adjustable maximums must use a range between 1 and m_CountdownStart to constrain between no quake and full quake.
		bool check = !!(m_Highpoint > 0 && m_Highpoint < m_CountdownStart);
		int divider = (check) ? m_Highpoint : m_CountdownStart;
		int scalar;
		
		if ((m_Flags & (QF_SCALEDOWN | QF_SCALEUP)) == (QF_SCALEDOWN | QF_SCALEUP))
		{
			if (check)
			{
				if (m_MiniCount > 0)
					scalar = (m_Flags & QF_MAX) ? m_MiniCount : (m_Highpoint - m_MiniCount);
				else
				{
					divider = m_CountdownStart - m_Highpoint;
					scalar = (m_Flags & QF_MAX) ? (divider - m_Countdown) : m_Countdown;
				}
			}
			else
			{
				// Defaults to middle of the road.
				divider = m_CountdownStart;
				scalar = (m_Flags & QF_MAX) ? MAX(m_Countdown, m_CountdownStart - m_Countdown)
					: MIN(m_Countdown, m_CountdownStart - m_Countdown);
			}
			scalar = (scalar > divider) ? divider : scalar;

			if (m_Flags & QF_FULLINTENSITY)
			{
				scalar *= 2;
			}
		}
		else 
		{
			if (m_Flags & QF_SCALEDOWN)
			{
				scalar = m_Countdown;
			}
			else			// QF_SCALEUP
			{ 
				scalar = m_CountdownStart - m_Countdown;
				if (m_Highpoint > 0)
				{
					if ((m_Highpoint - m_MiniCount) < divider)
						scalar = m_Highpoint - m_MiniCount;
					else
						scalar = divider;
				}
			}
			scalar = (scalar > divider) ? divider : scalar;			
		}		
		assert(divider > 0);
		intensity = Scale(intensity, scalar, divider);
	}
	return intensity;
}

//==========================================================================
//
// DEarthquake :: GetFalloff
//
// Given the distance of the player from the quake, find the multiplier.
// Process everything as doubles, and output again as fixed_t (mainly
// because fixed_t was misbehaving here...)
//
//==========================================================================

fixed_t DEarthquake::GetFalloff(fixed_t dist) const
{
	if ((dist < m_Falloff) || (m_Falloff >= m_TremorRadius) || (m_Falloff <= 0) || (m_TremorRadius - m_Falloff <= 0))
	{ //Player inside the minimum falloff range, or safety check kicked in.
		return FRACUNIT;
	}
	else if ((dist > m_Falloff) && (dist < m_TremorRadius))
	{ //Player inside the radius, and outside the min distance for falloff.
		fixed_t tremorsize = m_TremorRadius - m_Falloff;
		assert(tremorsize > 0);
		return (FRACUNIT - FixedDiv((dist - m_Falloff), tremorsize));
	}
	else 
	{ //Shouldn't happen.
		return FRACUNIT;
	}
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
			fixed_t dist = quake->m_Spot->AproxDistance (victim, true);
			if (dist < quake->m_TremorRadius)
			{
				fixed_t falloff = quake->GetFalloff(dist);
				++count;
				fixed_t x = quake->GetModIntensity(quake->m_IntensityX);
				fixed_t y = quake->GetModIntensity(quake->m_IntensityY);
				fixed_t z = quake->GetModIntensity(quake->m_IntensityZ);
				

				if (!(quake->m_Flags & QF_WAVE))
				{
					jiggers.Falloff = MAX(falloff, jiggers.Falloff);
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
					jiggers.WFalloff = MAX(falloff, jiggers.WFalloff);
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
	double waveSpeedX, double waveSpeedY, double waveSpeedZ, int falloff, int highpoint)
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
				quakesfx, flags, waveSpeedX, waveSpeedY, waveSpeedZ, falloff, highpoint);
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
				quakesfx, flags, waveSpeedX, waveSpeedY, waveSpeedZ, falloff, highpoint);
		}
	}
	
	return res;
}

bool P_StartQuake(AActor *activator, int tid, int intensity, int duration, int damrad, int tremrad, FSoundID quakesfx)
{	//Maintains original behavior by passing 0 to intensityZ, flags, and everything else after QSFX.
	return P_StartQuakeXYZ(activator, tid, intensity, intensity, 0, duration, damrad, tremrad, quakesfx, 0, 0, 0, 0, 0, 0);
}
