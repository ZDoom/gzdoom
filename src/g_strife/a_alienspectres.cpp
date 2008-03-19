#include "actor.h"
#include "m_random.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "s_sound.h"
#include "m_random.h"
#include "a_strifeglobal.h"
#include "c_console.h"
#include "gstrings.h"

static FRandom pr_spectrespawn ("AlienSpectreSpawn");
static FRandom pr_212e4 ("212e4");

void A_SentinelBob (AActor *);
void A_20538 (AActor *);
void A_SpotLightning (AActor *);
void A_212e4 (AActor *);
void A_2134c (AActor *);
void A_204d0 (AActor *);
void A_204a4 (AActor *);
void A_20314 (AActor *);
void A_20334 (AActor *);
void A_AlienSpectreDeath (AActor *);

void A_AlertMonsters (AActor *);
void A_Tracer2 (AActor *);

AActor *P_SpawnSubMissile (AActor *source, PClass *type, AActor *target);

// Alien Spectre 1 -----------------------------------------------------------

FState AAlienSpectre1::States[] =
{
#define S_ALIEN_STND 0							// 796
	S_NORMAL (ALN1, 'A',   10, A_Look,				&States[S_ALIEN_STND+1]),
	S_NORMAL (ALN1, 'B',   10, A_SentinelBob,		&States[S_ALIEN_STND]),

#define S_ALIEN_CHASE (S_ALIEN_STND+2)			// 798
	S_BRIGHT (ALN1, 'A',	4, A_Chase,				&States[S_ALIEN_CHASE+1]),
	S_BRIGHT (ALN1, 'B',	4, A_Chase,				&States[S_ALIEN_CHASE+2]),
	S_BRIGHT (ALN1, 'C',	4, A_SentinelBob,		&States[S_ALIEN_CHASE+3]),
	S_BRIGHT (ALN1, 'D',	4, A_Chase,				&States[S_ALIEN_CHASE+4]),
	S_BRIGHT (ALN1, 'E',	4, A_Chase,				&States[S_ALIEN_CHASE+5]),
	S_BRIGHT (ALN1, 'F',	4, A_Chase,				&States[S_ALIEN_CHASE+6]),
	S_BRIGHT (ALN1, 'G',	4, A_SentinelBob,		&States[S_ALIEN_CHASE+7]),
	S_BRIGHT (ALN1, 'H',	4, A_Chase,				&States[S_ALIEN_CHASE+8]),
	S_BRIGHT (ALN1, 'I',	4, A_Chase,				&States[S_ALIEN_CHASE+9]),
	S_BRIGHT (ALN1, 'J',	4, A_Chase,				&States[S_ALIEN_CHASE+10]),
	S_BRIGHT (ALN1, 'K',	4, A_SentinelBob,		&States[S_ALIEN_CHASE]),

#define S_ALIEN_MELEE (S_ALIEN_CHASE+11)		// 809
	S_BRIGHT (ALN1, 'J',	4, A_FaceTarget,		&States[S_ALIEN_MELEE+1]),
	S_BRIGHT (ALN1, 'I',	4, A_20538,				&States[S_ALIEN_MELEE+2]),
	S_BRIGHT (ALN1, 'H',	4, NULL,				&States[S_ALIEN_CHASE+2]),

#define S_ALIEN_MISSILE (S_ALIEN_MELEE+3)		// 812
	S_BRIGHT (ALN1, 'J',	4, A_FaceTarget,		&States[S_ALIEN_MISSILE+1]),
	S_BRIGHT (ALN1, 'I',	4, A_SpotLightning,		&States[S_ALIEN_MISSILE+2]),
	S_BRIGHT (ALN1, 'H',	4, NULL,				&States[S_ALIEN_CHASE+10]),

#define S_ALIEN_PAIN (S_ALIEN_MISSILE+3)		// 815
	S_NORMAL (ALN1, 'J',	2, A_Pain,				&States[S_ALIEN_CHASE+6]),

#define S_ALIEN_DIE (S_ALIEN_PAIN+1)			// 816
	S_BRIGHT (AL1P, 'A',	6, A_212e4,				&States[S_ALIEN_DIE+1]),
	S_BRIGHT (AL1P, 'B',	6, A_Scream,			&States[S_ALIEN_DIE+2]),
	S_BRIGHT (AL1P, 'C',	6, A_212e4,				&States[S_ALIEN_DIE+3]),
	S_BRIGHT (AL1P, 'D',	6, NULL,				&States[S_ALIEN_DIE+4]),
	S_BRIGHT (AL1P, 'E',	6, NULL,				&States[S_ALIEN_DIE+5]),
	S_BRIGHT (AL1P, 'F',	6, A_212e4,				&States[S_ALIEN_DIE+6]),
	S_BRIGHT (AL1P, 'G',	6, NULL,				&States[S_ALIEN_DIE+7]),
	S_BRIGHT (AL1P, 'H',	6, A_212e4,				&States[S_ALIEN_DIE+8]),
	S_BRIGHT (AL1P, 'I',	6, NULL,				&States[S_ALIEN_DIE+9]),
	S_BRIGHT (AL1P, 'J',	6, NULL,				&States[S_ALIEN_DIE+10]),
	S_BRIGHT (AL1P, 'K',	6, NULL,				&States[S_ALIEN_DIE+11]),
	S_BRIGHT (AL1P, 'L',	5, NULL,				&States[S_ALIEN_DIE+12]),
	S_BRIGHT (AL1P, 'M',	5, NULL,				&States[S_ALIEN_DIE+13]),
	S_BRIGHT (AL1P, 'N',	5, A_2134c,				&States[S_ALIEN_DIE+14]),
	S_BRIGHT (AL1P, 'O',	5, NULL,				&States[S_ALIEN_DIE+15]),
	S_BRIGHT (AL1P, 'P',	5, NULL,				&States[S_ALIEN_DIE+16]),
	S_BRIGHT (AL1P, 'Q',	5, NULL,				&States[S_ALIEN_DIE+17]),
	S_BRIGHT (AL1P, 'R',	5, A_AlienSpectreDeath,	NULL),

#define S_ALIEN2_MISSILE (S_ALIEN_DIE+18)		// 852
	S_NORMAL (ALN1, 'F',	4, A_FaceTarget,		&States[S_ALIEN2_MISSILE+1]),
	S_NORMAL (ALN1, 'I',	4, A_204d0,				&States[S_ALIEN2_MISSILE+2]),
	S_NORMAL (ALN1, 'E',	4, NULL,				&States[S_ALIEN_CHASE+10]),

#define S_ALIEN4_MISSILE (S_ALIEN2_MISSILE+3)	// 884
	S_NORMAL (ALN1, 'F',	4, A_FaceTarget,		&States[S_ALIEN4_MISSILE+1]),
	S_NORMAL (ALN1, 'I',	4, A_204a4,				&States[S_ALIEN4_MISSILE+2]),
	S_NORMAL (ALN1, 'E',	4, NULL,				&States[S_ALIEN_CHASE+10]),

#define S_ALIEN5_MISSILE (S_ALIEN4_MISSILE+3)	// 887
	S_NORMAL (ALN1, 'F',	4, A_FaceTarget,		&States[S_ALIEN5_MISSILE+1]),
	S_NORMAL (ALN1, 'I',	4, A_20314,				&States[S_ALIEN5_MISSILE+2]),
	S_NORMAL (ALN1, 'E',	4, NULL,				&States[S_ALIEN_CHASE])
};

