#include "actor.h"
#include "p_enemy.h"
#include "a_action.h"
#include "r_draw.h"
#include "m_random.h"
#include "p_local.h"
#include "a_doomglobal.h"
#include "s_sound.h"
#include "r_translate.h"

#define MARINE_PAIN_CHANCE 160

static FRandom pr_m_refire ("SMarineRefire");
static FRandom pr_m_punch ("SMarinePunch");
static FRandom pr_m_gunshot ("SMarineGunshot");
static FRandom pr_m_saw ("SMarineSaw");
static FRandom pr_m_fireshotgun2 ("SMarineFireSSG");

void A_MarineLook (AActor *);
void A_MarineNoise (AActor *);
void A_MarineChase (AActor *);
void A_M_Refire (AActor *);
void A_M_SawRefire (AActor *);
void A_M_Saw (AActor *);
void A_M_Punch (AActor *);
void A_M_BerserkPunch (AActor *);
void A_M_FirePistol (AActor *);
void A_M_FirePistolInaccurate (AActor *);
void A_M_FireShotgun (AActor *);
void A_M_CheckAttack (AActor *);
void A_M_FireShotgun2 (AActor *);
void A_M_FireCGunAccurate (AActor *);
void A_M_FireCGun (AActor *);
void A_M_FireMissile (AActor *);
void A_M_FireRailgun (AActor *);
void A_M_FirePlasma (AActor *);
void A_M_BFGsound (AActor *);
void A_M_FireBFG (AActor *);

// Scriptable marine -------------------------------------------------------

