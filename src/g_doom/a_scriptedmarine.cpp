/*
#include "actor.h"
#include "p_enemy.h"
#include "a_action.h"
#include "m_random.h"
#include "p_local.h"
#include "a_doomglobal.h"
#include "s_sound.h"
#include "r_data/r_translate.h"
#include "thingdef/thingdef.h"
#include "g_level.h"
*/

#define MARINE_PAIN_CHANCE 160

static FRandom pr_m_refire ("SMarineRefire");
static FRandom pr_m_punch ("SMarinePunch");
static FRandom pr_m_gunshot ("SMarineGunshot");
static FRandom pr_m_saw ("SMarineSaw");
static FRandom pr_m_fireshotgun2 ("SMarineFireSSG");

IMPLEMENT_CLASS (AScriptedMarine)

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
	arc << CurrentWeapon;
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

bool AScriptedMarine::GetWeaponStates(int weap, FState *&melee, FState *&missile)
{
	static ENamedName WeaponNames[] = 
	{
		NAME_None,
		NAME_Fist,
		NAME_Berserk,
		NAME_Chainsaw,
		NAME_Pistol,
		NAME_Shotgun,
		NAME_SSG,
		NAME_Chaingun,
		NAME_Rocket,
		NAME_Plasma,
		NAME_Railgun,
		NAME_BFG
	};

	if (weap < WEAPON_Dummy || weap > WEAPON_BFG) weap = WEAPON_Dummy;

	melee = FindState(NAME_Melee, WeaponNames[weap], true);
	missile = FindState(NAME_Missile, WeaponNames[weap], true);

	return melee != NULL || missile != NULL;
}

void AScriptedMarine::BeginPlay ()
{
	Super::BeginPlay ();

	// Set the current weapon
	for(int i=WEAPON_Dummy; i<=WEAPON_BFG; i++)
	{
		FState *melee, *missile;
		if (GetWeaponStates(i, melee, missile))
		{
			if (melee == MeleeState && missile == MissileState)
			{
				CurrentWeapon = i;
			}
		}
	}
}

void AScriptedMarine::Tick ()
{
	Super::Tick ();

	// Override the standard sprite, if desired
	if (SpriteOverride != 0 && sprite == SpawnState->sprite)
	{
		sprite = SpriteOverride;
	}

	if (special1 != 0)
	{
		if (CurrentWeapon == WEAPON_SuperShotgun)
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

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_M_Refire)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_BOOL_OPT(ignoremissile)	{ ignoremissile = false; }

	if (self->target == NULL || self->target->health <= 0)
	{
		if (self->MissileState && pr_m_refire() < 160)
		{ // Look for a new target most of the time
			if (P_LookForPlayers (self, true, NULL) && P_CheckMissileRange (self))
			{ // Found somebody new and in range, so don't stop shooting
				return 0;
			}
		}
		self->SetState (self->state + 1);
		return 0;
	}
	if (((ignoremissile || self->MissileState == NULL) && !self->CheckMeleeRange ()) ||
		!P_CheckSight (self, self->target) ||
		pr_m_refire() < 4)	// Small chance of stopping even when target not dead
	{
		self->SetState (self->state + 1);
	}
	return 0;
}

//============================================================================
//
// A_M_SawRefire
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_M_SawRefire)
{
	PARAM_ACTION_PROLOGUE;

	if (self->target == NULL || self->target->health <= 0)
	{
		self->SetState (self->state + 1);
		return 0;
	}
	if (!self->CheckMeleeRange ())
	{
		self->SetState (self->state + 1);
	}
	return 0;
}

//============================================================================
//
// A_MarineNoise
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_MarineNoise)
{
	PARAM_ACTION_PROLOGUE;

	if (static_cast<AScriptedMarine *>(self)->CurrentWeapon == AScriptedMarine::WEAPON_Chainsaw)
	{
		S_Sound (self, CHAN_WEAPON, "weapons/sawidle", 1, ATTN_NORM);
	}
	return 0;
}

//============================================================================
//
// A_MarineChase
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_MarineChase)
{
	PARAM_ACTION_PROLOGUE;
	CALL_ACTION(A_MarineNoise, self);
	A_Chase (stack, self);
	return 0;
}

//============================================================================
//
// A_MarineLook
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_MarineLook)
{
	PARAM_ACTION_PROLOGUE;
	CALL_ACTION(A_MarineNoise, self);
	CALL_ACTION(A_Look, self);
	return 0;
}