IMPLEMENT_ACTOR (AAlienSpectre1, Strife, 129, 0)
	PROP_StrifeType (67)
	PROP_SpawnState (S_ALIEN_STND)
	PROP_SpawnHealth (1000)
	PROP_SeeState (S_ALIEN_CHASE)
	PROP_PainState (S_ALIEN_PAIN)
	PROP_PainChance (250)
	PROP_MeleeState (S_ALIEN_MELEE)
	PROP_MissileState (S_ALIEN_MISSILE)
	PROP_DeathState (S_ALIEN_DIE)
	PROP_SpeedFixed (12)
	PROP_RadiusFixed (64)
	PROP_HeightFixed (64)
	PROP_FloatSpeed (5)
	PROP_Mass (1000)
	PROP_Flags (MF_SPECIAL|MF_SOLID|MF_SHOOTABLE|MF_NOGRAVITY|
				MF_FLOAT|MF_SHADOW|MF_COUNTKILL|MF_NOTDMATCH|MF_STRIFEx8000000)
	PROP_Flags2 (MF2_PASSMOBJ|MF2_PUSHWALL|MF2_MCROSS)
	PROP_Flags3 (MF3_DONTMORPH|MF3_NOBLOCKMONST)
	PROP_Flags4 (MF4_INCOMBAT|MF4_LOOKALLAROUND|MF4_SPECTRAL|MF4_NOICEDEATH)
	PROP_MinMissileChance (150)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (TRANSLUC66)
	PROP_SeeSound ("alienspectre/sight")
	PROP_AttackSound ("alienspectre/blade")
	PROP_PainSound ("alienspectre/pain")
	PROP_DeathSound ("alienspectre/death")
	PROP_ActiveSound ("alienspectre/active")
	PROP_Obituary ("$OB_ALIENSPECTE")