FState AScriptedMarine::States[] =
{
#define S_MPLAYSTILL 0
	S_NORMAL (PLAY, 'A',	4, A_MarineLook 				, &States[S_MPLAYSTILL+1]),
	S_NORMAL (PLAY, 'A',	4, A_MarineNoise 				, &States[S_MPLAYSTILL]),

#define S_MPLAY (S_MPLAYSTILL+2)
	S_NORMAL (PLAY, 'A',	4, A_MarineLook 				, &States[S_MPLAY+1]),
	S_NORMAL (PLAY, 'A',	4, A_MarineNoise 				, &States[S_MPLAY+2]),
	S_NORMAL (PLAY, 'A',	4, A_MarineLook 				, &States[S_MPLAY+3]),
	S_NORMAL (PLAY, 'B',	4, A_MarineNoise				, &States[S_MPLAY+4]),
	S_NORMAL (PLAY, 'B',	4, A_MarineLook					, &States[S_MPLAY+5]),
	S_NORMAL (PLAY, 'B',	4, A_MarineNoise 				, &States[S_MPLAY]),

#define S_MPLAY_RUN (S_MPLAY+6)
	S_NORMAL (PLAY, 'A',	4, A_MarineChase 				, &States[S_MPLAY_RUN+1]),
	S_NORMAL (PLAY, 'B',	4, A_MarineChase 				, &States[S_MPLAY_RUN+2]),
	S_NORMAL (PLAY, 'C',	4, A_MarineChase 				, &States[S_MPLAY_RUN+3]),
	S_NORMAL (PLAY, 'D',	4, A_MarineChase 				, &States[S_MPLAY_RUN+0]),

#define S_MPLAY_ATK (S_MPLAY_RUN+4)
	S_NORMAL (PLAY, 'E',   12, A_FaceTarget					, &States[S_MPLAY]),
	S_BRIGHT (PLAY, 'F',	6, NULL 						, &States[S_MPLAY_ATK+0]),

#define S_MPLAY_PAIN (S_MPLAY_ATK+2)
	S_NORMAL (PLAY, 'G',	4, NULL							, &States[S_MPLAY_PAIN+1]),
	S_NORMAL (PLAY, 'G',	4, A_Pain						, &States[S_MPLAY]),

#define S_MPLAY_DIE (S_MPLAY_PAIN+2)
	S_NORMAL (PLAY, 'H',   10, NULL 						, &States[S_MPLAY_DIE+1]),
	S_NORMAL (PLAY, 'I',   10, A_Scream						, &States[S_MPLAY_DIE+2]),
	S_NORMAL (PLAY, 'J',   10, A_NoBlocking					, &States[S_MPLAY_DIE+3]),
	S_NORMAL (PLAY, 'K',   10, NULL 						, &States[S_MPLAY_DIE+4]),
	S_NORMAL (PLAY, 'L',   10, NULL 						, &States[S_MPLAY_DIE+5]),
	S_NORMAL (PLAY, 'M',   10, NULL 						, &States[S_MPLAY_DIE+6]),
	S_NORMAL (PLAY, 'N',   -1, NULL							, NULL),

#define S_MPLAY_XDIE (S_MPLAY_DIE+7)
	S_NORMAL (PLAY, 'O',	5, NULL 						, &States[S_MPLAY_XDIE+1]),
	S_NORMAL (PLAY, 'P',	5, A_XScream					, &States[S_MPLAY_XDIE+2]),
	S_NORMAL (PLAY, 'Q',	5, A_NoBlocking					, &States[S_MPLAY_XDIE+3]),
	S_NORMAL (PLAY, 'R',	5, NULL 						, &States[S_MPLAY_XDIE+4]),
	S_NORMAL (PLAY, 'S',	5, NULL 						, &States[S_MPLAY_XDIE+5]),
	S_NORMAL (PLAY, 'T',	5, NULL 						, &States[S_MPLAY_XDIE+6]),
	S_NORMAL (PLAY, 'U',	5, NULL 						, &States[S_MPLAY_XDIE+7]),
	S_NORMAL (PLAY, 'V',	5, NULL 						, &States[S_MPLAY_XDIE+8]),
	S_NORMAL (PLAY, 'W',   -1, NULL							, NULL),

#define S_MPLAY_RAISE (S_MPLAY_XDIE+9)
	S_NORMAL (PLAY, 'M',	5, NULL							, &States[S_MPLAY_RAISE+1]),
	S_NORMAL (PLAY, 'L',	5, NULL							, &States[S_MPLAY_RAISE+2]),
	S_NORMAL (PLAY, 'K',	5, NULL							, &States[S_MPLAY_RAISE+3]),
	S_NORMAL (PLAY, 'J',	5, NULL							, &States[S_MPLAY_RAISE+4]),
	S_NORMAL (PLAY, 'I',	5, NULL							, &States[S_MPLAY_RAISE+5]),
	S_NORMAL (PLAY, 'H',	5, NULL							, &States[S_MPLAY_RUN]),

#define S_MPLAY_ATK_CHAINSAW (S_MPLAY_RAISE+6)
	S_NORMAL (PLAY, 'E',	4, A_MarineNoise				, &States[S_MPLAY_ATK_CHAINSAW+1]),
	S_BRIGHT (PLAY, 'F',	4, A_M_Saw						, &States[S_MPLAY_ATK_CHAINSAW+2]),
	S_BRIGHT (PLAY, 'F',	0, A_M_SawRefire				, &States[S_MPLAY_ATK_CHAINSAW+1]),
	S_NORMAL (PLAY, 'A',	0, NULL							, &States[S_MPLAY_RUN]),

#define S_MPLAY_ATK_FIST (S_MPLAY_ATK_CHAINSAW+4)
	S_NORMAL (PLAY, 'E',	4, A_FaceTarget					, &States[S_MPLAY_ATK_FIST+1]),
	S_NORMAL (PLAY, 'F',	4, A_M_Punch					, &States[S_MPLAY_ATK_FIST+2]),	// Purposefully not BRIGHT
	S_NORMAL (PLAY, 'A',	9, NULL							, &States[S_MPLAY_ATK_FIST+3]),
	S_NORMAL (PLAY, 'A',	0, A_M_Refire					, &States[S_MPLAY_ATK_FIST]),
	S_NORMAL (PLAY, 'A',	5, A_FaceTarget					, &States[S_MPLAY_RUN]),

#define S_MPLAY_ATK_BERSERK (S_MPLAY_ATK_FIST+5)
	S_NORMAL (PLAY, 'E',	4, A_FaceTarget					, &States[S_MPLAY_ATK_BERSERK+1]),
	S_NORMAL (PLAY, 'F',	4, A_M_BerserkPunch				, &States[S_MPLAY_ATK_BERSERK+2]),	// Purposefully not BRIGHT
	S_NORMAL (PLAY, 'A',	9, NULL							, &States[S_MPLAY_ATK_BERSERK+3]),
	S_NORMAL (PLAY, 'A',	0, A_M_Refire					, &States[S_MPLAY_ATK_BERSERK]),
	S_NORMAL (PLAY, 'A',	5, A_FaceTarget					, &States[S_MPLAY_RUN]),

#define S_MPLAY_ATK_PISTOL (S_MPLAY_ATK_BERSERK+5)
	S_NORMAL (PLAY, 'E',	4, A_FaceTarget					, &States[S_MPLAY_ATK_PISTOL+1]),
	S_BRIGHT (PLAY, 'F',	6, A_M_FirePistol				, &States[S_MPLAY_ATK_PISTOL+2]),
	S_NORMAL (PLAY, 'A',	4, A_FaceTarget					, &States[S_MPLAY_ATK_PISTOL+3]),
	S_NORMAL (PLAY, 'A',	0, A_M_Refire					, &States[S_MPLAY_ATK_PISTOL+5]),
	S_NORMAL (PLAY, 'A',	5, NULL							, &States[S_MPLAY_RUN]),
	S_BRIGHT (PLAY, 'F',	6, A_M_FirePistolInaccurate		, &States[S_MPLAY_ATK_PISTOL+6]),
	S_NORMAL (PLAY, 'A',	4, A_FaceTarget					, &States[S_MPLAY_ATK_PISTOL+7]),
	S_NORMAL (PLAY, 'A',	0, A_M_Refire					, &States[S_MPLAY_ATK_PISTOL+5]),
	S_NORMAL (PLAY, 'A',	5, A_FaceTarget					, &States[S_MPLAY_RUN]),

#define S_MPLAY_ATK_SHOTGUN (S_MPLAY_ATK_PISTOL+9)
	S_NORMAL (PLAY, 'E',	3, A_M_CheckAttack				, &States[S_MPLAY_ATK_SHOTGUN+1]),
	S_BRIGHT (PLAY, 'F',	7, A_M_FireShotgun				, &States[S_MPLAY_RUN]),

#define S_MPLAY_ATK_DSHOTGUN (S_MPLAY_ATK_SHOTGUN+2)
	S_NORMAL (PLAY, 'E',	3, A_M_CheckAttack				, &States[S_MPLAY_ATK_DSHOTGUN+1]),
	S_BRIGHT (PLAY, 'F',	7, A_M_FireShotgun2				, &States[S_MPLAY_RUN]),
#define S_MPLAY_SKIP_ATTACK (S_MPLAY_ATK_DSHOTGUN+2)
	S_NORMAL (PLAY, 'A',	1, NULL							, &States[S_MPLAY_RUN]),

#define S_MPLAY_ATK_CHAINGUN (S_MPLAY_SKIP_ATTACK+1)
	S_NORMAL (PLAY, 'E',	4, A_FaceTarget					, &States[S_MPLAY_ATK_CHAINGUN+1]),
	S_BRIGHT (PLAY, 'F',	4, A_M_FireCGunAccurate			, &States[S_MPLAY_ATK_CHAINGUN+2]),
	S_BRIGHT (PLAY, 'F',	4, A_M_FireCGunAccurate			, &States[S_MPLAY_ATK_CHAINGUN+3]),
	S_BRIGHT (PLAY, 'F',	4, A_M_FireCGun					, &States[S_MPLAY_ATK_CHAINGUN+4]),
	S_BRIGHT (PLAY, 'F',	4, A_M_FireCGun					, &States[S_MPLAY_ATK_CHAINGUN+5]),
	S_NORMAL (PLAY,	'A',	0, A_M_Refire					, &States[S_MPLAY_ATK_CHAINGUN+3]),
	S_NORMAL (PLAY, 'A',	0, NULL							, &States[S_MPLAY_RUN]),

#define S_MPLAY_ATK_ROCKET (S_MPLAY_ATK_CHAINGUN+7)
	S_NORMAL (PLAY, 'E',	8, A_FaceTarget					, &States[S_MPLAY_ATK_ROCKET+1]),
	S_BRIGHT (PLAY, 'F',	6, A_M_FireMissile				, &States[S_MPLAY_ATK_ROCKET+2]),
	S_NORMAL (PLAY,	'A',	0, A_M_Refire					, &States[S_MPLAY_ATK_ROCKET]),
	S_NORMAL (PLAY, 'A',	0, NULL							, &States[S_MPLAY_RUN]),

#define S_MPLAY_ATK_RAILGUN (S_MPLAY_ATK_ROCKET+4)
	S_NORMAL (PLAY, 'E',	4, A_M_CheckAttack				, &States[S_MPLAY_ATK_RAILGUN+1]),
	S_BRIGHT (PLAY, 'F',	6, A_M_FireRailgun				, &States[S_MPLAY_RUN]),

#define S_MPLAY_ATK_PLASMA (S_MPLAY_ATK_RAILGUN+2)
	S_NORMAL (PLAY, 'E',	2, A_FaceTarget					, &States[S_MPLAY_ATK_PLASMA+1]),
	S_BRIGHT (PLAY, 'F',	3, A_M_FirePlasma				, &States[S_MPLAY_ATK_PLASMA+2]),
	S_NORMAL (PLAY, 'A',	0, A_M_Refire					, &States[S_MPLAY_ATK_PLASMA+1]),
	S_NORMAL (PLAY, 'A',    0, NULL							, &States[S_MPLAY_RUN]),

#define S_MPLAY_ATK_BFG (S_MPLAY_ATK_PLASMA+4)
	S_NORMAL (PLAY, 'E',	5, A_M_BFGsound					, &States[S_MPLAY_ATK_BFG+1]),
	S_NORMAL (PLAY, 'E',	5, A_FaceTarget					, &States[S_MPLAY_ATK_BFG+2]),
	S_NORMAL (PLAY, 'E',	5, A_FaceTarget					, &States[S_MPLAY_ATK_BFG+3]),
	S_NORMAL (PLAY, 'E',	5, A_FaceTarget					, &States[S_MPLAY_ATK_BFG+4]),
	S_NORMAL (PLAY, 'E',	5, A_FaceTarget					, &States[S_MPLAY_ATK_BFG+5]),
	S_NORMAL (PLAY, 'E',	5, A_FaceTarget					, &States[S_MPLAY_ATK_BFG+6]),
	S_BRIGHT (PLAY, 'F',	6, A_M_FireBFG					, &States[S_MPLAY_ATK_BFG+7]),
	S_NORMAL (PLAY, 'A',	4, A_FaceTarget					, &States[S_MPLAY_ATK_BFG+8]),
	S_NORMAL (PLAY, 'A',	0, A_M_Refire					, &States[S_MPLAY_ATK_BFG]),
	S_NORMAL (PLAY, 'A',	0, NULL							, &States[S_MPLAY_RUN]),
};

