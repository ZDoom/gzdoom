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

DEarthquake::DEarthquake(AActor *center, int intensityX, int intensityY, int intensityZ, int duration,
	int damrad, int tremrad, FSoundID quakesound, int flags,
	double waveSpeedX, double waveSpeedY, double waveSpeedZ, int falloff, int highpoint, 
	double rollIntensity, double rollWave)
	: DThinker(STAT_EARTHQUAKE)
{
	m_QuakeSFX = quakesound;
	m_Spot = center;
	// Radii are specified in tile units (64 pixels)
	m_DamageRadius = damrad;
	m_TremorRadius = tremrad;
	m_Intensity = DVector3(intensityX, intensityY, intensityZ);
	m_CountdownStart = duration;
	m_Countdown = duration;
	m_Flags = flags;
	m_WaveSpeed = DVector3(waveSpeedX, waveSpeedY, waveSpeedZ);
	m_Falloff = falloff;
	m_Highpoint = highpoint;
	m_MiniCount = highpoint;
	m_RollIntensity = rollIntensity;
	m_RollWave = rollWave;
}

//==========================================================================
//
// DEarthquake :: Serialize
//
//==========================================================================

void DEarthquake::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Spot << m_Intensity << m_Countdown
		<< m_TremorRadius << m_DamageRadius
		<< m_QuakeSFX << m_Flags << m_CountdownStart
		<< m_WaveSpeed
		<< m_Falloff << m_Highpoint << m_MiniCount
		<< m_RollIntensity << m_RollWave;
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
				double dist;

				dist = m_Spot->Distance2D(victim, true);
				// Check if in damage radius
				if (dist < m_DamageRadius && victim->Z() <= victim->floorz)
				{
					if (pr_quake() < 50)
					{
						P_DamageMobj (victim, NULL, NULL, pr_quake.HitDice (1), NAME_None);
					}
					// Thrust player around
					DAngle an = victim->Angles.Yaw + pr_quake();
					victim->Vel.X += m_Intensity.X * an.Cos() * 0.5;
					victim->Vel.Y += m_Intensity.Y * an.Sin() * 0.5;
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

double DEarthquake::GetModWave(double waveMultiplier) const
{
	double time = m_Countdown - r_TicFracF;
	return g_sin(waveMultiplier * time * (M_PI * 2 / TICRATE));
}

//==========================================================================
//
// DEarthquake :: GetModIntensity
//
// Given a base intensity, modify it according to the quake's flags.
//
//==========================================================================

double DEarthquake::GetModIntensity(double intensity) const
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
		intensity = intensity * scalar / divider;
	}
	return intensity;
}

//==========================================================================
//
// DEarthquake :: GetFalloff
//
// Given the distance of the player from the quake, find the multiplier.
//
//==========================================================================

double DEarthquake::GetFalloff(double dist) const
{
	if ((dist < m_Falloff) || (m_Falloff >= m_TremorRadius) || (m_Falloff <= 0) || (m_TremorRadius - m_Falloff <= 0))
	{ //Player inside the minimum falloff range, or safety check kicked in.
		return 1.;
	}
	else if ((dist > m_Falloff) && (dist < m_TremorRadius))
	{ //Player inside the radius, and outside the min distance for falloff.
		double tremorsize = m_TremorRadius - m_Falloff;
		assert(tremorsize > 0);
		return (1. - ((dist - m_Falloff) / tremorsize));
	}
	else 
	{ //Shouldn't happen.
		return 1.;
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
			double dist = quake->m_Spot->Distance2D (victim, true);
			if (dist < quake->m_TremorRadius)
			{
				const double falloff = quake->GetFalloff(dist);
				const double rfalloff = (quake->m_RollIntensity != 0) ? falloff : 0.;
				++count;
				double x = quake->GetModIntensity(quake->m_Intensity.X);
				double y = quake->GetModIntensity(quake->m_Intensity.Y);
				double z = quake->GetModIntensity(quake->m_Intensity.Z);
				double r = quake->GetModIntensity(quake->m_RollIntensity);

				if (!(quake->m_Flags & QF_WAVE))
				{
					jiggers.Falloff = MAX(falloff, jiggers.Falloff);
					jiggers.RFalloff = MAX(rfalloff, jiggers.RFalloff);
					jiggers.RollIntensity = MAX(r, jiggers.RollIntensity);
					if (quake->m_Flags & QF_RELATIVE)
					{
						jiggers.RelIntensity.X = MAX(x, jiggers.RelIntensity.X);
						jiggers.RelIntensity.Y = MAX(y, jiggers.RelIntensity.Y);
						jiggers.RelIntensity.Z = MAX(z, jiggers.RelIntensity.Z);
					}
					else
					{
						jiggers.Intensity.X = MAX(x, jiggers.Intensity.X);
						jiggers.Intensity.Y = MAX(y, jiggers.Intensity.Y);
						jiggers.Intensity.Z = MAX(z, jiggers.Intensity.Z);
					}
				}
				else
				{
					jiggers.WFalloff = MAX(falloff, jiggers.WFalloff);
					jiggers.RWFalloff = MAX(rfalloff, jiggers.RWFalloff);
					jiggers.RollWave = r * quake->GetModWave(quake->m_RollWave);
					double mx = x * quake->GetModWave(quake->m_WaveSpeed.X);
					double my = y * quake->GetModWave(quake->m_WaveSpeed.Y);
					double mz = z * quake->GetModWave(quake->m_WaveSpeed.Z);

					// [RH] This only gives effect to the last sine quake. I would
					// prefer if some way was found to make multiples coexist
					// peacefully, but just summing them together is undesirable
					// because they could cancel each other out depending on their
					// relative phases.
					if (quake->m_Flags & QF_RELATIVE)
					{
						jiggers.RelOffset.X = mx;
						jiggers.RelOffset.Y = my;
						jiggers.RelOffset.Z = mz;
					}
					else
					{
						jiggers.Offset.X = mx;
						jiggers.Offset.Y = my;
						jiggers.Offset.Z = mz;
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
	double waveSpeedX, double waveSpeedY, double waveSpeedZ, int falloff, int highpoint, 
	double rollIntensity, double rollWave)
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
				quakesfx, flags, waveSpeedX, waveSpeedY, waveSpeedZ, falloff, highpoint, rollIntensity, rollWave);
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
				quakesfx, flags, waveSpeedX, waveSpeedY, waveSpeedZ, falloff, highpoint, rollIntensity, rollWave);
		}
	}
	
	return res;
}

bool P_StartQuake(AActor *activator, int tid, int intensity, int duration, int damrad, int tremrad, FSoundID quakesfx)
{	//Maintains original behavior by passing 0 to intensityZ, flags, and everything else after QSFX.
	return P_StartQuakeXYZ(activator, tid, intensity, intensity, 0, duration, damrad, tremrad, quakesfx, 0, 0, 0, 0, 0, 0, 0, 0);
}
