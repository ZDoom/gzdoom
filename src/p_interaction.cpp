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
//		Handling interactions (i.e., collisions).
//
//-----------------------------------------------------------------------------




// Data.
#include "doomdef.h"
#include "gstrings.h"

#include "doomstat.h"

#include "m_random.h"
#include "i_system.h"
#include "announcer.h"

#include "am_map.h"

#include "c_console.h"
#include "c_dispatch.h"

#include "p_local.h"

#include "p_lnspec.h"
#include "p_effect.h"
#include "p_acs.h"

#include "b_bot.h"	//Added by MC:

#include "ravenshared.h"
#include "a_hexenglobal.h"
#include "a_sharedglobal.h"
#include "a_pickups.h"
#include "gi.h"
#include "templates.h"
#include "sbar.h"
#include "s_sound.h"
#include "g_level.h"
#include "d_net.h"
#include "d_netinf.h"

static FRandom pr_obituary ("Obituary");
static FRandom pr_botrespawn ("BotRespawn");
static FRandom pr_killmobj ("ActorDie");
FRandom pr_damagemobj ("ActorTakeDamage");
static FRandom pr_lightning ("LightningDamage");
static FRandom pr_poison ("PoisonDamage");
static FRandom pr_switcher ("SwitchTarget");
static FRandom pr_kickbackdir ("KickbackDir");

CVAR (Bool, cl_showsprees, true, CVAR_ARCHIVE)
CVAR (Bool, cl_showmultikills, true, CVAR_ARCHIVE)
EXTERN_CVAR (Bool, show_obituaries)


FName MeansOfDeath;

//
// GET STUFF
//

//
// P_TouchSpecialThing
//
void P_TouchSpecialThing (AActor *special, AActor *toucher)
{
	fixed_t delta = special->Z() - toucher->Z();

	// The pickup is at or above the toucher's feet OR
	// The pickup is below the toucher.
	if (delta > toucher->height || delta < MIN(-32*FRACUNIT, -special->height))
	{ // out of reach
		return;
	}

	// Dead thing touching.
	// Can happen with a sliding player corpse.
	if (toucher->health <= 0)
		return;

	//Added by MC: Finished with this destination.
	if (toucher->player != NULL && toucher->player->Bot != NULL && special == toucher->player->Bot->dest)
	{
		toucher->player->Bot->prev = toucher->player->Bot->dest;
		toucher->player->Bot->dest = NULL;
	}

	special->Touch (toucher);
}


// [RH]
// SexMessage: Replace parts of strings with gender-specific pronouns
//
// The following expansions are performed:
//		%g -> he/she/it
//		%h -> him/her/it
//		%p -> his/her/its
//		%o -> other (victim)
//		%k -> killer
//
void SexMessage (const char *from, char *to, int gender, const char *victim, const char *killer)
{
	static const char *genderstuff[3][3] =
	{
		{ "he",  "him", "his" },
		{ "she", "her", "her" },
		{ "it",  "it",  "its" }
	};
	static const int gendershift[3][3] =
	{
		{ 2, 3, 3 },
		{ 3, 3, 3 },
		{ 2, 2, 3 }
	};
	const char *subst = NULL;

	do
	{
		if (*from != '%')
		{
			*to++ = *from;
		}
		else
		{
			int gendermsg = -1;
			
			switch (from[1])
			{
			case 'g':	gendermsg = 0;	break;
			case 'h':	gendermsg = 1;	break;
			case 'p':	gendermsg = 2;	break;
			case 'o':	subst = victim;	break;
			case 'k':	subst = killer;	break;
			}
			if (subst != NULL)
			{
				size_t len = strlen (subst);
				memcpy (to, subst, len);
				to += len;
				from++;
				subst = NULL;
			}
			else if (gendermsg < 0)
			{
				*to++ = '%';
			}
			else
			{
				strcpy (to, genderstuff[gender][gendermsg]);
				to += gendershift[gender][gendermsg];
				from++;
			}
		}
	} while (*from++);
}

// [RH]
// ClientObituary: Show a message when a player dies
//
void ClientObituary (AActor *self, AActor *inflictor, AActor *attacker, int dmgflags)
{
	FName mod;
	const char *message;
	const char *messagename;
	char gendermessage[1024];
	int  gender;

	// No obituaries for non-players, voodoo dolls or when not wanted
	if (self->player == NULL || self->player->mo != self || !show_obituaries)
		return;

	gender = self->player->userinfo.GetGender();

	// Treat voodoo dolls as unknown deaths
	if (inflictor && inflictor->player && inflictor->player->mo != inflictor)
		MeansOfDeath = NAME_None;

	mod = MeansOfDeath;
	message = NULL;
	messagename = NULL;

	if (attacker == NULL || attacker->player != NULL)
	{
		if (mod == NAME_Telefrag)
		{
			if (AnnounceTelefrag (attacker, self))
				return;
		}
		else
		{
			if (AnnounceKill (attacker, self))
				return;
		}
	}

	switch (mod)
	{
	case NAME_Suicide:		messagename = "OB_SUICIDE";		break;
	case NAME_Falling:		messagename = "OB_FALLING";		break;
	case NAME_Crush:		messagename = "OB_CRUSH";		break;
	case NAME_Exit:			messagename = "OB_EXIT";		break;
	case NAME_Drowning:		messagename = "OB_WATER";		break;
	case NAME_Slime:		messagename = "OB_SLIME";		break;
	case NAME_Fire:			if (attacker == NULL) messagename = "OB_LAVA";		break;
	}

	// Check for being killed by a voodoo doll.
	if (inflictor && inflictor->player && inflictor->player->mo != inflictor)
	{
		messagename = "OB_VOODOO";
	}

	if (messagename != NULL)
		message = GStrings(messagename);

	if (attacker != NULL && message == NULL)
	{
		if (attacker == self)
		{
			message = GStrings("OB_KILLEDSELF");
		}
		else if (attacker->player == NULL)
		{
			if (mod == NAME_Telefrag)
			{
				message = GStrings("OB_MONTELEFRAG");
			}
			else if (mod == NAME_Melee)
			{
				message = attacker->GetClass()->Meta.GetMetaString (AMETA_HitObituary);
				if (message == NULL)
				{
					message = attacker->GetClass()->Meta.GetMetaString (AMETA_Obituary);
				}
			}
			else
			{
				message = attacker->GetClass()->Meta.GetMetaString (AMETA_Obituary);
			}
		}
	}

	if (message == NULL && attacker != NULL && attacker->player != NULL)
	{
		if (self->player != attacker->player && self->IsTeammate(attacker))
		{
			self = attacker;
			gender = self->player->userinfo.GetGender();
			mysnprintf (gendermessage, countof(gendermessage), "OB_FRIENDLY%c", '1' + (pr_obituary() & 3));
			message = GStrings(gendermessage);
		}
		else
		{
			if (mod == NAME_Telefrag) message = GStrings("OB_MPTELEFRAG");
			if (message == NULL)
			{
				if (inflictor != NULL)
				{
					message = inflictor->GetClass()->Meta.GetMetaString (AMETA_Obituary);
				}
				if (message == NULL && (dmgflags & DMG_PLAYERATTACK) && attacker->player->ReadyWeapon != NULL)
				{
					message = attacker->player->ReadyWeapon->GetClass()->Meta.GetMetaString (AMETA_Obituary);
				}
				if (message == NULL)
				{
					switch (mod)
					{
					case NAME_BFGSplash:	messagename = "OB_MPBFG_SPLASH";	break;
					case NAME_Railgun:		messagename = "OB_RAILGUN";			break;
					}
					if (messagename != NULL)
						message = GStrings(messagename);
				}
				if (message == NULL)
				{
					message = attacker->GetClass()->Meta.GetMetaString (AMETA_Obituary);
				}
			}
		}
	}
	else attacker = self;	// for the message creation

	if (message != NULL && message[0] == '$') 
	{
		message = GStrings[message+1];
	}

	if (message == NULL)
	{
		message = GStrings("OB_DEFAULT");
	}

	// [CK] Don't display empty strings
	if (message == NULL || strlen(message) <= 0)
		return;
		
	SexMessage (message, gendermessage, gender,
		self->player->userinfo.GetName(), attacker->player->userinfo.GetName());
	Printf (PRINT_MEDIUM, "%s\n", gendermessage);
}


