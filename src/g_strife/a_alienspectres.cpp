/*
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
#include "thingdef/thingdef.h"
#include "doomstat.h"
*/

static FRandom pr_spectrespawn ("AlienSpectreSpawn");
static FRandom pr_spectrechunk ("212e4");

//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SpectreChunkSmall)
{
	PARAM_ACTION_PROLOGUE;

	AActor *foo = Spawn("AlienChunkSmall", self->PosPlusZ(10.), ALLOW_REPLACE);

	if (foo != NULL)
	{
		int t;

		t = pr_spectrechunk() & 15;
		foo->Vel.X = (t - (pr_spectrechunk() & 7));
		
		t = pr_spectrechunk() & 15;
		foo->Vel.Y = (t - (pr_spectrechunk() & 7));

		foo->Vel.Z = (pr_spectrechunk() & 15);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_SpectreChunkLarge)
{
	PARAM_ACTION_PROLOGUE;

	AActor *foo = Spawn("AlienChunkLarge", self->PosPlusZ(10.), ALLOW_REPLACE);

	if (foo != NULL)
	{
		int t;

		t = pr_spectrechunk() & 7;
		foo->Vel.X = (t - (pr_spectrechunk() & 15));
		
		t = pr_spectrechunk() & 7;
		foo->Vel.Y = (t - (pr_spectrechunk() & 15));

		foo->Vel.Z = (pr_spectrechunk() & 7);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_Spectre3Attack)
{
	PARAM_ACTION_PROLOGUE;

	if (self->target == NULL)
		return 0;

	AActor *foo = Spawn("SpectralLightningV2", self->PosPlusZ(32.), ALLOW_REPLACE);

	foo->Vel.Z = -12;
	foo->target = self;
	foo->FriendPlayer = 0;
	foo->tracer = self->target;

	self->Angles.Yaw -= 90.;
	for (int i = 0; i < 20; ++i)
	{
		self->Angles.Yaw += 9.;
		P_SpawnSubMissile (self, PClass::FindActor("SpectralLightningBall2"), self);
	}
	self->Angles.Yaw -= 90.;
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_AlienSpectreDeath)
{
	PARAM_ACTION_PROLOGUE;

	AActor *player;
	char voc[32];
	int log;
	int i;

	A_Unblock(self, true); // [RH] Need this for Sigil rewarding
	if (!CheckBossDeath (self))
	{
		return 0;
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
		return 0;
	}

	switch (self->GetClass()->TypeName)
	{
	case NAME_AlienSpectre1:
		EV_DoFloor (DFloor::floorLowerToLowest, NULL, 999, 1., 0., -1, 0, false);
		log = 95;
		break;

	case NAME_AlienSpectre2:
		C_MidPrint(SmallFont, GStrings("TXT_KILLED_BISHOP"));
		log = 74;
		player->GiveInventoryType (QuestItemClasses[20]);
		break;

	case NAME_AlienSpectre3:
	{
		C_MidPrint(SmallFont, GStrings("TXT_KILLED_ORACLE"));
		// If there are any Oracles still alive, kill them.
		TThinkerIterator<AActor> it(NAME_Oracle);
		AActor *oracle;

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
		{	// You wield the power of the complete Sigil.
			log = 85;
		}
		EV_DoDoor (DDoor::doorOpen, NULL, NULL, 222, 8., 0, 0, 0);
		break;
	}

	case NAME_AlienSpectre4:
		C_MidPrint(SmallFont, GStrings("TXT_KILLED_MACIL"));
		player->GiveInventoryType (QuestItemClasses[23]);
		if (player->FindInventory (QuestItemClasses[24]) == NULL)
		{	// Richter has taken over. Macil is a snake.
			log = 79;
		}
		else
		{	// Back to the factory for another Sigil!
			log = 106;
		}
		break;

	case NAME_AlienSpectre5:
		C_MidPrint(SmallFont, GStrings("TXT_KILLED_LOREMASTER"));
		ASigil *sigil;

		player->GiveInventoryType (QuestItemClasses[25]);
		if (!multiplayer)
		{
			player->GiveInventoryType (RUNTIME_CLASS(AUpgradeStamina));
			player->GiveInventoryType (RUNTIME_CLASS(AUpgradeAccuracy));
		}
		sigil = player->FindInventory<ASigil>();
		if (sigil != NULL && sigil->NumPieces == 5)
		{	// You wield the power of the complete Sigil.
			log = 85;
		}
		else
		{	// Another Sigil piece. Woohoo!
			log = 83;
		}
		EV_DoFloor (DFloor::floorLowerToLowest, NULL, 666, 1., 0., -1, 0, false);
		break;

	default:
		return 0;
	}
	mysnprintf (voc, countof(voc), "svox/voc%d", log);
	S_Sound (CHAN_VOICE, voc, 1, ATTN_NORM);
	player->player->SetLogNumber (log);
	return 0;
}