IMPLEMENT_ACTOR (AScriptedMarine, Doom, 9100, 151)
	PROP_SpawnHealth (100)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (56)
	PROP_Mass (100)
	PROP_SpeedFixed (8)
	PROP_PainChance (MARINE_PAIN_CHANCE)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE)
	PROP_Flags2 (MF2_MCROSS|MF2_PASSMOBJ|MF2_PUSHWALL|MF2_FLOORCLIP)
	PROP_Flags3 (MF3_ISMONSTER)
	PROP_Translation (TRANSLATION_Standard,0)	// Scripted marines wear black
	PROP_Damage (100)

	PROP_SpawnState (S_MPLAYSTILL)
	PROP_SeeState (S_MPLAY_RUN)
	PROP_PainState (S_MPLAY_PAIN)
	PROP_MissileState (S_MPLAY_ATK)
	PROP_DeathState (S_MPLAY_DIE)
	PROP_XDeathState (S_MPLAY_XDIE)
	PROP_RaiseState (S_MPLAY_RAISE)

	PROP_DeathSound ("*death")
	PROP_PainSound ("*pain50")
END_DEFAULTS

void AScriptedMarine::Serialize (FArchive &arc)
{
	Super::Serialize (arc);

	if (arc.IsStoring ())
	{
		arc.WriteSprite (SpriteOverride);
	}
	else
	{
		SpriteOverride = arc.ReadSprite ();
	}
}