//
// KillMobj
//
EXTERN_CVAR (Int, fraglimit)

void AActor::Die (AActor *source, AActor *inflictor, int dmgflags)
{
	// Handle possible unmorph on death
	bool wasgibbed = (health < GibHealth());

	AActor *realthis = NULL;
	int realstyle = 0;
	int realhealth = 0;
	if (P_MorphedDeath(this, &realthis, &realstyle, &realhealth))
	{
		if (!(realstyle & MORPH_UNDOBYDEATHSAVES))
		{
			if (wasgibbed)
			{
				int realgibhealth = realthis->GibHealth();
				if (realthis->health >= realgibhealth)
				{
					realthis->health = realgibhealth -1; // if morphed was gibbed, so must original be (where allowed)l
				}
			}
			realthis->Die(source, inflictor, dmgflags);
		}
		return;
	}

	// [SO] 9/2/02 -- It's rather funny to see an exploded player body with the invuln sparkle active :) 
	effects &= ~FX_RESPAWNINVUL;
	//flags &= ~MF_INVINCIBLE;

	if (debugfile && this->player)
	{
		static int dieticks[MAXPLAYERS]; // [ZzZombo] not used? Except if for peeking in debugger...
		int pnum = int(this->player-players);
		dieticks[pnum] = gametic;
		fprintf (debugfile, "died (%d) on tic %d (%s)\n", pnum, gametic,
		this->player->cheats&CF_PREDICTING?"predicting":"real");
	}

	// [RH] Notify this actor's items.
	for (AInventory *item = Inventory; item != NULL; )
	{
		AInventory *next = item->Inventory;
		item->OwnerDied();
		item = next;
	}

	if (flags & MF_MISSILE)
	{ // [RH] When missiles die, they just explode
		P_ExplodeMissile (this, NULL, NULL);
		return;
	}
	// [RH] Set the target to the thing that killed it. Strife apparently does this.
	if (source != NULL)
	{
		target = source;
	}

	flags &= ~(MF_SHOOTABLE|MF_FLOAT|MF_SKULLFLY);
	if (!(flags4 & MF4_DONTFALL)) flags&=~MF_NOGRAVITY;
	flags |= MF_DROPOFF;
	if ((flags3 & MF3_ISMONSTER) || FindState(NAME_Raise) != NULL || IsKindOf(RUNTIME_CLASS(APlayerPawn)))
	{	// [RH] Only monsters get to be corpses.
		// Objects with a raise state should get the flag as well so they can
		// be revived by an Arch-Vile. Batman Doom needs this.
		// [RC] And disable this if DONTCORPSE is set, of course.
		if(!(flags6 & MF6_DONTCORPSE)) flags |= MF_CORPSE;
	}
	flags6 |= MF6_KILLED;

	// [RH] Allow the death height to be overridden using metadata.
	fixed_t metaheight = 0;
	if (DamageType == NAME_Fire)
	{
		metaheight = GetClass()->Meta.GetMetaFixed (AMETA_BurnHeight);
	}
	if (metaheight == 0)
	{
		metaheight = GetClass()->Meta.GetMetaFixed (AMETA_DeathHeight);
	}
	if (metaheight != 0)
	{
		height = MAX<fixed_t> (metaheight, 0);
	}
	else
	{
		height >>= 2;
	}

	// [RH] If the thing has a special, execute and remove it
	//		Note that the thing that killed it is considered
	//		the activator of the script.
	// New: In Hexen, the thing that died is the activator,
	//		so now a level flag selects who the activator gets to be.
	// Everything is now moved to P_ActivateThingSpecial().
	if (special && (!(flags & MF_SPECIAL) || (flags3 & MF3_ISMONSTER))
		&& !(activationtype & THINGSPEC_NoDeathSpecial))
	{
		P_ActivateThingSpecial(this, source, true); 
	}

	if (CountsAsKill())
		level.killed_monsters++;
		
	if (source && source->player)
	{
		if (CountsAsKill())
		{ // count for intermission
			source->player->killcount++;
		}

		// Don't count any frags at level start, because they're just telefrags
		// resulting from insufficient deathmatch starts, and it wouldn't be
		// fair to count them toward a player's score.
		if (player && level.maptime)
		{
			source->player->frags[player - players]++;
			if (player == source->player)	// [RH] Cumulative frag count
			{
				char buff[256];

				player->fragcount--;
				if (deathmatch && player->spreecount >= 5 && cl_showsprees)
				{
					SexMessage (GStrings("SPREEKILLSELF"), buff,
						player->userinfo.GetGender(), player->userinfo.GetName(),
						player->userinfo.GetName());
					StatusBar->AttachMessage (new DHUDMessageFadeOut (SmallFont, buff,
							1.5f, 0.2f, 0, 0, CR_WHITE, 3.f, 0.5f), MAKE_ID('K','S','P','R'));
				}
			}
			else
			{
				if ((dmflags2 & DF2_YES_LOSEFRAG) && deathmatch)
					player->fragcount--;

				if (this->IsTeammate(source))
				{
					source->player->fragcount--;
				}
				else
				{
					++source->player->fragcount;
					++source->player->spreecount;
				}

				if (source->player->morphTics)
				{ // Make a super chicken
					source->GiveInventoryType (RUNTIME_CLASS(APowerWeaponLevel2));
				}

				if (deathmatch && cl_showsprees)
				{
					const char *spreemsg;
					char buff[256];

					switch (source->player->spreecount)
					{
					case 5:
						spreemsg = GStrings("SPREE5");
						break;
					case 10:
						spreemsg = GStrings("SPREE10");
						break;
					case 15:
						spreemsg = GStrings("SPREE15");
						break;
					case 20:
						spreemsg = GStrings("SPREE20");
						break;
					case 25:
						spreemsg = GStrings("SPREE25");
						break;
					default:
						spreemsg = NULL;
						break;
					}

					if (spreemsg == NULL && player->spreecount >= 5)
					{
						if (!AnnounceSpreeLoss (this))
						{
							SexMessage (GStrings("SPREEOVER"), buff, player->userinfo.GetGender(),
								player->userinfo.GetName(), source->player->userinfo.GetName());
							StatusBar->AttachMessage (new DHUDMessageFadeOut (SmallFont, buff,
								1.5f, 0.2f, 0, 0, CR_WHITE, 3.f, 0.5f), MAKE_ID('K','S','P','R'));
						}
					}
					else if (spreemsg != NULL)
					{
						if (!AnnounceSpree (source))
						{
							SexMessage (spreemsg, buff, player->userinfo.GetGender(),
								player->userinfo.GetName(), source->player->userinfo.GetName());
							StatusBar->AttachMessage (new DHUDMessageFadeOut (SmallFont, buff,
								1.5f, 0.2f, 0, 0, CR_WHITE, 3.f, 0.5f), MAKE_ID('K','S','P','R'));
						}
					}
				}
			}

			// [RH] Multikills
			if (player != source->player)
			{
				source->player->multicount++;
				if (source->player->lastkilltime > 0)
				{
					if (source->player->lastkilltime < level.time - 3*TICRATE)
					{
						source->player->multicount = 1;
					}

					if (deathmatch &&
						source->CheckLocalView (consoleplayer) &&
						cl_showmultikills)
					{
						const char *multimsg;

						switch (source->player->multicount)
						{
						case 1:
							multimsg = NULL;
							break;
						case 2:
							multimsg = GStrings("MULTI2");
							break;
						case 3:
							multimsg = GStrings("MULTI3");
							break;
						case 4:
							multimsg = GStrings("MULTI4");
							break;
						default:
							multimsg = GStrings("MULTI5");
							break;
						}
						if (multimsg != NULL)
						{
							char buff[256];

							if (!AnnounceMultikill (source))
							{
								SexMessage (multimsg, buff, player->userinfo.GetGender(),
									player->userinfo.GetName(), source->player->userinfo.GetName());
								StatusBar->AttachMessage (new DHUDMessageFadeOut (SmallFont, buff,
									1.5f, 0.8f, 0, 0, CR_RED, 3.f, 0.5f), MAKE_ID('M','K','I','L'));
							}
						}
					}
				}
				source->player->lastkilltime = level.time;
			}

			// [RH] Implement fraglimit
			if (deathmatch && fraglimit &&
				fraglimit <= D_GetFragCount (source->player))
			{
				Printf ("%s\n", GStrings("TXT_FRAGLIMIT"));
				G_ExitLevel (0, false);
			}
		}
	}
	else if (!multiplayer && CountsAsKill())
	{
		// count all monster deaths,
		// even those caused by other monsters
		players[0].killcount++;
	}

	if (player)
	{
		// [RH] Death messages
		ClientObituary (this, inflictor, source, dmgflags);

		// Death script execution, care of Skull Tag
		FBehavior::StaticStartTypedScripts (SCRIPT_Death, this, true);

		// [RH] Force a delay between death and respawn
		player->respawn_time = level.time + TICRATE;

		//Added by MC: Respawn bots
		if (bglobal.botnum && !demoplayback)
		{
			if (player->Bot != NULL)
				player->Bot->t_respawn = (pr_botrespawn()%15)+((bglobal.botnum-1)*2)+TICRATE+1;

			//Added by MC: Discard enemies.
			for (int i = 0; i < MAXPLAYERS; i++)
			{
				if (players[i].Bot != NULL && this == players[i].Bot->enemy)
				{
					if (players[i].Bot->dest ==  players[i].Bot->enemy)
						players[i].Bot->dest = NULL;
					players[i].Bot->enemy = NULL;
				}
			}

			player->spreecount = 0;
			player->multicount = 0;
		}

		// count environment kills against you
		if (!source)
		{
			player->frags[player - players]++;
			player->fragcount--;	// [RH] Cumulative frag count
		}
						
		flags &= ~MF_SOLID;
		player->playerstate = PST_DEAD;
		P_DropWeapon (player);
		if (this == players[consoleplayer].camera && automapactive)
		{
			// don't die in auto map, switch view prior to dying
			AM_Stop ();
		}

		// [GRB] Clear extralight. When you killed yourself with weapon that
		// called A_Light1/2 before it called A_Light0, extraligh remained.
		player->extralight = 0;
	}

	// [RH] If this is the unmorphed version of another monster, destroy this
	// actor, because the morphed version is the one that will stick around in
	// the level.
	if (flags & MF_UNMORPHED)
	{
		Destroy ();
		return;
	}



	FState *diestate = NULL;
	int gibhealth = GibHealth();
	ActorFlags4 iflags4 = inflictor == NULL ? ActorFlags4::FromInt(0) : inflictor->flags4;
	bool extremelydead = ((health < gibhealth || iflags4 & MF4_EXTREMEDEATH) && !(iflags4 & MF4_NOEXTREMEDEATH));

	// Special check for 'extreme' damage type to ensure that it gets recorded properly as an extreme death for subsequent checks.
	if (DamageType == NAME_Extreme)
	{
		extremelydead = true;
		DamageType = NAME_None;
	}

	// find the appropriate death state. The order is:
	//
	// 1. If damagetype is not 'none' and death is extreme, try a damage type specific extreme death state
	// 2. If no such state is found or death is not extreme try a damage type specific normal death state
	// 3. If damagetype is 'ice' and actor is a monster or player, try the generic freeze death (unless prohibited)
	// 4. If no state has been found and death is extreme, try the extreme death state
	// 5. If no such state is found or death is not extreme try the regular death state.
	// 6. If still no state has been found, destroy the actor immediately.

	if (DamageType != NAME_None)
	{
		if (extremelydead)
		{
			FName labels[] = { NAME_Death, NAME_Extreme, DamageType };
			diestate = FindState(3, labels, true);
		}
		if (diestate == NULL)
		{
			diestate = FindState (NAME_Death, DamageType, true);
			if (diestate != NULL) extremelydead = false;
		}
		if (diestate == NULL)
		{
			if (DamageType == NAME_Ice)
			{ // If an actor doesn't have an ice death, we can still give them a generic one.

				if (!deh.NoAutofreeze && !(flags4 & MF4_NOICEDEATH) && (player || (flags3 & MF3_ISMONSTER)))
				{
					diestate = FindState(NAME_GenericFreezeDeath);
					extremelydead = false;
				}
			}
		}
	}
	if (diestate == NULL)
	{
		
		// Don't pass on a damage type this actor cannot handle.
		// (most importantly, prevent barrels from passing on ice damage.)
		// Massacre must be preserved though.
		if (DamageType != NAME_Massacre)
		{
			DamageType = NAME_None;	
		}

		if (extremelydead)
		{ // Extreme death
			diestate = FindState (NAME_Death, NAME_Extreme, true);
		}
		if (diestate == NULL)
		{ // Normal death
			extremelydead = false;
			diestate = FindState (NAME_Death);
		}
	}

	if (extremelydead)
	{ 
		// We'll only get here if an actual extreme death state was used.

		// For players, mark the appropriate flag.
		if (player != NULL)
		{
			player->cheats |= CF_EXTREMELYDEAD;
		}
		// If a non-player, mark as extremely dead for the crash state.
		else if (health >= gibhealth)
		{
			health = gibhealth - 1;
		}
	}

	if (diestate != NULL)
	{
		SetState (diestate);

		if (tics > 1)
		{
			tics -= pr_killmobj() & 3;
			if (tics < 1)
				tics = 1;
		}
	}
	else
	{
		Destroy();
	}
}