END_DEFAULTS

void AAlienSpectre1::Touch (AActor *toucher)
{
	P_DamageMobj (toucher, this, this, 5, NAME_Melee);
}

// Alien Spectre 2 -----------------------------------------------------------

class AAlienSpectre2 : public AAlienSpectre1
{
	DECLARE_STATELESS_ACTOR (AAlienSpectre2, AAlienSpectre1)
public:
	void NoBlockingSet ();
};

IMPLEMENT_STATELESS_ACTOR (AAlienSpectre2, Strife, 75, 0)
	PROP_StrifeType (70)
	PROP_SpawnHealth (1200)
	PROP_PainChance (50)
	PROP_MissileState (S_ALIEN2_MISSILE)
	PROP_RadiusFixed (24)
END_DEFAULTS

//============================================================================
//
// AAlienSpectre2 :: NoBlockingSet
//
//============================================================================

void AAlienSpectre2::NoBlockingSet ()
{
	P_DropItem (this, "Sigil2", -1, 256);
}

// Alien Spectre 3 ----------------------------------------------------------
// This is the Oracle's personal spectre, so it's a little different.

FState AAlienSpectre3::States[] =
{
#define S_ALIEN3_STND 0						// 855
	S_NORMAL (ALN1, 'A',	5, NULL,		&States[S_ALIEN3_STND+1]),
	S_NORMAL (ALN1, 'B',	5, NULL,		&States[S_ALIEN3_STND+2]),
	S_NORMAL (ALN1, 'C',	5, NULL,		&States[S_ALIEN3_STND+3]),
	S_NORMAL (ALN1, 'D',	5, NULL,		&States[S_ALIEN3_STND+4]),
	S_NORMAL (ALN1, 'E',	5, NULL,		&States[S_ALIEN3_STND+5]),
	S_NORMAL (ALN1, 'F',	5, NULL,		&States[S_ALIEN3_STND+6]),
	S_NORMAL (ALN1, 'G',	5, NULL,		&States[S_ALIEN3_STND+7]),
	S_NORMAL (ALN1, 'H',	5, NULL,		&States[S_ALIEN3_STND+8]),
	S_NORMAL (ALN1, 'I',	5, NULL,		&States[S_ALIEN3_STND+9]),
	S_NORMAL (ALN1, 'J',	5, NULL,		&States[S_ALIEN3_STND+10]),
	S_NORMAL (ALN1, 'K',	5, NULL,		&States[S_ALIEN3_STND]),

#define S_ALIEN3_CHASE (S_ALIEN3_STND+11)	// 866
	S_NORMAL (ALN1, 'A',	5, A_Chase,				&States[S_ALIEN3_CHASE+1]),
	S_NORMAL (ALN1, 'B',	5, A_Chase,				&States[S_ALIEN3_CHASE+2]),
	S_NORMAL (ALN1, 'C',	5, A_SentinelBob,		&States[S_ALIEN3_CHASE+3]),
	S_NORMAL (ALN1, 'D',	5, A_Chase,				&States[S_ALIEN3_CHASE+4]),
	S_NORMAL (ALN1, 'E',	5, A_Chase,				&States[S_ALIEN3_CHASE+5]),
	S_NORMAL (ALN1, 'F',	5, A_Chase,				&States[S_ALIEN3_CHASE+6]),
	S_NORMAL (ALN1, 'G',	5, A_SentinelBob,		&States[S_ALIEN3_CHASE+7]),
	S_NORMAL (ALN1, 'H',	5, A_Chase,				&States[S_ALIEN3_CHASE+8]),
	S_NORMAL (ALN1, 'I',	5, A_Chase,				&States[S_ALIEN3_CHASE+9]),
	S_NORMAL (ALN1, 'J',	5, A_Chase,				&States[S_ALIEN3_CHASE+10]),
	S_NORMAL (ALN1, 'K',	5, A_SentinelBob,		&States[S_ALIEN3_CHASE]),

#define S_ALIEN3_MELEE (S_ALIEN3_CHASE+11)	// 877
	S_NORMAL (ALN1, 'J',	4, A_FaceTarget,		&States[S_ALIEN3_MELEE+1]),
	S_NORMAL (ALN1, 'I',	4, A_20538,				&States[S_ALIEN3_MELEE+2]),
	S_NORMAL (ALN1, 'C',	4, NULL,				&States[S_ALIEN3_CHASE+2]),

#define S_ALIEN3_MISSILE (S_ALIEN3_MELEE+3)	// 880
	S_NORMAL (ALN1, 'F',	4, A_FaceTarget,		&States[S_ALIEN3_MISSILE+1]),
	S_NORMAL (ALN1, 'I',	4, A_20334,				&States[S_ALIEN3_MISSILE+2]),
	S_NORMAL (ALN1, 'E',	4, NULL,				&States[S_ALIEN3_CHASE+10]),

#define S_ALIEN3_PAIN (S_ALIEN3_MISSILE+3)	// 883
	S_NORMAL (ALN1, 'J',	2, A_Pain,				&States[S_ALIEN3_CHASE+6])
};

