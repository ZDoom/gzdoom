#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"

void A_Pain (AActor *);
void A_PlayerScream (AActor *);
void A_SkullPop (AActor *);
void A_CheckBurnGone (AActor *);

// The fighter --------------------------------------------------------------

class AFighterPlayer : public APlayerPawn
{
	DECLARE_ACTOR (AFighterPlayer, APlayerPawn)
public:
	void PlayAttacking2 ();
	void GiveDefaultInventory ();
	const char *GetSoundClass ();
	fixed_t GetJumpZ () { return 9*FRACUNIT; }
};

FState AFighterPlayer::States[] =
{
#define S_FPLAY 0
	S_NORMAL (PLAY, 'A',   -1, NULL 					, NULL),

#define S_FPLAY_RUN (S_FPLAY+1)
	S_NORMAL (PLAY, 'A',	4, NULL 					, &States[S_FPLAY_RUN+1]),
	S_NORMAL (PLAY, 'B',	4, NULL 					, &States[S_FPLAY_RUN+2]),
	S_NORMAL (PLAY, 'C',	4, NULL 					, &States[S_FPLAY_RUN+3]),
	S_NORMAL (PLAY, 'D',	4, NULL 					, &States[S_FPLAY_RUN+0]),

#define S_FPLAY_ATK (S_FPLAY_RUN+4)
	S_NORMAL (PLAY, 'E',	8, NULL 					, &States[S_FPLAY_ATK+1]),
	S_NORMAL (PLAY, 'F',	8, NULL 					, &States[S_FPLAY]),

#define S_FPLAY_PAIN (S_FPLAY_ATK+2)
	S_NORMAL (PLAY, 'G',	4, NULL 					, &States[S_FPLAY_PAIN+1]),
	S_NORMAL (PLAY, 'G',	4, A_Pain					, &States[S_FPLAY]),

#define S_FPLAY_DIE (S_FPLAY_PAIN+2)
	S_NORMAL (PLAY, 'H',	6, NULL 					, &States[S_FPLAY_DIE+1]),
	S_NORMAL (PLAY, 'I',	6, A_PlayerScream 			, &States[S_FPLAY_DIE+2]),
	S_NORMAL (PLAY, 'J',	6, NULL 					, &States[S_FPLAY_DIE+3]),
	S_NORMAL (PLAY, 'K',	6, NULL 					, &States[S_FPLAY_DIE+4]),
	S_NORMAL (PLAY, 'L',	6, A_NoBlocking 			, &States[S_FPLAY_DIE+5]),
	S_NORMAL (PLAY, 'M',	6, NULL 					, &States[S_FPLAY_DIE+6]),
	S_NORMAL (PLAY, 'N',   -1, NULL/*A_AddPlayerCorpse*/, NULL),

#define S_FPLAY_XDIE (S_FPLAY_DIE+7)
	S_NORMAL (PLAY, 'O',	5, A_PlayerScream 			, &States[S_FPLAY_XDIE+1]),
	S_NORMAL (PLAY, 'P',	5, A_SkullPop				, &States[S_FPLAY_XDIE+2]),
	S_NORMAL (PLAY, 'R',	5, A_NoBlocking 			, &States[S_FPLAY_XDIE+3]),
	S_NORMAL (PLAY, 'S',	5, NULL 					, &States[S_FPLAY_XDIE+4]),
	S_NORMAL (PLAY, 'T',	5, NULL 					, &States[S_FPLAY_XDIE+5]),
	S_NORMAL (PLAY, 'U',	5, NULL 					, &States[S_FPLAY_XDIE+6]),
	S_NORMAL (PLAY, 'V',	5, NULL 					, &States[S_FPLAY_XDIE+7]),
	S_NORMAL (PLAY, 'W',   -1, NULL/*A_AddPlayerCorpse*/, NULL),

#define S_FPLAY_ICE (S_FPLAY_XDIE+8)
	S_NORMAL (PLAY, 'X',	5, A_FreezeDeath			, &States[S_FPLAY_ICE+1]),
	S_NORMAL (PLAY, 'X',	1, A_FreezeDeathChunks		, &States[S_FPLAY_ICE+1]),

#define S_PLAY_FDTH (S_FPLAY_ICE+2)
	S_BRIGHT (FDTH, 'G',	5, NULL						, &States[S_PLAY_FDTH+1]),
	S_BRIGHT (FDTH, 'H',	4, A_PlayerScream 			, &States[S_PLAY_FDTH+2]),
	S_BRIGHT (FDTH, 'I',	5, NULL 					, &States[S_PLAY_FDTH+3]),
	S_BRIGHT (FDTH, 'J',	4, NULL 					, &States[S_PLAY_FDTH+4]),
	S_BRIGHT (FDTH, 'K',	5, NULL 					, &States[S_PLAY_FDTH+5]),
	S_BRIGHT (FDTH, 'L',	4, NULL 					, &States[S_PLAY_FDTH+6]),
	S_BRIGHT (FDTH, 'M',	5, NULL 					, &States[S_PLAY_FDTH+7]),
	S_BRIGHT (FDTH, 'N',	4, NULL 					, &States[S_PLAY_FDTH+8]),
	S_BRIGHT (FDTH, 'O',	5, NULL 					, &States[S_PLAY_FDTH+9]),
	S_BRIGHT (FDTH, 'P',	4, NULL 					, &States[S_PLAY_FDTH+10]),
	S_BRIGHT (FDTH, 'Q',	5, NULL 					, &States[S_PLAY_FDTH+11]),
	S_BRIGHT (FDTH, 'R',	4, NULL 					, &States[S_PLAY_FDTH+12]),
	S_BRIGHT (FDTH, 'S',	5, A_NoBlocking 			, &States[S_PLAY_FDTH+13]),
	S_BRIGHT (FDTH, 'T',	4, NULL 					, &States[S_PLAY_FDTH+14]),
	S_BRIGHT (FDTH, 'U',	5, NULL 					, &States[S_PLAY_FDTH+15]),
	S_BRIGHT (FDTH, 'V',	4, NULL 					, &States[S_PLAY_FDTH+16]),
	S_NORMAL (ACLO, 'E',   35, A_CheckBurnGone			, &States[S_PLAY_FDTH+16]),
	S_NORMAL (ACLO, 'E',	8, NULL 					, NULL),

#define S_PLAY_F_FDTH (S_PLAY_FDTH+18)
	S_BRIGHT (FDTH, 'A',	5, NULL 					, &States[S_PLAY_F_FDTH+1]),
	S_BRIGHT (FDTH, 'B',	4, NULL 					, &States[S_PLAY_FDTH+2]),

#define S_PLAY_C_FDTH (S_PLAY_F_FDTH+2)
	S_BRIGHT (FDTH, 'C',	5, NULL 					, &States[S_PLAY_C_FDTH+1]),
	S_BRIGHT (FDTH, 'D',	4, NULL 					, &States[S_PLAY_FDTH+2]),

#define S_PLAY_M_FDTH (S_PLAY_C_FDTH+2)
	S_BRIGHT (FDTH, 'E',	5, NULL 					, &States[S_PLAY_M_FDTH+1]),
	S_BRIGHT (FDTH, 'F',	4, NULL 					, &States[S_PLAY_FDTH+2])
};