//---------------------------------------------------------------------------
//
// PROC P_AutoUseHealth
//
//---------------------------------------------------------------------------
static int CountHealth(TArray<AInventory *> &Items)
{
	int counted = 0;
	for(unsigned i = 0; i < Items.Size(); i++)
	{
		counted += Items[i]->Amount * Items[i]->health;
	}
	return counted;
}

static int UseHealthItems(TArray<AInventory *> &Items, int &saveHealth)
{
	int saved = 0;

	while (Items.Size() > 0 && saveHealth > 0)
	{
		int maxhealth = 0;
		int index = -1;

		// Find the largest item in the list
		for(unsigned i = 0; i < Items.Size(); i++)
		{
			if (Items[i]->health > maxhealth)
			{
				index = i;
				maxhealth = Items[i]->health;
			}
		}

		// Now apply the health items, using the same logic as Heretic and Hexen.
		int count = (saveHealth + maxhealth-1) / maxhealth;
		for(int i = 0; i < count; i++)
		{
			saved += maxhealth;
			saveHealth -= maxhealth;
			if (--Items[index]->Amount == 0)
			{
				Items[index]->DepleteOrDestroy ();
				Items.Delete(index);
				break;
			}
		}
	}
	return saved;
}

void P_AutoUseHealth(player_t *player, int saveHealth)
{
	TArray<AInventory *> NormalHealthItems;
	TArray<AInventory *> LargeHealthItems;

	for(AInventory *inv = player->mo->Inventory; inv != NULL; inv = inv->Inventory)
	{
		if (inv->Amount > 0 && inv->IsKindOf(RUNTIME_CLASS(AHealthPickup)))
		{
			int mode = static_cast<AHealthPickup*>(inv)->autousemode;

			if (mode == 1) NormalHealthItems.Push(inv);
			else if (mode == 2) LargeHealthItems.Push(inv);
		}
	}

	int normalhealth = CountHealth(NormalHealthItems);
	int largehealth = CountHealth(LargeHealthItems);

	bool skilluse = !!G_SkillProperty(SKILLP_AutoUseHealth);

	if (skilluse && normalhealth >= saveHealth)
	{ // Use quartz flasks
		player->health += UseHealthItems(NormalHealthItems, saveHealth);
	}
	else if (largehealth >= saveHealth)
	{ 
		// Use mystic urns
		player->health += UseHealthItems(LargeHealthItems, saveHealth);
	}
	else if (skilluse && normalhealth + largehealth >= saveHealth)
	{ // Use mystic urns and quartz flasks
		player->health += UseHealthItems(NormalHealthItems, saveHealth);
		if (saveHealth > 0) player->health += UseHealthItems(LargeHealthItems, saveHealth);
	}
	player->mo->health = player->health;
}