void AScriptedMarine::Activate (AActor *activator)
{
	if (flags2 & MF2_DORMANT)
	{
		flags2 &= ~MF2_DORMANT;
		tics = 1;
	}
}

void AScriptedMarine::Deactivate (AActor *activator)
{
	if (!(flags2 & MF2_DORMANT))
	{
		flags2 |= MF2_DORMANT;
		tics = -1;
	}
}

void AScriptedMarine::BeginPlay ()
{
	Super::BeginPlay ();

	// Copy the standard player's scaling
	AActor * playerdef = GetDefaultByName("DoomPlayer");
	if (playerdef != NULL)
	{
		scaleX = playerdef->scaleX;
		scaleY = playerdef->scaleY;
	}
}

void AScriptedMarine::Tick ()
{
	Super::Tick ();

	// Override the standard sprite, if desired
	if (SpriteOverride != 0 && sprite == States[0].sprite.index)
	{
		sprite = SpriteOverride;
	}

	if (special1 != 0)
	{
		if (MissileState == &States[S_MPLAY_ATK_DSHOTGUN])
		{ // Play SSG reload sounds
			int ticks = level.maptime - special1;
			if (ticks < 47)
			{
				switch (ticks)
				{
				case 14:
					S_Sound (this, CHAN_WEAPON, "weapons/sshoto", 1, ATTN_NORM);
					break;
				case 28:
					S_Sound (this, CHAN_WEAPON, "weapons/sshotl", 1, ATTN_NORM);
					break;
				case 41:
					S_Sound (this, CHAN_WEAPON, "weapons/sshotc", 1, ATTN_NORM);
					break;
				}
			}
			else
			{
				special1 = 0;
			}
		}
		else
		{ // Wait for a long refire time
			if (level.maptime >= special1)
			{
				special1 = 0;
			}
			else
			{
				flags |= MF_JUSTATTACKED;
			}
		}
	}
}

//============================================================================
//
// A_M_Refire
//
//============================================================================

void A_M_Refire (AActor *self)
{
	if (self->target == NULL || self->target->health <= 0)
	{
		if (self->MissileState && pr_m_refire() < 160)
		{ // Look for a new target most of the time
			if (P_LookForPlayers (self, true) && P_CheckMissileRange (self))
			{ // Found somebody new and in range, so don't stop shooting
				return;
			}
		}
		self->SetState (self->state + 1);
		return;
	}
	if ((self->MissileState == NULL && !self->CheckMeleeRange ()) ||
		!P_CheckSight (self, self->target) ||
		pr_m_refire() < 4)	// Small chance of stopping even when target not dead
	{
		self->SetState (self->state + 1);
	}
}

//============================================================================
//
// A_M_SawRefire
//
//============================================================================

void A_M_SawRefire (AActor *self)
{
	if (self->target == NULL || self->target->health <= 0)
	{
		self->SetState (self->state + 1);
		return;
	}
	if (!self->CheckMeleeRange ())
	{
		self->SetState (self->state + 1);
	}
}

//============================================================================
//
// A_MarineChase
//
//============================================================================