IMPLEMENT_ACTOR (AAlienSpectre3, Strife, 76, 0)
	PROP_StrifeType (71)
	PROP_SpawnState (S_ALIEN3_STND)
	PROP_SpawnHealth (1500)
	PROP_SeeState (S_ALIEN3_CHASE)
	PROP_PainState (S_ALIEN3_PAIN)
	PROP_PainChance (50)
	PROP_MeleeState (S_ALIEN3_MELEE)
	PROP_MissileState (S_ALIEN3_MISSILE)
	PROP_RadiusFixed (24)
	PROP_FlagsSet (MF_SPAWNCEILING)
	PROP_Flags3 (MF3_DONTMORPH|MF3_NOBLOCKMONST)
END_DEFAULTS

//============================================================================
//
// AAlienSpectre3 :: NoBlockingSet
//
//============================================================================

void AAlienSpectre3::NoBlockingSet ()
{
	P_DropItem (this, "Sigil3", -1, 256);
}

// Alien Spectre 4 -----------------------------------------------------------

class AAlienSpectre4 : public AAlienSpectre1
{
	DECLARE_STATELESS_ACTOR (AAlienSpectre4, AAlienSpectre1)
public:
	void NoBlockingSet ();
};

IMPLEMENT_STATELESS_ACTOR (AAlienSpectre4, Strife, 167, 0)
	PROP_StrifeType (72)
	PROP_SpawnHealth (1700)
	PROP_PainChance (50)
	PROP_MissileState (S_ALIEN4_MISSILE)
	PROP_RadiusFixed (24)
