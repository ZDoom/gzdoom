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
	DECLARE_SERIAL (DEarthquake, DThinker);
public:
	DEarthquake (AActor *center, int intensity, int duration, int damrad, int tremrad);

	void RunThink ();

	AActor *m_Spot;
	fixed_t m_TremorBox[4];
	fixed_t m_DamageBox[4];
	int m_Intensity;
	int m_Countdown;
private:
	DEarthquake () {}
};

IMPLEMENT_SERIAL (DEarthquake, DThinker)

void DEarthquake::Serialize (FArchive &arc)
{
	int i;

	if (arc.IsStoring ())
	{
		arc << m_Spot << m_Intensity << m_Countdown;
		for (i = 0; i < 4; i++)
			arc << m_TremorBox[i] << m_DamageBox[i];
	}
	else
	{
		arc >> m_Spot >> m_Intensity >> m_Countdown;
		for (i = 0; i < 4; i++)
			arc >> m_TremorBox[i] >> m_DamageBox[i];
	}
}

void DEarthquake::RunThink ()
{
	int i;

	if (level.time % 48 == 0)
		S_Sound (m_Spot, CHAN_BODY, "world/quake", 1, ATTN_NORM);

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && !(players[i].cheats & CF_NOCLIP))
		{
			AActor *mo = players[i].mo;

			if (!(level.time & 7) &&
				 mo->x >= m_DamageBox[BOXLEFT] && mo->x < m_DamageBox[BOXRIGHT] &&
				 mo->y >= m_DamageBox[BOXTOP] && mo->y < m_DamageBox[BOXBOTTOM])
			{
				int mult = 1024 * m_Intensity;
				P_DamageMobj (mo, NULL, NULL, m_Intensity / 2, MOD_UNKNOWN);
				mo->momx += (P_Random (pr_quake)-128) * mult;
				mo->momy += (P_Random (pr_quake)-128) * mult;
			}

			if (mo->x >= m_TremorBox[BOXLEFT] && mo->x < m_TremorBox[BOXRIGHT] &&
				 mo->y >= m_TremorBox[BOXTOP] && mo->y < m_TremorBox[BOXBOTTOM])
			{
				players[i].xviewshift = m_Intensity;
			}
		}
	}
	if (--m_Countdown == 0)
	{
		delete this;
	}
}

static void setbox (fixed_t *box, AActor *c, fixed_t size)
{
	if (size)
	{
		box[BOXLEFT] = c->x - size + 1;
		box[BOXRIGHT] = c->x + size;
		box[BOXTOP] = c->y - size + 1;
		box[BOXBOTTOM] = c->y + size;
	} else
		box[BOXLEFT] = box[BOXRIGHT] = box[BOXTOP] = box[BOXBOTTOM] = 0;
}

DEarthquake::DEarthquake (AActor *center, int intensity, int duration,
						  int damrad, int tremrad)
{
	m_Spot = center;
	setbox (m_TremorBox, center, tremrad * FRACUNIT * 64);
	setbox (m_DamageBox, center, damrad * FRACUNIT * 64);
	m_Intensity = intensity;
	m_Countdown = duration;
}

BOOL P_StartQuake (int tid, int intensity, int duration, int damrad, int tremrad)
{
	AActor *center = NULL;
	BOOL res = false;

	if (intensity > 9)
		intensity = 9;
	else if (intensity < 1)
		intensity = 1;

	while ( (center = AActor::FindByTID (center, tid)) )
	{
		res = true;
		new DEarthquake (center, intensity, duration, damrad, tremrad);
	}
	
	return res;
}