void A_MarineChase (AActor *self)
{
	if (self->MeleeState == &AScriptedMarine::States[S_MPLAY_ATK_CHAINSAW])
	{
		S_Sound (self, CHAN_WEAPON, "weapons/sawidle", 1, ATTN_NORM);
	}
	A_Chase (self);
}

//============================================================================
//
// A_MarineLook
//
//============================================================================

void A_MarineLook (AActor *self)
{
	A_MarineNoise (self);
	A_Look (self);
}

//============================================================================
//
// A_MarineNoise
//
//============================================================================

void A_MarineNoise (AActor *self)
{
	if (self->MeleeState == &AScriptedMarine::States[S_MPLAY_ATK_CHAINSAW])
	{
		S_Sound (self, CHAN_WEAPON, "weapons/sawidle", 1, ATTN_NORM);
	}
}

//============================================================================
//
// A_M_Saw
//
//============================================================================

void A_M_Saw (AActor *self)
{
	if (self->target == NULL)
		return;

	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		angle_t 	angle;
		int 		damage;
		AActor		*linetarget;

		damage = 2 * (pr_m_saw()%10+1);
		angle = self->angle + (pr_m_saw.Random2() << 18);
		
		P_LineAttack (self, angle, MELEERANGE+1,
					P_AimLineAttack (self, angle, MELEERANGE+1, &linetarget), damage,
					NAME_Melee, NAME_BulletPuff);

		if (!linetarget)
		{
			S_Sound (self, CHAN_WEAPON, "weapons/sawfull", 1, ATTN_NORM);
			return;
		}
		S_Sound (self, CHAN_WEAPON, "weapons/sawhit", 1, ATTN_NORM);
			
		// turn to face target
		angle = R_PointToAngle2 (self->x, self->y, linetarget->x, linetarget->y);
		if (angle - self->angle > ANG180)
		{
			if (angle - self->angle < (angle_t)(-ANG90/20))
				self->angle = angle + ANG90/21;
			else
				self->angle -= ANG90/20;
		}
		else
		{
			if (angle - self->angle > ANG90/20)
				self->angle = angle - ANG90/21;
			else
				self->angle += ANG90/20;
		}
	}
	else
	{
		S_Sound (self, CHAN_WEAPON, "weapons/sawfull", 1, ATTN_NORM);
	}
	//A_Chase (self);
}

//============================================================================
//
// A_M_Punch
//
//============================================================================

void A_M_Punch (AActor *self)
{
	angle_t 	angle;
	int 		damage;
	int 		pitch;
	AActor		*linetarget;

	if (self->target == NULL)
		return;

	damage = (pr_m_punch()%10+1) << 1;

	A_FaceTarget (self);
	angle = self->angle + (pr_m_punch.Random2() << 18);
	pitch = P_AimLineAttack (self, angle, MELEERANGE, &linetarget);
	P_LineAttack (self, angle, MELEERANGE, pitch, damage, NAME_Melee, NAME_BulletPuff, true);

	// turn to face target
	if (linetarget)
	{
		S_Sound (self, CHAN_WEAPON, "*fist", 1, ATTN_NORM);
		self->angle = R_PointToAngle2 (self->x, self->y, linetarget->x, linetarget->y);
	}
}

//============================================================================
//
// A_M_BerserkPunch
//
//============================================================================

void A_M_BerserkPunch (AActor *self)
{
	angle_t 	angle;
	int 		damage;
	int 		pitch;
	AActor		*linetarget;

	if (self->target == NULL)
		return;

	damage = ((pr_m_punch()%10+1) << 1) * 10;

	A_FaceTarget (self);
	angle = self->angle + (pr_m_punch.Random2() << 18);
	pitch = P_AimLineAttack (self, angle, MELEERANGE, &linetarget);
	P_LineAttack (self, angle, MELEERANGE, pitch, damage, NAME_Melee, NAME_BulletPuff, true);

	// turn to face target
	if (linetarget)
	{
		S_Sound (self, CHAN_WEAPON, "*fist", 1, ATTN_NORM);
		self->angle = R_PointToAngle2 (self->x, self->y, linetarget->x, linetarget->y);
	}
}

//============================================================================
//
// P_GunShot2
//
//============================================================================

void P_GunShot2 (AActor *mo, bool accurate, int pitch, const PClass *pufftype)
{
	angle_t 	angle;
	int 		damage;
		
	damage = 5*(pr_m_gunshot()%3+1);
	angle = mo->angle;

	if (!accurate)
	{
		angle += pr_m_gunshot.Random2 () << 18;
	}

	P_LineAttack (mo, angle, MISSILERANGE, pitch, damage, NAME_None, pufftype);
}

//============================================================================
//
// A_M_FirePistol
//
//============================================================================

void A_M_FirePistol (AActor *self)
{
	if (self->target == NULL)
		return;

	S_Sound (self, CHAN_WEAPON, "weapons/pistol", 1, ATTN_NORM);
	A_FaceTarget (self);
	P_GunShot2 (self, true, P_AimLineAttack (self, self->angle, MISSILERANGE),
		PClass::FindClass(NAME_BulletPuff));
}

