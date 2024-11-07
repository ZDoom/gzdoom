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
// Hexen's earthquake system, significantly enhanced
//


#include "doomtype.h"
#include "doomstat.h"
#include "p_local.h"
#include "actor.h"
#include "a_sharedglobal.h"
#include "serializer.h"
#include "serialize_obj.h"
#include "d_player.h"
#include "r_utility.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include <p_maputl.h>

static FCRandom pr_quake ("Quake");

IMPLEMENT_CLASS(DEarthquake, false, true)

IMPLEMENT_POINTERS_START(DEarthquake)
	IMPLEMENT_POINTER(m_Spot)
IMPLEMENT_POINTERS_END

//==========================================================================
//
// DEarthquake :: DEarthquake public constructor
//
//==========================================================================

void DEarthquake::Construct(AActor *center, double intensityX, double intensityY, double intensityZ, int duration,
	double damrad, double tremrad, FSoundID quakesound, int flags,
	double waveSpeedX, double waveSpeedY, double waveSpeedZ, double falloff, int highpoint, 
	double rollIntensity, double rollWave, double damageMultiplier, double thrustMultiplier, int damage)
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
	m_DamageMultiplier = damageMultiplier;
	m_ThrustMultiplier = thrustMultiplier;
	m_Damage = damage;
}

//==========================================================================
//
// DEarthquake :: Serialize
//
//==========================================================================

void DEarthquake::Serialize(FSerializer &arc)
{
	Super::Serialize (arc);
	arc("spot", m_Spot)
		("intensity", m_Intensity)
		("countdown", m_Countdown)
		("tremorradius", m_TremorRadius)
		("damageradius", m_DamageRadius)
		("quakesfx", m_QuakeSFX)
		("quakeflags", m_Flags)
		("countdownstart", m_CountdownStart)
		("wavespeed", m_WaveSpeed)
		("falloff", m_Falloff)
		("highpoint", m_Highpoint)
		("minicount", m_MiniCount)
		("rollintensity", m_RollIntensity)
		("rollwave", m_RollWave);
		("damagemultiplier", m_DamageMultiplier);
		("thrustmultiplier", m_ThrustMultiplier);
		("damage", m_Damage);
}