//============================================================================
//
// P_AutoUseStrifeHealth
//
//============================================================================
CVAR(Bool, sv_disableautohealth, false, CVAR_ARCHIVE|CVAR_SERVERINFO)

void P_AutoUseStrifeHealth (player_t *player)
{
	TArray<AInventory *> Items;

	for(AInventory *inv = player->mo->Inventory; inv != NULL; inv = inv->Inventory)
	{
		if (inv->Amount > 0 && inv->IsKindOf(RUNTIME_CLASS(AHealthPickup)))
		{
			int mode = static_cast<AHealthPickup*>(inv)->autousemode;

			if (mode == 3) Items.Push(inv);
		}
	}

	if (!sv_disableautohealth)
	{
		while (Items.Size() > 0)
		{
			int maxhealth = 0;
			int index = -1;

			// Find the largest item in the list
			for(unsigned i = 0; i < Items.Size(); i++)
			{
				if (Items[i]->health > maxhealth)
				{
					index = i;
					maxhealth = Items[i]->Amount;
				}
			}

			while (player->health < 50)
			{
				if (!player->mo->UseInventory (Items[index]))
					break;
			}
			if (player->health >= 50) return;
			// Using all of this item was not enough so delete it and restart with the next best one
			Items.Delete(index);
		}
	}
}

/*
=================
=
= P_DamageMobj
=
= Damages both enemies and players
= inflictor is the thing that caused the damage
= 		creature or missile, can be NULL (slime, etc)
= source is the thing to target after taking damage
=		creature or NULL
= Source and inflictor are the same for melee attacks
= source can be null for barrel explosions and other environmental stuff
==================
*/

static inline bool MustForcePain(AActor *target, AActor *inflictor)
{
	return (!(target->flags5 & MF5_NOPAIN) && inflictor != NULL &&
		(inflictor->flags6 & MF6_FORCEPAIN) && !(inflictor->flags5 & MF5_PAINLESS));
}

static inline bool isFakePain(AActor *target, AActor *inflictor, int damage)
{
	return ((target->flags7 & MF7_ALLOWPAIN && damage > 0) || ((inflictor != NULL) && (inflictor->flags7 & MF7_CAUSEPAIN)));
}