//============================================================================
//
// A_M_Saw
//
//============================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_M_Saw)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_SOUND_OPT	(fullsound)			{ fullsound = "weapons/sawfull"; }
	PARAM_SOUND_OPT	(hitsound)			{ hitsound = "weapons/sawhit"; }
	PARAM_INT_OPT	(damage)			{ damage = 2; }
	PARAM_CLASS_OPT	(pufftype, AActor)	{ pufftype = NULL; }

	if (self->target == NULL)
		return 0;

	if (pufftype == NULL)
	{
		pufftype = PClass::FindActor(NAME_BulletPuff);
	}
	if (damage == 0)
	{
		damage = 2;
	}

	A_FaceTarget (self);
	if (self->CheckMeleeRange ())
	{
		DAngle angle;
		FTranslatedLineTarget t;

		damage *= (pr_m_saw()%10+1);
		angle = self->Angles.Yaw + pr_m_saw.Random2() * (5.625 / 256);
		
		P_LineAttack (self, angle, SAWRANGE,
					P_AimLineAttack (self, angle, SAWRANGE), damage,
					NAME_Melee, pufftype, false, &t);

		if (!t.linetarget)
		{
			S_Sound (self, CHAN_WEAPON, fullsound, 1, ATTN_NORM);
			return 0;
		}
		S_Sound (self, CHAN_WEAPON, hitsound, 1, ATTN_NORM);
			
		// turn to face target
		angle = t.angleFromSource;
		DAngle anglediff = deltaangle(self->Angles.Yaw, angle);

		if (anglediff < 0.0)
		{
			if (anglediff < -4.5)
				self->Angles.Yaw = angle + 90.0 / 21;
			else
				self->Angles.Yaw -= 4.5;
		}
		else
		{
			if (anglediff > 4.5)
				self->Angles.Yaw = angle - 90.0 / 21;
			else
				self->Angles.Yaw += 4.5;
		}
	}
	else
	{
		S_Sound (self, CHAN_WEAPON, fullsound, 1, ATTN_NORM);
	}
	//A_Chase (self);
	return 0;
}

//============================================================================
//
// A_M_Punch
//
//============================================================================

static void MarinePunch(AActor *self, int damagemul)
{
	DAngle 	angle;
	int 		damage;
	DAngle 		pitch;
	FTranslatedLineTarget t;

	if (self->target == NULL)
		return;

	damage = ((pr_m_punch()%10+1) << 1) * damagemul;

	A_FaceTarget (self);
	angle = self->Angles.Yaw + pr_m_punch.Random2() * (5.625 / 256);
	pitch = P_AimLineAttack (self, angle, MELEERANGE);
	P_LineAttack (self, angle, MELEERANGE, pitch, damage, NAME_Melee, NAME_BulletPuff, true, &t);

	// turn to face target
	if (t.linetarget)
	{
		S_Sound (self, CHAN_WEAPON, "*fist", 1, ATTN_NORM);
		self->Angles.Yaw = t.angleFromSource;
	}
}

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_M_Punch)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_INT(mult);

	MarinePunch(self, mult);
	return 0;
}

//============================================================================
//
// P_GunShot2
//
//============================================================================

void P_GunShot2 (AActor *mo, bool accurate, DAngle pitch, PClassActor *pufftype)
{
	DAngle 	angle;
	int 		damage;
		
	damage = 5*(pr_m_gunshot()%3+1);
	angle = mo->Angles.Yaw;

	if (!accurate)
	{
		angle += pr_m_gunshot.Random2() * (5.625 / 256);
	}

	P_LineAttack (mo, angle, MISSILERANGE, pitch, damage, NAME_Hitscan, pufftype);
}

//============================================================================
//
// A_M_FirePistol
//
//============================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_M_FirePistol)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_BOOL(accurate);

	if (self->target == NULL)
		return 0;

	S_Sound (self, CHAN_WEAPON, "weapons/pistol", 1, ATTN_NORM);
	A_FaceTarget (self);
	P_GunShot2 (self, accurate, P_AimLineAttack (self, self->Angles.Yaw, MISSILERANGE),
		PClass::FindActor(NAME_BulletPuff));
	return 0;
}

//============================================================================
//
// A_M_FireShotgun
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_M_FireShotgun)
{
	PARAM_ACTION_PROLOGUE;

	DAngle pitch;

	if (self->target == NULL)
		return 0;

	S_Sound (self, CHAN_WEAPON,  "weapons/shotgf", 1, ATTN_NORM);
	A_FaceTarget (self);
	pitch = P_AimLineAttack (self, self->Angles.Yaw, MISSILERANGE);
	for (int i = 0; i < 7; ++i)
	{
		P_GunShot2 (self, false, pitch, PClass::FindActor(NAME_BulletPuff));
	}
	self->special1 = level.maptime + 27;
	return 0;
}

//============================================================================
//
// A_M_CheckAttack
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_M_CheckAttack)
{
	PARAM_ACTION_PROLOGUE;

	if (self->special1 != 0 || self->target == NULL)
	{
		self->SetState (self->FindState("SkipAttack"));
	}
	else
	{
		A_FaceTarget (self);
	}
	return 0;
}