END_DEFAULTS

//============================================================================
//
// AAlienSpectre4 :: NoBlockingSet
//
//============================================================================

void AAlienSpectre4::NoBlockingSet ()
{
	P_DropItem (this, "Sigil4", -1, 256);
}

// Alien Spectre 5 -----------------------------------------------------------

class AAlienSpectre5 : public AAlienSpectre1
{
	DECLARE_STATELESS_ACTOR (AAlienSpectre5, AAlienSpectre1)
public:
	void NoBlockingSet ();
};

IMPLEMENT_STATELESS_ACTOR (AAlienSpectre5, Strife, 168, 0)
	PROP_StrifeType (73)
	PROP_SpawnHealth (2000)
	PROP_PainChance (50)
	PROP_MissileState (S_ALIEN5_MISSILE)
	PROP_RadiusFixed (24)
END_DEFAULTS

//============================================================================
//
// AAlienSpectre5 :: NoBlockingSet
//
//============================================================================

void AAlienSpectre5::NoBlockingSet ()
{
	P_DropItem (this, "Sigil5", -1, 256);
}

// Small Alien Chunk --------------------------------------------------------

class AAlienChunkSmall : public AActor
{
	DECLARE_ACTOR (AAlienChunkSmall, AActor)
};

FState AAlienChunkSmall::States[] =
{
	S_BRIGHT (NODE, 'A', 6, NULL, &States[1]),
	S_BRIGHT (NODE, 'B', 6, NULL, &States[2]),
	S_BRIGHT (NODE, 'C', 6, NULL, &States[3]),
	S_BRIGHT (NODE, 'D', 6, NULL, &States[4]),
	S_BRIGHT (NODE, 'E', 6, NULL, &States[5]),
	S_BRIGHT (NODE, 'F', 6, NULL, &States[6]),
	S_BRIGHT (NODE, 'G', 6, NULL, NULL),
};

IMPLEMENT_ACTOR (AAlienChunkSmall, Strife, -1, 0)
	PROP_StrifeType (68)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOCLIP|MF_NOTDMATCH)
END_DEFAULTS

// Large Alien Chunk --------------------------------------------------------

class AAlienChunkLarge : public AActor
{
	DECLARE_ACTOR (AAlienChunkLarge, AActor)
};

FState AAlienChunkLarge::States[] =
{
	S_BRIGHT (MTHD, 'A', 5, NULL, &States[1]),
	S_BRIGHT (MTHD, 'B', 5, NULL, &States[2]),
	S_BRIGHT (MTHD, 'C', 5, NULL, &States[3]),
	S_BRIGHT (MTHD, 'D', 5, NULL, &States[4]),
	S_BRIGHT (MTHD, 'E', 5, NULL, &States[5]),
	S_BRIGHT (MTHD, 'F', 5, NULL, &States[6]),
	S_BRIGHT (MTHD, 'G', 5, NULL, &States[7]),
	S_BRIGHT (MTHD, 'H', 5, NULL, &States[8]),
	S_BRIGHT (MTHD, 'I', 5, NULL, &States[9]),
	S_BRIGHT (MTHD, 'J', 5, NULL, &States[10]),
	S_BRIGHT (MTHD, 'K', 5, NULL, NULL),
};

IMPLEMENT_ACTOR (AAlienChunkLarge, Strife, -1, 0)
	PROP_StrifeType (69)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOCLIP|MF_NOTDMATCH)
END_DEFAULTS

//============================================================================

