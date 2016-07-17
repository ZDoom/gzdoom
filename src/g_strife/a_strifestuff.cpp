#include "actor.h"
#include "g_level.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "a_strifeglobal.h"
#include "p_enemy.h"
#include "p_lnspec.h"
#include "c_console.h"
#include "thingdef/thingdef.h"
#include "doomstat.h"
#include "gstrings.h"
#include "a_keys.h"
#include "a_sharedglobal.h"
#include "templates.h"
#include "d_event.h"
#include "v_font.h"
#include "farchive.h"
#include "p_spec.h"
#include "portal.h"

// Include all the other Strife stuff here to reduce compile time
#include "a_acolyte.cpp"
#include "a_spectral.cpp"
#include "a_alienspectres.cpp"
#include "a_coin.cpp"
#include "a_crusader.cpp"
#include "a_entityboss.cpp"
#include "a_inquisitor.cpp"
#include "a_loremaster.cpp"
//#include "a_macil.cpp"
#include "a_oracle.cpp"
#include "a_programmer.cpp"
#include "a_reaver.cpp"
#include "a_rebels.cpp"
#include "a_sentinel.cpp"
#include "a_stalker.cpp"
#include "a_strifeitems.cpp"
#include "a_strifeweapons.cpp"
#include "a_templar.cpp"
#include "a_thingstoblowup.cpp"

// Notes so I don't forget them:
// Strife does some extra stuff in A_Explode if a player caused the explosion. (probably NoiseAlert)
// See the instructions @ 21249.
//
// Strife's FLOATSPEED is 5 and not 4.
//
// In P_CheckMissileRange, mobjtypes 53,54,55,56,57,58 shift the distance right 4 bits (some, but not all the acolytes)
//						   mobjtypes 61,63,91 shift it right 1 bit
//
// When shooting missiles at something, if MF_SHADOW is set, the angle is adjusted with the formula:
//		angle += pr_spawnmissile.Random2() << 21
// When MF_STRIFEx4000000 is set, the angle is adjusted similarly:
//		angle += pr_spawnmissile.Random2() << 22
// Note that these numbers are different from those used by all the other Doom engine games.

static FRandom pr_gibtosser ("GibTosser");

// Force Field Guard --------------------------------------------------------

void A_RemoveForceField (AActor *);

class AForceFieldGuard : public AActor
{
	DECLARE_CLASS (AForceFieldGuard, AActor)
public:
	int TakeSpecialDamage (AActor *inflictor, AActor *source, int damage, FName damagetype);
};

IMPLEMENT_CLASS (AForceFieldGuard)

int AForceFieldGuard::TakeSpecialDamage (AActor *inflictor, AActor *source, int damage, FName damagetype)
{
	if (inflictor == NULL || !inflictor->IsKindOf (RUNTIME_CLASS(ADegninOre)))
	{
		return -1;
	}
	return health;
}

// Kneeling Guy -------------------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_SetShadow)
{
	PARAM_ACTION_PROLOGUE;

	self->flags |= MF_STRIFEx8000000|MF_SHADOW;
	self->RenderStyle = STYLE_Translucent;
	self->Alpha = HR_SHADOW;
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_ClearShadow)
{
	PARAM_ACTION_PROLOGUE;

	self->flags &= ~(MF_STRIFEx8000000|MF_SHADOW);
	self->RenderStyle = STYLE_Normal;
	self->Alpha = 1.;
	return 0;
}

static FRandom pr_gethurt ("HurtMe!");

DEFINE_ACTION_FUNCTION(AActor, A_GetHurt)
{
	PARAM_ACTION_PROLOGUE;

	self->flags4 |= MF4_INCOMBAT;
	if ((pr_gethurt() % 5) == 0)
	{
		S_Sound (self, CHAN_VOICE, self->PainSound, 1, ATTN_NORM);
		self->health--;
	}
	if (self->health <= 0)
	{
		self->Die (self->target, self->target);
	}
	return 0;
}

// Klaxon Warning Light -----------------------------------------------------