IMPLEMENT_ACTOR (AFighterPlayer, Hexen, -1, 0)
	PROP_SpawnHealth (100)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (64)
	PROP_Mass (100)
	PROP_PainChance (255)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_DROPOFF|MF_PICKUP|MF_NOTDMATCH)
	PROP_Flags2 (MF2_WINDTHRUST|MF2_FLOORCLIP|MF2_SLIDE|MF2_PASSMOBJ|MF2_TELESTOMP|MF2_PUSHWALL)
	PROP_Flags3 (MF3_NOBLOCKMONST)

	PROP_SpawnState (S_FPLAY)
	PROP_SeeState (S_FPLAY_RUN)
	PROP_PainState (S_FPLAY_PAIN)
	PROP_MissileState (S_FPLAY_ATK)
	PROP_DeathState (S_FPLAY_DIE)
	PROP_XDeathState (S_FPLAY_XDIE)
	PROP_BDeathState (S_PLAY_F_FDTH)
	PROP_IDeathState (S_FPLAY_ICE)

	PROP_PainSound ("PlayerFighterPain")
END_DEFAULTS

const char *AFighterPlayer::GetSoundClass ()
{
	if (player == NULL || player->userinfo.skin == 0)
	{
		return "fighter";
	}
	return Super::GetSoundClass ();
}
void AFighterPlayer::PlayAttacking2 ()
{
	SetState (MissileState);
}