static void GenericSpectreSpawn (AActor *actor, const PClass *type)
{
	AActor *spectre = Spawn (type, actor->x, actor->y, actor->z, ALLOW_REPLACE);
	if (spectre != NULL)
	{
		spectre->momz = pr_spectrespawn() << 9;
	}
}

void A_SpawnSpectre1 (AActor *actor)
{
	GenericSpectreSpawn (actor, RUNTIME_CLASS(AAlienSpectre1));
}

void A_SpawnSpectre3 (AActor *actor)
{
	GenericSpectreSpawn (actor, RUNTIME_CLASS(AAlienSpectre3));
}

void A_SpawnSpectre4 (AActor *actor)
{
	GenericSpectreSpawn (actor, RUNTIME_CLASS(AAlienSpectre4));
}

void A_SpawnSpectre5 (AActor *actor)
{
	GenericSpectreSpawn (actor, RUNTIME_CLASS(AAlienSpectre5));
}

void A_212e4 (AActor *self)
{
	AActor *foo = Spawn<AAlienChunkSmall> (self->x, self->y, self->z + 10*FRACUNIT, ALLOW_REPLACE);

	if (foo != NULL)
	{
		int t;

		t = pr_212e4() & 15;
		foo->momx = (t - (pr_212e4() & 7)) << FRACBITS;
		
		t = pr_212e4() & 15;
		foo->momy = (t - (pr_212e4() & 7)) << FRACBITS;

		foo->momz = (pr_212e4() & 15) << FRACBITS;
	}
}

void A_2134c (AActor *self)
{
	AActor *foo = Spawn<AAlienChunkLarge> (self->x, self->y, self->z + 10*FRACUNIT, ALLOW_REPLACE);

	if (foo != NULL)
	{
		int t;

		t = pr_212e4() & 7;
		foo->momx = (t - (pr_212e4() & 15)) << FRACBITS;
		
		t = pr_212e4() & 7;
		foo->momy = (t - (pr_212e4() & 15)) << FRACBITS;

		foo->momz = (pr_212e4() & 7) << FRACBITS;
	}

}

void A_204a4 (AActor *self)
{
	if (self->target != NULL)
	{
		AActor *missile = P_SpawnMissile (self, self->target, RUNTIME_CLASS(ASpectralLightningBigV2));
		if (missile != NULL)
		{
			missile->tracer = self->target;
			missile->health = -2;
		}
	}
}

void A_204d0 (AActor *self)
{
	if (self->target != NULL)
	{
		AActor *missile = P_SpawnMissile (self, self->target, RUNTIME_CLASS(ASpectralLightningH3));
		if (missile != NULL)
		{
			missile->health = -2;
		}
	}
}

void A_20314 (AActor *self)
{
	if (self->target != NULL)
	{
		AActor *missile = P_SpawnMissile (self, self->target, RUNTIME_CLASS(ASpectralLightningBigBall2));
		if (missile != NULL)
		{
			missile->health = -2;
		}
	}
}

void A_20424 (AActor *self)
{
	self->angle += ANGLE_90;
	P_SpawnSubMissile (self, RUNTIME_CLASS(ASpectralLightningH3), self);
	self->angle += ANGLE_180;
	P_SpawnSubMissile (self, RUNTIME_CLASS(ASpectralLightningH3), self);
	self->angle += ANGLE_90;
	P_SpawnSubMissile (self, RUNTIME_CLASS(ASpectralLightningH3), self);
}

void A_20334 (AActor *self)
{
	if (self->target == NULL)
		return;

	AActor *foo = Spawn<ASpectralLightningV2> (self->x, self->y, self->z + 32*FRACUNIT, ALLOW_REPLACE);

	foo->momz = -12*FRACUNIT;
	foo->target = self;
	foo->health = -2;
	foo->tracer = self->target;

	self->angle -= ANGLE_180 / 20 * 10;
	for (int i = 0; i < 20; ++i)
	{
		self->angle += ANGLE_180 / 20;
		P_SpawnSubMissile (self, RUNTIME_CLASS(ASpectralLightningBall2), self);
	}
	self->angle -= ANGLE_180 / 20 * 10;
}

