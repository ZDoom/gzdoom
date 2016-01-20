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

AActor *P_SpawnSubMissile (AActor *source, const PClass *type, AActor *target);

//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_SpectreChunkSmall)
{
	AActor *foo = Spawn("AlienChunkSmall", self->PosPlusZ(10*FRACUNIT), ALLOW_REPLACE);

	if (foo != NULL)
	{
		int t;

		t = pr_spectrechunk() & 15;
		foo->velx = (t - (pr_spectrechunk() & 7)) << FRACBITS;
		
		t = pr_spectrechunk() & 15;
		foo->vely = (t - (pr_spectrechunk() & 7)) << FRACBITS;

		foo->velz = (pr_spectrechunk() & 15) << FRACBITS;
	}
}

DEFINE_ACTION_FUNCTION(AActor, A_SpectreChunkLarge)
{
	AActor *foo = Spawn("AlienChunkLarge", self->PosPlusZ(10*FRACUNIT), ALLOW_REPLACE);

	if (foo != NULL)
	{
		int t;

		t = pr_spectrechunk() & 7;
		foo->velx = (t - (pr_spectrechunk() & 15)) << FRACBITS;
		
		t = pr_spectrechunk() & 7;
		foo->vely = (t - (pr_spectrechunk() & 15)) << FRACBITS;

		foo->velz = (pr_spectrechunk() & 7) << FRACBITS;
	}

}

DEFINE_ACTION_FUNCTION(AActor, A_Spectre3Attack)
{
	if (self->target == NULL)
		return;

	AActor *foo = Spawn("SpectralLightningV2", self->PosPlusZ(32*FRACUNIT), ALLOW_REPLACE);

	foo->velz = -12*FRACUNIT;
	foo->target = self;
	foo->FriendPlayer = 0;
	foo->tracer = self->target;

	self->angle -= ANGLE_180 / 20 * 10;
	for (int i = 0; i < 20; ++i)
	{
		self->angle += ANGLE_180 / 20;
		P_SpawnSubMissile (self, PClass::FindClass("SpectralLightningBall2"), self);
	}
	self->angle -= ANGLE_180 / 20 * 10;
}

DEFINE_ACTION_FUNCTION(AActor, A_AlienSpectreDeath)
{
	AActor *player;
	char voc[32];
	int log;
	int i;

	A_Unblock(self, true); // [RH] Need this for Sigil rewarding
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

	switch (self->GetClass()->TypeName)
	{
	case NAME_AlienSpectre1:
		EV_DoFloor (DFloor::floorLowerToLowest, NULL, 999, FRACUNIT, 0, -1, 0, false);
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
		EV_DoDoor (DDoor::doorOpen, NULL, NULL, 222, 8*FRACUNIT, 0, 0, 0);
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
		EV_DoFloor (DFloor::floorLowerToLowest, NULL, 666, FRACUNIT, 0, -1, 0, false);
		break;

	default:
		return;
	}
	mysnprintf (voc, countof(voc), "svox/voc%d", log);
	S_Sound (CHAN_VOICE, voc, 1, ATTN_NORM);
	player->player->SetLogNumber (log);
}