//==========================================================================
//
// DEarthquake :: Tick
//
// Deals damage to any actors near the earthquake and makes sure it's
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
		S_Sound (m_Spot, CHAN_BODY, CHANF_LOOP, m_QuakeSFX, 1, ATTN_NORM);
	}

	if (m_DamageRadius > 0)
	{
		if (m_Flags & QF_AFFECTACTORS)
		{
			FPortalGroupArray check(FPortalGroupArray::PGA_Full3d);
			FMultiBlockThingsIterator it(check,m_Spot,m_DamageRadius,false);
			FMultiBlockThingsIterator::CheckResult cres;

			while (it.Next(&cres))
			{
				AActor *mo = cres.thing;
				if (mo == nullptr || mo == m_Spot) //Ignore null references and the earthquake origin.
					continue;

				DoQuakeDamage(this, mo, !!(m_Flags & QF_DAMAGEFALLOFF));
			}
		}
		else
		{
			for (i = 0; i < MAXPLAYERS; i++)
			{
				if (Level->PlayerInGame(i) && !(Level->Players[i]->cheats & CF_NOCLIP))
				{
					AActor* victim = Level->Players[i]->mo;
					DoQuakeDamage(this, victim, !!(m_Flags & QF_DAMAGEFALLOFF));
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
// DEarthquake :: DoQuakeDamage
//
// Handles performing earthquake damage and thrust to the specified victim.
//
//==========================================================================

void DEarthquake::DoQuakeDamage(DEarthquake *quake, AActor *victim, bool falloff) const
{
	double dist;
	double thrustfalloff;
	int damage;

	if (!quake || !victim) return;

	dist = quake->m_Spot->Distance2D(victim);
	thrustfalloff = falloff ? GetFalloff(dist, m_DamageRadius) : 1.0;
	// Check if in damage radius
	if (dist < m_DamageRadius && victim->Z() <= victim->floorz)
	{
		if (!(quake->m_Flags & QF_SHAKEONLY) && pr_quake() < 50)
		{
			if (m_Damage < 1)
				damage = falloff ? (int)(pr_quake.HitDice(1) * GetFalloff(dist, m_DamageRadius) * m_DamageMultiplier) : (int)(pr_quake.HitDice(1) * m_DamageMultiplier);
			//[inkoalawetrust] Do the exact specified damage.
			else
				damage = falloff ? (int)(m_Damage * GetFalloff(dist, m_DamageRadius) * m_DamageMultiplier) : (int)(m_Damage * m_DamageMultiplier);

			damage = damage < 1 ? 1 : damage; //Do at least a tiny bit of damage when in radius.
			
			P_DamageMobj(victim, NULL, NULL, damage, NAME_Quake);
		}
		// Thrust pushable actor around
		if (!(victim->flags7 & MF7_DONTTHRUST) && m_ThrustMultiplier > 0)
		{
			DAngle an = victim->Angles.Yaw + DAngle::fromDeg(pr_quake());
			victim->Vel.X += m_Intensity.X * an.Cos() * m_ThrustMultiplier * thrustfalloff;
			victim->Vel.Y += m_Intensity.Y * an.Sin() * m_ThrustMultiplier * thrustfalloff;
		}
	}
	return;
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

double DEarthquake::GetModWave(double ticFrac, double waveMultiplier) const
{
	double time = m_Countdown - ticFrac;
	return g_sin(waveMultiplier * time * (M_PI * 2 / TICRATE));
}

//==========================================================================
//
// DEarthquake :: GetModIntensity
//
// Given a base intensity, modify it according to the quake's flags.
//
//==========================================================================

double DEarthquake::GetModIntensity(double intensity, bool fake) const
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
				scalar = (m_Flags & QF_MAX) ? max(m_Countdown, m_CountdownStart - m_Countdown)
					: min(m_Countdown, m_CountdownStart - m_Countdown);
			}
			scalar = (scalar > divider) ? divider : scalar;

			if (!fake && (m_Flags & QF_FULLINTENSITY))
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
// Given the distance of the actor from the quake, find the multiplier.
//
//==========================================================================

double DEarthquake::GetFalloff(double dist, double radius) const
{
	if ((dist < m_Falloff) || (m_Falloff >= radius) || (m_Falloff <= 0) || (radius - m_Falloff <= 0))
	{ //Actor inside the minimum falloff range, or safety check kicked in.
		return 1.;
	}
	else if ((dist > m_Falloff) && (dist < radius))
	{ //Actor inside the radius, and outside the min distance for falloff.
		double tremorsize = radius - m_Falloff;
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

int DEarthquake::StaticGetQuakeIntensities(double ticFrac, AActor *victim, FQuakeJiggers &jiggers)
{
	if (victim->player != NULL && (victim->player->cheats & CF_NOCLIP))
	{
		return 0;
	}

	auto iterator = victim->Level->GetThinkerIterator<DEarthquake>(NAME_None, STAT_EARTHQUAKE);
	DEarthquake *quake;
	int count = 0;

	while ( (quake = iterator.Next()) != nullptr)
	{
		if (quake->m_Spot != nullptr && !(quake->m_Flags & QF_GROUNDONLY && victim->Z() > victim->floorz))
		{
			double dist;

			if (quake->m_Flags & QF_3D)	dist = quake->m_Spot->Distance3D(victim);
			else						dist = quake->m_Spot->Distance2D(victim);

			if (dist < quake->m_TremorRadius)
			{
				++count;
				const double falloff = quake->GetFalloff(dist, quake->m_TremorRadius);
				const double r = quake->GetModIntensity(quake->m_RollIntensity);
				const double strength = quake->GetModIntensity(1.0, true);
				DVector3 intensity;
				intensity.X = quake->GetModIntensity(quake->m_Intensity.X);
				intensity.Y = quake->GetModIntensity(quake->m_Intensity.Y);
				intensity.Z = quake->GetModIntensity(quake->m_Intensity.Z);

				if (!(quake->m_Flags & QF_WAVE))
				{
					jiggers.RollIntensity = max(r, jiggers.RollIntensity) * falloff;

					intensity *= falloff;
					if (quake->m_Flags & QF_RELATIVE)
					{
						jiggers.RelIntensity.X = max(intensity.X, jiggers.RelIntensity.X);
						jiggers.RelIntensity.Y = max(intensity.Y, jiggers.RelIntensity.Y);
						jiggers.RelIntensity.Z = max(intensity.Z, jiggers.RelIntensity.Z);
					}
					else
					{
						jiggers.Intensity.X = max(intensity.X, jiggers.Intensity.X);
						jiggers.Intensity.Y = max(intensity.Y, jiggers.Intensity.Y);
						jiggers.Intensity.Z = max(intensity.Z, jiggers.Intensity.Z);
					}
				}
				else
				{
					jiggers.RollWave = r * quake->GetModWave(ticFrac, quake->m_RollWave) * falloff * strength;

					
					intensity.X *= quake->GetModWave(ticFrac, quake->m_WaveSpeed.X);
					intensity.Y *= quake->GetModWave(ticFrac, quake->m_WaveSpeed.Y);
					intensity.Z *= quake->GetModWave(ticFrac, quake->m_WaveSpeed.Z);
					intensity *= strength * falloff;

					// [RH] This only gives effect to the last sine quake. I would
					// prefer if some way was found to make multiples coexist
					// peacefully, but just summing them together is undesirable
					// because they could cancel each other out depending on their
					// relative phases.

					// [MC] Now does so. And they stack rather well. I'm a little
					// surprised at how easy it was.

					
					if (quake->m_Flags & QF_RELATIVE)
					{
						jiggers.RelOffset += intensity;
					}
					else
					{
						jiggers.Offset += intensity;
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

bool P_StartQuakeXYZ(FLevelLocals *Level, AActor *activator, int tid, double intensityX, double intensityY, double intensityZ, int duration,
	double damrad, double tremrad, FSoundID quakesfx, int flags,
	double waveSpeedX, double waveSpeedY, double waveSpeedZ, double falloff, int highpoint, 
	double rollIntensity, double rollWave, double damageMultiplier, double thrustMultiplier, int damage)
{
	AActor *center;
	bool res = false;

	intensityX = clamp<double>(intensityX, 0.0, 9.0);
	intensityY = clamp<double>(intensityY, 0.0, 9.0);
	intensityZ = clamp<double>(intensityZ, 0.0, 9.0);

	if (tid == 0)
	{
		if (activator != NULL)
		{
			Level->CreateThinker<DEarthquake>(activator, intensityX, intensityY, intensityZ, duration, damrad, tremrad,
				quakesfx, flags, waveSpeedX, waveSpeedY, waveSpeedZ, falloff, highpoint, rollIntensity, rollWave, damageMultiplier, thrustMultiplier, damage);
			return true;
		}
	}
	else
	{
		auto iterator = Level->GetActorIterator(tid);
		while ( (center = iterator.Next ()) )
		{
			res = true;
			Level->CreateThinker<DEarthquake>(center, intensityX, intensityY, intensityZ, duration, damrad, tremrad,
				quakesfx, flags, waveSpeedX, waveSpeedY, waveSpeedZ, falloff, highpoint, rollIntensity, rollWave, damageMultiplier, thrustMultiplier, damage);
		}
	}
	
	return res;
}

bool P_StartQuake(FLevelLocals * Level, AActor * activator, int tid, double intensity, int duration, double damrad, double tremrad, FSoundID quakesfx)
{	//Maintains original behavior by passing 0 to intensityZ, flags, and everything else after QSFX.
	return P_StartQuakeXYZ(Level, activator, tid, intensity, intensity, 0, duration, damrad, tremrad, quakesfx, 0, 0, 0, 0, 0, 0, 0, 0, 1.0, 0.5, 0);
}