void AFighterPlayer::GiveDefaultInventory ()
{
	player->health = GetDefault()->health;
	player->readyweapon = player->pendingweapon = wp_ffist;
	player->weaponowned[wp_ffist] = true;
}

// --- Fighter Weapons ------------------------------------------------------

//============================================================================
//
//	AdjustPlayerAngle
//
//============================================================================

#define MAX_ANGLE_ADJUST (5*ANGLE_1)

void AdjustPlayerAngle (AActor *pmo)
{
	angle_t angle;
	int difference;

	angle = R_PointToAngle2 (pmo->x, pmo->y, linetarget->x, linetarget->y);
	difference = (int)angle - (int)pmo->angle;
	if (abs(difference) > MAX_ANGLE_ADJUST)
	{
		pmo->angle += difference > 0 ? MAX_ANGLE_ADJUST : -MAX_ANGLE_ADJUST;
	}
	else
	{
		pmo->angle = angle;
	}
}

// Fist (first weapon) ------------------------------------------------------

void A_FPunchAttack (player_t *, pspdef_t *);

class AFWeapFist : public AWeapon
{
	DECLARE_ACTOR (AFWeapFist, AWeapon)
	AT_GAME_SET_FRIEND (FWeapFist)
private:
	static FWeaponInfo WeaponInfo;
};

FState AFWeapFist::States[] =
{
#define S_PUNCHREADY 0
	S_NORMAL (FPCH, 'A',	1, A_WeaponReady			, &States[S_PUNCHREADY]),

#define S_PUNCHDOWN (S_PUNCHREADY+1)
	S_NORMAL (FPCH, 'A',	1, A_Lower					, &States[S_PUNCHDOWN]),

#define S_PUNCHUP (S_PUNCHDOWN+1)
	S_NORMAL (FPCH, 'A',	1, A_Raise					, &States[S_PUNCHUP]),

#define S_PUNCHATK1 (S_PUNCHUP+1)
	S_NORMAL2(FPCH, 'B',	5, NULL 					, &States[S_PUNCHATK1+1], 5, 40),
	S_NORMAL2(FPCH, 'C',	4, NULL 					, &States[S_PUNCHATK1+2], 5, 40),
	S_NORMAL2(FPCH, 'D',	4, A_FPunchAttack			, &States[S_PUNCHATK1+3], 5, 40),
	S_NORMAL2(FPCH, 'C',	4, NULL 					, &States[S_PUNCHATK1+4], 5, 40),
	S_NORMAL2(FPCH, 'B',	5, A_ReFire 				, &States[S_PUNCHREADY], 5, 40),

#define S_PUNCHATK2 (S_PUNCHATK1+5)
	S_NORMAL2(FPCH, 'D',	4, NULL 					, &States[S_PUNCHATK2+1], 5, 40),
	S_NORMAL2(FPCH, 'E',	4, NULL 					, &States[S_PUNCHATK2+2], 5, 40),
	S_NORMAL2(FPCH, 'E',	1, NULL 					, &States[S_PUNCHATK2+3], 15, 50),
	S_NORMAL2(FPCH, 'E',	1, NULL 					, &States[S_PUNCHATK2+4], 25, 60),
	S_NORMAL2(FPCH, 'E',	1, NULL 					, &States[S_PUNCHATK2+5], 35, 70),
	S_NORMAL2(FPCH, 'E',	1, NULL 					, &States[S_PUNCHATK2+6], 45, 80),
	S_NORMAL2(FPCH, 'E',	1, NULL 					, &States[S_PUNCHATK2+7], 55, 90),
	S_NORMAL2(FPCH, 'E',	1, NULL 					, &States[S_PUNCHATK2+8], 65, 100),
	S_NORMAL2(FPCH, 'E',   10, NULL 					, &States[S_PUNCHREADY], 0, 150)
};

FWeaponInfo AFWeapFist::WeaponInfo =
{
	0,
	MANA_NONE,
	0,
	0,
	&States[S_PUNCHUP],
	&States[S_PUNCHDOWN],
	&States[S_PUNCHREADY],
	&States[S_PUNCHATK1],
	&States[S_PUNCHATK1],
	NULL,
	NULL,
	150,
	0,
	NULL,
	NULL,
	RUNTIME_CLASS(AFWeapFist)
};

IMPLEMENT_ACTOR (AFWeapFist, Hexen, -1, 0)
END_DEFAULTS