// Returns the amount of damage actually inflicted upon the target, or -1 if
// the damage was cancelled.
int P_DamageMobj (AActor *target, AActor *inflictor, AActor *source, int damage, FName mod, int flags)
{
	unsigned ang;
	player_t *player = NULL;
	fixed_t thrust;
	int temp;
	int painchance = 0;
	FState * woundstate = NULL;
	PainChanceList * pc = NULL;
	bool justhit = false;
	bool plrDontThrust = false;
	bool invulpain = false;
	bool fakedPain = false;
	bool forcedPain = false;
	int fakeDamage = 0;
	int holdDamage = 0;
	int rawdamage = damage; 
	
	if (damage < 0) damage = 0;

	if (target == NULL || !((target->flags & MF_SHOOTABLE) || (target->flags6 & MF6_VULNERABLE)))
	{ // Shouldn't happen
		return -1;
	}

	//Rather than unnecessarily call the function over and over again, let's be a little more efficient.
	fakedPain = (isFakePain(target, inflictor, damage)); 
	forcedPain = (MustForcePain(target, inflictor));

	// Spectral targets only take damage from spectral projectiles.
	if (target->flags4 & MF4_SPECTRAL && damage < TELEFRAG_DAMAGE)
	{
		if (inflictor == NULL || !(inflictor->flags4 & MF4_SPECTRAL))
		{
			return -1;
		}
	}
	if (target->health <= 0)
	{
		if (inflictor && mod == NAME_Ice && !(inflictor->flags7 & MF7_ICESHATTER))
		{
			return -1;
		}
		else if (target->flags & MF_ICECORPSE) // frozen
		{
			target->tics = 1;
			target->flags6 |= MF6_SHATTERING;
			target->velx = target->vely = target->velz = 0;
		}
		return -1;
	}
	// [MC] Changed it to check rawdamage here for consistency, even though that doesn't actually do anything
	// different here. At any rate, invulnerable is being checked before type factoring, which is then being 
	// checked by player cheats/invul/buddha followed by monster buddha. This is inconsistent. Don't let the 
	// original telefrag damage CHECK (rawdamage) be influenced by outside factors when looking at cheats/invul.
	if ((target->flags2 & MF2_INVULNERABLE) && (rawdamage < TELEFRAG_DAMAGE) && (!(flags & DMG_FORCED)))
	{ // actor is invulnerable
		if (target->player == NULL)
		{
			if (inflictor == NULL || (!(inflictor->flags3 & MF3_FOILINVUL) && !(flags & DMG_FOILINVUL)))
			{
				if (fakedPain)
				{
					// big mess here: What do we use for the pain threshold?
					// We cannot run the various damage filters below so for consistency it needs to be 0.
					damage = 0;
					invulpain = true;
					goto fakepain;
				}
				else
					return -1;
			}
		}
		else
		{
			// Players are optionally excluded from getting thrust by damage.
			if (static_cast<APlayerPawn *>(target)->PlayerFlags & PPF_NOTHRUSTWHENINVUL)
			{
				if (fakedPain)
					plrDontThrust = 1;
				else
					return -1;
			}
		}
		
	}

	if (inflictor != NULL)
	{
		if (inflictor->flags5 & MF5_PIERCEARMOR)
			flags |= DMG_NO_ARMOR;
	}
	
	MeansOfDeath = mod;
	// [RH] Andy Baker's Stealth monsters
	if (target->flags & MF_STEALTH)
	{
		target->alpha = OPAQUE;
		target->visdir = -1;
	}
	if (target->flags & MF_SKULLFLY)
	{
		target->velx = target->vely = target->velz = 0;
	}

	player = target->player;
	if (!(flags & DMG_FORCED))	// DMG_FORCED skips all special damage checks, TELEFRAG_DAMAGE may not be reduced at all
	{
		if (target->flags2 & MF2_DORMANT)
		{
			// Invulnerable, and won't wake up
			return -1;
		}

		if ((rawdamage < TELEFRAG_DAMAGE) || (target->flags7 & MF7_LAXTELEFRAGDMG)) // TELEFRAG_DAMAGE may only be reduced with LAXTELEFRAGDMG or it may not guarantee its effect.
		{
			if (player && damage > 1)
			{
				// Take half damage in trainer mode
				damage = FixedMul(damage, G_SkillProperty(SKILLP_DamageFactor));
			}
			// Special damage types
			if (inflictor)
			{
				if (inflictor->flags4 & MF4_SPECTRAL)
				{
					if (player != NULL)
					{
						if (!deathmatch && inflictor->FriendPlayer > 0)
							return -1;
					}
					else if (target->flags4 & MF4_SPECTRAL)
					{
						if (inflictor->FriendPlayer == 0 && !target->IsHostile(inflictor))
							return -1;
					}
				}

				damage = inflictor->DoSpecialDamage(target, damage, mod);
				if (damage < 0)
				{
					return -1;
				}
			}

			int olddam = damage;

			if (damage > 0 && source != NULL)
			{
				damage = FixedMul(damage, source->DamageMultiply);

				// Handle active damage modifiers (e.g. PowerDamage)
				if (damage > 0 && source->Inventory != NULL)
				{
					source->Inventory->ModifyDamage(damage, mod, damage, false);
				}
			}
			// Handle passive damage modifiers (e.g. PowerProtection), provided they are not afflicted with protection penetrating powers.
			if (damage > 0 && (target->Inventory != NULL) && !(flags & DMG_NO_PROTECT))
			{
				target->Inventory->ModifyDamage(damage, mod, damage, true);
			}
			if (damage > 0 && !(flags & DMG_NO_FACTOR))
			{
				damage = FixedMul(damage, target->DamageFactor);
				if (damage > 0)
				{
					damage = DamageTypeDefinition::ApplyMobjDamageFactor(damage, mod, target->GetClass()->ActorInfo->DamageFactors);
				}
			}

			if (damage >= 0)
			{
				damage = target->TakeSpecialDamage(inflictor, source, damage, mod);
			}

			// '<0' is handled below. This only handles the case where damage gets reduced to 0.
			if (damage == 0 && olddam > 0)
			{
				{ // Still allow FORCEPAIN
					if (forcedPain)
					{
						goto dopain;
					}
					else if (fakedPain)
					{
						goto fakepain;
					}
					return -1;
				}
			}
		}
		if (target->flags5 & MF5_NODAMAGE)
		{
			damage = 0;
		}
	}
	if (damage < 0)
	{
		// any negative value means that something in the above chain has cancelled out all damage and all damage effects, including pain.
		return -1;
	}
	// Push the target unless the source's weapon's kickback is 0.
	// (i.e. Gauntlets/Chainsaw)
	if (!plrDontThrust && inflictor && inflictor != target	// [RH] Not if hurting own self
		&& !(target->flags & MF_NOCLIP)
		&& !(inflictor->flags2 & MF2_NODMGTHRUST)
		&& !(flags & DMG_THRUSTLESS)
		&& !(target->flags7 & MF7_DONTTHRUST)
		&& (source == NULL || source->player == NULL || !(source->flags2 & MF2_NODMGTHRUST)))
	{
		int kickback;

		if (inflictor && inflictor->projectileKickback)
			kickback = inflictor->projectileKickback;
		else if (!source || !source->player || !source->player->ReadyWeapon)
			kickback = gameinfo.defKickback;
		else
			kickback = source->player->ReadyWeapon->Kickback;

		if (kickback)
		{
			AActor *origin = (source && (flags & DMG_INFLICTOR_IS_PUFF))? source : inflictor;

			// If the origin and target are in exactly the same spot, choose a random direction.
			// (Most likely cause is from telefragging somebody during spawning because they
			// haven't moved from their spawn spot at all.)
			if (origin->X() == target->X() && origin->Y() == target->Y())
			{
				ang = pr_kickbackdir.GenRand32();
			}
			else
			{
				ang = origin->AngleTo(target);
			}

			// Calculate this as float to avoid overflows so that the
			// clamping that had to be done here can be removed.
            double fltthrust;

            fltthrust = mod == NAME_MDK ? 10 : 32;
            if (target->Mass > 0)
            {
                fltthrust = clamp((damage * 0.125 * kickback) / target->Mass, 0., fltthrust);
            }

			thrust = FLOAT2FIXED(fltthrust);

			// Don't apply ultra-small damage thrust
			if (thrust < FRACUNIT/100) thrust = 0;

			// make fall forwards sometimes
			if ((damage < 40) && (damage > target->health)
				 && (target->Z() - origin->Z() > 64*FRACUNIT)
				 && (pr_damagemobj()&1)
				 // [RH] But only if not too fast and not flying
				 && thrust < 10*FRACUNIT
				 && !(target->flags & MF_NOGRAVITY)
				 && (inflictor == NULL || !(inflictor->flags5 & MF5_NOFORWARDFALL))
				 )
			{
				ang += ANG180;
				thrust *= 4;
			}
			ang >>= ANGLETOFINESHIFT;
			if (source && source->player && (flags & DMG_INFLICTOR_IS_PUFF)
				&& source->player->ReadyWeapon != NULL &&
				(source->player->ReadyWeapon->WeaponFlags & WIF_STAFF2_KICKBACK))
			{
				// Staff power level 2
				target->velx += FixedMul (10*FRACUNIT, finecosine[ang]);
				target->vely += FixedMul (10*FRACUNIT, finesine[ang]);
				if (!(target->flags & MF_NOGRAVITY))
				{
					target->velz += 5*FRACUNIT;
				}
			}
			else
			{
				target->velx += FixedMul (thrust, finecosine[ang]);
				target->vely += FixedMul (thrust, finesine[ang]);
			}
		}
	}

	// [RH] Avoid friendly fire if enabled
	if (!(flags & DMG_FORCED) && source != NULL &&
		((player && player != source->player) || (!player && target != source)) &&
		target->IsTeammate (source))
	{
		//Use the original damage to check for telefrag amount. Don't let the now-amplified damagetypes do it.
		if (rawdamage < TELEFRAG_DAMAGE || (target->flags7 & MF7_LAXTELEFRAGDMG)) 
		{ // Still allow telefragging :-(
			damage = (int)((float)damage * level.teamdamage);
			if (damage < 0)
			{
				return damage;
			}
			else if (damage == 0)
			{
				if (forcedPain)
				{
					goto dopain;
				}
				else if (fakedPain)
				{
					goto fakepain;
				}
				return -1;
			}
		}
	}

	//
	// player specific
	//
	if (player)
	{
		//Added by MC: Lets bots look allround for enemies if they survive an ambush.
		if (player->Bot != NULL)
		{
			player->Bot->allround = true;
		}

		// end of game hell hack
		if ((target->Sector->Flags & SECF_ENDLEVEL) && damage >= target->health)
		{
			damage = target->health - 1;
		}

		if (!(flags & DMG_FORCED))
		{
			// check the real player, not a voodoo doll here for invulnerability effects
			if ((rawdamage < TELEFRAG_DAMAGE && ((player->mo->flags2 & MF2_INVULNERABLE) ||
				(player->cheats & CF_GODMODE))) ||
				(player->cheats & CF_GODMODE2) || (player->mo->flags5 & MF5_NODAMAGE))
				//Absolutely no hurting if NODAMAGE is involved. Same for GODMODE2.
			{ // player is invulnerable, so don't hurt him
				//Make sure no godmodes and NOPAIN flags are found first.
				//Then, check to see if the player has NODAMAGE or ALLOWPAIN, or inflictor has CAUSEPAIN.
				if ((player->cheats & CF_GODMODE) || (player->cheats & CF_GODMODE2) || (player->mo->flags5 & MF5_NOPAIN))
					return -1;
				else if ((((player->mo->flags7 & MF7_ALLOWPAIN) || (player->mo->flags5 & MF5_NODAMAGE)) || ((inflictor != NULL) && (inflictor->flags7 & MF7_CAUSEPAIN))))
				{
					invulpain = true;
					goto fakepain;
				}
				else
					return -1;
			}

			if (!(flags & DMG_NO_ARMOR) && player->mo->Inventory != NULL)
			{
				int newdam = damage;
				if (damage > 0)
				{
					player->mo->Inventory->AbsorbDamage(damage, mod, newdam);
				}
				if ((rawdamage < TELEFRAG_DAMAGE) || (player->mo->flags7 & MF7_LAXTELEFRAGDMG)) //rawdamage is never modified.
				{
					// if we are telefragging don't let the damage value go below that magic value. Some further checks would fail otherwise.
					damage = newdam;
				}

				if (damage <= 0)
				{
					// If MF6_FORCEPAIN is set, make the player enter the pain state.
					if (!(target->flags5 & MF5_NOPAIN) && inflictor != NULL &&
						(inflictor->flags6 & MF6_FORCEPAIN) && !(inflictor->flags5 & MF5_PAINLESS) 
						&& (!(player->mo->flags2 & MF2_INVULNERABLE)) && (!(player->cheats & CF_GODMODE)) && (!(player->cheats & CF_GODMODE2)))
					{
						goto dopain;
					}
					return damage;
				}
			}
			
			if (damage >= player->health && rawdamage < TELEFRAG_DAMAGE
				&& (G_SkillProperty(SKILLP_AutoUseHealth) || deathmatch)
				&& !player->morphTics)
			{ // Try to use some inventory health
				P_AutoUseHealth (player, damage - player->health + 1);
			}
		}

		player->health -= damage;		// mirror mobj health here for Dave
		// [RH] Make voodoo dolls and real players record the same health
		target->health = player->mo->health -= damage;
		if (player->health < 50 && !deathmatch && !(flags & DMG_FORCED))
		{
			P_AutoUseStrifeHealth (player);
			player->mo->health = player->health;
		}
		if (player->health <= 0)
		{
			// [SP] Buddha cheat: if the player is about to die, rescue him to 1 health.
			// This does not save the player if damage >= TELEFRAG_DAMAGE, still need to
			// telefrag him right? ;) (Unfortunately the damage is "absorbed" by armor,
			// but telefragging should still do enough damage to kill the player)
			// Ignore players that are already dead.
			// [MC]Buddha2 absorbs telefrag damage, and anything else thrown their way.
			if (!(flags & DMG_FORCED) && (((player->cheats & CF_BUDDHA2) || (((player->cheats & CF_BUDDHA) || (player->mo->flags7 & MF7_BUDDHA)) && (rawdamage < TELEFRAG_DAMAGE))) && (player->playerstate != PST_DEAD)))
			{
				// If this is a voodoo doll we need to handle the real player as well.
				player->mo->health = target->health = player->health = 1;
			}
			else
			{
				player->health = 0;
			}
		}
		player->LastDamageType = mod;
		player->attacker = source;
		player->damagecount += damage;	// add damage after armor / invuln
		if (player->damagecount > 100)
		{
			player->damagecount = 100;	// teleport stomp does 10k points...
		}
		temp = damage < 100 ? damage : 100;
		if (player == &players[consoleplayer])
		{
			I_Tactile (40,10,40+temp*2);
		}
	}
	else
	{
		// Armor for monsters.
		if (!(flags & (DMG_NO_ARMOR|DMG_FORCED)) && target->Inventory != NULL && damage > 0)
		{
			int newdam = damage;
			target->Inventory->AbsorbDamage (damage, mod, newdam);
			damage = newdam;
			if (damage <= 0)
			{
				if (fakedPain)
					goto fakepain;
				else
					return damage;
			}
		}
	
		target->health -= damage;	
	}

	//
	// the damage has been dealt; now deal with the consequences
	//
	target->DamageTypeReceived = mod;

	// If the damaging player has the power of drain, give the player 50% of the damage
	// done in health.
	if ( source && source->player && source->player->cheats & CF_DRAIN && !(target->flags5 & MF5_DONTDRAIN))
	{
		if (!target->player || target->player != source->player)
		{
			if ( P_GiveBody( source, damage / 2 ))
			{
				S_Sound( source, CHAN_ITEM, "*drainhealth", 1, ATTN_NORM );
			}
		}
	}


	if (target->health <= 0)
	{ 
		//[MC]Buddha flag for monsters.
		if (!(flags & DMG_FORCED) && ((target->flags7 & MF7_BUDDHA) && (rawdamage < TELEFRAG_DAMAGE) && ((inflictor == NULL || !(inflictor->flags7 & MF7_FOILBUDDHA)) && !(flags & DMG_FOILBUDDHA))))
		{ //FOILBUDDHA or Telefrag damage must kill it.
			target->health = 1;
		}
		else
		{
		
			// Death
			target->special1 = damage;

			// use inflictor's death type if it got one.
			if (inflictor && inflictor->DeathType != NAME_None) mod = inflictor->DeathType;

			// check for special fire damage or ice damage deaths
			if (mod == NAME_Fire)
			{
				if (player && !player->morphTics)
				{ // Check for flame death
					if (!inflictor ||
						((target->health > -50) && (damage > 25)) ||
						!(inflictor->flags5 & MF5_SPECIALFIREDAMAGE))
					{
						target->DamageType = NAME_Fire;
					}
				}
				else
				{
					target->DamageType = NAME_Fire;
				}
			}
			else
			{
				target->DamageType = mod;
			}
			if (source && source->tracer && (source->flags5 & MF5_SUMMONEDMONSTER))
			{ // Minotaur's kills go to his master
				// Make sure still alive and not a pointer to fighter head
				if (source->tracer->player && (source->tracer->player->mo == source->tracer))
				{
					source = source->tracer;
				}
			}
			target->Die (source, inflictor, flags);
			return damage;
		}
	}

	woundstate = target->FindState(NAME_Wound, mod);
	if (woundstate != NULL)
	{
		int woundhealth = RUNTIME_TYPE(target)->Meta.GetMetaInt (AMETA_WoundHealth, 6);

		if (target->health <= woundhealth)
		{
			target->SetState (woundstate);
			return damage;
		}
	}

fakepain: //Needed so we can skip the rest of the above, but still obey the original rules.

	if (!(target->flags5 & MF5_NOPAIN) && (inflictor == NULL || !(inflictor->flags5 & MF5_PAINLESS)) &&
		(target->player != NULL || !G_SkillProperty(SKILLP_NoPain)) && !(target->flags & MF_SKULLFLY))
	{
		pc = target->GetClass()->ActorInfo->PainChances;
		painchance = target->PainChance;
		if (pc != NULL)
		{
			int *ppc = pc->CheckKey(mod);
			if (ppc != NULL)
			{
				painchance = *ppc;
			}
		}

		if (((damage >= target->PainThreshold) && (pr_damagemobj() < painchance)) 
			|| (inflictor != NULL && (inflictor->flags6 & MF6_FORCEPAIN)))
		{
dopain:	
			if (mod == NAME_Electric)
			{
				if (pr_lightning() < 96)
				{
					justhit = true;
					FState *painstate = target->FindState(NAME_Pain, mod);
					if (painstate != NULL)
						target->SetState(painstate);
				}
				else
				{ // "electrocute" the target
					target->renderflags |= RF_FULLBRIGHT;
					if ((target->flags3 & MF3_ISMONSTER) && pr_lightning() < 128)
					{
						target->Howl ();
					}
				}
			}
			else
			{
				justhit = true;
				FState *painstate = target->FindState(NAME_Pain, ((inflictor && inflictor->PainType != NAME_None) ? inflictor->PainType : mod));
				if (painstate != NULL)
					target->SetState(painstate);
				if (mod == NAME_PoisonCloud)
				{
					if ((target->flags3 & MF3_ISMONSTER) && pr_poison() < 128)
					{
						target->Howl ();
					}
				}
			}
		}
	}
	//ALLOWPAIN and CAUSEPAIN can still trigger infighting, even if no pain state is worked out.
	target->reactiontime = 0;			// we're awake now...	
	if (source)
	{
		if (source == target->target)
		{
			target->threshold = BASETHRESHOLD;
			if (target->state == target->SpawnState && target->SeeState != NULL)
			{
				target->SetState (target->SeeState);
			}
		}
		else if (source != target->target && target->OkayToSwitchTarget (source))
		{
			// Target actor is not intent on another actor,
			// so make him chase after source

			// killough 2/15/98: remember last enemy, to prevent
			// sleeping early; 2/21/98: Place priority on players

			if (target->lastenemy == NULL ||
				(target->lastenemy->player == NULL && target->TIDtoHate == 0) ||
				target->lastenemy->health <= 0)
			{
				target->lastenemy = target->target; // remember last enemy - killough
			}
			target->target = source;
			target->threshold = BASETHRESHOLD;
			if (target->state == target->SpawnState && target->SeeState != NULL)
			{
				target->SetState (target->SeeState);
			}
		}
	}

	// killough 11/98: Don't attack a friend, unless hit by that friend.
	if (justhit && (target->target == source || !target->target || !target->IsFriend(target->target)))
		target->flags |= MF_JUSTHIT;    // fight back!

	if (invulpain) //Note that this takes into account all the cheats a player has, in terms of invulnerability.
	{
		return -1; //NOW we return -1!
	}
	return damage;
}