//============================================================================
//
// A_M_FirePistolInaccurate
//
//============================================================================

void A_M_FirePistolInaccurate (AActor *self)
{
	if (self->target == NULL)
		return;

	S_Sound (self, CHAN_WEAPON, "weapons/pistol", 1, ATTN_NORM);
	A_FaceTarget (self);
	P_GunShot2 (self, false, P_AimLineAttack (self, self->angle, MISSILERANGE),
		PClass::FindClass(NAME_BulletPuff));
}

//============================================================================
//
// A_M_FireShotgun
//
//============================================================================

void A_M_FireShotgun (AActor *self)
{
	int pitch;

	if (self->target == NULL)
		return;

	S_Sound (self, CHAN_WEAPON,  "weapons/shotgf", 1, ATTN_NORM);
	A_FaceTarget (self);
	pitch = P_AimLineAttack (self, self->angle, MISSILERANGE);
	for (int i = 0; i < 7; ++i)
	{
		P_GunShot2 (self, false, pitch, PClass::FindClass(NAME_BulletPuff));
	}
	self->special1 = level.maptime + 27;
}

//============================================================================
//
// A_M_CheckAttack
//
//============================================================================

void A_M_CheckAttack (AActor *self)
{
	if (self->special1 != 0 || self->target == NULL)
	{
		self->SetState (&AScriptedMarine::States[S_MPLAY_SKIP_ATTACK]);
	}
	else
	{
		A_FaceTarget (self);
	}
}

//============================================================================
//
// A_M_FireShotgun2
//
//============================================================================

void A_M_FireShotgun2 (AActor *self)
{
	int pitch;

	if (self->target == NULL)
		return;

	S_Sound (self, CHAN_WEAPON, "weapons/sshotf", 1, ATTN_NORM);
	A_FaceTarget (self);
	pitch = P_AimLineAttack (self, self->angle, MISSILERANGE);
	for (int i = 0; i < 20; ++i)
	{
		int damage = 5*(pr_m_fireshotgun2()%3+1);
		angle_t angle = self->angle + (pr_m_fireshotgun2.Random2() << 19);

		P_LineAttack (self, angle, MISSILERANGE,
					  pitch + (pr_m_fireshotgun2.Random2() * 332063), damage,
					  NAME_None, PClass::FindClass(NAME_BulletPuff));
	}
	self->special1 = level.maptime;
}

//============================================================================
//
// A_M_FireCGunAccurate
//
//============================================================================

void A_M_FireCGunAccurate (AActor *self)
{
	if (self->target == NULL)
		return;

	S_Sound (self, CHAN_WEAPON, "weapons/chngun", 1, ATTN_NORM);
	A_FaceTarget (self);
	P_GunShot2 (self, true, P_AimLineAttack (self, self->angle, MISSILERANGE),
		PClass::FindClass(NAME_BulletPuff));
}

//============================================================================
//
// A_M_FireCGun
//
//============================================================================

void A_M_FireCGun (AActor *self)
{
	if (self->target == NULL)
		return;

	S_Sound (self, CHAN_WEAPON, "weapons/chngun", 1, ATTN_NORM);
	A_FaceTarget (self);
	P_GunShot2 (self, false, P_AimLineAttack (self, self->angle, MISSILERANGE),
		PClass::FindClass(NAME_BulletPuff));
}

//============================================================================
//
// A_M_FireMissile
//
// Giving a marine a rocket launcher is probably a bad idea unless you pump
// up his health, because he's just as likely to kill himself as he is to
// kill anything else with it.
//
//============================================================================

void A_M_FireMissile (AActor *self)
{
	if (self->target == NULL)
		return;

	if (self->CheckMeleeRange ())
	{ // If too close, punch it
		A_M_Punch (self);
	}
	else
	{
		A_FaceTarget (self);
		P_SpawnMissile (self, self->target, PClass::FindClass("Rocket"));
	}
}

//============================================================================
//
// A_M_FireRailgun
//
//============================================================================

void A_M_FireRailgun (AActor *self)
{
	if (self->target == NULL)
		return;

	A_MonsterRail (self);
	self->special1 = level.maptime + 50;
}

//============================================================================
//
// A_M_FirePlasma
//
//============================================================================

void A_M_FirePlasma (AActor *self)
{
	if (self->target == NULL)
		return;

	A_FaceTarget (self);
	P_SpawnMissile (self, self->target, PClass::FindClass("PlasmaBall"));
	self->special1 = level.maptime + 20;
}

//============================================================================
//
// A_M_BFGsound
//
//============================================================================

void A_M_BFGsound (AActor *self)
{
	if (self->target == NULL)
		return;

	if (self->special1 != 0)
	{
		self->SetState (self->SeeState);
	}
	else
	{
		A_FaceTarget (self);
		S_Sound (self, CHAN_WEAPON, "weapons/bfgf", 1, ATTN_NORM);
		// Don't interrupt the firing sequence
		self->PainChance = 0;
	}
}