AT_GAME_SET (FWeapFist)
{
	wpnlev1info[wp_ffist] = wpnlev2info[wp_ffist] = &AFWeapFist::WeaponInfo;
}

// Punch puff ---------------------------------------------------------------

class APunchPuff : public AActor
{
	DECLARE_ACTOR (APunchPuff, AActor)
public:
	void BeginPlay ();
};

FState APunchPuff::States[] =
{
	S_NORMAL (FHFX, 'S',	4, NULL 					, &States[1]),
	S_NORMAL (FHFX, 'T',	4, NULL 					, &States[2]),
	S_NORMAL (FHFX, 'U',	4, NULL 					, &States[3]),
	S_NORMAL (FHFX, 'V',	4, NULL 					, &States[4]),
	S_NORMAL (FHFX, 'W',	4, NULL 					, NULL)
};

IMPLEMENT_ACTOR (APunchPuff, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)
	PROP_Mass (5)

	PROP_SpawnState (0)

	PROP_SeeSound ("FighterPunchHitThing")
	PROP_AttackSound ("FighterPunchHitWall")
	PROP_ActiveSound ("FighterPunchMiss")
END_DEFAULTS

void APunchPuff::BeginPlay ()
{
	Super::BeginPlay ();
	momz = FRACUNIT;
}

// Hammer puff (also used by fist) ------------------------------------------

class AHammerPuff : public AActor
{
	DECLARE_ACTOR (AHammerPuff, AActor)
public:
	void BeginPlay ();
};

FState AHammerPuff::States[] =
{
	S_NORMAL (FHFX, 'S',	4, NULL 					, &States[1]),
	S_NORMAL (FHFX, 'T',	4, NULL 					, &States[2]),
	S_NORMAL (FHFX, 'U',	4, NULL 					, &States[3]),
	S_NORMAL (FHFX, 'V',	4, NULL 					, &States[4]),
	S_NORMAL (FHFX, 'W',	4, NULL 					, NULL),
};

IMPLEMENT_ACTOR (AHammerPuff, Hexen, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)
	PROP_Mass (5)

	PROP_SpawnState (0)

	PROP_SeeSound ("FighterHammerHitThing")
	PROP_AttackSound ("FighterHammmerHitWall")
	PROP_ActiveSound ("FighterHammerMiss")
END_DEFAULTS

void AHammerPuff::BeginPlay ()
{
	Super::BeginPlay ();
	momz = FRACUNIT*8/10;
}

//============================================================================
//
// A_FPunchAttack
//
//============================================================================

void A_FPunchAttack (player_t *player, pspdef_t *psp)
{
	angle_t angle;
	int damage;
	int slope;
	APlayerPawn *pmo = player->mo;
	fixed_t power;
	int i;

	damage = 40+(P_Random()&15);
	power = 2*FRACUNIT;
	PuffType = RUNTIME_CLASS(APunchPuff);
	for (i = 0; i < 16; i++)
	{
		angle = pmo->angle + i*(ANG45/16);
		slope = P_AimLineAttack (pmo, angle, 2*MELEERANGE);
		if (linetarget)
		{
			pmo->special1++;
			if (pmo->special1 == 3)
			{
				damage <<= 1;
				power = 6*FRACUNIT;
				PuffType = RUNTIME_CLASS(AHammerPuff);
			}
			P_LineAttack (pmo, angle, 2*MELEERANGE, slope, damage);
			if (linetarget->flags&MF_COUNTKILL || linetarget->player)
			{
				P_ThrustMobj (linetarget, angle, power);
			}
			AdjustPlayerAngle (pmo);
			goto punchdone;
		}
		angle = pmo->angle-i * (ANG45/16);
		slope = P_AimLineAttack (pmo, angle, 2*MELEERANGE);
		if (linetarget)
		{
			pmo->special1++;
			if (pmo->special1 == 3)
			{
				damage <<= 1;
				power = 6*FRACUNIT;
				PuffType = RUNTIME_CLASS(AHammerPuff);
			}
			P_LineAttack (pmo, angle, 2*MELEERANGE, slope, damage);
			if (linetarget->flags&MF_COUNTKILL || linetarget->player)
			{
				P_ThrustMobj (linetarget, angle, power);
			}
			AdjustPlayerAngle (pmo);
			goto punchdone;
		}
	}
	// didn't find any creatures, so try to strike any walls
	pmo->special1 = 0;

	angle = pmo->angle;
	slope = P_AimLineAttack (pmo, angle, MELEERANGE);
	P_LineAttack (pmo, angle, MELEERANGE, slope, damage);

punchdone:
	if (pmo->special1 == 3)
	{
		pmo->special1 = 0;
		P_SetPsprite (player, ps_weapon, &AFWeapFist::States[S_PUNCHATK2]);
		S_Sound (pmo, CHAN_VOICE, "FighterGrunt", 1, ATTN_NORM);
	}
	return;		
}

