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
#include "a_hereticglobal.h"
#include "ravenshared.h"
#include "a_hexenglobal.h"
#include "a_sharedglobal.h"
#include "a_pickups.h"
#include "gi.h"
#include "templates.h"
#include "sbar.h"

static FRandom pr_obituary ("Obituary");
static FRandom pr_botrespawn ("BotRespawn");
static FRandom pr_killmobj ("ActorDie");
static FRandom pr_damagemobj ("ActorTakeDamage");
static FRandom pr_lightning ("LightningDamage");
static FRandom pr_poison ("PoisonDamage");
static FRandom pr_switcher ("SwitchTarget");

CVAR (Bool, cl_showsprees, true, CVAR_ARCHIVE)
CVAR (Bool, cl_showmultikills, true, CVAR_ARCHIVE)

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
	if (toucher->player->isbot && special == toucher->player->dest)
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
	int	mod;
	const char *message;
	int messagenum;
	char gendermessage[1024];
	BOOL friendly;
	int  gender;

	if (self->player == NULL)
		return;

	// No obituaries for voodoo dolls
	if (self->player->mo != self)
		return;

	gender = self->player->userinfo.gender;

	// Treat voodoo dolls as unknown deaths
	if (inflictor && inflictor->player == self->player)
		MeansOfDeath = MOD_UNKNOWN;

	if (multiplayer && !deathmatch)
		MeansOfDeath |= MOD_FRIENDLY_FIRE;

	friendly = MeansOfDeath & MOD_FRIENDLY_FIRE;
	mod = MeansOfDeath & ~MOD_FRIENDLY_FIRE;
	message = NULL;
	messagenum = 0;

	if (attacker == NULL || attacker->player != NULL)
	{
		if (mod == MOD_TELEFRAG)
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
	case MOD_SUICIDE:		messagenum = OB_SUICIDE;	break;
	case MOD_FALLING:		messagenum = OB_FALLING;	break;
	case MOD_CRUSH:			messagenum = OB_CRUSH;		break;
	case MOD_EXIT:			messagenum = OB_EXIT;		break;
	case MOD_WATER:			messagenum = OB_WATER;		break;
	case MOD_SLIME:			messagenum = OB_SLIME;		break;
	case MOD_FIRE:			messagenum = OB_LAVA;		break;
	case MOD_BARREL:		messagenum = OB_BARREL;		break;
	case MOD_SPLASH:		messagenum = OB_SPLASH;		break;
	}

	if (messagenum)
		message = GStrings(messagenum);

	if (attacker != NULL && message == NULL)
	{
		if (attacker == self)
		{
			switch (mod)
			{
			case MOD_R_SPLASH:	messagenum = OB_R_SPLASH;		break;
			case MOD_ROCKET:	messagenum = OB_ROCKET;			break;
			default:			messagenum = OB_KILLEDSELF;		break;
			}
			message = GStrings(messagenum);
		}
		else if (attacker->player == NULL)
		{
			if (mod == MOD_TELEFRAG)
			{
				message = GStrings(OB_MONTELEFRAG);
			}
			else if (mod == MOD_HIT)
			{
				message = attacker->GetClass()->Meta.GetMetaString (AMETA_HitObituary);
				if (message == NULL)
				{
					message = attacker->GetClass()->Meta.GetMetaString (AMETA_Obituary);
					if (message == NULL)
					{
						message = attacker->GetHitObituary ();
					}
				}
			}
			else
			{
				message = attacker->GetClass()->Meta.GetMetaString (AMETA_Obituary);
				if (message == NULL)
				{
					message = attacker->GetObituary ();
				}
			}
		}
	}

	if (message)
	{
		SexMessage (message, gendermessage, gender,
			self->player->userinfo.netname, self->player->userinfo.netname);
		Printf (PRINT_MEDIUM, "%s\n", gendermessage);
		return;
	}

	if (attacker != NULL && attacker->player != NULL)
	{
		if (friendly)
		{
			int rnum = pr_obituary();

			attacker->player->fragcount -= 2;
			attacker->player->frags[attacker->player - players]++;
			self = attacker;
			gender = self->player->userinfo.gender;

			messagenum = OB_FRIENDLY1 + (rnum & 3);
		}
		else
		{
			switch (mod)
			{
			default:
				if (attacker->player->ReadyWeapon != NULL)
				{
					message = attacker->player->ReadyWeapon->GetObituary ();
				}
				break;
			case MOD_ROCKET:		messagenum = OB_MPROCKET;		break;
			case MOD_R_SPLASH:		messagenum = OB_MPR_SPLASH;		break;
			case MOD_PLASMARIFLE:	messagenum = OB_MPPLASMARIFLE;	break;
			case MOD_BFG_BOOM:		messagenum = OB_MPBFG_BOOM;		break;
			case MOD_BFG_SPLASH:	messagenum = OB_MPBFG_SPLASH;	break;
			case MOD_TELEFRAG:		messagenum = OB_MPTELEFRAG;		break;
			case MOD_RAILGUN:		messagenum = OB_RAILGUN;		break;
			}
		}
		if (messagenum)
			message = GStrings(messagenum);
	}

	if (message)
	{
		SexMessage (message, gendermessage, gender,
			self->player->userinfo.netname, attacker->player->userinfo.netname);
		Printf (PRINT_MEDIUM, "%s\n", gendermessage);
		return;
	}

	SexMessage (GStrings(OB_DEFAULT), gendermessage, gender,
		self->player->userinfo.netname, self->player->userinfo.netname);
	Printf (PRINT_MEDIUM, "%s\n", gendermessage);
}


