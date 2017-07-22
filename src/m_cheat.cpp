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
#include <math.h>

#include "m_cheat.h"
#include "d_player.h"
#include "doomstat.h"
#include "gstrings.h"
#include "p_local.h"
#include "gi.h"
#include "p_enemy.h"
#include "sbar.h"
#include "c_dispatch.h"
#include "v_video.h"
#include "w_wad.h"
#include "a_keys.h"
#include "templates.h"
#include "c_console.h"
#include "r_data/r_translate.h"
#include "g_level.h"
#include "d_net.h"
#include "d_dehacked.h"
#include "gi.h"
#include "serializer.h"
#include "r_utility.h"
#include "a_morph.h"
#include "g_levellocals.h"
#include "vm.h"
#include "events.h"
#include "p_acs.h"

// [RH] Actually handle the cheat. The cheat code in st_stuff.c now just
// writes some bytes to the network data stream, and the network code
// later calls us.

void cht_DoMDK(player_t *player, const char *mod)
{
	if (player->mo == NULL)
	{
		Printf("What do you want to kill outside of a game?\n");
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
	AInventory *item;
	const char *msg = "";
	char msgbuild[32];
	int i;

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
	case CHT_GOD:
		player->cheats ^= CF_GODMODE;
		if (player->cheats & CF_GODMODE)
			msg = GStrings("STSTR_DQDON");
		else
			msg = GStrings("STSTR_DQDOFF");
		break;

	case CHT_BUDDHA:
		player->cheats ^= CF_BUDDHA;
		if (player->cheats & CF_BUDDHA)
			msg = GStrings("TXT_BUDDHAON");
		else
			msg = GStrings("TXT_BUDDHAOFF");
		break;

	case CHT_GOD2:
		player->cheats ^= CF_GODMODE2;
		if (player->cheats & CF_GODMODE2)
			msg = GStrings("STSTR_DQD2ON");
		else
			msg = GStrings("STSTR_DQD2OFF");
		break;

	case CHT_BUDDHA2:
		player->cheats ^= CF_BUDDHA2;
		if (player->cheats & CF_BUDDHA2)
			msg = GStrings("TXT_BUDDHA2ON");
		else
			msg = GStrings("TXT_BUDDHA2OFF");
		break;

	case CHT_NOCLIP:
		player->cheats ^= CF_NOCLIP;
		if (player->cheats & CF_NOCLIP)
			msg = GStrings("STSTR_NCON");
		else
			msg = GStrings("STSTR_NCOFF");
		break;

	case CHT_NOCLIP2:
		player->cheats ^= CF_NOCLIP2;
		if (player->cheats & CF_NOCLIP2)
		{
			player->cheats |= CF_NOCLIP;
			msg = GStrings("STSTR_NC2ON");
		}
		else
		{
			player->cheats &= ~CF_NOCLIP;
			msg = GStrings("STSTR_NCOFF");
		}
		if (player->mo->Vel.X == 0) player->mo->Vel.X = MinVel;	// force some lateral movement so that internal variables are up to date
		break;

	case CHT_NOVELOCITY:
		player->cheats ^= CF_NOVELOCITY;
		if (player->cheats & CF_NOVELOCITY)
			msg = GStrings("TXT_LEADBOOTSON");
		else
			msg = GStrings("TXT_LEADBOOTSOFF");
		break;

	case CHT_FLY:
		if (player->mo != NULL)
		{
			player->mo->flags7 ^= MF7_FLYCHEAT;
			if (player->mo->flags7 & MF7_FLYCHEAT)
			{
				player->mo->flags |= MF_NOGRAVITY;
				player->mo->flags2 |= MF2_FLY;
				msg = GStrings("TXT_LIGHTER");
			}
			else
			{
				player->mo->flags &= ~MF_NOGRAVITY;
				player->mo->flags2 &= ~MF2_FLY;
				msg = GStrings("TXT_GRAVITY");
			}
		}
		break;

	case CHT_MORPH:
		msg = cht_Morph (player, PClass::FindActor (gameinfo.gametype == GAME_Heretic ? NAME_ChickenPlayer : NAME_PigPlayer), true);
		break;

	case CHT_NOTARGET:
		player->cheats ^= CF_NOTARGET;
		if (player->cheats & CF_NOTARGET)
			msg = "notarget ON";
		else
			msg = "notarget OFF";
		break;

	case CHT_ANUBIS:
		player->cheats ^= CF_FRIGHTENING;
		if (player->cheats & CF_FRIGHTENING)
			msg = "\"Quake with fear!\"";
		else
			msg = "No more ogre armor";
		break;

	case CHT_CHASECAM:
		player->cheats ^= CF_CHASECAM;
		if (player->cheats & CF_CHASECAM)
			msg = "chasecam ON";
		else
			msg = "chasecam OFF";
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
			msg = GStrings("STSTR_CHOPPERS");
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
				msg = GStrings("TXT_CHEATPOWEROFF");
			}
			else
			{
				player->mo->GiveInventoryType (PClass::FindActor(NAME_PowerWeaponLevel2));
				msg = GStrings("TXT_CHEATPOWERON");
			}
		}
		break;

	case CHT_IDKFA:
		cht_Give (player, "backpack");
		cht_Give (player, "weapons");
		cht_Give (player, "ammo");
		cht_Give (player, "keys");
		cht_Give (player, "armor");
		msg = GStrings("STSTR_KFAADDED");
		break;

	case CHT_IDFA:
		cht_Give (player, "backpack");
		cht_Give (player, "weapons");
		cht_Give (player, "ammo");
		cht_Give (player, "armor");
		msg = GStrings("STSTR_FAADDED");
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
			level.flags2 ^= LEVEL2_ALLMAP;
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
		msg = GStrings("STSTR_BEHOLDX");
		break;

	case CHT_MASSACRE:
	case CHT_MASSACRE2:
		{
			int killcount = P_Massacre (cheat == CHT_MASSACRE2);
			// killough 3/22/98: make more intelligent about plural
			// Ty 03/27/98 - string(s) *not* externalized
			mysnprintf (msgbuild, countof(msgbuild), "%d %s%s Killed", killcount,
				cheat==CHT_MASSACRE2 ? "Baddie" : "Monster", killcount==1 ? "" : "s");
			msg = msgbuild;
		}
		break;

	case CHT_HEALTH:
		if (player->mo != NULL && player->playerstate == PST_LIVE)
		{
			player->health = player->mo->health = player->mo->GetDefault()->health;
			msg = GStrings("TXT_CHEATHEALTH");
		}
		break;

	case CHT_KEYS:
		cht_Give (player, "keys");
		msg = GStrings("TXT_CHEATKEYS");
		break;

	// [GRB]
	case CHT_RESSURECT:
		if (player->playerstate != PST_LIVE && player->mo != nullptr)
		{
			if (player->mo->IsKindOf("PlayerChunk"))
			{
				Printf("Unable to resurrect. Player is no longer connected to its body.\n");
			}
			else
			{
				player->Resurrect();

			}
		}
		break;

	case CHT_GIMMIEA:
		cht_Give (player, "ArtiInvulnerability");
		msg = "Valador's Ring of Invunerability";
		break;

	case CHT_GIMMIEB:
		cht_Give (player, "ArtiInvisibility");
		msg = "Shadowsphere";
		break;

	case CHT_GIMMIEC:
		cht_Give (player, "ArtiHealth");
		msg = "Quartz Flask";
		break;

	case CHT_GIMMIED:
		cht_Give (player, "ArtiSuperHealth");
		msg = "Mystic Urn";
		break;

	case CHT_GIMMIEE:
		cht_Give (player, "ArtiTomeOfPower");
		msg = "Tyketto's Tome of Power";
		break;

	case CHT_GIMMIEF:
		cht_Give (player, "ArtiTorch");
		msg = "Torch";
		break;

	case CHT_GIMMIEG:
		cht_Give (player, "ArtiTimeBomb");
		msg = "Delmintalintar's Time Bomb of the Ancients";
		break;

	case CHT_GIMMIEH:
		cht_Give (player, "ArtiEgg");
		msg = "Torpol's Morph Ovum";
		break;

	case CHT_GIMMIEI:
		cht_Give (player, "ArtiFly");
		msg = "Inhilicon's Wings of Wrath";
		break;

	case CHT_GIMMIEJ:
		cht_Give (player, "ArtiTeleport");
		msg = "Darchala's Chaos Device";
		break;

	case CHT_GIMMIEZ:
		for (int i=0;i<16;i++)
		{
			cht_Give (player, "artifacts");
		}
		msg = "All artifacts!";
		break;

	case CHT_TAKEWEAPS:
		if (player->morphTics || player->mo == NULL || player->mo->health <= 0)
		{
			return;
		}
		{
			// Take away all weapons that are either non-wimpy or use ammo.
			AInventory **invp = &player->mo->Inventory, **lastinvp;
			for (item = *invp; item != NULL; item = *invp)
			{
				lastinvp = invp;
				invp = &(*invp)->Inventory;
				if (item->IsKindOf(NAME_Weapon))
				{
					AWeapon *weap = static_cast<AWeapon *> (item);
					if (!(weap->WeaponFlags & WIF_WIMPY_WEAPON) ||
						weap->AmmoType1 != NULL)
					{
						item->Destroy ();
						invp = lastinvp;
					}
				}
			}
		}
		msg = GStrings("TXT_CHEATIDKFA");
		break;

	case CHT_NOWUDIE:
		cht_Suicide (player);
		msg = GStrings("TXT_CHEATIDDQD");
		break;

	case CHT_ALLARTI:
		for (int i=0;i<25;i++)
		{
			cht_Give (player, "artifacts");
		}
		msg = GStrings("TXT_CHEATARTIFACTS3");
		break;

	case CHT_PUZZLE:
		cht_Give (player, "puzzlepieces");
		msg = GStrings("TXT_CHEATARTIFACTS3");
		break;

	case CHT_MDK:
		if (player->mo == NULL)
		{
			Printf ("What do you want to kill outside of a game?\n");
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
		msg = GStrings("TXT_MIDASTOUCH");
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
						player->PendingWeapon = static_cast<AWeapon *> (item);
					}
				}
			}
		}
		break;

	case CHT_PUMPUPH:
		cht_Give (player, "MedPatch");
		cht_Give (player, "MedicalKit");
		cht_Give (player, "SurgeryKit");
		msg = GStrings("TXT_GOTSTUFF");
		break;

	case CHT_PUMPUPP:
		cht_Give (player, "AmmoSatchel");
		msg = GStrings("TXT_GOTSTUFF");
		break;

	case CHT_PUMPUPS:
		cht_Give (player, "UpgradeStamina", 10);
		cht_Give (player, "UpgradeAccuracy");
		msg = GStrings("TXT_GOTSTUFF");
		break;

	case CHT_CLEARFROZENPROPS:
		player->cheats &= ~(CF_FROZEN|CF_TOTALLYFROZEN);
		msg = "Frozen player properties turned off";
		break;

	case CHT_FREEZE:
		bglobal.changefreeze ^= 1;
		if (bglobal.freeze ^ bglobal.changefreeze)
		{
			msg = GStrings("TXT_FREEZEON");
		}
		else
		{
			msg = GStrings("TXT_FREEZEOFF");
		}
		break;
	}

	if (!*msg)              // [SO] Don't print blank lines!
		return;

	if (player == &players[consoleplayer])
		Printf ("%s\n", msg);
	else if (cheat != CHT_CHASECAM)
		Printf ("%s cheats: %s\n", player->userinfo.GetName(), msg);
}