// Axe (second weapon) ------------------------------------------------------

#if 0
class AFWeapAxe : public AWeapon
{
	DECLARE_ACTOR (AFWeapAxe, AWeapon)
public:
	bool TryPickup (AActor *toucher)
	{
		return P_GiveWeapon
protected:

FState AFWeapAxe::States[] =
{
#define S_AXE 0
	S_NORMAL (WFAX, 'A',   -1, NULL 					, NULL),

#define S_FAXEREADY (S_AXE+1)
	S_NORMAL (FAXE, 'A',	1, A_WeaponReady			, &States[S_FAXEREADY]),

#define S_FAXEDOWN (S_FAXEREADY+1)
	S_NORMAL (FAXE, 'A',	1, A_Lower					, &States[S_FAXEDOWN]),

#define S_FAXEUP (S_FAXEDOWN+1)
	S_NORMAL (FAXE, 'A',	1, A_Raise					, &States[S_FAXEUP]),

#define S_FAXEATK (S_FAXEUP+1)
	S_NORMAL2(FAXE, 'B',	4, NULL 					, &States[S_FAXEATK+1], 15, 32),
	S_NORMAL2(FAXE, 'C',	3, NULL 					, &States[S_FAXEATK+2], 15, 32),
	S_NORMAL2(FAXE, 'D',	2, NULL 					, &States[S_FAXEATK+3], 15, 32),
	S_NORMAL2(FAXE, 'D',	1, A_FAxeAttack 			, &States[S_FAXEATK+4], -5, 70),
	S_NORMAL2(FAXE, 'D',	2, NULL 					, &States[S_FAXEATK+5], -25, 90),
	S_NORMAL2(FAXE, 'E',	1, NULL 					, &States[S_FAXEATK+6], 15, 32),
	S_NORMAL2(FAXE, 'E',	2, NULL 					, &States[S_FAXEATK+7], 10, 54),
	S_NORMAL2(FAXE, 'E',	7, NULL 					, &States[S_FAXEATK+8], 10, 150),
	S_NORMAL2(FAXE, 'A',	1, A_ReFire 				, &States[S_FAXEATK+9], 0, 60),
	S_NORMAL2(FAXE, 'A',	1, NULL 					, &States[S_FAXEATK+10], 0, 52),
	S_NORMAL2(FAXE, 'A',	1, NULL 					, &States[S_FAXEATK+11], 0, 44),
	S_NORMAL2(FAXE, 'A',	1, NULL 					, &States[S_FAXEATK+12], 0, 36),
	S_NORMAL (FAXE, 'A',	1, NULL 					, &States[S_FAXEREADY]),

#define S_FAXEREADY_G (S_FAXEATK+13)
	S_NORMAL (FAXE, 'L',	1, A_WeaponReady			, &States[S_FAXEREADY_G+0]),
	S_NORMAL (FAXE, 'L',	1, A_WeaponReady			, &States[S_FAXEREADY_G+1]),
	S_NORMAL (FAXE, 'L',	1, A_WeaponReady			, &States[S_FAXEREADY_G+2]),
	S_NORMAL (FAXE, 'M',	1, A_WeaponReady			, &States[S_FAXEREADY_G+3]),
	S_NORMAL (FAXE, 'M',	1, A_WeaponReady			, &States[S_FAXEREADY_G+4]),
	S_NORMAL (FAXE, 'M',	1, A_WeaponReady			, &States[S_FAXEREADY_G]),

#define S_FAXEDOWN_G (S_FAXEREADY_G+6)
	S_NORMAL (FAXE, 'L',	1, A_Lower					, &States[S_FAXEDOWN_G]),

#define S_FAXEUP_G (S_FAXEDOWN_G+1)
	S_NORMAL (FAXE, 'L',	1, A_Raise					, &States[S_FAXEUP_G]),

#define S_FAXEATK_G (S_FAXEUP_G+1)
	S_NORMAL2(FAXE, 'N',	4, NULL 					, &States[S_FAXEATK_G+1], 15, 32),
	S_NORMAL2(FAXE, 'O',	3, NULL 					, &States[S_FAXEATK_G+2], 15, 32),
	S_NORMAL2(FAXE, 'P',	2, NULL 					, &States[S_FAXEATK_G+3], 15, 32),
	S_NORMAL2(FAXE, 'P',	1, A_FAxeAttack 			, &States[S_FAXEATK_G+4], -5, 70),
	S_NORMAL2(FAXE, 'P',	2, NULL 					, &States[S_FAXEATK_G+5], -25, 90),
	S_NORMAL2(FAXE, 'Q',	1, NULL 					, &States[S_FAXEATK_G+6], 15, 32),
	S_NORMAL2(FAXE, 'Q',	2, NULL 					, &States[S_FAXEATK_G+7], 10, 54),
	S_NORMAL2(FAXE, 'Q',	7, NULL 					, &States[S_FAXEATK_G+8], 10, 150),
	S_NORMAL2(FAXE, 'A',	1, A_ReFire 				, &States[S_FAXEATK_G+9], 0, 60),
	S_NORMAL2(FAXE, 'A',	1, NULL 					, &States[S_FAXEATK_G+10], 0, 52),
	S_NORMAL2(FAXE, 'A',	1, NULL 					, &States[S_FAXEATK_G+11], 0, 44),
	S_NORMAL2(FAXE, 'A',	1, NULL 					, &States[S_FAXEATK_G+12], 0, 36),
	S_NORMAL (FAXE, 'A',	1, NULL 					, &States[S_FAXEREADY_G])
};

FState AAxePuffGlow::States[] =
{
#define S_AXEPUFF_GLOW 0
	S_BRIGHT (FAXE, 'R',	4, NULL 					, &States[S_AXEPUFF_GLOW+1]),
	S_BRIGHT (FAXE, 'S',	4, NULL 					, &States[S_AXEPUFF_GLOW+2]),
	S_BRIGHT (FAXE, 'T',	4, NULL 					, &States[S_AXEPUFF_GLOW+3]),
	S_BRIGHT (FAXE, 'U',	4, NULL 					, &States[S_AXEPUFF_GLOW+4]),
	S_BRIGHT (FAXE, 'V',	4, NULL 					, &States[S_AXEPUFF_GLOW+5]),
	S_BRIGHT (FAXE, 'W',	4, NULL 					, &States[S_AXEPUFF_GLOW+6]),
	S_BRIGHT (FAXE, 'X',	4, NULL 					, NULL)
};

FState AAxeBlood::States[] =
{
#define S_AXEBLOOD 0
	S_NORMAL (FAXE, 'F',	3, NULL 					, &States[S_AXEBLOOD+1]),
	S_NORMAL (FAXE, 'G',	3, NULL 					, &States[S_AXEBLOOD+2]),
	S_NORMAL (FAXE, 'H',	3, NULL 					, &States[S_AXEBLOOD+3]),
	S_NORMAL (FAXE, 'I',	3, NULL 					, &States[S_AXEBLOOD+4]),
	S_NORMAL (FAXE, 'J',	3, NULL 					, &States[S_AXEBLOOD+5]),
	S_NORMAL (FAXE, 'K',	3, NULL 					, NULL)
};

void AAxePuff::SetDefaults (FActorInfo *info)
{
	INHERIT_DEFS;
	info->activesound = "FighterHammerMiss";
}

void AAxePuffGlow::SetDefaults (FActorInfo *info)
{
	INHERIT_DEFS;
	info->activesound = "FighterHammerMiss";
}

void AFlamePuff::SetDefaults (FActorInfo *info)
{
	INHERIT_DEFS;
	info->flags3 = MF3_ALWAYSPUFF;
}
#endif
//===========================================================================
//
//  P_BloodSplatter2
//
//===========================================================================

void P_BloodSplatter2 (fixed_t x, fixed_t y, fixed_t z, AActor *originator)
{
#if 0
	AActor *mo;

	mo = Spawn<AAxeBlood> (x+((P_Random()-128)<<11), y+((P_Random()-128)<<11), z);
	mo->target = originator;
#endif
}
