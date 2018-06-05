//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1994-1996 Raven Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
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

#include "d_player.h"
#include "gi.h"
#include "sbar.h"
#include "d_net.h"
#include "d_netinf.h"
#include "a_morph.h"
#include "vm.h"
#include "g_levellocals.h"
#include "events.h"
#include "actorinlines.h"

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

CVAR (Float, sv_damagefactormobj, 1.0, CVAR_SERVERINFO|CVAR_CHEAT)
CVAR (Float, sv_damagefactorfriendly, 1.0, CVAR_SERVERINFO|CVAR_CHEAT)
CVAR (Float, sv_damagefactorplayer, 1.0, CVAR_SERVERINFO|CVAR_CHEAT)

FName MeansOfDeath;

//
// GET STUFF
//

//
// P_TouchSpecialThing
//
void P_TouchSpecialThing (AActor *special, AActor *toucher)
{
	double delta = special->Z() - toucher->Z();

	// The pickup is at or above the toucher's feet OR
	// The pickup is below the toucher.
	if (delta > toucher->Height || delta < MIN(-32., -special->Height))
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
	special->CallTouch (toucher);
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
	FString ret;
	const char *message;
	const char *messagename;
	char gendermessage[1024];

	// No obituaries for non-players, voodoo dolls or when not wanted
	if (self->player == NULL || self->player->mo != self || !show_obituaries)
		return;

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

	FString obit = DamageTypeDefinition::GetObituary(mod);
	if (attacker == nullptr && obit.IsNotEmpty()) messagename = obit;
	else
	{
		switch (mod)
		{
		case NAME_Suicide:		messagename = "$OB_SUICIDE";	break;
		case NAME_Falling:		messagename = "$OB_FALLING";	break;
		case NAME_Crush:		messagename = "$OB_CRUSH";		break;
		case NAME_Exit:			messagename = "$OB_EXIT";		break;
		case NAME_Drowning:		messagename = "$OB_WATER";		break;
		case NAME_Slime:		messagename = "$OB_SLIME";		break;
		case NAME_Fire:			messagename = "$OB_LAVA";		break;
		}
	}

	// Check for being killed by a voodoo doll.
	if (inflictor && inflictor->player && inflictor->player->mo != inflictor)
	{
		messagename = "$OB_VOODOO";
	}

	if (attacker != NULL && message == NULL)
	{
		if (attacker == self)
		{
			message = "$OB_KILLEDSELF";
		}
		else 
		{
			IFVIRTUALPTR(attacker, AActor, GetObituary)
			{
				VMValue params[] = { attacker, self, inflictor, mod.GetIndex(), !!(dmgflags & DMG_PLAYERATTACK) };
				VMReturn rett(&ret);
				VMCall(func, params, countof(params), &rett, 1);
				if (ret.IsNotEmpty()) message = ret;
			}
		}
	}
	if (message == nullptr) message = messagename;	// fallback to defaults if possible.
	if (attacker == nullptr) attacker = self; // world
	if (attacker->player == nullptr) attacker = self;	// for the message creation

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
		
	SexMessage (message, gendermessage, self->player->userinfo.GetGender(),
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
	bool wasgibbed = (health < GetGibHealth());
	AActor *realthis = NULL;
	int realstyle = 0;
	int realhealth = 0;
	if (P_MorphedDeath(this, &realthis, &realstyle, &realhealth))
	{
		if (!(realstyle & MORPH_UNDOBYDEATHSAVES))
		{
			if (wasgibbed)
			{
				int realgibhealth = realthis->GetGibHealth();
				if (realthis->health >= realgibhealth)
				{
					realthis->health = realgibhealth -1; // if morphed was gibbed, so must original be (where allowed)l
				}
			}
			realthis->CallDie(source, inflictor, dmgflags);
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
		fprintf(debugfile, "died (%d) on tic %d (%s)\n", pnum, gametic,
			this->player->cheats&CF_PREDICTING ? "predicting" : "real");
	}

	// [RH] Notify this actor's items.
	for (AInventory *item = Inventory; item != NULL; )
	{
		AInventory *next = item->Inventory;
		IFVIRTUALPTR(item, AInventory, OwnerDied)
		{
			VMValue params[1] = { item };
			VMCall(func, params, 1, nullptr, 0);
		}
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

	// [ZZ] Fire WorldThingDied script hook.
	E_WorldThingDied(this, inflictor);

	// [JM] Fire KILL type scripts for actor. Not needed for players, since they have the "DEATH" script type.
	if (!player && !(flags7 & MF7_NOKILLSCRIPTS) && ((flags7 & MF7_USEKILLSCRIPTS) || gameinfo.forcekillscripts))
	{
		FBehavior::StaticStartTypedScripts(SCRIPT_Kill, this, true, 0, true);
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

	IFVIRTUAL(AActor, GetDeathHeight)
	{
		VMValue params[] = { (DObject*)this };
		VMReturn ret(&Height);
		VMCall(func, params, 1, &ret, 1);
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
					StatusBar->AttachMessage (Create<DHUDMessageFadeOut>(SmallFont, buff,
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
					source->GiveInventoryType (PClass::FindActor(NAME_PowerWeaponLevel2));
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
							StatusBar->AttachMessage (Create<DHUDMessageFadeOut> (SmallFont, buff,
								1.5f, 0.2f, 0, 0, CR_WHITE, 3.f, 0.5f), MAKE_ID('K','S','P','R'));
						}
					}
					else if (spreemsg != NULL)
					{
						if (!AnnounceSpree (source))
						{
							SexMessage (spreemsg, buff, player->userinfo.GetGender(),
								player->userinfo.GetName(), source->player->userinfo.GetName());
							StatusBar->AttachMessage (Create<DHUDMessageFadeOut> (SmallFont, buff,
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
								StatusBar->AttachMessage (Create<DHUDMessageFadeOut> (SmallFont, buff,
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

		// [ZZ] fire player death hook
		E_PlayerDied(int(player - players));

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
	int gibhealth = GetGibHealth();
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
	// The following condition is needed to avoid crash when player class has no death states
	// Instance of player pawn will be garbage collected on reloading of level
	else if (player == nullptr)
	{
		Destroy();
	}
}

DEFINE_ACTION_FUNCTION(AActor, Die)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_OBJECT(source, AActor);
	PARAM_OBJECT(inflictor, AActor);
	PARAM_INT_DEF(dmgflags);
	self->Die(source, inflictor, dmgflags);
	return 0;
}

void AActor::CallDie(AActor *source, AActor *inflictor, int dmgflags)
{
	IFVIRTUAL(AActor, Die)
	{
		VMValue params[4] = { (DObject*)this, source, inflictor, dmgflags };
		VMCall(func, params, 4, nullptr, 0);
	}
	else return Die(source, inflictor, dmgflags);
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

	auto hptype = PClass::FindActor(NAME_HealthPickup);
	for(AInventory *inv = player->mo->Inventory; inv != NULL; inv = inv->Inventory)
	{
		if (inv->Amount > 0 && inv->IsKindOf(hptype))
		{
			int mode = inv->IntVar(NAME_autousemode);

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

	auto hptype = PClass::FindActor(NAME_HealthPickup);
	for(AInventory *inv = player->mo->Inventory; inv != NULL; inv = inv->Inventory)
	{
		if (inv->Amount > 0 && inv->IsKindOf(hptype))
		{
			int mode = inv->IntVar(NAME_autousemode);
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
	return (inflictor && (inflictor->flags6 & MF6_FORCEPAIN));
}

static inline bool isFakePain(AActor *target, AActor *inflictor, int damage)
{
	return ((target->flags7 & MF7_ALLOWPAIN && damage > 0) || ((inflictor != NULL) && (inflictor->flags7 & MF7_CAUSEPAIN)));
}


// Returns the amount of damage actually inflicted upon the target, or -1 if
// the damage was cancelled.
static int DamageMobj (AActor *target, AActor *inflictor, AActor *source, int damage, FName mod, int flags, DAngle angle, bool& needevent)
{
	DAngle ang;
	player_t *player = NULL;
	double thrust;
	int temp;
	int painchance = 0;
	FState * woundstate = NULL;
	bool justhit = false;
	bool plrDontThrust = false;
	bool invulpain = false;
	bool fakedPain = false;
	bool forcedPain = false;
	bool noPain = false;
	int fakeDamage = 0;
	int holdDamage = 0;
	const int rawdamage = damage;
	const bool telefragDamage = (rawdamage >= TELEFRAG_DAMAGE);
	
	if (damage < 0) damage = 0;

	if (target == NULL || !((target->flags & MF_SHOOTABLE) || (target->flags6 & MF6_VULNERABLE)))
	{ // Shouldn't happen
		return 0;
	}

	// Rather than unnecessarily call the function over and over again, let's be a little more efficient.
	// But first, check and see if it's even needed, which it won't be if pain must not be triggered.
	noPain = ((flags & DMG_NO_PAIN) || (target->flags5 & MF5_NOPAIN) || (inflictor && (inflictor->flags5 & MF5_PAINLESS)));
	if (!noPain)
	{
		fakedPain = (isFakePain(target, inflictor, damage));
		forcedPain = (MustForcePain(target, inflictor));
	}

	// Spectral targets only take damage from spectral projectiles.
	if (target->flags4 & MF4_SPECTRAL && !telefragDamage)
	{
		if (inflictor == NULL || !(inflictor->flags4 & MF4_SPECTRAL))
		{
			return 0;
		}
	}
	if (target->health <= 0)
	{
		if (inflictor && mod == NAME_Ice && !(inflictor->flags7 & MF7_ICESHATTER))
		{
			return 0;
		}
		else if (target->flags & MF_ICECORPSE) // frozen
		{
			target->tics = 1;
			target->flags6 |= MF6_SHATTERING;
			target->Vel.Zero();
		}
		return 0;
	}
	if (target == source && (!telefragDamage || target->flags7 & MF7_LAXTELEFRAGDMG))
	{
		damage = int(damage * target->SelfDamageFactor);
	}

	// [MC] Changed it to check rawdamage here for consistency, even though that doesn't actually do anything
	// different here. At any rate, invulnerable is being checked before type factoring, which is then being 
	// checked by player cheats/invul/buddha followed by monster buddha. This is inconsistent. Don't let the 
	// original telefrag damage CHECK (rawdamage) be influenced by outside factors when looking at cheats/invul.
	if ((target->flags2 & MF2_INVULNERABLE) && !telefragDamage && (!(flags & DMG_FORCED)))
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
					return 0;
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
					return 0;
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
		target->Alpha = 1.;
		target->visdir = -1;
	}
	if (target->flags & MF_SKULLFLY)
	{
		target->Vel.Zero();
	}

	player = target->player;
	if (!(flags & DMG_FORCED))	// DMG_FORCED skips all special damage checks, TELEFRAG_DAMAGE may not be reduced at all
	{
		if (target->flags2 & MF2_DORMANT)
		{
			// Invulnerable, and won't wake up
			return 0;
		}

		if (!telefragDamage || (target->flags7 & MF7_LAXTELEFRAGDMG)) // TELEFRAG_DAMAGE may only be reduced with LAXTELEFRAGDMG or it may not guarantee its effect.
		{
			if (player && damage > 1)
			{
				// Take half damage in trainer mode
				damage = int(damage * G_SkillProperty(SKILLP_DamageFactor) * sv_damagefactorplayer);
			}
			else if (!player && damage > 1 && !(target->flags & MF_FRIENDLY))
			{
				// inflict scaled damage to non-players
				damage = int(damage * sv_damagefactormobj);
			}
			else if (!player && damage > 1 && (target->flags & MF_FRIENDLY))
			{
				// inflict scaled damage to non-player friends
				damage = int(damage * sv_damagefactorfriendly);
			}
			// Special damage types
			if (inflictor)
			{
				if (inflictor->flags4 & MF4_SPECTRAL)
				{
					if (player != NULL)
					{
						if (!deathmatch && inflictor->FriendPlayer > 0)
							return 0;
					}
					else if (target->flags4 & MF4_SPECTRAL)
					{
						if (inflictor->FriendPlayer == 0 && !target->IsHostile(inflictor))
							return 0;
					}
				}

				damage = inflictor->CallDoSpecialDamage(target, damage, mod);
				if (damage < 0)
				{
					return 0;
				}
			}

			int olddam = damage;

			if (damage > 0 && source != NULL)
			{
				damage = int(damage * source->DamageMultiply);

				// Handle active damage modifiers (e.g. PowerDamage)
				if (damage > 0)
				{
					damage = source->GetModifiedDamage(mod, damage, false);
				}
			}
			// Handle passive damage modifiers (e.g. PowerProtection), provided they are not afflicted with protection penetrating powers.
			if (damage > 0 && !(flags & DMG_NO_PROTECT))
			{
				damage = target->GetModifiedDamage(mod, damage, true);
			}
			if (damage > 0 && !(flags & DMG_NO_FACTOR))
			{
				damage = target->ApplyDamageFactor(mod, damage);
			}

			if (damage >= 0)
			{
				damage = target->CallTakeSpecialDamage(inflictor, source, damage, mod);
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
					return 0;
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
		return 0;
	}


	//[RC] Backported from the Zandronum source.. Mostly.
	if( target->player  &&
		damage > 0 &&
		source &&
		mod != NAME_Reflection &&
		target != source)

	{
		int reflectdamage = 0;
		bool reflecttype = false;
		for (auto p = target->player->mo->Inventory; p != nullptr; p = p->Inventory)
		{
			// This picks the reflection item with the maximum efficiency for the given damage type.
			if (p->IsKindOf(NAME_PowerReflection))
			{
				double alwaysreflect = p->FloatVar(NAME_Strength);
				int alwaysdamage = clamp(int(damage * alwaysreflect), 0, damage);
				int mydamage = alwaysdamage + p->ApplyDamageFactor(mod, damage - alwaysdamage);
				if (mydamage > reflectdamage)
				{
					reflectdamage = mydamage;
					reflecttype = p->BoolVar(NAME_ReflectType);
				}
			}
		}

		if (reflectdamage > 0)
		{
			// use the reflect item's damage factors to get the final value here.
			P_DamageMobj(source, nullptr, target, reflectdamage, reflecttype? mod : NAME_Reflection );

			// Reset means of death flag.
			MeansOfDeath = mod;
		}
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

		kickback = int(kickback * G_SkillProperty(SKILLP_KickbackFactor));
		if (kickback)
		{
			AActor *origin = (source && (flags & DMG_INFLICTOR_IS_PUFF))? source : inflictor;

			if (flags & DMG_USEANGLE)
			{
				ang = angle;
			}
			else if (origin->X() == target->X() && origin->Y() == target->Y())
			{
				// If the origin and target are in exactly the same spot, choose a random direction.
				// (Most likely cause is from telefragging somebody during spawning because they
				// haven't moved from their spawn spot at all.)
				ang = pr_kickbackdir.GenRand_Real2() * 360.;
			}
			else
			{
				ang = origin->AngleTo(target);
			}

            thrust = mod == NAME_MDK ? 10 : 32;
            if (target->Mass > 0)
            {
                thrust = clamp((damage * 0.125 * kickback) / target->Mass, 0., thrust);
            }

			// Don't apply ultra-small damage thrust
			if (thrust < 0.01) thrust = 0;

			// make fall forwards sometimes
			if ((damage < 40) && (damage > target->health)
				 && (target->Z() - origin->Z() > 64)
				 && (pr_damagemobj()&1)
				 // [RH] But only if not too fast and not flying
				 && thrust < 10
				 && !(target->flags & MF_NOGRAVITY)
				 && (inflictor == NULL || !(inflictor->flags5 & MF5_NOFORWARDFALL))
				 )
			{
				ang += 180.;
				thrust *= 4;
			}
			if (source && source->player && (flags & DMG_INFLICTOR_IS_PUFF)
				&& source->player->ReadyWeapon != NULL &&
				(source->player->ReadyWeapon->WeaponFlags & WIF_STAFF2_KICKBACK))
			{
				// Staff power level 2
				target->Thrust(ang, 10);
				if (!(target->flags & MF_NOGRAVITY))
				{
					target->Vel.Z += 5.;
				}
			}
			else
			{
				target->Thrust(ang, thrust);
			}
		}
	}

	// [RH] Avoid friendly fire if enabled
	if (!(flags & DMG_FORCED) && source != NULL &&
		((player && player != source->player) || (!player && target != source)) &&
		target->IsTeammate (source))
	{
		//Use the original damage to check for telefrag amount. Don't let the now-amplified damagetypes do it.
		if (!telefragDamage || (target->flags7 & MF7_LAXTELEFRAGDMG))
		{ // Still allow telefragging :-(
			damage = (int)(damage * level.teamdamage);
			if (damage < 0)
			{
				return 0;
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
				return 0;
			}
		}
	}

	//
	// player specific
	//
	if (player)
	{
		// Don't allow DMG_FORCED to work on ultimate degreeslessness/buddha and nodamage.
		if ((player->cheats & (CF_GODMODE2 | CF_BUDDHA2)) || (player->mo->flags5 & MF5_NODAMAGE))
		{
			flags &= ~DMG_FORCED;
		}
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
			if ((!telefragDamage && ((player->mo->flags2 & MF2_INVULNERABLE) ||
				(player->cheats & CF_GODMODE))) ||
				(player->cheats & CF_GODMODE2) || (player->mo->flags5 & MF5_NODAMAGE))
				//Absolutely no hurting if NODAMAGE is involved. Same for GODMODE2.
			{ // player is invulnerable, so don't hurt him
				//Make sure no godmodes and NOPAIN flags are found first.
				//Then, check to see if the player has NODAMAGE or ALLOWPAIN, or inflictor has CAUSEPAIN.
				if ((flags & DMG_NO_PAIN) || (player->cheats & CF_GODMODE) || (player->cheats & CF_GODMODE2) || (player->mo->flags5 & MF5_NOPAIN))
					return 0;
				else if ((((player->mo->flags7 & MF7_ALLOWPAIN) || (player->mo->flags5 & MF5_NODAMAGE)) || ((inflictor != NULL) && (inflictor->flags7 & MF7_CAUSEPAIN))))
				{
					invulpain = true;
					goto fakepain;
				}
				else
					return 0;
			}
			// Armor for players.
			if (!(flags & DMG_NO_ARMOR) && player->mo->Inventory != NULL)
			{
				int newdam = damage;
				if (damage > 0)
				{
					newdam = player->mo->AbsorbDamage(damage, mod);
				}
				if (!telefragDamage || (player->mo->flags7 & MF7_LAXTELEFRAGDMG)) //rawdamage is never modified.
				{
					// if we are telefragging don't let the damage value go below that magic value. Some further checks would fail otherwise.
					damage = newdam;
				}
				
				if (damage <= 0)
				{
					// [MC] Godmode doesn't need checking here, it's already being handled above.
					if (noPain)
						return 0;
					
					// If MF6_FORCEPAIN is set, make the player enter the pain state.
					if ((inflictor && (inflictor->flags6 & MF6_FORCEPAIN)))
						goto dopain;
					else if (((player->mo->flags7 & MF7_ALLOWPAIN) && (rawdamage > 0)) ||
							(inflictor && (inflictor->flags7 & MF7_CAUSEPAIN)))
					{
						invulpain = true;
						goto fakepain;
					}
					return 0;
				}
			}
			
			if (damage >= player->health && !telefragDamage
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
			if (!(flags & DMG_FORCED) && (((player->cheats & CF_BUDDHA2) || (((player->cheats & CF_BUDDHA) ||
				(player->mo->FindInventory (PClass::FindActor(NAME_PowerBuddha),true) != nullptr) ||
				(player->mo->flags7 & MF7_BUDDHA)) && !telefragDamage)) && (player->playerstate != PST_DEAD)))
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
			newdam = target->AbsorbDamage(damage, mod);
			damage = newdam;
			if (damage <= 0)
			{
				if (fakedPain)
					goto fakepain;
				else
					return 0;
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
	if ( source && source->player && !(target->flags5 & MF5_DONTDRAIN))
	{
		if (!target->player || target->player != source->player)
		{
			double draindamage = 0;
			for (auto p = source->player->mo->Inventory; p != nullptr; p = p->Inventory)
			{
				// This picks the item with the maximum efficiency.
				if (p->IsKindOf(NAME_PowerDrain))
				{
					double mydamage = p->FloatVar(NAME_Strength);
					if (mydamage > draindamage) draindamage = mydamage;
				}
			}

			if (draindamage > 0)
			{
				int draindmg = int(draindamage * damage);
				IFVIRTUALPTR(source, AActor, OnDrain)
				{
					VMValue params[] = { source, target, draindmg, mod.GetIndex() };
					VMReturn ret(&draindmg);
					VMCall(func, params, countof(params), &ret, 1);
				}
				if (P_GiveBody(source, draindmg))
				{
					S_Sound(source, CHAN_ITEM, "*drainhealth", 1, ATTN_NORM);
				}
			}
		}
	}


	if (target->health <= 0)
	{ 
		//[MC]Buddha flag for monsters.
		if (!(flags & DMG_FORCED) && ((target->flags7 & MF7_BUDDHA) && !telefragDamage && ((inflictor == NULL || !(inflictor->flags7 & MF7_FOILBUDDHA)) && !(flags & DMG_FOILBUDDHA))))
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

			const int realdamage = MAX(0, damage);
			E_WorldThingDamaged(target, inflictor, source, realdamage, mod, flags, angle);
			needevent = false;

			target->CallDie (source, inflictor, flags);
			return realdamage;
		}
	}

	woundstate = target->FindState(NAME_Wound, mod);
	if (woundstate != NULL)
	{
		int woundhealth = target->WoundHealth;

		if (target->health <= woundhealth)
		{
			target->SetState (woundstate);
			return MAX(0, damage);
		}
	}

fakepain: //Needed so we can skip the rest of the above, but still obey the original rules.

	if (!noPain &&
		(target->player != NULL || !G_SkillProperty(SKILLP_NoPain)) && !(target->flags & MF_SKULLFLY))
	{
		painchance = target->PainChance;
		for (auto & pc : target->GetInfo()->PainChances)
		{
			if (pc.first == mod)
			{
				painchance = pc.second;
				break;
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
			target->threshold = target->DefThreshold;
			if (target->state == target->SpawnState && target->SeeState != NULL)
			{
				target->SetState (target->SeeState);
			}
		}
		else if (source != target->target && target->CallOkayToSwitchTarget (source))
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
			target->threshold = target->DefThreshold;
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
		return 0; //NOW we return -1!
	}
	return MAX(0, damage);
}

DEFINE_ACTION_FUNCTION(AActor, DamageMobj)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_OBJECT(inflictor, AActor);
	PARAM_OBJECT(source, AActor);
	PARAM_INT(damage);
	PARAM_NAME(mod);
	PARAM_INT_DEF(flags);
	PARAM_FLOAT_DEF(angle);

	// [ZZ] event handlers need the result.
	bool needevent = true;
	int realdamage = DamageMobj(self, inflictor, source, damage, mod, flags, angle, needevent);
	if (realdamage && needevent)
	{
		E_WorldThingDamaged(self, inflictor, source, realdamage, mod, flags, angle);
	}
	ACTION_RETURN_INT(realdamage);
}

int P_DamageMobj(AActor *target, AActor *inflictor, AActor *source, int damage, FName mod, int flags, DAngle angle)
{
	IFVIRTUALPTR(target, AActor, DamageMobj)
	{
		VMValue params[7] = { target, inflictor, source, damage, mod.GetIndex(), flags, angle.Degrees };
		VMReturn ret;
		int retval;
		ret.IntAt(&retval);
		VMCall(func, params, 7, &ret, 1);
		return retval;
	}
	else
	{
		bool needevent = true;
		int realdamage = DamageMobj(target, inflictor, source, damage, mod, flags, angle, needevent);
		if (realdamage && needevent)
		{
			// [ZZ] event handlers only need the resultant damage (they can't do anything about it anyway)
			E_WorldThingDamaged(target, inflictor, source, realdamage, mod, flags, angle);
		}
		return realdamage;
	}
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

DEFINE_ACTION_FUNCTION(AActor, PoisonMobj)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_OBJECT_NOT_NULL(inflictor, AActor);
	PARAM_OBJECT(source, AActor);
	PARAM_INT(damage);
	PARAM_INT(duration);
	PARAM_INT(period);
	PARAM_NAME(mod);
	P_PoisonMobj(self, inflictor, source, damage, duration, period, mod);
	return 0;
}

//==========================================================================
//
// OkayToSwitchTarget
//
//==========================================================================

bool AActor::OkayToSwitchTarget(AActor *other)
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

	if ((flags7 & MF7_NOINFIGHTSPECIES) && GetSpecies() == other->GetSpecies())
		return false;		// Don't fight own species.

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
	if (flags7 & MF7_FORCEINFIGHTING) infight = 1;
	else if (flags5 & MF5_NOINFIGHTING) infight = -1;
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

DEFINE_ACTION_FUNCTION(AActor, OkayToSwitchTarget)
{
	PARAM_SELF_PROLOGUE(AActor);
	PARAM_OBJECT(other, AActor);
	ACTION_RETURN_BOOL(self->OkayToSwitchTarget(other));
}

bool AActor::CallOkayToSwitchTarget(AActor *other)
{
	IFVIRTUAL(AActor, OkayToSwitchTarget)
	{
		VMValue params[] = { (DObject*)this, other };
		int retv;
		VMReturn ret(&retv);
		VMCall(func, params, 2, &ret, 1);
		return !!retv;
	}
	return OkayToSwitchTarget(other);
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
		poison = (int)(poison * level.teamdamage);
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

DEFINE_ACTION_FUNCTION(_PlayerInfo, PoisonPlayer)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	PARAM_OBJECT(poisoner, AActor);
	PARAM_OBJECT(source, AActor);
	PARAM_INT(poison);
	ACTION_RETURN_BOOL(P_PoisonPlayer(self, poisoner, source, poison));
}

//==========================================================================
//
// P_PoisonDamage - Similar to P_DamageMobj
//
//==========================================================================

void P_PoisonDamage (player_t *player, AActor *source, int damage, bool playPainSound)
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
	damage = int(damage * G_SkillProperty(SKILLP_DamageFactor) * sv_damagefactorplayer);
	// Handle passive damage modifiers (e.g. PowerProtection)
	damage = target->GetModifiedDamage(player->poisontype, damage, true);
	// Modify with damage factors
	damage = target->ApplyDamageFactor(player->poisontype, damage);

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
		if ((((player->cheats & CF_BUDDHA) || (player->cheats & CF_BUDDHA2) ||
			(player->mo->flags7 & MF7_BUDDHA)) && damage < TELEFRAG_DAMAGE) ||
			(player->mo->FindInventory (PClass::FindActor(NAME_PowerBuddha),true) != nullptr))
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
			target->CallDie(source, source);
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

DEFINE_ACTION_FUNCTION(_PlayerInfo, PoisonDamage)
{
	PARAM_SELF_STRUCT_PROLOGUE(player_t);
	PARAM_OBJECT(source, AActor);
	PARAM_INT(damage);
	PARAM_BOOL(playsound);
	P_PoisonDamage(self, source, damage, playsound);
	return 0;
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
		else if (!stricmp (argv[1], "baddies"))
		{
			// Kill all the unfriendly monsters
			if (CheckCheatmode ())
				return;

			Net_WriteByte (DEM_GENERICCHEAT);
			Net_WriteByte (CHT_MASSACRE2);
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
