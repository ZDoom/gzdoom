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

#include "a_doomglobal.h"
#include "ravenshared.h"
#include "a_hexenglobal.h"
#include "a_sharedglobal.h"
#include "a_pickups.h"
#include "gi.h"
#include "templates.h"
#include "sbar.h"
#include "s_sound.h"

static FRandom pr_obituary ("Obituary");
static FRandom pr_botrespawn ("BotRespawn");
static FRandom pr_killmobj ("ActorDie");
static FRandom pr_damagemobj ("ActorTakeDamage");
static FRandom pr_lightning ("LightningDamage");
static FRandom pr_poison ("PoisonDamage");
static FRandom pr_switcher ("SwitchTarget");

CVAR (Bool, cl_showsprees, true, CVAR_ARCHIVE)
CVAR (Bool, cl_showmultikills, true, CVAR_ARCHIVE)
EXTERN_CVAR (Bool, show_obituaries)


FName MeansOfDeath;
bool FriendlyFire;

//
// GET STUFF
//

//
// P_TouchSpecialThing
//
void P_TouchSpecialThing (AActor *special, AActor *toucher)
{
	fixed_t delta = special->z - toucher->z;

	if (delta > toucher->height || delta < -32*FRACUNIT)
	{ // out of reach
		return;
	}

	// Dead thing touching.
	// Can happen with a sliding player corpse.
	if (toucher->health <= 0)
		return;

	//Added by MC: Finished with this destination.
	if (toucher->player != NULL && toucher->player->isbot && special == toucher->player->dest)
	{
		toucher->player->prev = toucher->player->dest;
    	toucher->player->dest = NULL;
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
void ClientObituary (AActor *self, AActor *inflictor, AActor *attacker)
{
	FName mod;
	const char *message;
	const char *messagename;
	char gendermessage[1024];
	bool friendly;
	int  gender;

	// No obituaries for non-players, voodoo dolls or when not wanted
	if (self->player == NULL || self->player->mo != self || !show_obituaries)
		return;

	gender = self->player->userinfo.gender;

	// Treat voodoo dolls as unknown deaths
	if (inflictor && inflictor->player == self->player)
		MeansOfDeath = NAME_None;

	if (multiplayer && !deathmatch)
		FriendlyFire = true;

	friendly = FriendlyFire;
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
		if (friendly)
		{
			attacker->player->fragcount -= 2;
			attacker->player->frags[attacker->player - players]++;
			self = attacker;
			gender = self->player->userinfo.gender;
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
				if (message == NULL && attacker->player->ReadyWeapon != NULL)
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
			}
		}
	}
	else attacker = self;	// for the message creation

	if (message != NULL && message[0] == '$') 
	{
		message=GStrings[message+1];
	}

	if (message == NULL)
	{
		message = GStrings("OB_DEFAULT");
	}

	SexMessage (message, gendermessage, gender,
		self->player->userinfo.netname, attacker->player->userinfo.netname);
	Printf (PRINT_MEDIUM, "%s\n", gendermessage);
}


//
// KillMobj
//
EXTERN_CVAR (Int, fraglimit)

static int GibHealth(AActor *actor)
{
	return -abs(
		actor->GetClass()->Meta.GetMetaInt (
			AMETA_GibHealth,
			gameinfo.gametype == GAME_Doom ?
				-actor->GetDefault()->health :
				-actor->GetDefault()->health/2));
}

