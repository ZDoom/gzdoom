#include "actor.h"
#include "m_random.h"
#include "p_local.h"
#include "c_console.h"
#include "p_enemy.h"
#include "a_action.h"
#include "gstrings.h"

static FRandom pr_bang4cloud ("Bang4Cloud");
static FRandom pr_lightout ("LightOut");

extern const PClass *QuestItemClasses[31];

void A_TossGib (AActor *);
void A_LoopActiveSound (AActor *);
void A_LightGoesOut (AActor *);

// A Cloud used for varius explosions ---------------------------------------
// This actor has no direct equivalent in strife. To create this, Strife
// spawned a spark and then changed its state to that of this explosion
// cloud. Weird.

class ABang4Cloud : public AActor
{
	DECLARE_ACTOR (ABang4Cloud, AActor);
public:
	void BeginPlay ()
	{
		momz = FRACUNIT;
	}
};

FState ABang4Cloud::States[] =
{
	S_BRIGHT (BNG4, 'B', 3, NULL,		&States[1]),
	S_BRIGHT (BNG4, 'C', 3, NULL,		&States[2]),
	S_BRIGHT (BNG4, 'D', 3, NULL,		&States[3]),
	S_BRIGHT (BNG4, 'E', 3, NULL,		&States[4]),
	S_BRIGHT (BNG4, 'F', 3, NULL,		&States[5]),
	S_BRIGHT (BNG4, 'G', 3, NULL,		&States[6]),
	S_BRIGHT (BNG4, 'H', 3, NULL,		&States[7]),
	S_BRIGHT (BNG4, 'I', 3, NULL,		&States[8]),
	S_BRIGHT (BNG4, 'J', 3, NULL,		&States[9]),
	S_BRIGHT (BNG4, 'K', 3, NULL,		&States[10]),
	S_BRIGHT (BNG4, 'L', 3, NULL,		&States[11]),
	S_BRIGHT (BNG4, 'M', 3, NULL,		&States[12]),
	S_BRIGHT (BNG4, 'N', 3, NULL,		NULL)
};

IMPLEMENT_ACTOR (ABang4Cloud, Strife, -1, 0)
	PROP_SpawnState (0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Add)
END_DEFAULTS

void A_Bang4Cloud (AActor *self)
{
	fixed_t spawnx, spawny;

	spawnx = self->x + (pr_bang4cloud.Random2() & 3) * 10240;
	spawny = self->y + (pr_bang4cloud.Random2() & 3) * 10240;

	Spawn<ABang4Cloud> (spawnx, spawny, self->z, ALLOW_REPLACE);
}

// Piston -------------------------------------------------------------------

void A_GiveQuestItem (AActor *self)
{
	int questitem = (self->Speed >> FRACBITS);
	
	// Give one of these quest items to every player in the game
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i])
		{
			AInventory *item = static_cast<AInventory *>(Spawn (QuestItemClasses[questitem-1], 0,0,0, NO_REPLACE));
			if (!item->TryPickup (players[i].mo))
			{
				item->Destroy ();
			}
		}
	}

	char messageid[64];

	mysnprintf(messageid, countof(messageid), "TXT_QUEST_%d", questitem);
	const char * name = GStrings[messageid];

	if (name != NULL)
	{
		C_MidPrint (name);
	}
}

class APiston : public AActor
{
	DECLARE_ACTOR (APiston, AActor);
};

FState APiston::States[] =
{
	S_NORMAL (PSTN, 'A',  8, NULL,				&States[1]),
	S_NORMAL (PSTN, 'B',  8, NULL,				&States[0]),
	
	S_BRIGHT (PSTN, 'A',  4, A_Scream,			&States[3]),
	S_BRIGHT (PSTN, 'B',  4, A_NoBlocking,		&States[4]),
	S_BRIGHT (PSTN, 'C',  4, A_GiveQuestItem,	&States[5]),
	S_BRIGHT (PSTN, 'D',  4, A_Bang4Cloud,		&States[6]),
	S_BRIGHT (PSTN, 'E',  4, A_TossGib,			&States[7]),
	S_BRIGHT (PSTN, 'F',  4, NULL,				&States[8]),
	S_BRIGHT (PSTN, 'G',  4, A_Bang4Cloud,		&States[9]),
	S_NORMAL (PSTN, 'H',  4, NULL,				&States[10]),
	S_NORMAL (PSTN, 'I', -1, NULL,				NULL)
};