void P_PoisonMobj (AActor *target, AActor *inflictor, AActor *source, int damage, int duration, int period, FName type)
{
	// Check for invulnerability.
	if (!(inflictor->flags6 & MF6_POISONALWAYS))
	{
		if (target->flags2 & MF2_INVULNERABLE)
		{ // actor is invulnerable
			if (target->player == NULL)
			{
				if (!(inflictor->flags3 & MF3_FOILINVUL))
				{
					return;
				}
			}
			else
			{
				return;
			}
		}
	}

	target->Poisoner = source;
	target->PoisonDamageTypeReceived = type;
	target->PoisonPeriodReceived = period;

	if (inflictor->flags6 & MF6_ADDITIVEPOISONDAMAGE)
	{
		target->PoisonDamageReceived += damage;
	}
	else
	{
		target->PoisonDamageReceived = damage;
	}

	if (inflictor->flags6 & MF6_ADDITIVEPOISONDURATION)
	{
		target->PoisonDurationReceived += duration;
	}
	else
	{
		target->PoisonDurationReceived = duration;
	}

}

bool AActor::OkayToSwitchTarget (AActor *other)
{
	if (other == this)
		return false;		// [RH] Don't hate self (can happen when shooting barrels)

	if (other->flags7 & MF7_NEVERTARGET)
		return false;		// never EVER target me!

	if (!(other->flags & MF_SHOOTABLE))
		return false;		// Don't attack things that can't be hurt

	if ((flags4 & MF4_NOTARGETSWITCH) && target != NULL)
		return false;		// Don't switch target if not allowed

	if ((master != NULL && other->IsA(master->GetClass())) ||		// don't attack your master (or others of its type)
		(other->master != NULL && IsA(other->master->GetClass())))	// don't attack your minion (or those of others of your type)
	{
		if (!IsHostile (other) &&								// allow target switch if other is considered hostile
			(other->tid != TIDtoHate || TIDtoHate == 0) &&		// or has the tid we hate
			other->TIDtoHate == TIDtoHate)						// or has different hate information
		{
			return false;
		}
	}

	if ((other->flags3 & MF3_NOTARGET) &&
		(other->tid != TIDtoHate || TIDtoHate == 0) &&
		!IsHostile (other))
		return false;
	if (threshold != 0 && !(flags4 & MF4_QUICKTORETALIATE))
		return false;
	if (IsFriend (other))
	{ // [RH] Friendlies don't target other friendlies
		return false;
	}
	
	int infight;
	if (flags5 & MF5_NOINFIGHTING) infight=-1;	
	else infight = G_SkillProperty(SKILLP_Infight);

	if (infight < 0 &&	other->player == NULL && !IsHostile (other))
	{
		return false;	// infighting off: Non-friendlies don't target other non-friendlies
	}
	if (TIDtoHate != 0 && TIDtoHate == other->TIDtoHate)
		return false;		// [RH] Don't target "teammates"
	if (other->player != NULL && (flags4 & MF4_NOHATEPLAYERS))
		return false;		// [RH] Don't target players
	if (target != NULL && target->health > 0 &&
		TIDtoHate != 0 && target->tid == TIDtoHate && pr_switcher() < 128 &&
		P_CheckSight (this, target))
		return false;		// [RH] Don't be too quick to give up things we hate

	return true;
}

