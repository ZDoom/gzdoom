// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// $Log:$
//
// DESCRIPTION:
//		Cheat sequence checking.
//
//-----------------------------------------------------------------------------


#include <stdlib.h>
#include <math.h>

#include "m_cheat.h"
#include "d_player.h"
#include "doomstat.h"
#include "gstrings.h"
#include "p_inter.h"
#include "d_items.h"
#include "p_local.h"
#include "a_doomglobal.h"
#include "gi.h"
#include "p_enemy.h"
#include "sbar.h"
#include "c_dispatch.h"
#include "v_video.h"

// [RH] Actually handle the cheat. The cheat code in st_stuff.c now just
// writes some bytes to the network data stream, and the network code
// later calls us.

void cht_DoCheat (player_t *player, int cheat)
{
	const char *msg = "";
	char msgbuild[32];
	int i;

	switch (cheat)
	{
	case CHT_IDDQD:
		if (!(player->cheats & CF_GODMODE))
		{
			if (player->mo)
				player->mo->health = deh.GodHealth;

			player->health = deh.GodHealth;
		}
	case CHT_GOD:
		player->cheats ^= CF_GODMODE;
		if (player->cheats & CF_GODMODE)
			msg = GStrings(STSTR_DQDON);
		else
			msg = GStrings(STSTR_DQDOFF);
		SB_state = screen->GetPageCount ();
		break;

	case CHT_NOCLIP:
		player->cheats ^= CF_NOCLIP;
		if (player->cheats & CF_NOCLIP)
			msg = GStrings(STSTR_NCON);
		else
			msg = GStrings(STSTR_NCOFF);
		break;

	case CHT_FLY:
		player->cheats ^= CF_FLY;
		if (player->cheats & CF_FLY)
			msg = "You feel lighter";
		else
			msg = "Gravity weighs you down";
		break;

	case CHT_MORPH:
		if (player->morphTics)
		{
			if (P_UndoPlayerMorph (player))
			{
				msg = "You feel like yourself again";
			}
		}
		else if (P_MorphPlayer (player))
		{
			msg = "You feel strange...";
		}
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
		break;

	case CHT_CHAINSAW:
		player->weaponowned[wp_chainsaw] = true;
		player->powers[pw_invulnerability] = true;
		msg = GStrings(STSTR_CHOPPERS);
		break;

	case CHT_POWER:
		P_GivePower (player, pw_weaponlevel2);
		break;

	case CHT_IDKFA:
		cht_Give (player, "backpack");
		cht_Give (player, "weapons");
		cht_Give (player, "ammo");
		cht_Give (player, "keys");
		player->armorpoints[0] = deh.KFAArmor;
		player->armortype = deh.KFAAC;
		msg = GStrings(STSTR_KFAADDED);
		break;

	case CHT_IDFA:
		cht_Give (player, "backpack");
		cht_Give (player, "weapons");
		cht_Give (player, "ammo");
		player->armorpoints[0] = deh.FAArmor;
		player->armortype = deh.FAAC;
		msg = GStrings(STSTR_FAADDED);
		break;

	case CHT_BEHOLDV:
	case CHT_BEHOLDS:
	case CHT_BEHOLDI:
	case CHT_BEHOLDR:
	case CHT_BEHOLDA:
	case CHT_BEHOLDL:
		i = cheat - CHT_BEHOLDV;

		if (!player->powers[i])
			P_GivePower (player, (powertype_t)i);
		else if (i!=pw_strength)
			player->powers[i] = 1;
		else
			player->powers[i] = 0;

		msg = GStrings(STSTR_BEHOLDX);
		break;

	case CHT_MASSACRE:
		{
			int killcount = P_Massacre ();
			// killough 3/22/98: make more intelligent about plural
			// Ty 03/27/98 - string(s) *not* externalized
			sprintf (msgbuild, "%d Monster%s Killed", killcount, killcount==1 ? "" : "s");
			msg = msgbuild;
		}
		break;

	case CHT_HEALTH:
		player->health = player->mo->health = player->mo->GetDefault()->health;
		msg = GStrings(TXT_CHEATHEALTH);
		break;

	case CHT_KEYS:
		cht_Give (player, "keys");
		msg = GStrings(TXT_CHEATKEYS);
		break;

	case CHT_TAKEWEAPS:
		if (player->morphTics)
		{
			return;
		}
		for (i = 0; i < NUMWEAPONS; i++)
		{
			player->weaponowned[i] = false;
		}
		player->weaponowned[wp_staff] = true;
		player->pendingweapon = wp_staff;
		msg = GStrings(TXT_CHEATIDKFA);
		break;

	case CHT_NOWUDIE:
		cht_Suicide (player);
		msg = GStrings(TXT_CHEATIDDQD);
		break;

	case CHT_ALLARTI:
		for (i = arti_none+1; i < arti_firstpuzzitem; i++)
		{
			int j;
			for (j = 0; j < 25; j++)
			{
				P_GiveArtifact (player, (artitype_t)i);
			}
		}
		msg = GStrings(TXT_CHEATARTIFACTS3);
		break;

	case CHT_PUZZLE:
		for (i = arti_firstpuzzitem; i < NUMARTIFACTS; i++)
		{
			P_GiveArtifact (player, (artitype_t)i);
		}
		msg = GStrings(TXT_CHEATARTIFACTS3);
		break;

	case CHT_MDK:
		P_LineAttack (player->mo, player->mo->angle, MISSILERANGE,
			P_AimLineAttack (player->mo, player->mo->angle, MISSILERANGE), 10000);
		break;
	}

	if (player == &players[consoleplayer])
		Printf ("%s\n", msg);
	else
		Printf ("%s is a cheater: %s\n", player->userinfo.netname, msg);
}