IMPLEMENT_ACTOR (APiston, Strife, 45, 0)
	PROP_StrifeType (123)
	PROP_SpawnHealth (100)
	PROP_SpawnState (0)
	PROP_DeathState (2)
	PROP_SpeedFixed (16)	// Was a byte in Strife
	PROP_RadiusFixed (20)
	PROP_HeightFixed (76)
	PROP_MassLong (10000000)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_Flags4 (MF4_INCOMBAT)
	PROP_DeathSound ("misc/explosion")
END_DEFAULTS

// Computer -----------------------------------------------------------------

class AComputer : public AActor
{
	DECLARE_ACTOR (AComputer, AActor)
};

FState AComputer::States[] =
{
	S_BRIGHT (SECR, 'A', 4, NULL,				&States[1]),
	S_BRIGHT (SECR, 'B', 4, NULL,				&States[2]),
	S_BRIGHT (SECR, 'C', 4, NULL,				&States[3]),
	S_BRIGHT (SECR, 'D', 4, NULL,				&States[0]),

	S_BRIGHT (SECR, 'E', 5, A_Bang4Cloud,		&States[5]),
	S_BRIGHT (SECR, 'F', 5, A_NoBlocking,		&States[6]),
	S_BRIGHT (SECR, 'G', 5, A_GiveQuestItem,	&States[7]),
	S_BRIGHT (SECR, 'H', 5, A_TossGib,			&States[8]),
	S_BRIGHT (SECR, 'I', 5, A_Bang4Cloud,		&States[9]),
	S_NORMAL (SECR, 'J', 5, NULL,				&States[10]),
	S_NORMAL (SECR, 'K', 5, A_Bang4Cloud,		&States[11]),
	S_NORMAL (SECR, 'L', 5, NULL,				&States[12]),
	S_NORMAL (SECR, 'M', 5, A_Bang4Cloud,		&States[13]),
	S_NORMAL (SECR, 'N', 5, NULL,				&States[14]),
	S_NORMAL (SECR, 'O', 5, A_Bang4Cloud,		&States[15]),
	S_NORMAL (SECR, 'P',-1, NULL,				NULL)
};

IMPLEMENT_ACTOR (AComputer, Strife, 182, 0)
	PROP_SpawnState (0)
	PROP_SpawnHealth (80)
	PROP_DeathState (4)
	PROP_SpeedFixed (27)		// was a byte in Strife
	PROP_RadiusFixed (26)
	PROP_HeightFixed (128)
	PROP_MassLong (100000)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOBLOOD)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_Flags4 (MF4_INCOMBAT)
	PROP_StrifeType (124)
	PROP_DeathSound ("misc/explosion")
END_DEFAULTS

// Power Crystal ------------------------------------------------------------

void A_ExtraLightOff (AActor *);
void A_Explode512 (AActor *);

class APowerCrystal : public AActor
{
	DECLARE_ACTOR (APowerCrystal, AActor)
};