//==========================================================================
//
// P_PoisonPlayer - Sets up all data concerning poisoning
//
// poisoner is the object directly responsible for poisoning the player,
// such as a missile. source is the actor responsible for creating the
// poisoner.
//
//==========================================================================

bool P_PoisonPlayer (player_t *player, AActor *poisoner, AActor *source, int poison)
{
	if((player->cheats&CF_GODMODE) || (player->mo->flags2 & MF2_INVULNERABLE) || (player->cheats & CF_GODMODE2))
	{
		return false;
	}
	if (source != NULL && source->player != player && player->mo->IsTeammate (source))
	{
		poison = (int)((float)poison * level.teamdamage);
	}
	if (poison > 0)
	{
		player->poisoncount += poison;
		player->poisoner = source;
		if (poisoner == NULL)
		{
			player->poisontype = player->poisonpaintype = NAME_None;
		}
		else
		{ // We need to record these in case the poisoner disappears before poisoncount reaches 0.
			player->poisontype = poisoner->DamageType;
			player->poisonpaintype = poisoner->PainType != NAME_None ? poisoner->PainType : poisoner->DamageType;
		}
		if(player->poisoncount > 100)
		{
			player->poisoncount = 100;
		}
	}
	return true;
}

//==========================================================================
//
// P_PoisonDamage - Similar to P_DamageMobj
//
//==========================================================================

