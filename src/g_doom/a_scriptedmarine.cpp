#include "actor.h"
#include "p_enemy.h"
#include "a_action.h"
#include "r_draw.h"

// Scriptable marine -------------------------------------------------------

class AScriptedMarine : public AActor
{
	DECLARE_ACTOR (AScriptedMarine, AActor)
public:
	void Activate (AActor *activator);
	void Deactivate (AActor *activator);
};

FState AScriptedMarine::States[] =
{
#define S_MPLAY 0
	S_NORMAL (PLAY, 'A',   10, A_Look 						, &States[S_MPLAY]),

#define S_MPLAY_RUN (S_MPLAY+1)
	S_NORMAL (PLAY, 'A',	4, A_Chase 						, &States[S_MPLAY_RUN+1]),
	S_NORMAL (PLAY, 'B',	4, A_Chase 						, &States[S_MPLAY_RUN+2]),
	S_NORMAL (PLAY, 'C',	4, A_Chase 						, &States[S_MPLAY_RUN+3]),
	S_NORMAL (PLAY, 'D',	4, A_Chase 						, &States[S_MPLAY_RUN+0]),

#define S_MPLAY_ATK (S_MPLAY_RUN+4)
	S_NORMAL (PLAY, 'E',   12, A_FaceTarget					, &States[S_MPLAY]),
	S_BRIGHT (PLAY, 'F',	6, NULL 						, &States[S_MPLAY_ATK+0]),

#define S_MPLAY_PAIN (S_MPLAY_ATK+2)
	S_NORMAL (PLAY, 'G',	4, NULL 						, &States[S_MPLAY_PAIN+1]),
	S_NORMAL (PLAY, 'G',	4, A_Pain						, &States[S_MPLAY]),

#define S_MPLAY_DIE (S_MPLAY_PAIN+2)
	S_NORMAL (PLAY, 'H',   10, NULL 						, &States[S_MPLAY_DIE+1]),
	S_NORMAL (PLAY, 'I',   10, A_Scream						, &States[S_MPLAY_DIE+2]),
	S_NORMAL (PLAY, 'J',   10, A_NoBlocking					, &States[S_MPLAY_DIE+3]),
	S_NORMAL (PLAY, 'K',   10, NULL 						, &States[S_MPLAY_DIE+4]),
	S_NORMAL (PLAY, 'L',   10, NULL 						, &States[S_MPLAY_DIE+5]),
	S_NORMAL (PLAY, 'M',   10, NULL 						, &States[S_MPLAY_DIE+6]),
	S_NORMAL (PLAY, 'N',   -1, NULL							, NULL),

#define S_MPLAY_XDIE (S_MPLAY_DIE+7)
	S_NORMAL (PLAY, 'O',	5, NULL 						, &States[S_MPLAY_XDIE+1]),
	S_NORMAL (PLAY, 'P',	5, A_XScream					, &States[S_MPLAY_XDIE+2]),
	S_NORMAL (PLAY, 'Q',	5, A_NoBlocking					, &States[S_MPLAY_XDIE+3]),
	S_NORMAL (PLAY, 'R',	5, NULL 						, &States[S_MPLAY_XDIE+4]),
	S_NORMAL (PLAY, 'S',	5, NULL 						, &States[S_MPLAY_XDIE+5]),
	S_NORMAL (PLAY, 'T',	5, NULL 						, &States[S_MPLAY_XDIE+6]),
	S_NORMAL (PLAY, 'U',	5, NULL 						, &States[S_MPLAY_XDIE+7]),
	S_NORMAL (PLAY, 'V',	5, NULL 						, &States[S_MPLAY_XDIE+8]),
	S_NORMAL (PLAY, 'W',   -1, NULL							, NULL),

#define S_MPLAY_RAISE (S_MPLAY_XDIE+9)
	S_NORMAL (PLAY, 'M',	5, NULL							, &States[S_MPLAY_RAISE+1]),
	S_NORMAL (PLAY, 'L',	5, NULL							, &States[S_MPLAY_RAISE+2]),
	S_NORMAL (PLAY, 'K',	5, NULL							, &States[S_MPLAY_RAISE+3]),
	S_NORMAL (PLAY, 'J',	5, NULL							, &States[S_MPLAY_RAISE+4]),
	S_NORMAL (PLAY, 'I',	5, NULL							, &States[S_MPLAY_RAISE+5]),
	S_NORMAL (PLAY, 'H',	5, NULL							, &States[S_MPLAY_RUN])
};

IMPLEMENT_ACTOR (AScriptedMarine, Doom, 9100, 151)
	PROP_SpawnHealth (100)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (56)
	PROP_Mass (100)
	PROP_SpeedFixed (8)
	PROP_PainChance (200)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE)
	PROP_Flags2 (MF2_MCROSS|MF2_PUSHWALL|MF2_FLOORCLIP)
	PROP_Translation (TRANSLATION_Standard,0)	// Scripted marines wear black

	PROP_SpawnState (S_MPLAY)
	PROP_SeeState (S_MPLAY_RUN)
	PROP_PainState (S_MPLAY_PAIN)
	PROP_MissileState (S_MPLAY_ATK)
	PROP_DeathState (S_MPLAY_DIE)
	PROP_XDeathState (S_MPLAY_XDIE)
	PROP_RaiseState (S_MPLAY_RAISE)

	PROP_DeathSound ("*death")
	PROP_PainSound ("*pain50")
END_DEFAULTS

void AScriptedMarine::Activate (AActor *activator)
{
	if (flags2 & MF2_DORMANT)
	{
		flags2 &= ~MF2_DORMANT;
		tics = 1;
	}
}

void AScriptedMarine::Deactivate (AActor *activator)
{
	if (!(flags2 & MF2_DORMANT))
	{
		flags2 |= MF2_DORMANT;
		tics = -1;
	}
}