FState APowerCrystal::States[] =
{
	S_BRIGHT (CRYS, 'A', 16, A_LoopActiveSound,		&States[1]),
	S_BRIGHT (CRYS, 'B',  5, A_LoopActiveSound,		&States[2]),
	S_BRIGHT (CRYS, 'C',  4, A_LoopActiveSound,		&States[3]),
	S_BRIGHT (CRYS, 'D',  4, A_LoopActiveSound,		&States[4]),
	S_BRIGHT (CRYS, 'E',  4, A_LoopActiveSound,		&States[5]),
	S_BRIGHT (CRYS, 'F',  4, A_LoopActiveSound,		&States[0]),

	S_BRIGHT (BOOM, 'A',  0, A_Scream,				&States[7]),	// [RH] This seems like a good idea
	S_BRIGHT (BOOM, 'A',  1, A_Explode512,			&States[8]),
	S_BRIGHT (BOOM, 'B',  3, A_GiveQuestItem,		&States[9]),
	S_BRIGHT (BOOM, 'C',  2, A_LightGoesOut,		&States[10]),
	S_BRIGHT (BOOM, 'D',  3, A_Bang4Cloud,			&States[11]),
	S_BRIGHT (BOOM, 'E',  3, NULL,					&States[12]),
	S_BRIGHT (BOOM, 'F',  3, NULL,					&States[13]),
	S_BRIGHT (BOOM, 'G',  3, A_Bang4Cloud,			&States[14]),
	S_BRIGHT (BOOM, 'H',  1, A_Explode512,			&States[15]),
	S_BRIGHT (BOOM, 'I',  3, NULL,					&States[16]),
	S_BRIGHT (BOOM, 'J',  3, A_Bang4Cloud,			&States[17]),
	S_BRIGHT (BOOM, 'K',  3, A_Bang4Cloud,			&States[18]),
	S_BRIGHT (BOOM, 'L',  3, A_Bang4Cloud,			&States[19]),
	S_BRIGHT (BOOM, 'M',  3, NULL,					&States[20]),
	S_BRIGHT (BOOM, 'N',  3, NULL,					&States[21]),
	S_BRIGHT (BOOM, 'O',  3, A_Bang4Cloud,			&States[22]),
	S_BRIGHT (BOOM, 'P',  3, NULL,					&States[23]),
	S_BRIGHT (BOOM, 'Q',  3, NULL,					&States[24]),
	S_BRIGHT (BOOM, 'R',  3, NULL,					&States[25]),
	S_BRIGHT (BOOM, 'S',  3, NULL,					&States[26]),
	S_BRIGHT (BOOM, 'T',  3, NULL,					&States[27]),
	S_BRIGHT (BOOM, 'U',  3, A_ExtraLightOff,		&States[28]),
	S_BRIGHT (BOOM, 'V',  3, NULL,					&States[29]),
	S_BRIGHT (BOOM, 'W',  3, NULL,					&States[30]),
	S_BRIGHT (BOOM, 'X',  3, NULL,					&States[31]),
	S_BRIGHT (BOOM, 'Y',  3, NULL,					NULL)
};

IMPLEMENT_ACTOR (APowerCrystal, Strife, 92, 0)
	PROP_SpawnState (0)
	PROP_SpawnHealth (50)
	PROP_DeathState (6)
	PROP_SpeedFixed (14)		// Was a byte in Strife
	PROP_RadiusFixed (20)		// This size seems odd, but that's what the mobjinfo says
	PROP_HeightFixed (16)
	PROP_MassLong (99999999)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_NOGRAVITY|MF_NOBLOOD)
	PROP_Flags2 (MF2_FLOORCLIP)
	PROP_StrifeType (201)
	PROP_DeathSound ("misc/explosion")
	PROP_ActiveSound ("misc/reactor")
END_DEFAULTS

void A_ExtraLightOff (AActor *self)
{
	if (self->target != NULL && self->target->player != NULL)
	{
		self->target->player->extralight = 0;
	}
}

void A_Explode512 (AActor *self)
{
	P_RadiusAttack (self, self->target, 512, 512, NAME_None, true);
	if (self->target != NULL && self->target->player != NULL)
	{
		self->target->player->extralight = 5;
	}
	if (self->z <= self->floorz + (512<<FRACBITS))
	{
		P_HitFloor (self);
	}

	// Strife didn't do this next part, but it looks good
	self->RenderStyle = STYLE_Add;
}

void A_LightGoesOut (AActor *self)
{
	AActor *foo;
	sector_t *sec = self->Sector;
	vertex_t *spot;
	fixed_t newheight;

	sec->lightlevel = 0;

	newheight = sec->FindLowestFloorSurrounding (&spot);
	sec->floorplane.d = sec->floorplane.PointToDist (spot, newheight);

	for (int i = 0; i < 8; ++i)
	{
		foo = Spawn("Rubble1", self->x, self->y, self->z, ALLOW_REPLACE);
		if (foo != NULL)
		{
			int t = pr_lightout() & 15;
			foo->momx = (t - (pr_lightout() & 7)) << FRACBITS;
			foo->momy = (pr_lightout.Random2() & 7) << FRACBITS;
			foo->momz = (7 + (pr_lightout() & 3)) << FRACBITS;
		}
	}
}