void AActor::Die (AActor *source, AActor *inflictor)
{
	// Handle possible unmorph on death
	bool wasgibbed = (health < GibHealth(this));
	AActor *realthis = NULL;
	int realstyle = 0;
	int realhealth = 0;
	if (P_MorphedDeath(this, &realthis, &realstyle, &realhealth))
	{
		if (!(realstyle & MORPH_UNDOBYDEATHSAVES))
		{
			if (wasgibbed)
			{
				int realgibhealth = GibHealth(realthis);
				if (realthis->health >= realgibhealth)
				{
					realthis->health = realgibhealth -1; // if morphed was gibbed, so must original be (where allowed)
				}
			}
			realthis->Die(source, inflictor);
		}
		return;
	}

	// [SO] 9/2/02 -- It's rather funny to see an exploded player body with the invuln sparkle active :) 
	effects &= ~FX_RESPAWNINVUL;
	//flags &= ~MF_INVINCIBLE;

	if (debugfile && this->player)
	{
		static int dieticks[MAXPLAYERS];
		int pnum = this->player-players;
		if (dieticks[pnum] == gametic)
			gametic=gametic;
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
		flags |= MF_CORPSE;
	}
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
	if (special && (!(flags & MF_SPECIAL) || (flags3 & MF3_ISMONSTER)))
	{
		LineSpecials[special] (NULL, level.flags & LEVEL_ACTOWNSPECIAL
			? this : source, false, args[0], args[1], args[2], args[3], args[4]);
		special = 0;
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
						player->userinfo.gender, player->userinfo.netname,
						player->userinfo.netname);
					StatusBar->AttachMessage (new DHUDMessageFadeOut (buff,
							1.5f, 0.2f, 0, 0, CR_WHITE, 3.f, 0.5f), MAKE_ID('K','S','P','R'));
				}
			}
			else
			{
				if ((dmflags2 & DF2_YES_LOSEFRAG) && deathmatch)
					player->fragcount--;

				++source->player->fragcount;
				++source->player->spreecount;
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
							SexMessage (GStrings("SPREEOVER"), buff, player->userinfo.gender,
								player->userinfo.netname, source->player->userinfo.netname);
							StatusBar->AttachMessage (new DHUDMessageFadeOut (buff,
								1.5f, 0.2f, 0, 0, CR_WHITE, 3.f, 0.5f), MAKE_ID('K','S','P','R'));
						}
					}
					else if (spreemsg != NULL)
					{
						if (!AnnounceSpree (source))
						{
							SexMessage (spreemsg, buff, player->userinfo.gender,
								player->userinfo.netname, source->player->userinfo.netname);
							StatusBar->AttachMessage (new DHUDMessageFadeOut (buff,
								1.5f, 0.2f, 0, 0, CR_WHITE, 3.f, 0.5f), MAKE_ID('K','S','P','R'));
						}
					}
				}
			}

			// [RH] Multikills
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
							SexMessage (multimsg, buff, player->userinfo.gender,
								player->userinfo.netname, source->player->userinfo.netname);
							StatusBar->AttachMessage (new DHUDMessageFadeOut (buff,
								1.5f, 0.8f, 0, 0, CR_RED, 3.f, 0.5f), MAKE_ID('M','K','I','L'));
						}
					}
				}
			}
			source->player->lastkilltime = level.time;

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
		ClientObituary (this, inflictor, source);

		// Death script execution, care of Skull Tag
		FBehavior::StaticStartTypedScripts (SCRIPT_Death, this, true);

		// [RH] Force a delay between death and respawn
		player->respawn_time = level.time + TICRATE;

		//Added by MC: Respawn bots
		if (bglobal.botnum && consoleplayer == Net_Arbitrator && !demoplayback)
		{
			if (player->isbot)
				player->t_respawn = (pr_botrespawn()%15)+((bglobal.botnum-1)*2)+TICRATE+1;

			//Added by MC: Discard enemies.
			for (int i = 0; i < MAXPLAYERS; i++)
			{
				if (players[i].isbot && this == players[i].enemy)
				{
					if (players[i].dest ==  players[i].enemy)
						players[i].dest = NULL;
					players[i].enemy = NULL;
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

	if (DamageType != NAME_None)
	{
		diestate = FindState (NAME_Death, DamageType, true);
		if (diestate == NULL)
		{
			if (DamageType == NAME_Ice)
			{ // If an actor doesn't have an ice death, we can still give them a generic one.

				if (!deh.NoAutofreeze && !(flags4 & MF4_NOICEDEATH) && (player || (flags3 & MF3_ISMONSTER)))
				{
					diestate = FindState(NAME_GenericFreezeDeath);
				}
			}
		}
	}
	if (diestate == NULL)
	{
		int flags4 = inflictor == NULL ? 0 : inflictor->flags4;

		int gibhealth = GibHealth(this);
		
		// Don't pass on a damage type this actor cannot handle.
		// (most importantly, prevent barrels from passing on ice damage.)
		// Massacre must be preserved though.
		if (DamageType != NAME_Massacre)
		{
			DamageType = NAME_None;	
		}

		if ((health < gibhealth || flags4 & MF4_EXTREMEDEATH) && !(flags4 & MF4_NOEXTREMEDEATH))
		{ // Extreme death
			diestate = FindState (NAME_Death, NAME_Extreme, true);
			// If a non-player, mark as extremely dead for the crash state.
			if (diestate != NULL && player == NULL && health >= gibhealth)
			{
				health = gibhealth - 1;
			}
			// For players, mark the appropriate flag.
			else if (player != NULL)
			{
				player->cheats |= CF_EXTREMELYDEAD;
			}
		}
		if (diestate == NULL)
		{ // Normal death
			diestate = FindState (NAME_Death);
		}
	}

	if (diestate != NULL)
	{
		SetState (diestate);

		tics -= pr_killmobj() & 3;
		if (tics < 1)
			tics = 1;
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

void P_AutoUseHealth(player_t *player, int saveHealth)
{
	int i;
	int count;
	const PClass *normalType = PClass::FindClass (NAME_ArtiHealth);
	const PClass *superType = PClass::FindClass (NAME_ArtiSuperHealth);
	AInventory *normalItem = player->mo->FindInventory (normalType);
	AInventory *superItem = player->mo->FindInventory (superType);
	int normalAmount, superAmount;

	normalAmount = normalItem != NULL ? normalItem->Amount : 0;
	superAmount = superItem != NULL ? superItem->Amount : 0;

	bool skilluse = !!G_SkillProperty(SKILLP_AutoUseHealth);

	if (skilluse && (normalAmount*25 >= saveHealth))
	{ // Use quartz flasks
		count = (saveHealth+24)/25;
		for(i = 0; i < count; i++)
		{
			player->health += 25;
			if (--normalItem->Amount == 0)
			{
				normalItem->Destroy ();
				break;
			}
		}
	}
	else if (superAmount*100 >= saveHealth)
	{ // Use mystic urns
		count = (saveHealth+99)/100;
		for(i = 0; i < count; i++)
		{
			player->health += 100;
			if (--superItem->Amount == 0)
			{
				superItem->Destroy ();
				break;
			}
		}
	}
	else if (skilluse
		&& (superAmount*100+normalAmount*25 >= saveHealth))
	{ // Use mystic urns and quartz flasks
		count = (saveHealth+24)/25;
		saveHealth -= count*25;
		for(i = 0; i < count; i++)
		{
			player->health += 25;
			if (--normalItem->Amount == 0)
			{
				normalItem->Destroy ();
				break;
			}
		}
		count = (saveHealth+99)/100;
		for(i = 0; i < count; i++)
		{
			player->health += 100;
			if (--superItem->Amount == 0)
			{
				superItem->Destroy ();
				break;
			}
		}
	}
	player->mo->health = player->health;
}

//============================================================================
//
// P_AutoUseStrifeHealth
//
//============================================================================

void P_AutoUseStrifeHealth (player_t *player)
{
	static const ENamedName healthnames[2] = { NAME_MedicalKit, NAME_MedPatch };

	for (int i = 0; i < 2; ++i)
	{
		const PClass *type = PClass::FindClass (healthnames[i]);

		while (player->health < 50)
		{
			AInventory *item = player->mo->FindInventory (type);
			if (item == NULL)
				break;
			if (!player->mo->UseInventory (item))
				break;
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


void P_DamageMobj (AActor *target, AActor *inflictor, AActor *source, int damage, FName mod, int flags)
{
	unsigned ang;
	player_t *player;
	fixed_t thrust;
	int temp;

	if (target == NULL || !(target->flags & MF_SHOOTABLE))
	{ // Shouldn't happen
		return;
	}

	// Spectral targets only take damage from spectral projectiles.
	if (target->flags4 & MF4_SPECTRAL && damage < 1000000)
	{
		if (inflictor == NULL || !(inflictor->flags4 & MF4_SPECTRAL))
		{
			/*
			if (target->MissileState != NULL)
			{
				target->SetState (target->MissileState);
			}
			*/
			return;
		}
	}
	if (target->health <= 0)
	{
		if (inflictor && mod == NAME_Ice)
		{
			return;
		}
		else if (target->flags & MF_ICECORPSE) // frozen
		{
			target->tics = 1;
			target->momx = target->momy = target->momz = 0;
		}
		return;
	}
	if ((target->flags2 & MF2_INVULNERABLE) && damage < 1000000)
	{ // actor is invulnerable
		if (!target->player)
		{
			if (!inflictor || !(inflictor->flags3 & MF3_FOILINVUL))
			{
				return;
			}
		}
		else
		{
			// Only in Hexen invulnerable players are excluded from getting
			// thrust by damage.
			if (gameinfo.gametype == GAME_Hexen) return;
		}
		
	}
	if (inflictor != NULL)
	{
		if (inflictor->flags5 & MF5_PIERCEARMOR) flags |= DMG_NO_ARMOR;
	}
	
	MeansOfDeath = mod;
	FriendlyFire = false;
	// [RH] Andy Baker's Stealth monsters
	if (target->flags & MF_STEALTH)
	{
		target->alpha = OPAQUE;
		target->visdir = -1;
	}
	if (target->flags & MF_SKULLFLY)
	{
		target->momx = target->momy = target->momz = 0;
	}
	if (target->flags2 & MF2_DORMANT)
	{
		// Invulnerable, and won't wake up
		return;
	}
	player = target->player;
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
				if (!deathmatch && inflictor->health == -1)
					return;
			}
			else if (target->flags4 & MF4_SPECTRAL)
			{
				if (inflictor->health == -2 && !target->IsHostile(inflictor))
					return;
			}
		}

		damage = inflictor->DoSpecialDamage (target, damage);
		if (damage == -1)
		{
			return;
		}

	}
	// Handle active damage modifiers (e.g. PowerDamage)
	if (source != NULL && source->Inventory != NULL)
	{
		int olddam = damage;
		source->Inventory->ModifyDamage(olddam, mod, damage, false);
		if (olddam != damage && damage <= 0) return;
	}
	// Handle passive damage modifiers (e.g. PowerProtection)
	if (target->Inventory != NULL)
	{
 		int olddam = damage;
		target->Inventory->ModifyDamage(olddam, mod, damage, true);
		if (olddam != damage && damage <= 0) return;
	}

	// to be removed and replaced by an actual damage factor 
	// once the actors using it are converted to DECORATE.
	if (mod == NAME_Fire && target->flags4 & MF4_FIRERESIST)
	{
		damage /= 2;
	}
	else
	{
		DmgFactors * df = target->GetClass()->ActorInfo->DamageFactors;
		if (df != NULL)
		{
			fixed_t * pdf = df->CheckKey(mod);
			if (pdf != NULL)
			{
				damage = FixedMul(damage, *pdf);
				if (damage <= 0) return;
			}
		}
	}

	damage = target->TakeSpecialDamage (inflictor, source, damage, mod);

	if (damage == -1)
	{
		return;
	}
	// Push the target unless the source's weapon's kickback is 0.
	// (i.e. Guantlets/Chainsaw)
	if (inflictor && inflictor != target	// [RH] Not if hurting own self
		&& !(target->flags & MF_NOCLIP)
		&& !(inflictor->flags2 & MF2_NODMGTHRUST)
		&& !(flags & DMG_THRUSTLESS))
	{
		int kickback;

		if (!source || !source->player || !source->player->ReadyWeapon)
			kickback = gameinfo.defKickback;
		else
			kickback = source->player->ReadyWeapon->Kickback;

		if (kickback)
		{
			AActor *origin = (source && (flags & DMG_INFLICTOR_IS_PUFF))? source : inflictor;
			
			ang = R_PointToAngle2 (origin->x, origin->y,
				target->x, target->y);
			thrust = damage*(FRACUNIT>>3)*kickback / target->Mass;
			// [RH] If thrust overflows, use a more reasonable amount
			if (thrust < 0 || thrust > 10*FRACUNIT)
			{
				thrust = 10*FRACUNIT;
			}
			// make fall forwards sometimes
			if ((damage < 40) && (damage > target->health)
				 && (target->z - origin->z > 64*FRACUNIT)
				 && (pr_damagemobj()&1)
				 // [RH] But only if not too fast and not flying
				 && thrust < 10*FRACUNIT
				 && !(target->flags & MF_NOGRAVITY))
			{
				ang += ANG180;
				thrust *= 4;
			}
			ang >>= ANGLETOFINESHIFT;
			if (source && source->player && (source == inflictor)
				&& source->player->ReadyWeapon != NULL &&
				(source->player->ReadyWeapon->WeaponFlags & WIF_STAFF2_KICKBACK))
			{
				// Staff power level 2
				target->momx += FixedMul (10*FRACUNIT, finecosine[ang]);
				target->momy += FixedMul (10*FRACUNIT, finesine[ang]);
				if (!(target->flags & MF_NOGRAVITY))
				{
					target->momz += 5*FRACUNIT;
				}
			}
			else
			{
				target->momx += FixedMul (thrust, finecosine[ang]);
				target->momy += FixedMul (thrust, finesine[ang]);
			}
		}
	}

	//
	// player specific
	//
	if (player)
	{
		if ((target->flags2 & MF2_INVULNERABLE) && damage < 1000000)
		{ // player is invulnerable, so don't hurt him
			return;
		}

        //Added by MC: Lets bots look allround for enemies if they survive an ambush.
        if (player->isbot)
		{
            player->allround = true;
		}

		// end of game hell hack
		if ((target->Sector->special & 255) == dDamage_End
			&& damage >= target->health)
		{
			damage = target->health - 1;
		}

		if (damage < 1000 && ((target->player->cheats & CF_GODMODE)
			|| (target->player->mo->flags2 & MF2_INVULNERABLE)))
		{
			return;
		}

		// [RH] Avoid friendly fire if enabled
		if (source != NULL && player != source->player && target->IsTeammate (source))
		{
			FriendlyFire = true;
			if (damage < 1000000)
			{ // Still allow telefragging :-(
				damage = (int)((float)damage * level.teamdamage);
				if (damage <= 0)
					return;
			}
		}
		if (!(flags & DMG_NO_ARMOR) && player->mo->Inventory != NULL)
		{
			int newdam = damage;
			player->mo->Inventory->AbsorbDamage (damage, mod, newdam);
			damage = newdam;
			if (damage <= 0)
			{
				return;
			}
		}

		if (damage >= player->health
			&& (G_SkillProperty(SKILLP_AutoUseHealth) || deathmatch)
			&& !player->morphTics)
		{ // Try to use some inventory health
			P_AutoUseHealth (player, damage - player->health + 1);
		}
		player->health -= damage;		// mirror mobj health here for Dave
		// [RH] Make voodoo dolls and real players record the same health
		target->health = player->mo->health -= damage;
		if (player->health < 50 && !deathmatch)
		{
			P_AutoUseStrifeHealth (player);
			player->mo->health = player->health;
		}
		if (player->health < 0)
		{
			player->health = 0;
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
		target->health -= damage;	
	}

	//
	// the damage has been dealt; now deal with the consequences
	//

	// If the damaging player has the power of drain, give the player 50% of the damage
	// done in health.
	if ( source && source->player && source->player->cheats & CF_DRAIN)
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
	{ // Death
		target->special1 = damage;
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
		target->Die (source, inflictor);
		return;
	}

	FState * woundstate = target->FindState(NAME_Wound, mod);
	if (woundstate != NULL)
	{
		int woundhealth = RUNTIME_TYPE(target)->Meta.GetMetaInt (AMETA_WoundHealth, 6);

		if (target->health <= woundhealth)
		{
			target->SetState (woundstate);
			return;
		}
	}
	
	PainChanceList * pc = target->GetClass()->ActorInfo->PainChances;
	int painchance = target->PainChance;
	if (pc != NULL)
	{
		BYTE * ppc = pc->CheckKey(mod);
		if (ppc != NULL)
		{
			painchance = *ppc;
		}
	}
	
	if (!(target->flags5 & MF5_NOPAIN) && (pr_damagemobj() < painchance) && !(target->flags & MF_SKULLFLY))
	{
		if (mod == NAME_Electric)
		{
			if (pr_lightning() < 96)
			{
				target->flags |= MF_JUSTHIT; // fight back!
				FState * painstate = target->FindState(NAME_Pain, mod);
				if (painstate != NULL) target->SetState (painstate);
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
			target->flags |= MF_JUSTHIT; // fight back!
			FState * painstate = target->FindState(NAME_Pain, mod);
			if (painstate != NULL) target->SetState (painstate);
			if (mod == NAME_PoisonCloud)
			{
				if ((target->flags3 & MF3_ISMONSTER) && pr_poison() < 128)
				{
					target->Howl ();
				}
			}
		}
	}
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
}

bool AActor::OkayToSwitchTarget (AActor *other)
{
	if (other == this)
		return false;		// [RH] Don't hate self (can happen when shooting barrels)

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
	else if (level.flags & LEVEL_TOTALINFIGHTING) infight=1;
	else if (level.flags & LEVEL_NOINFIGHTING) infight=-1;	
	else infight = infighting;
	
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
//==========================================================================

void P_PoisonPlayer (player_t *player, AActor *poisoner, AActor *source, int poison)
{
	if((player->cheats&CF_GODMODE) || (player->mo->flags2 & MF2_INVULNERABLE))
	{
		return;
	}
	if (source != NULL && source->player != player && player->mo->IsTeammate (source))
	{
		poison = (int)((float)poison * level.teamdamage);
	}
	if (poison > 0)
	{
		player->poisoncount += poison;
		player->poisoner = poisoner;
		if(player->poisoncount > 100)
		{
			player->poisoncount = 100;
		}
	}
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
	AActor *inflictor;

	target = player->mo;
	inflictor = source;
	if (target->health <= 0)
	{
		return;
	}
	if (target->flags2&MF2_INVULNERABLE && damage < 1000000)
	{ // target is invulnerable
		return;
	}
	if (player)
	{
		// Take half damage in trainer mode
		damage = FixedMul(damage, G_SkillProperty(SKILLP_DamageFactor));
	}
	if(damage < 1000 && ((player->cheats&CF_GODMODE)
		|| (player->mo->flags2 & MF2_INVULNERABLE)))
	{
		return;
	}
	if (damage >= player->health
		&& (G_SkillProperty(SKILLP_AutoUseHealth) || deathmatch)
		&& !player->morphTics)
	{ // Try to use some inventory health
		P_AutoUseHealth (player, damage - player->health+1);
	}
	player->health -= damage; // mirror mobj health here for Dave
	if (player->health < 50 && !deathmatch)
	{
		P_AutoUseStrifeHealth (player);
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
		target->special1 = damage;
		if (player && inflictor && !player->morphTics)
		{ // Check for flame death
			if ((inflictor->DamageType == NAME_Fire)
				&& (target->health > -50) && (damage > 25))
			{
				target->DamageType = NAME_Fire;
			}
			else target->DamageType = inflictor->DamageType;
		}
		target->Die (source, source);
		return;
	}
	if (!(level.time&63) && playPainSound)
	{
		FState * painstate = target->FindState(NAME_Pain, target->DamageType);
		if (painstate != NULL) target->SetState (painstate);
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

bool CheckCheatmode ();

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
		// Kill the player
		Net_WriteByte (DEM_SUICIDE);
	}
	C_HideConsole ();
}