const char *cht_Morph (player_t *player, PClassActor *morphclass, bool quickundo)
{
	if (player->mo == NULL)
	{
		return "";
	}
	auto oldclass = player->mo->GetClass();

	// Set the standard morph style for the current game
	int style = MORPH_UNDOBYTOMEOFPOWER;
	if (gameinfo.gametype == GAME_Hexen) style |= MORPH_UNDOBYCHAOSDEVICE;

	if (player->morphTics)
	{
		if (P_UndoPlayerMorph (player, player))
		{
			if (!quickundo && oldclass != morphclass && P_MorphPlayer (player, player, morphclass, 0, style))
			{
				return GStrings("TXT_STRANGER");
			}
			return GStrings("TXT_NOTSTRANGE");
		}
	}
	else if (P_MorphPlayer (player, player, morphclass, 0, style))
	{
		return GStrings("TXT_STRANGE");
	}
	return "";
}

void cht_SetInv(player_t *player, const char *string, int amount, bool beyond)
{
	if (!stricmp(string, "health"))
	{
		if (amount <= 0)
		{
			cht_Suicide(player);
			return;
		}
		if (!beyond) amount = MIN(amount, player->mo->GetMaxHealth(true));
		player->health = player->mo->health = amount;
	}
	else
	{
		auto item = PClass::FindActor(string);
		if (item != nullptr && item->IsDescendantOf(RUNTIME_CLASS(AInventory)))
		{
			player->mo->SetInventory(item, amount, beyond);
			return;
		}
		Printf("Unknown item \"%s\"\n", string);
	}
}