void P_PoisonDamage (player_t *player, AActor *source, int damage,
	bool playPainSound)
{
	AActor *target;

	if (player == NULL)
	{
		return;
	}
	target = player->mo;
	if (target->health <= 0)
	{
		return;
	}
	if ((damage < TELEFRAG_DAMAGE && ((target->flags2 & MF2_INVULNERABLE) ||
		(player->cheats & CF_GODMODE))) || (player->cheats & CF_GODMODE2))
	{ // target is invulnerable
		return;
	}
	// Take half damage in trainer mode
	damage = FixedMul(damage, G_SkillProperty(SKILLP_DamageFactor));
	// Handle passive damage modifiers (e.g. PowerProtection)
	if (target->Inventory != NULL)
	{
		target->Inventory->ModifyDamage(damage, player->poisontype, damage, true);
	}
	// Modify with damage factors
	damage = FixedMul(damage, target->DamageFactor);
	if (damage > 0)
	{
		damage = DamageTypeDefinition::ApplyMobjDamageFactor(damage, player->poisontype, target->GetClass()->ActorInfo->DamageFactors);
	}
	if (damage <= 0)
	{ // Damage was reduced to 0, so don't bother further.
		return;
	}
	if (damage >= player->health
		&& (G_SkillProperty(SKILLP_AutoUseHealth) || deathmatch)
		&& !player->morphTics)
	{ // Try to use some inventory health
		P_AutoUseHealth(player, damage - player->health+1);
	}
	player->health -= damage; // mirror mobj health here for Dave
	if (player->health < 50 && !deathmatch)
	{
		P_AutoUseStrifeHealth(player);
	}
	if (player->health < 0)
	{
		player->health = 0;
	}
	player->attacker = source;

	//
	// do the damage
	//
	target->health -= damage;
	if (target->health <= 0)
	{ // Death
		if ((((player->cheats & CF_BUDDHA) || (player->mo->flags7 & MF7_BUDDHA)) && damage < TELEFRAG_DAMAGE) || (player->cheats & CF_BUDDHA2))
		{ // [SP] Save the player... 
			player->health = target->health = 1;
		}
		else
		{
			target->special1 = damage;
			if (player && !player->morphTics)
			{ // Check for flame death
				if ((player->poisontype == NAME_Fire) && (target->health > -50) && (damage > 25))
				{
					target->DamageType = NAME_Fire;
				}
				else
				{
					target->DamageType = player->poisontype;
				}
			}
			target->Die(source, source);
			return;
		}
	}
	if (!(level.time&63) && playPainSound)
	{
		FState *painstate = target->FindState(NAME_Pain, player->poisonpaintype);
		if (painstate != NULL)
		{
			target->SetState(painstate);
		}
	}
/*
	if((P_Random() < target->info->painchance)
		&& !(target->flags&MF_SKULLFLY))
	{
		target->flags |= MF_JUSTHIT; // fight back!
		P_SetMobjState(target, target->info->painstate);
	}
*/
}


CCMD (kill)
{
	if (argv.argc() > 1)
	{
		if (CheckCheatmode ())
			return;

		if (!stricmp (argv[1], "monsters"))
		{
			// Kill all the monsters
			if (CheckCheatmode ())
				return;

			Net_WriteByte (DEM_GENERICCHEAT);
			Net_WriteByte (CHT_MASSACRE);
		}
		else
		{
			Net_WriteByte (DEM_KILLCLASSCHEAT);
			Net_WriteString (argv[1]);
		}
	}
	else
	{
		// If suiciding is disabled, then don't do it.
		if (dmflags2 & DF2_NOSUICIDE)
			return;

		// Kill the player
		Net_WriteByte (DEM_SUICIDE);
	}
	C_HideConsole ();
}

CCMD(remove)
{
	if (argv.argc() == 2)
	{
		if (CheckCheatmode())
			return;

		Net_WriteByte(DEM_REMOVE);
		Net_WriteString(argv[1]);
		C_HideConsole();
	}
	else
	{
		Printf("Usage: remove <actor class name>\n");
		return;
	}
	
}