void cht_Give (player_t *player, char *name)
{
	BOOL giveall;
	int i;
	gitem_t *it;

	if (player != &players[consoleplayer])
		Printf ("%s is a cheater: give %s\n", player->userinfo.netname, name);

	if (stricmp (name, "all") == 0)
		giveall = true;
	else
		giveall = false;

	if (giveall || strnicmp (name, "health", 6) == 0) {
		int h;

		if (0 < (h = atoi (name + 6))) {
			if (player->mo) {
				player->mo->health += h;
	  			player->health = player->mo->health;
			} else {
				player->health += h;
			}
		} else {
			if (player->mo)
				player->mo->health = deh.GodHealth;
	  
			player->health = deh.GodHealth;
		}

		if (!giveall)
			return;
	}

	if (giveall || stricmp (name, "backpack") == 0) {
		if (!player->backpack) {
			for (i=0 ; i<NUMAMMO ; i++)
			player->maxammo[i] *= 2;
			player->backpack = true;
		}
		for (i=0 ; i<NUMAMMO ; i++)
			P_GiveAmmo (player, (ammotype_t)i, 1);

		if (!giveall)
			return;
	}

	if (giveall || stricmp (name, "weapons") == 0) {
		weapontype_t pendweap = player->pendingweapon;
		for (i = 0; i<NUMWEAPONS; i++)
			P_GiveWeapon (player, (weapontype_t)i, false);
		player->pendingweapon = pendweap;

		if (!giveall)
			return;
	}

	if (giveall || stricmp (name, "ammo") == 0) {
		for (i=0;i<NUMAMMO;i++)
			player->ammo[i] = player->maxammo[i];

		if (!giveall)
			return;
	}

	if (giveall || stricmp (name, "armor") == 0) {
		player->armorpoints[0] = 200;
		player->armortype = 2;

		if (!giveall)
			return;
	}

	if (giveall || stricmp (name, "keys") == 0) {
		for (i=0;i<NUMKEYS;i++)
			player->keys[i] = true;

		if (!giveall)
			return;
	}

	if (giveall)
		return;

	it = FindItem (name);
	if (!it) {
		it = FindItemByClassname (name);
		if (!it) {
			if (player == &players[consoleplayer])
				Printf ("Unknown item\n");
			return;
		}
	}

	if (it->flags & IT_AMMO) {
		int howmuch;

	/*	if (argc == 3)
			howmuch = atoi (argv[2]);
		else */
			howmuch = it->quantity;

		P_GiveAmmo (player, (ammotype_t)it->offset, howmuch);
	} else if (it->flags & IT_WEAPON) {
		P_GiveWeapon (player, (weapontype_t)it->offset, 0);
	} else if (it->flags & IT_KEY) {
		P_GiveKey (player, (keytype_t)it->offset);
	} else if (it->flags & IT_POWERUP) {
		P_GivePower (player, (powertype_t)it->offset);
	} else if (it->flags & IT_ARMOR) {
		P_GiveArmor (player, (armortype_t)it->offset, it->offset * 100);
	}
}

void cht_Suicide (player_t *plyr)
{
	plyr->mo->flags |= MF_SHOOTABLE;
	while (plyr->health > 0)
		P_DamageMobj (plyr->mo, plyr->mo, plyr->mo, 10000, MOD_SUICIDE);
	plyr->mo->flags &= ~MF_SHOOTABLE;
}

BOOL CheckCheatmode ();

CCMD (mdk)
{
	if (CheckCheatmode ())
		return;

	Net_WriteByte (DEM_GENERICCHEAT);
	Net_WriteByte (CHT_MDK);
}