void cht_Give (player_t *player, const char *name, int amount)
{
	if (player->mo == nullptr)	return;

	IFVIRTUALPTR(player->mo, APlayerPawn, CheatGive)
	{
		FString namestr = name;
		VMValue params[3] = { player->mo, &namestr, amount };
		VMCall(func, params, 3, nullptr, 0);
	}
}

void cht_Take (player_t *player, const char *name, int amount)
{
	if (player->mo == nullptr) return;

	IFVIRTUALPTR(player->mo, APlayerPawn, CheatTake)
	{
		FString namestr = name;
		VMValue params[3] = { player->mo, &namestr, amount };
		VMCall(func, params, 3, nullptr, 0);
	}
}

class DSuicider : public DThinker
{
	DECLARE_CLASS(DSuicider, DThinker)
	HAS_OBJECT_POINTERS;
public:
	TObjPtr<APlayerPawn*> Pawn;

	void Tick()
	{
		Pawn->flags |= MF_SHOOTABLE;
		Pawn->flags2 &= ~MF2_INVULNERABLE;
		// Store the player's current damage factor, to restore it later.
		double plyrdmgfact = Pawn->DamageFactor;
		Pawn->DamageFactor = 1.;
		P_DamageMobj (Pawn, Pawn, Pawn, TELEFRAG_DAMAGE, NAME_Suicide);
		Pawn->DamageFactor = plyrdmgfact;
		if (Pawn->health <= 0)
		{
			Pawn->flags &= ~MF_SHOOTABLE;
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
		DSuicider *suicide = Create<DSuicider>();
		suicide->Pawn = plyr->mo;
		GC::WriteBarrier(suicide, suicide->Pawn);
	}
}

DEFINE_ACTION_FUNCTION(APlayerPawn, CheatSuicide)
{
	PARAM_SELF_PROLOGUE(APlayerPawn);
	cht_Suicide(self->player);
	return 0;
}

CCMD (mdk)
{
	if (CheckCheatmode ())
		return;

	const char *name = argv.argc() > 1 ? argv[1] : "";
	Net_WriteByte (DEM_MDK);
	Net_WriteString(name);
}