void A_AlienSpectreDeath (AActor *self)
{
	AActor *player;
	char voc[32];
	int log;
	int i;

	A_NoBlocking (self);	// [RH] Need this for Sigil rewarding
	if (!CheckBossDeath (self))
	{
		return;
	}
	for (i = 0, player = NULL; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i] && players[i].health > 0)
		{
			player = players[i].mo;
			break;
		}
	}
	if (player == NULL)
	{
		return;
	}

	if (self->GetClass() == RUNTIME_CLASS(AAlienSpectre1))
	{
		EV_DoFloor (DFloor::floorLowerToLowest, NULL, 999, FRACUNIT, 0, 0, 0, false);
		log = 95;
	}
	else if (self->GetClass() == RUNTIME_CLASS(AAlienSpectre2))
	{
		C_MidPrint(GStrings("TXT_KILLED_BISHOP"));
		log = 74;
		player->GiveInventoryType (QuestItemClasses[20]);
	}
	else if (self->GetClass() == RUNTIME_CLASS(AAlienSpectre3))
	{
		C_MidPrint(GStrings("TXT_KILLED_ORACLE"));
		// If there are any Oracles still alive, kill them.
		TThinkerIterator<AOracle> it;
		AOracle *oracle;

		while ( (oracle = it.Next()) != NULL)
		{
			if (oracle->health > 0)
			{
				oracle->health = 0;
				oracle->Die (self, self);
			}
		}
		player->GiveInventoryType (QuestItemClasses[22]);
		if (player->FindInventory (QuestItemClasses[20]))
		{ // If the Bishop is dead, set quest item 22
			player->GiveInventoryType (QuestItemClasses[21]);
		}
		if (player->FindInventory (QuestItemClasses[23]) == NULL)
		{	// Macil is calling us back...
			log = 87;
		}
		else
		{	// You weild the power of the complete Sigil.
			log = 85;
		}
		EV_DoDoor (DDoor::doorOpen, NULL, NULL, 222, 8*FRACUNIT, 0, 0, 0);
	}
	else if (self->GetClass() == RUNTIME_CLASS(AAlienSpectre4))
	{
		C_MidPrint(GStrings("TXT_KILLED_MACIL"));
		player->GiveInventoryType (QuestItemClasses[23]);
		if (player->FindInventory (QuestItemClasses[24]) == NULL)
		{	// Richter has taken over. Macil is a snake.
			log = 79;
		}
		else
		{	// Back to the factory for another Sigil!
			log = 106;
		}
	}
	else if (self->GetClass() == RUNTIME_CLASS(AAlienSpectre5))
	{
		C_MidPrint(GStrings("TXT_KILLED_LOREMASTER"));
		ASigil *sigil;

		player->GiveInventoryType (QuestItemClasses[25]);
		if (!multiplayer)
		{
			player->GiveInventoryType (RUNTIME_CLASS(AUpgradeStamina));
			player->GiveInventoryType (RUNTIME_CLASS(AUpgradeAccuracy));
		}
		sigil = player->FindInventory<ASigil>();
		if (sigil != NULL && sigil->NumPieces == 5)
		{	// You weild the power of the complete Sigil.
			log = 85;
		}
		else
		{	// Another Sigil piece. Woohoo!
			log = 83;
		}
		EV_DoFloor (DFloor::floorLowerToLowest, NULL, 666, FRACUNIT, 0, 0, 0, false);
	}
	else
	{
		return;
	}
	sprintf (voc, "svox/voc%d", log);
	S_Sound (CHAN_VOICE, voc, 1, ATTN_NORM);
	player->player->SetLogNumber (log);
}