//
// KillMobj
//
EXTERN_CVAR (Int, fraglimit)

void AActor::Die (AActor *source, AActor *inflictor)
{
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

	if (flags & MF_MISSILE)
	{ // [RH] When missiles die, they just explode
		P_ExplodeMissile (this, NULL);
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
	if (flags3 & MF3_ISMONSTER)
	{ // [RH] Only monsters get to be corpses.
		flags |= MF_CORPSE;
	}
	// [RH] Allow the death height to be overridden using metadata.
	fixed_t metaheight = 0;
	if (DamageType == MOD_FIRE)
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

	if (source && source->player)
	{
		if (flags & MF_COUNTKILL)
		{ // count for intermission
			source->player->killcount++;
			level.killed_monsters++;
		}

		// Don't count any frags at level start, because they're just telefrags
		// resulting from insufficient deathmatch starts, and it wouldn't be
		// fair to count them toward a player's score.
		if (player && level.time)
		{
			source->player->frags[player - players]++;
			if (player == source->player)	// [RH] Cumulative frag count
			{
				char buff[256];

				player->fragcount--;
				if (deathmatch && player->spreecount >= 5 && cl_showsprees)
				{
					SexMessage (GStrings(SPREEKILLSELF), buff,
						player->userinfo.gender, player->userinfo.netname,
						player->userinfo.netname);
					StatusBar->AttachMessage (new DHUDMessageFadeOut (buff,
							1.5f, 0.2f, 0, 0, CR_WHITE, 3.f, 0.5f), MAKE_ID('K','S','P','R'));
				}
			}
			else
			{
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
						spreemsg = GStrings(SPREE5);
						break;
					case 10:
						spreemsg = GStrings(SPREE10);
						break;
					case 15:
						spreemsg = GStrings(SPREE15);
						break;
					case 20:
						spreemsg = GStrings(SPREE20);
						break;
					case 25:
						spreemsg = GStrings(SPREE25);
						break;
					default:
						spreemsg = NULL;
						break;
					}

					if (spreemsg == NULL && player->spreecount >= 5)
					{
						if (!AnnounceSpreeLoss (this))
						{
							SexMessage (GStrings(SPREEOVER), buff, player->userinfo.gender,
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
						multimsg = GStrings(MULTI2);
						break;
					case 3:
						multimsg = GStrings(MULTI3);
						break;
					case 4:
						multimsg = GStrings(MULTI4);
						break;
					default:
						multimsg = GStrings(MULTI5);
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
				fraglimit == D_GetFragCount (source->player))
			{
				Printf ("%s\n", GStrings(TXT_FRAGLIMIT));
				G_ExitLevel (0, false);
			}
		}
	}
	else if (!multiplayer && (flags & MF_COUNTKILL))
	{
		// count all monster deaths,
		// even those caused by other monsters
		players[0].killcount++;
		level.killed_monsters++;
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
	}

	// [RH] If this is the unmorphed version of another monster, destroy this
	// actor, because the morphed version is the one that will stick around in
	// the level.
	if (flags & MF_UNMORPHED)
	{
		Destroy ();
		return;
	}

	if (DamageType == MOD_DISINTEGRATE && EDeathState)
	{ // Electrocution death
		SetState (EDeathState);
	}
	else if (DamageType == MOD_FIRE && BDeathState)
	{ // Burn death
		SetState (BDeathState);
	}
	else if (DamageType == MOD_ICE &&
		(IDeathState || (
		(!deh.NoAutofreeze && !(flags4 & MF4_NOICEDEATH)) &&
		(player || (flags3 & MF3_ISMONSTER)))))
	{ // Ice death
		flags |= MF_ICECORPSE;
		if (IDeathState)
		{
			SetState (IDeathState);
		}
		else
		{
			SetState (&AActor::States[S_GENERICFREEZEDEATH]);
		}
	}
	else if (XDeathState &&
		health < (gameinfo.gametype == GAME_Doom
				  ? -GetDefault()->health : -GetDefault()->health/2))
	{ // Extreme death
		SetState (XDeathState);
	}
	else
	{ // Normal death
		DamageType = MOD_UNKNOWN;	// [RH] "Frozen" barrels shouldn't do freezing damage
		SetState (DeathState);
	}

	tics -= pr_killmobj() & 3;
	if (tics < 1)
		tics = 1;
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
	const TypeInfo *normalType = TypeInfo::FindType ("ArtiHealth");
	const TypeInfo *superType = TypeInfo::FindType ("ArtiSuperHealth");
	AInventory *normalItem = player->mo->FindInventory (normalType);
	AInventory *superItem = player->mo->FindInventory (superType);
	int normalAmount, superAmount;

	normalAmount = normalItem != NULL ? normalItem->Amount : 0;
	superAmount = superItem != NULL ? superItem->Amount : 0;

	if ((gameskill == sk_baby) && (normalAmount*25 >= saveHealth))
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
	else if ((gameskill == sk_baby)
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
	static const char *healthnames[2] = { "MedicalKit", "MedPatch" };

	for (int i = 0; i < 2; ++i)
	{
		const TypeInfo *type = TypeInfo::FindType (healthnames[i]);

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

int MeansOfDeath;

void P_DamageMobj (AActor *target, AActor *inflictor, AActor *source, int damage, int mod, int flags)
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
			if (target->MissileState != NULL)
			{
				target->SetState (target->MissileState);
			}
			return;
		}
	}
	if (target->health <= 0)
	{
		if (inflictor && mod == MOD_ICE)
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
	MeansOfDeath = mod;
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
	if (player && gameskill == sk_baby)
	{
		// Take half damage in trainer mode
		if (damage > 1)
		{
			damage >>= 1;
		}
	}
	// Special damage types
	if (inflictor)
	{
		if (inflictor->flags4 & MF4_SPECTRAL)
		{
			if (player != NULL)
			{
				if (inflictor->health == -1)
					return;
			}
			else if (target->flags4 & MF4_SPECTRAL)
			{
				if (inflictor->health == -2)
					return;
			}
		}
		if (mod == MOD_FIRE && target->flags4 & MF4_FIRERESIST)
		{
			damage /= 2;
		}
		damage = inflictor->DoSpecialDamage (target, damage);
		if (damage == -1)
		{
			return;
		}
	}
	damage = target->TakeSpecialDamage (inflictor, source, damage);
	if (damage == -1)
	{
		return;
	}
	// Push the target unless the source's weapon's kickback is 0.
	// (i.e. Guantlets/Chainsaw)
	if (inflictor && inflictor != target	// [RH] Not if hurting own self
		&& !(target->flags & MF_NOCLIP)
		&& (!source || !source->player || !(inflictor->flags2 & MF2_NODMGTHRUST)))
	{
		int kickback;

		if (!source || !source->player || !source->player->ReadyWeapon)
			kickback = gameinfo.defKickback;
		else
			kickback = source->player->ReadyWeapon->Kickback;

		if (kickback)
		{
			ang = R_PointToAngle2 (inflictor->x, inflictor->y,
				target->x, target->y);
			thrust = damage*(FRACUNIT>>3)*kickback / target->Mass;
			// [RH] If thrust overflows, use a more reasonable amount
			if (thrust < 0 || thrust > 10*FRACUNIT)
			{
				thrust = 10*FRACUNIT;
			}
			// make fall forwards sometimes
			if ((damage < 40) && (damage > target->health)
				 && (target->z - inflictor->z > 64*FRACUNIT)
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
			MeansOfDeath |= MOD_FRIENDLY_FIRE;
			if (damage < 1000000)
			{ // Still allow telefragging :-(
				damage = (int)((float)damage * teamdamage);
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
			&& ((gameskill == sk_baby) || deathmatch)
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
	if (target->health <= 0)
	{ // Death
		target->special1 = damage;
		if (source && target->IsKindOf (RUNTIME_CLASS(AExplosiveBarrel))
			&& !source->IsKindOf (RUNTIME_CLASS(AExplosiveBarrel)))
		{ // Make sure players get frags for chain-reaction kills
			target->target = source;
		}
		// check for special fire damage or ice damage deaths
		if (mod == MOD_FIRE)
		{
			if (player && !player->morphTics)
			{ // Check for flame death
				if (!inflictor ||
					((target->health > -50) && (damage > 25)) ||
					!inflictor->IsKindOf (RUNTIME_CLASS(APhoenixFX1)))
				{
					target->DamageType = MOD_FIRE;
				}
			}
			else
			{
				target->DamageType = MOD_FIRE;
			}
		}
		else
		{
			target->DamageType = mod;
		}
		if (source && source->tracer && source->IsKindOf (RUNTIME_CLASS (AMinotaur)))
		{ // Minotaur's kills go to his master
			// Make sure still alive and not a pointer to fighter head
			if (source->tracer->player && (source->tracer->player->mo == source->tracer))
			{
				source = source->tracer;
			}
		}
		if (source && (source->player) && source->player->ReadyWeapon != NULL &&
			(source->player->ReadyWeapon->WeaponFlags & WIF_EXTREME_DEATH))
		{
			// Always extreme death from fourth weapon
			target->health = -target->GetDefault()->health * 3;
		}
		target->Die (source, inflictor);
		return;
	}
	if (target->health <= 6 && target->WoundState != NULL)
	{
		target->SetState (target->WoundState);
		return;
	}
	if ((pr_damagemobj() < target->PainChance)
		 && !(target->flags & MF_SKULLFLY))
	{
		if (inflictor && inflictor->IsKindOf (RUNTIME_CLASS(ALightning)))
		{
			if (pr_lightning() < 96)
			{
				target->flags |= MF_JUSTHIT; // fight back!
				target->SetState (target->PainState);
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
			target->SetState (target->PainState);	
			if (inflictor && inflictor->IsKindOf (RUNTIME_CLASS(APoisonCloud)))
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
	if ((other->flags3 & MF3_NOTARGET) &&
		(other->tid != TIDtoHate || TIDtoHate == 0) &&
		((other->flags ^ flags) & MF_FRIENDLY) == 0)
		return false;
	if (threshold != 0 && !(flags4 & MF4_QUICKTORETALIATE))
		return false;
	if (other->flags & flags & MF_FRIENDLY)
	{ // [RH] Friendlies don't target other friendlies
		if (!deathmatch || other->FriendPlayer == 0 || FriendPlayer == 0 ||
			other->FriendPlayer != FriendPlayer)
		{
			return false;
		}
	}
	if ((flags & MF_FRIENDLY) && other->player != NULL)
	{ // [RH] Friendlies don't target their player friends either
		if (!deathmatch || other->player - players == FriendPlayer+1)
		{
			return false;
		}
	}
	if (gameinfo.gametype == GAME_Strife)
		if (!(other->flags & MF_FRIENDLY) && !(flags & MF_FRIENDLY))
			return false;	// Strife: Non-friendlies don't target other non-friendlies
	if (infighting < 0 && other->player == NULL)
		return false;		// [RH] Allow monsters to ignore each other
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
		poison = (int)((float)poison * teamdamage);
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
	if (player && gameskill == sk_baby)
	{
		// Take half damage in trainer mode
		damage >>= 1;
	}
	if(damage < 1000 && ((player->cheats&CF_GODMODE)
		|| (player->mo->flags2 & MF2_INVULNERABLE)))
	{
		return;
	}
	if (damage >= player->health
		&& ((gameskill == sk_baby) || deathmatch)
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
			if ((inflictor->DamageType == MOD_FIRE)
				&& (target->health > -50) && (damage > 25))
			{
				target->DamageType = MOD_FIRE;
			}
			if (inflictor->DamageType == MOD_ICE)
			{
				target->DamageType = MOD_ICE;
			}
		}
		target->Die (source, source);
		return;
	}
	if (!(level.time&63) && playPainSound)
	{
		target->SetState (target->PainState);
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

BOOL CheckCheatmode ();

CCMD (kill)
{
	if (argv.argc() > 1 && !stricmp (argv[1], "monsters"))
	{
		// Kill all the monsters
		if (CheckCheatmode ())
			return;

		Net_WriteByte (DEM_GENERICCHEAT);
		Net_WriteByte (CHT_MASSACRE);
	}
	else
	{
		// Kill the player
		Net_WriteByte (DEM_SUICIDE);
	}
	C_HideConsole ();
}