//============================================================================
//
// A_M_FireShotgun2
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_M_FireShotgun2)
{
	PARAM_ACTION_PROLOGUE;

	DAngle pitch;

	if (self->target == NULL)
		return 0;

	S_Sound (self, CHAN_WEAPON, "weapons/sshotf", 1, ATTN_NORM);
	A_FaceTarget (self);
	pitch = P_AimLineAttack (self, self->Angles.Yaw, MISSILERANGE);
	for (int i = 0; i < 20; ++i)
	{
		int damage = 5*(pr_m_fireshotgun2()%3+1);
		DAngle angle = self->Angles.Yaw + pr_m_fireshotgun2.Random2() * (11.25 / 256);

		P_LineAttack (self, angle, MISSILERANGE,
					  pitch + pr_m_fireshotgun2.Random2() * (7.097 / 256), damage,
					  NAME_Hitscan, NAME_BulletPuff);
	}
	self->special1 = level.maptime;
	return 0;
}

//============================================================================
//
// A_M_FireCGun
//
//============================================================================

DEFINE_ACTION_FUNCTION_PARAMS(AActor, A_M_FireCGun)
{
	PARAM_ACTION_PROLOGUE;
	PARAM_BOOL(accurate);

	if (self->target == NULL)
		return 0;

	S_Sound (self, CHAN_WEAPON, "weapons/chngun", 1, ATTN_NORM);
	A_FaceTarget (self);
	P_GunShot2 (self, accurate, P_AimLineAttack (self, self->Angles.Yaw, MISSILERANGE),
		PClass::FindActor(NAME_BulletPuff));
	return 0;
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

DEFINE_ACTION_FUNCTION(AActor, A_M_FireMissile)
{
	PARAM_ACTION_PROLOGUE;

	if (self->target == NULL)
		return 0;

	if (self->CheckMeleeRange ())
	{ // If too close, punch it
		MarinePunch(self, 1);
	}
	else
	{
		A_FaceTarget (self);
		P_SpawnMissile (self, self->target, PClass::FindActor("Rocket"));
	}
	return 0;
}

//============================================================================
//
// A_M_FireRailgun
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_M_FireRailgun)
{
	PARAM_ACTION_PROLOGUE;

	if (self->target == NULL)
		return 0;

	CALL_ACTION(A_MonsterRail, self);
	self->special1 = level.maptime + 50;
	return 0;
}

//============================================================================
//
// A_M_FirePlasma
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_M_FirePlasma)
{
	PARAM_ACTION_PROLOGUE;

	if (self->target == NULL)
		return 0;

	A_FaceTarget (self);
	P_SpawnMissile (self, self->target, PClass::FindActor("PlasmaBall"));
	self->special1 = level.maptime + 20;
	return 0;
}

//============================================================================
//
// A_M_BFGsound
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_M_BFGsound)
{
	PARAM_ACTION_PROLOGUE;

	if (self->target == NULL)
		return 0;

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
	return 0;
}

//============================================================================
//
// A_M_FireBFG
//
//============================================================================

DEFINE_ACTION_FUNCTION(AActor, A_M_FireBFG)
{
	PARAM_ACTION_PROLOGUE;

	if (self->target == NULL)
		return 0;

	A_FaceTarget (self);
	P_SpawnMissile (self, self->target, PClass::FindActor("BFGBall"));
	self->special1 = level.maptime + 30;
	self->PainChance = MARINE_PAIN_CHANCE;
	return 0;
}

//---------------------------------------------------------------------------

void AScriptedMarine::SetWeapon (EMarineWeapon type)
{
	if (GetWeaponStates(type, MeleeState, MissileState))
	{
		static const char *classes[] = {
			"ScriptedMarine",
			"MarineFist",
			"MarineBerserk",
			"MarineChainsaw",
			"MarinePistol",
			"MarineShotgun",
			"MarineSSG",
			"MarineChaingun",
			"MarineRocket",
			"MarinePlasma",
			"MarineRailgun",
			"MarineBFG"
		};

		const PClass *cls = PClass::FindClass(classes[type]);
		if (cls != NULL)
			DecalGenerator = GetDefaultByType(cls)->DecalGenerator;
		else 
			DecalGenerator = NULL;
	}
}

void AScriptedMarine::SetSprite (PClassActor *source)
{
	if (source == NULL)
	{ // A valid actor class wasn't passed, so use the standard sprite
		SpriteOverride = sprite = GetClass()->OwnedStates[0].sprite;
		// Copy the standard scaling
		Scale = GetDefault()->Scale;
	}
	else
	{ // Use the same sprite and scaling the passed class spawns with
		SpriteOverride = sprite = GetDefaultByType (source)->SpawnState->sprite;
		Scale = GetDefaultByType(source)->Scale;
	}
}
