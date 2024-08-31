/*
**
**
**---------------------------------------------------------------------------
** Copyright 1999-2016 Randy Heit
** Copyright 2005-2016 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/
// DESCRIPTION:
//		Cheat sequence checking.
//


#include <stdlib.h>

#include "m_cheat.h"
#include "d_player.h"
#include "gstrings.h"
#include "p_local.h"
#include "gi.h"
#include "p_enemy.h"
#include "sbar.h"
#include "c_dispatch.h"
#include "a_keys.h"
#include "d_net.h"
#include "serializer.h"
#include "serialize_obj.h"
#include "r_utility.h"
#include "a_morph.h"
#include "g_levellocals.h"
#include "vm.h"
#include "d_main.h"

uint8_t globalfreeze, globalchangefreeze;	// user's freeze state.

// [RH] Actually handle the cheat. The cheat code in st_stuff.c now just
// writes some bytes to the network data stream, and the network code
// later calls us.

void cht_DoMDK(player_t *player, const char *mod)
{
	if (player->mo == NULL)
	{
		Printf("%s\n", GStrings.GetString("TXT_WHAT_KILL"));
	}
	else if (!deathmatch)
	{
		// Don't allow this in deathmatch even with cheats enabled, because it's
		// a very very cheap kill.
		P_LineAttack(player->mo, player->mo->Angles.Yaw, PLAYERMISSILERANGE,
			P_AimLineAttack(player->mo, player->mo->Angles.Yaw, PLAYERMISSILERANGE), TELEFRAG_DAMAGE,
			mod, NAME_BulletPuff);
	}
}

void cht_DoCheat (player_t *player, int cheat)
{
	static const char * const BeholdPowers[9] =
	{
		"PowerInvulnerable",
		"PowerStrength",
		"PowerInvisibility",
		"PowerIronFeet",
		"MapRevealer",
		"PowerLightAmp",
		"PowerShadow",
		"PowerMask",
		"PowerTargeter",
	};
	PClassActor *type;
	AActor *item;
	FString smsg;
	const char *msg = "";
	int i;

	// No cheating when not having a pawn attached.
	if (player->mo == nullptr)
	{
		return;
	}

	switch (cheat)
	{
	case CHT_IDDQD:
		if (!(player->cheats & CF_GODMODE) && player->playerstate == PST_LIVE)
		{
			if (player->mo)
				player->mo->health = deh.GodHealth;

			player->health = deh.GodHealth;
		}
		// fall through to CHT_GOD
		[[fallthrough]];
	case CHT_GOD:
		player->cheats ^= CF_GODMODE;
		if (player->cheats & CF_GODMODE)
			msg = GStrings.GetString("STSTR_DQDON");
		else
			msg = GStrings.GetString("STSTR_DQDOFF");
		break;

	case CHT_BUDDHA:
		player->cheats ^= CF_BUDDHA;
		if (player->cheats & CF_BUDDHA)
			msg = GStrings.GetString("TXT_BUDDHAON");
		else
			msg = GStrings.GetString("TXT_BUDDHAOFF");
		break;

	case CHT_GOD2:
		player->cheats ^= CF_GODMODE2;
		if (player->cheats & CF_GODMODE2)
			msg = GStrings.GetString("STSTR_DQD2ON");
		else
			msg = GStrings.GetString("STSTR_DQD2OFF");
		break;

	case CHT_BUDDHA2:
		player->cheats ^= CF_BUDDHA2;
		if (player->cheats & CF_BUDDHA2)
			msg = GStrings.GetString("TXT_BUDDHA2ON");
		else
			msg = GStrings.GetString("TXT_BUDDHA2OFF");
		break;

	case CHT_NOCLIP:
		player->cheats ^= CF_NOCLIP;
		if (player->cheats & CF_NOCLIP)
			msg = GStrings.GetString("STSTR_NCON");
		else
			msg = GStrings.GetString("STSTR_NCOFF");
		break;

	case CHT_NOCLIP2:
		player->cheats ^= CF_NOCLIP2;
		if (player->cheats & CF_NOCLIP2)
		{
			player->cheats |= CF_NOCLIP;
			msg = GStrings.GetString("STSTR_NC2ON");
		}
		else
		{
			player->cheats &= ~CF_NOCLIP;
			msg = GStrings.GetString("STSTR_NC2OFF");
		}
		if (player->mo->Vel.X == 0) player->mo->Vel.X = MinVel;	// force some lateral movement so that internal variables are up to date
		break;

	case CHT_NOVELOCITY:
		player->cheats ^= CF_NOVELOCITY;
		if (player->cheats & CF_NOVELOCITY)
			msg = GStrings.GetString("TXT_LEADBOOTSON");
		else
			msg = GStrings.GetString("TXT_LEADBOOTSOFF");
		break;

	case CHT_FLY:
		if (player->mo != NULL)
		{
			player->mo->flags7 ^= MF7_FLYCHEAT;
			if (player->mo->flags7 & MF7_FLYCHEAT)
			{
				player->mo->flags |= MF_NOGRAVITY;
				player->mo->flags2 |= MF2_FLY;
				msg = GStrings.GetString("TXT_LIGHTER");
			}
			else
			{
				player->mo->flags &= ~MF_NOGRAVITY;
				player->mo->flags2 &= ~MF2_FLY;
				msg = GStrings.GetString("TXT_GRAVITY");
			}
		}
		break;

	case CHT_MORPH:
		smsg = cht_Morph (player, PClass::FindActor (gameinfo.gametype == GAME_Heretic ? NAME_ChickenPlayer : NAME_PigPlayer), true);
		msg = smsg.GetChars();
		break;

	case CHT_NOTARGET:
		player->cheats ^= CF_NOTARGET;
		if (player->cheats & CF_NOTARGET)
			msg = GStrings.GetString("TXT_NOTARGET_ON");
		else
			msg = GStrings.GetString("TXT_NOTARGET_OFF");
		break;

	case CHT_ANUBIS:
		player->cheats ^= CF_FRIGHTENING;
		if (player->cheats & CF_FRIGHTENING)
			msg = GStrings.GetString("TXT_ANUBIS_ON");
		else
			msg = GStrings.GetString("TXT_ANUBIS_OFF");
		break;

	case CHT_CHASECAM:
		player->cheats ^= CF_CHASECAM;
		if (player->cheats & CF_CHASECAM)
			msg = GStrings.GetString("TXT_CHASECAM_ON");
		else
			msg = GStrings.GetString("TXT_CHASECAM_OFF");
		R_ResetViewInterpolation ();
		break;

	case CHT_CHAINSAW:
		if (player->mo != NULL && player->health >= 0)
		{
			type = PClass::FindActor("Chainsaw");
			if (player->mo->FindInventory (type) == NULL)
			{
				player->mo->GiveInventoryType (type);
			}
			msg = GStrings.GetString("STSTR_CHOPPERS");
		}
		// [RH] The original cheat also set powers[pw_invulnerability] to true.
		// Since this is a timer and not a boolean, it effectively turned off
		// the invulnerability powerup, although it looks like it was meant to
		// turn it on.
		break;

	case CHT_POWER:
		if (player->mo != NULL && player->health >= 0)
		{
			item = player->mo->FindInventory (PClass::FindActor(NAME_PowerWeaponLevel2), true);
			if (item != NULL)
			{
				item->Destroy ();
				msg = GStrings.GetString("TXT_CHEATPOWEROFF");
			}
			else
			{
				player->mo->GiveInventoryType (PClass::FindActor(NAME_PowerWeaponLevel2));
				msg = GStrings.GetString("TXT_CHEATPOWERON");
			}
		}
		break;

	case CHT_IDKFA:
		cht_Give (player, "backpack");
		cht_Give (player, "weapons");
		cht_Give (player, "ammo");
		cht_Give (player, "keys");
		cht_Give (player, "armor");
		msg = GStrings.GetString("STSTR_KFAADDED");
		break;

	case CHT_IDFA:
		cht_Give (player, "backpack");
		cht_Give (player, "weapons");
		cht_Give (player, "ammo");
		cht_Give (player, "armor");
		msg = GStrings.GetString("STSTR_FAADDED");
		break;

	case CHT_BEHOLDV:
	case CHT_BEHOLDS:
	case CHT_BEHOLDI:
	case CHT_BEHOLDR:
	case CHT_BEHOLDA:
	case CHT_BEHOLDL:
	case CHT_PUMPUPI:
	case CHT_PUMPUPM:
	case CHT_PUMPUPT:
		i = cheat - CHT_BEHOLDV;

		if (i == 4)
		{
			for (auto Level : AllLevels())
			{
				Level->flags2 ^= LEVEL2_ALLMAP;
			}
		}
		else if (player->mo != NULL && player->health >= 0)
		{
			item = player->mo->FindInventory(BeholdPowers[i]);
			if (item == NULL)
			{
				if (i != 0)
				{
					cht_Give(player, BeholdPowers[i]);
					if (cheat == CHT_BEHOLDS)
					{
						P_GiveBody (player->mo, -100);
					}
				}
				else
				{
					// Let's give the item here so that the power doesn't need colormap information.
					cht_Give(player, "InvulnerabilitySphere");
				}
			}
			else
			{
				item->Destroy ();
			}
		}
		msg = GStrings.GetString("STSTR_BEHOLDX");
		break;

	case CHT_MASSACRE:
	case CHT_MASSACRE2:
		{
			int killcount = primaryLevel->Massacre (cheat == CHT_MASSACRE2);
			// killough 3/22/98: make more intelligent about plural
			if (killcount == 1)
			{
				msg = GStrings.GetString(cheat == CHT_MASSACRE? "TXT_MONSTER_KILLED" : "TXT_BADDIE_KILLED");
			}
			else
			{
				// Note: Do not use the language string directly as a format template!
				smsg = GStrings.GetString(cheat == CHT_MASSACRE? "TXT_MONSTERS_KILLED" : "TXT_BADDIES_KILLED");
				FStringf countstr("%d", killcount);
				smsg.Substitute("%d", countstr);
				msg = smsg.GetChars();
			}
		}
		break;

	case CHT_HEALTH:
		if (player->mo != NULL && player->playerstate == PST_LIVE)
		{
			player->health = player->mo->health = player->mo->GetDefault()->health;
			msg = GStrings.GetString("TXT_CHEATHEALTH");
		}
		break;

	case CHT_KEYS:
		cht_Give (player, "keys");
		msg = GStrings.GetString("TXT_CHEATKEYS");
		break;

	// [GRB]
	case CHT_RESSURECT:
		if (player->playerstate != PST_LIVE && player->mo != nullptr)
		{
			if (player->mo->IsKindOf("PlayerChunk"))
			{
				Printf("%s\n", GStrings.GetString("TXT_NO_RESURRECT"));
				return;
			}
			else
			{
				player->Resurrect();
			}
		}
		break;

	case CHT_GIMMIEA:
		cht_Give (player, "ArtiInvulnerability");
		msg = GStrings.GetString("TAG_ARTIINVULNERABILITY");
		break;

	case CHT_GIMMIEB:
		cht_Give (player, "ArtiInvisibility");
		msg = GStrings.GetString("TAG_ARTIINVISIBILITY");
		break;

	case CHT_GIMMIEC:
		cht_Give (player, "ArtiHealth");
		msg = GStrings.GetString("TAG_ARTIHEALTH");
		break;

	case CHT_GIMMIED:
		cht_Give (player, "ArtiSuperHealth");
		msg = GStrings.GetString("TAG_ARTISUPERHEALTH");
		break;

	case CHT_GIMMIEE:
		cht_Give (player, "ArtiTomeOfPower");
		msg = GStrings.GetString("TAG_ARTITOMEOFPOWER");
		break;

	case CHT_GIMMIEF:
		cht_Give (player, "ArtiTorch");
		msg = GStrings.GetString("TAG_ARTITORCH");
		break;

	case CHT_GIMMIEG:
		cht_Give (player, "ArtiTimeBomb");
		msg = GStrings.GetString("TAG_ARTIFIREBOMB");
		break;

	case CHT_GIMMIEH:
		cht_Give (player, "ArtiEgg");
		msg = GStrings.GetString("TAG_ARTIEGG");
		break;

	case CHT_GIMMIEI:
		cht_Give (player, "ArtiFly");
		msg = GStrings.GetString("TAG_ARTIFLY");
		break;

	case CHT_GIMMIEJ:
		cht_Give (player, "ArtiTeleport");
		msg = GStrings.GetString("TAG_ARTITELEPORT");
		break;

	case CHT_GIMMIEZ:
		for (int i=0;i<16;i++)
		{
			cht_Give (player, "artifacts");
		}
		msg = GStrings.GetString("TAG_ALL_ARTIFACTS");
		break;

	case CHT_TAKEWEAPS:
		cht_Takeweaps(player);
		msg = GStrings.GetString("TXT_CHEATIDKFA");
		break;

	case CHT_NOWUDIE:
		cht_Suicide (player);
		msg = GStrings.GetString("TXT_CHEATIDDQD");
		break;

	case CHT_ALLARTI:
		for (int i=0;i<25;i++)
		{
			cht_Give (player, "artifacts");
		}
		msg = GStrings.GetString("TXT_CHEATARTIFACTS3");
		break;

	case CHT_PUZZLE:
		cht_Give (player, "puzzlepieces");
		msg = GStrings.GetString("TXT_CHEATARTIFACTS3");
		break;

	case CHT_MDK:
		if (player->mo == nullptr)
		{
			Printf ("%s\n", GStrings.GetString("TXT_WHAT_KILL"));
			return;
		}
		else if (!deathmatch)
		{
			// Don't allow this in deathmatch even with cheats enabled, because it's
			// a very very cheap kill.
			P_LineAttack (player->mo, player->mo->Angles.Yaw, PLAYERMISSILERANGE,
				P_AimLineAttack (player->mo, player->mo->Angles.Yaw, PLAYERMISSILERANGE), TELEFRAG_DAMAGE,
				NAME_MDK, NAME_BulletPuff);
		}
		break;

	case CHT_DONNYTRUMP:
		cht_Give (player, "HealthTraining");
		msg = GStrings.GetString("TXT_MIDASTOUCH");
		break;

	case CHT_LEGO:
		if (player->mo != NULL && player->health >= 0)
		{
			static VMFunction *gsp = nullptr;
			if (gsp == nullptr) PClass::FindFunction(&gsp, NAME_Sigil, NAME_GiveSigilPiece);
			if (gsp)
			{
				VMValue params[1] = { player->mo };
				VMReturn ret;
				int oldpieces = 1;
				ret.IntAt(&oldpieces);
				VMCall(gsp, params, 1, &ret, 1);
				item = player->mo->FindInventory(NAME_Sigil);

				if (item != NULL)
				{
					if (oldpieces == 5)
					{
						item->Destroy();
					}
					else
					{
						player->PendingWeapon = item;
					}
				}
			}
		}
		break;

	case CHT_PUMPUPH:
		cht_Give (player, "MedPatch");
		cht_Give (player, "MedicalKit");
		cht_Give (player, "SurgeryKit");
		msg = GStrings.GetString("TXT_GOTSTUFF");
		break;

	case CHT_PUMPUPP:
		cht_Give (player, "AmmoSatchel");
		msg = GStrings.GetString("TXT_GOTSTUFF");
		break;

	case CHT_PUMPUPS:
		cht_Give (player, "UpgradeStamina", 10);
		cht_Give (player, "UpgradeAccuracy");
		msg = GStrings.GetString("TXT_GOTSTUFF");
		break;

	case CHT_CLEARFROZENPROPS:
		player->cheats &= ~(CF_FROZEN|CF_TOTALLYFROZEN);
		msg = GStrings.GetString("TXT_NOT_FROZEN");
		break;

	case CHT_FREEZE:
		globalchangefreeze ^= 1;
		if (globalfreeze ^ globalchangefreeze)
		{
			msg = GStrings.GetString("TXT_FREEZEON");
		}
		else
		{
			msg = GStrings.GetString("TXT_FREEZEOFF");
		}
		break;
	}

	if (!msg || !*msg)              // [SO] Don't print blank lines!
		return;

	if (player == &players[consoleplayer])
		Printf ("%s\n", msg);
	else if (cheat != CHT_CHASECAM)
	{
		FString message = GStrings.GetString("TXT_X_CHEATS");
		message.Substitute("%s", player->userinfo.GetName());
		Printf("%s: %s\n", message.GetChars(), msg);
	}
}

FString cht_Morph(player_t *player, PClassActor *morphclass, bool quickundo)
{
	if (player->mo == nullptr)	return "";

	IFVIRTUALPTRNAME(player->mo, NAME_PlayerPawn, CheatMorph)
	{
		FString message;
		VMReturn msgret(&message);
		VMValue params[3] = { player->mo, morphclass, quickundo };
		VMCall(func, params, 3, &msgret, 1);
		return message;
	}
	return "";
}

void cht_SetInv(player_t *player, const char *string, int amount, bool beyond)
{
	if (!player->mo) return;
	IFVIRTUALPTRNAME(player->mo, NAME_PlayerPawn, CheatTakeInv)
	{
		FString message = string;
		VMValue params[] = { player->mo, &message, amount, beyond };
		VMCall(func, params, 4, nullptr, 0);
	}
}

void cht_Give (player_t *player, const char *name, int amount)
{
	if (!player->mo) return;
	IFVIRTUALPTRNAME(player->mo, NAME_PlayerPawn, CheatGive)
	{
		FString namestr = name;
		VMValue params[3] = { player->mo, &namestr, amount };
		VMCall(func, params, 3, nullptr, 0);
	}
}

void cht_Take (player_t *player, const char *name, int amount)
{
	if (!player->mo) return;
	IFVIRTUALPTRNAME(player->mo, NAME_PlayerPawn, CheatTake)
	{
		FString namestr = name;
		VMValue params[3] = { player->mo, &namestr, amount };
		VMCall(func, params, 3, nullptr, 0);
	}
}

void cht_Takeweaps(player_t *player)
{
	if (!player->mo) return;
	IFVIRTUALPTRNAME(player->mo, NAME_PlayerPawn, CheatTakeWeaps)
	{
		VMValue params[3] = { player->mo };
		VMCall(func, params, 1, nullptr, 0);
	}
}

class DSuicider : public DThinker
{
	DECLARE_CLASS(DSuicider, DThinker)
	HAS_OBJECT_POINTERS;
public:
	TObjPtr<AActor*> Pawn;

	void Construct() {}
	void Tick()
	{
		Pawn->flags |= MF_SHOOTABLE;
		Pawn->flags2 &= ~MF2_INVULNERABLE;
		// Store the player's current damage factor, to restore it later.
		double plyrdmgfact = Pawn->DamageFactor;
		Pawn->DamageFactor = 1.;
		P_DamageMobj (Pawn, Pawn, Pawn, TELEFRAG_DAMAGE, NAME_Suicide);
		if (Pawn != nullptr)
		{
			Pawn->DamageFactor = plyrdmgfact;
			if (Pawn->health <= 0)
			{
				Pawn->flags &= ~MF_SHOOTABLE;
			}
		}
		Destroy();
	}
	// You'll probably never be able to catch this in a save game, but
	// just in case, add a proper serializer.
	void Serialize(FSerializer &arc)
	{ 
		Super::Serialize(arc);
		arc("pawn", Pawn);
	}
};

IMPLEMENT_CLASS(DSuicider, false, true)

IMPLEMENT_POINTERS_START(DSuicider)
	IMPLEMENT_POINTER(Pawn)
IMPLEMENT_POINTERS_END

void cht_Suicide (player_t *plyr)
{
	// If this cheat was initiated by the suicide ccmd, and this is a single
	// player game, the CHT_SUICIDE will be processed before the tic is run,
	// so the console has not gone up yet. Use a temporary thinker to delay
	// the suicide until the game ticks so that death noises can be heard on
	// the initial tick.
	if (plyr->mo != NULL)
	{
		DSuicider *suicide = plyr->mo->Level->CreateThinker<DSuicider>();
		suicide->Pawn = plyr->mo;
		GC::WriteBarrier(suicide, suicide->Pawn);
	}
}

DEFINE_ACTION_FUNCTION(APlayerPawn, CheatSuicide)
{
	PARAM_SELF_PROLOGUE(AActor);
	cht_Suicide(self->player);
	return 0;
}

CCMD (mdk)
{
	if (CheckCheatmode ())
		return;

	const char *name = argv.argc() > 1 ? argv[1] : "";
	Net_WriteInt8 (DEM_MDK);
	Net_WriteString(name);
}