//============================================================================
//
// A_M_FireBFG
//
//============================================================================

void A_M_FireBFG (AActor *self)
{
	if (self->target == NULL)
		return;

	A_FaceTarget (self);
	P_SpawnMissile (self, self->target, PClass::FindClass("BFGBall"));
	self->special1 = level.maptime + 30;
	self->PainChance = MARINE_PAIN_CHANCE;
}

//---------------------------------------------------------------------------

class AMarineFist : public AScriptedMarine
{
	DECLARE_STATELESS_ACTOR (AMarineFist, AScriptedMarine)
};

IMPLEMENT_STATELESS_ACTOR (AMarineFist, Doom, 9101, 0)
	PROP_SpawnState (S_MPLAYSTILL)
	PROP_MeleeState (S_MPLAY_ATK_FIST)
	PROP_MissileState (255)
END_DEFAULTS

//---------------------------------------------------------------------------

class AMarineBerserk : public AScriptedMarine
{
	DECLARE_STATELESS_ACTOR (AMarineBerserk, AScriptedMarine)
};

IMPLEMENT_STATELESS_ACTOR (AMarineBerserk, Doom, 9102, 0)
	PROP_SpawnState (S_MPLAYSTILL)
	PROP_MeleeState (S_MPLAY_ATK_BERSERK)
	PROP_MissileState (255)
END_DEFAULTS

//---------------------------------------------------------------------------

class AMarineChainsaw : public AScriptedMarine
{
	DECLARE_STATELESS_ACTOR (AMarineChainsaw, AScriptedMarine)
};

IMPLEMENT_STATELESS_ACTOR (AMarineChainsaw, Doom, 9103, 0)
	PROP_SpawnState (S_MPLAYSTILL)
	PROP_MeleeState (S_MPLAY_ATK_CHAINSAW)
	PROP_MissileState (255)
END_DEFAULTS

//---------------------------------------------------------------------------

class AMarinePistol : public AScriptedMarine
{
	DECLARE_STATELESS_ACTOR (AMarinePistol, AScriptedMarine)
};

IMPLEMENT_STATELESS_ACTOR (AMarinePistol, Doom, 9104, 0)
	PROP_SpawnState (S_MPLAYSTILL)
	PROP_MeleeState (255)
	PROP_MissileState (S_MPLAY_ATK_PISTOL)
END_DEFAULTS

//---------------------------------------------------------------------------

class AMarineShotgun : public AScriptedMarine
{
	DECLARE_STATELESS_ACTOR (AMarineShotgun, AScriptedMarine)
};

IMPLEMENT_STATELESS_ACTOR (AMarineShotgun, Doom, 9105, 0)
	PROP_SpawnState (S_MPLAYSTILL)
	PROP_MeleeState (255)
	PROP_MissileState (S_MPLAY_ATK_SHOTGUN)
END_DEFAULTS

//---------------------------------------------------------------------------

class AMarineSSG : public AScriptedMarine
{
	DECLARE_STATELESS_ACTOR (AMarineSSG, AScriptedMarine)
};

IMPLEMENT_STATELESS_ACTOR (AMarineSSG, Doom, 9106, 0)
	PROP_SpawnState (S_MPLAYSTILL)
	PROP_MeleeState (255)
	PROP_MissileState (S_MPLAY_ATK_DSHOTGUN)
END_DEFAULTS

//---------------------------------------------------------------------------

class AMarineChaingun : public AScriptedMarine
{
	DECLARE_STATELESS_ACTOR (AMarineChaingun, AScriptedMarine)
};

IMPLEMENT_STATELESS_ACTOR (AMarineChaingun, Doom, 9107, 0)
	PROP_SpawnState (S_MPLAYSTILL)
	PROP_MeleeState (255)
	PROP_MissileState (S_MPLAY_ATK_CHAINGUN)
END_DEFAULTS

//---------------------------------------------------------------------------

class AMarineRocket : public AScriptedMarine
{
	DECLARE_STATELESS_ACTOR (AMarineRocket, AScriptedMarine)
};

IMPLEMENT_STATELESS_ACTOR (AMarineRocket, Doom, 9108, 0)
	PROP_SpawnState (S_MPLAYSTILL)
	PROP_MeleeState (255)
	PROP_MissileState (S_MPLAY_ATK_ROCKET)
END_DEFAULTS

//---------------------------------------------------------------------------

class AMarinePlasma : public AScriptedMarine
{
	DECLARE_STATELESS_ACTOR (AMarinePlasma, AScriptedMarine)
};

IMPLEMENT_STATELESS_ACTOR (AMarinePlasma, Doom, 9109, 0)
	PROP_SpawnState (S_MPLAYSTILL)
	PROP_MeleeState (255)
	PROP_MissileState (S_MPLAY_ATK_PLASMA)
END_DEFAULTS

