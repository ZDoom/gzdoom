#include "templates.h"
#include "doomtype.h"
#include "doomstat.h"
#include "p_local.h"
#include "actor.h"
#include "m_bbox.h"
#include "m_random.h"
#include "z_zone.h"
#include "s_sound.h"

class DEarthquake : public DThinker
{
	DECLARE_CLASS (DEarthquake, DThinker)
	HAS_OBJECT_POINTERS
public:
	DEarthquake (AActor *center, int intensity, int duration, int damrad, int tremrad);

	void Serialize (FArchive &arc);
	void Tick ();

	AActor *m_Spot;
	fixed_t m_TremorRadius, m_DamageRadius;
	int m_Intensity;
	int m_Countdown;
	int m_QuakeSFX;
private:
	DEarthquake () {}
};

IMPLEMENT_POINTY_CLASS (DEarthquake)
 DECLARE_POINTER (m_Spot)
END_POINTERS

void DEarthquake::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << m_Spot << m_Intensity << m_Countdown
		<< m_TremorRadius << m_DamageRadius;
	m_QuakeSFX = S_FindSound ("world/quake");
}

void DEarthquake::Tick ()
{
	int i;

	if (!S_GetSoundPlayingInfo (m_Spot, m_QuakeSFX))
		S_SoundID (m_Spot, CHAN_BODY, m_QuakeSFX, 1, ATTN_NORM);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && !(players[i].cheats & CF_NOCLIP))
		{
			AActor *victim = players[i].mo;
			fixed_t dist;

			dist = P_AproxDistance (victim->x - m_Spot->x, victim->y - m_Spot->y);
			// Tested in tile units (64 pixels)
			if (dist < m_TremorRadius)
			{
				players[i].xviewshift = m_Intensity;
			}
			// Check if in damage radius
			if (dist < m_DamageRadius && victim->z <= victim->floorz)
			{
				if (P_Random (pr_quake) < 50)
				{
					P_DamageMobj (victim, NULL, NULL, HITDICE(1), MOD_UNKNOWN);
				}
				// Thrust player around
				angle_t an = victim->angle + ANGLE_1*P_Random(pr_quake);
				P_ThrustMobj (victim, an, m_Intensity << (FRACBITS-1));
			}
		}
	}
	if (--m_Countdown == 0)
	{
		Destroy ();
	}
}

DEarthquake::DEarthquake (AActor *center, int intensity, int duration,
						  int damrad, int tremrad)
{
	m_QuakeSFX = S_FindSound ("world/quake");
	m_Spot = center;
	m_DamageRadius = damrad << (FRACBITS+6);
	m_TremorRadius = tremrad << (FRACBITS+6);
	m_Intensity = intensity;
	m_Countdown = duration;
}

bool P_StartQuake (int tid, int intensity, int duration, int damrad, int tremrad)
{
	AActor *center;
	FActorIterator iterator (tid);
	bool res = false;

	intensity = clamp (intensity, 1, 9);

	while ( (center = iterator.Next ()) )
	{
		res = true;
		new DEarthquake (center, intensity, duration, damrad, tremrad);
	}
	
	return res;
}