DEFINE_ACTION_FUNCTION(AActor, A_TurretLook)
{
	PARAM_ACTION_PROLOGUE;

	AActor *target;

	if (self->flags5 & MF5_INCONVERSATION)
		return 0;

	self->threshold = 0;
	target = self->LastHeard;
	if (target != NULL &&
		target->health > 0 &&
		target->flags & MF_SHOOTABLE &&
		(self->flags & MF_FRIENDLY) != (target->flags & MF_FRIENDLY))
	{
		self->target = target;
		if ((self->flags & MF_AMBUSH) && !P_CheckSight (self, target))
		{
			return 0;
		}
		if (self->SeeSound != 0)
		{
			S_Sound (self, CHAN_VOICE, self->SeeSound, 1, ATTN_NORM);
		}
		self->LastHeard = NULL;
		self->threshold = 10;
		self->SetState (self->SeeState);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_KlaxonBlare)
{
	PARAM_ACTION_PROLOGUE;

	if (--self->reactiontime < 0)
	{
		self->target = NULL;
		self->reactiontime = self->GetDefault()->reactiontime;
		CALL_ACTION(A_TurretLook, self);
		if (self->target == NULL)
		{
			self->SetIdle();
		}
		else
		{
			self->reactiontime = 50;
		}
	}
	if (self->reactiontime == 2)
	{
		// [RH] Unalert monsters near the alarm and not just those in the same sector as it.
		P_NoiseAlert (NULL, self, false);
	}
	else if (self->reactiontime > 50)
	{
		S_Sound (self, CHAN_VOICE, "misc/alarm", 1, ATTN_NORM);
	}
	return 0;
}

// Power Coupling -----------------------------------------------------------

class APowerCoupling : public AActor
{
	DECLARE_CLASS (APowerCoupling, AActor)
public:
	void Die (AActor *source, AActor *inflictor, int dmgflags);
};

IMPLEMENT_CLASS (APowerCoupling)

void APowerCoupling::Die (AActor *source, AActor *inflictor, int dmgflags)
{
	Super::Die (source, inflictor, dmgflags);

	int i;

	for (i = 0; i < MAXPLAYERS; ++i)
		if (playeringame[i] && players[i].health > 0)
			break;

	if (i == MAXPLAYERS)
		return;

	// [RH] In case the player broke it with the dagger, alert the guards now.
	if (LastHeard != source)
	{
		P_NoiseAlert (source, this);
	}
	EV_DoDoor (DDoor::doorClose, NULL, players[i].mo, 225, 2., 0, 0, 0);
	EV_DoFloor (DFloor::floorLowerToHighest, NULL, 44, 1., 0., -1, 0, false);
	players[i].mo->GiveInventoryType (QuestItemClasses[5]);
	S_Sound (CHAN_VOICE, "svox/voc13", 1, ATTN_NORM);
	players[i].SetLogNumber (13);
	P_DropItem (this, PClass::FindActor("BrokenPowerCoupling"), -1, 256);
	Destroy ();
}

// Gibs for things that bleed -----------------------------------------------

class AMeat : public AActor
{
	DECLARE_CLASS (AMeat, AActor)
public:
	void BeginPlay ()
	{
		// Strife used mod 19, but there are 20 states. Hmm.
		SetState (SpawnState + pr_gibtosser() % 20);
	}
};

IMPLEMENT_CLASS (AMeat)

//==========================================================================
//
// A_TossGib
//
//==========================================================================

DEFINE_ACTION_FUNCTION(AActor, A_TossGib)
{
	PARAM_ACTION_PROLOGUE;

	const char *gibtype = (self->flags & MF_NOBLOOD) ? "Junk" : "Meat";
	AActor *gib = Spawn (gibtype, self->PosPlusZ(24.), ALLOW_REPLACE);

	if (gib == NULL)
	{
		return 0;
	}

	gib->Angles.Yaw = pr_gibtosser() * (360 / 256.f);
	gib->VelFromAngle(pr_gibtosser() & 15);
	gib->Vel.Z = pr_gibtosser() & 15;
	return 0;
}

//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_FLoopActiveSound)
{
	PARAM_ACTION_PROLOGUE;

	if (self->ActiveSound != 0 && !(level.time & 7))
	{
		S_Sound (self, CHAN_VOICE, self->ActiveSound, 1, ATTN_NORM);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_Countdown)
{
	PARAM_ACTION_PROLOGUE;

	if (--self->reactiontime <= 0)
	{
		P_ExplodeMissile (self, NULL, NULL);
		self->flags &= ~MF_SKULLFLY;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_LoopActiveSound)
{
	PARAM_ACTION_PROLOGUE;

	if (self->ActiveSound != 0 && !S_IsActorPlayingSomething (self, CHAN_VOICE, -1))
	{
		S_Sound (self, CHAN_VOICE|CHAN_LOOP, self->ActiveSound, 1, ATTN_NORM);
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_CheckTerrain)
{
	PARAM_ACTION_PROLOGUE;

	sector_t *sec = self->Sector;

	if (self->Z() == sec->floorplane.ZatPoint(self) && sec->PortalBlocksMovement(sector_t::floor))
	{
		if (sec->special == Damage_InstantDeath)
		{
			P_DamageMobj (self, NULL, NULL, 999, NAME_InstantDeath);
		}
		else if (sec->special == Scroll_StrifeCurrent)
		{
			int anglespeed = tagManager.GetFirstSectorTag(sec) - 100;
			double speed = (anglespeed % 10) / 16.;
			DAngle an = (anglespeed / 10) * (360 / 8.);
			self->Thrust(an, speed);
		}
	}
	return 0;
}

//============================================================================
//
// A_ClearSoundTarget
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_ClearSoundTarget)
{
	PARAM_ACTION_PROLOGUE;

	AActor *actor;

	self->Sector->SoundTarget = NULL;
	for (actor = self->Sector->thinglist; actor != NULL; actor = actor->snext)
	{
		actor->LastHeard = NULL;
	}
	return 0;
}


DEFINE_ACTION_FUNCTION(AActor, A_ItBurnsItBurns)
{
	PARAM_ACTION_PROLOGUE;

	S_Sound (self, CHAN_VOICE, "human/imonfire", 1, ATTN_NORM);

	if (self->player != nullptr && self->player->mo == self)
	{
		P_SetPsprite(self->player, PSP_STRIFEHANDS, self->FindState("FireHands"));

		self->player->ReadyWeapon = nullptr;
		self->player->PendingWeapon = WP_NOCHANGE;
		self->player->playerstate = PST_LIVE;
		self->player->extralight = 3;
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_DropFire)
{
	PARAM_ACTION_PROLOGUE;

	AActor *drop = Spawn("FireDroplet", self->PosPlusZ(24.), ALLOW_REPLACE);
	drop->Vel.Z = -1.;
	P_RadiusAttack (self, self, 64, 64, NAME_Fire, 0);
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_CrispyPlayer)
{
	PARAM_ACTION_PROLOGUE;

	if (self->player != nullptr && self->player->mo == self)
	{
		DPSprite *psp;
		psp = self->player->GetPSprite(PSP_STRIFEHANDS);

		FState *firehandslower = self->FindState("FireHandsLower");
		FState *firehands = self->FindState("FireHands");
		FState *state = psp->GetState();

		if (state != nullptr && firehandslower != nullptr && firehands != nullptr && firehands < firehandslower)
		{
			self->player->playerstate = PST_DEAD;
			psp->SetState(state + (firehandslower - firehands));
		}
		else if (state == nullptr)
		{
			psp->SetState(nullptr);
		}
	}
	return 0;
}

DEFINE_ACTION_FUNCTION(AActor, A_HandLower)
{
	PARAM_ACTION_PROLOGUE;

	if (self->player != nullptr)
	{
		DPSprite *psp = self->player->GetPSprite(PSP_STRIFEHANDS);

		if (psp->GetState() == nullptr)
		{
			psp->SetState(nullptr);
			return 0;
		}

		psp->y += 9;
		if (psp->y > WEAPONBOTTOM*2)
		{
			psp->SetState(nullptr);
		}

		if (self->player->extralight > 0) self->player->extralight--;
	}
	return 0;
}