//---------------------------------------------------------------------------

class AMarineRailgun : public AScriptedMarine
{
	DECLARE_STATELESS_ACTOR (AMarineRailgun, AScriptedMarine)
};

IMPLEMENT_STATELESS_ACTOR (AMarineRailgun, Doom, 9110, 0)
	PROP_SpawnState (S_MPLAYSTILL)
	PROP_MeleeState (255)
	PROP_MissileState (S_MPLAY_ATK_RAILGUN)
END_DEFAULTS

//---------------------------------------------------------------------------

class AMarineBFG : public AScriptedMarine
{
	DECLARE_STATELESS_ACTOR (AMarineBFG, AScriptedMarine)
};

IMPLEMENT_STATELESS_ACTOR (AMarineBFG, Doom, 9111, 0)
	PROP_SpawnState (S_MPLAYSTILL)
	PROP_MeleeState (255)
	PROP_MissileState (S_MPLAY_ATK_BFG)
END_DEFAULTS

//---------------------------------------------------------------------------

void AScriptedMarine::SetWeapon (EMarineWeapon type)
{
	MissileState = NULL;
	MeleeState = NULL;
	DecalGenerator = NULL;

	switch (type)
	{
	default:
	case WEAPON_Dummy:
		MissileState = &States[S_MPLAY_ATK];
		DecalGenerator = GetDefaultByType (RUNTIME_CLASS(AScriptedMarine))->DecalGenerator;
		break;

	case WEAPON_Fist:
		MeleeState = &States[S_MPLAY_ATK_FIST];
		DecalGenerator = GetDefaultByType (RUNTIME_CLASS(AMarineFist))->DecalGenerator;
		break;

	case WEAPON_BerserkFist:
		MeleeState = &States[S_MPLAY_ATK_BERSERK];
		DecalGenerator = GetDefaultByType (RUNTIME_CLASS(AMarineBerserk))->DecalGenerator;
		break;

	case WEAPON_Chainsaw:
		MeleeState = &States[S_MPLAY_ATK_CHAINSAW];
		DecalGenerator = GetDefaultByType (RUNTIME_CLASS(AMarineChainsaw))->DecalGenerator;
		break;

	case WEAPON_Pistol:
		MissileState = &States[S_MPLAY_ATK_PISTOL];
		DecalGenerator = GetDefaultByType (RUNTIME_CLASS(AMarinePistol))->DecalGenerator;
		break;

	case WEAPON_Shotgun:
		MissileState = &States[S_MPLAY_ATK_SHOTGUN];
		DecalGenerator = GetDefaultByType (RUNTIME_CLASS(AMarineShotgun))->DecalGenerator;
		break;

	case WEAPON_SuperShotgun:
		MissileState = &States[S_MPLAY_ATK_DSHOTGUN];
		DecalGenerator = GetDefaultByType (RUNTIME_CLASS(AMarineSSG))->DecalGenerator;
		break;

	case WEAPON_Chaingun:
		MissileState = &States[S_MPLAY_ATK_CHAINGUN];
		DecalGenerator = GetDefaultByType (RUNTIME_CLASS(AMarineChaingun))->DecalGenerator;
		break;

	case WEAPON_RocketLauncher:
		MissileState = &States[S_MPLAY_ATK_ROCKET];
		DecalGenerator = GetDefaultByType (RUNTIME_CLASS(AMarineRocket))->DecalGenerator;
		break;

	case WEAPON_PlasmaRifle:
		MissileState = &States[S_MPLAY_ATK_PLASMA];
		DecalGenerator = GetDefaultByType (RUNTIME_CLASS(AMarinePlasma))->DecalGenerator;
		break;

	case WEAPON_Railgun:
		MissileState = &States[S_MPLAY_ATK_RAILGUN];
		DecalGenerator = GetDefaultByType (RUNTIME_CLASS(AMarineRailgun))->DecalGenerator;
		break;

	case WEAPON_BFG:
		MissileState = &States[S_MPLAY_ATK_BFG];
		DecalGenerator = GetDefaultByType (RUNTIME_CLASS(AMarineBFG))->DecalGenerator;
		break;
	}
}

void AScriptedMarine::SetSprite (const PClass *source)
{
	if (source == NULL || source->ActorInfo == NULL)
	{ // A valid actor class wasn't passed, so use the standard sprite
		SpriteOverride = sprite = States[0].sprite.index;
		// Copy the standard player's scaling
		AActor * playerdef = GetDefaultByName("DoomPlayer");
		if (playerdef == NULL) playerdef = GetDefaultByType(RUNTIME_CLASS(AScriptedMarine));
		scaleX = playerdef->scaleX;
		scaleY = playerdef->scaleY;
	}
	else
	{ // Use the same sprite the passed class spawns with
		SpriteOverride = sprite = GetDefaultByType (source)->SpawnState->sprite.index;
		scaleX = GetDefaultByType(source)->scaleX;
		scaleY = GetDefaultByType(source)->scaleY;
	}
}
