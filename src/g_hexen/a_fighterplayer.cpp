#include "actor.h"
#include "gi.h"
#include "m_random.h"
#include "s_sound.h"
#include "d_player.h"
#include "a_action.h"
#include "p_local.h"
#include "p_enemy.h"
#include "a_action.h"
#include "a_hexenglobal.h"

static FRandom pr_fpatk ("FPunchAttack");

void A_FSwordFlames (AActor *);

// The fighter --------------------------------------------------------------

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
	S_BRIGHT (FDTH, 'A',	5, A_FireScream				, &States[S_PLAY_F_FDTH+1]),
	S_BRIGHT (FDTH, 'B',	4, NULL 					, &States[S_PLAY_FDTH]),
};

IMPLEMENT_ACTOR (AFighterPlayer, Hexen, -1, 0)
	PROP_SpawnHealth (100)
	PROP_RadiusFixed (16)
	PROP_HeightFixed (64)
	PROP_Mass (100)
	PROP_PainChance (255)
	PROP_SpeedFixed (1)
	PROP_Flags (MF_SOLID|MF_SHOOTABLE|MF_DROPOFF|MF_PICKUP|MF_NOTDMATCH|MF_FRIENDLY)
	PROP_Flags2 (MF2_WINDTHRUST|MF2_FLOORCLIP|MF2_SLIDE|MF2_PASSMOBJ|MF2_TELESTOMP|MF2_PUSHWALL)
	PROP_Flags3 (MF3_NOBLOCKMONST)
	PROP_Flags4 (MF4_NOSKIN)

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
	player->ReadyWeapon = player->PendingWeapon = static_cast<AWeapon *>
		(GiveInventoryType (PClass::FindClass ("FWeapFist")));

	GiveInventoryType (RUNTIME_CLASS(AHexenArmor));
	AHexenArmor *armor = FindInventory<AHexenArmor>();
	armor->Slots[4] = 15*FRACUNIT;
	armor->SlotsIncrement[0] = 25*FRACUNIT;
	armor->SlotsIncrement[1] = 20*FRACUNIT;
	armor->SlotsIncrement[2] = 15*FRACUNIT;
	armor->SlotsIncrement[3] =  5*FRACUNIT;
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
		if (difference > 0)
		{
			pmo->angle += MAX_ANGLE_ADJUST;
		}
		else
		{
			pmo->angle -= MAX_ANGLE_ADJUST;
		}
	}
	else
	{
		pmo->angle = angle;
	}
}

// Fist (first weapon) ------------------------------------------------------

void A_FPunchAttack (AActor *actor);

class AFWeapFist : public AFighterWeapon
{
	DECLARE_ACTOR (AFWeapFist, AFighterWeapon)
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

IMPLEMENT_ACTOR (AFWeapFist, Hexen, -1, 0)
	PROP_Flags5 (MF5_BLOODSPLATTER)
	PROP_Weapon_SelectionOrder (3400)
	PROP_Weapon_Flags (WIF_BOT_MELEE)
	PROP_Weapon_UpState (S_PUNCHUP)
	PROP_Weapon_DownState (S_PUNCHDOWN)
	PROP_Weapon_ReadyState (S_PUNCHREADY)
	PROP_Weapon_AtkState (S_PUNCHATK1)
	PROP_Weapon_HoldAtkState (S_PUNCHATK1)
	PROP_Weapon_Kickback (150)
END_DEFAULTS

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
	PROP_Flags3 (MF3_PUFFONACTORS)
	PROP_RenderStyle (STYLE_Translucent)
	PROP_Alpha (HX_SHADOW)

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

//============================================================================
//
// A_FPunchAttack
//
//============================================================================

void A_FPunchAttack (AActor *actor)
{
	angle_t angle;
	int damage;
	int slope;
	fixed_t power;
	int i;
	player_t *player;
	const PClass *pufftype;

	if (NULL == (player = actor->player))
	{
		return;
	}
	APlayerPawn *pmo = player->mo;

	damage = 40+(pr_fpatk()&15);
	power = 2*FRACUNIT;
	pufftype = RUNTIME_CLASS(APunchPuff);
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
				pufftype = RUNTIME_CLASS(AHammerPuff);
			}
			P_LineAttack (pmo, angle, 2*MELEERANGE, slope, damage, MOD_HIT, pufftype);
			if (linetarget->flags3&MF3_ISMONSTER || linetarget->player)
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
				pufftype = RUNTIME_CLASS(AHammerPuff);
			}
			P_LineAttack (pmo, angle, 2*MELEERANGE, slope, damage, MOD_HIT, pufftype);
			if (linetarget->flags3&MF3_ISMONSTER || linetarget->player)
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
	P_LineAttack (pmo, angle, MELEERANGE, slope, damage, MOD_HIT, pufftype);

punchdone:
	if (pmo->special1 == 3)
	{
		pmo->special1 = 0;
		P_SetPsprite (player, ps_weapon, &AFWeapFist::States[S_PUNCHATK2]);
		S_Sound (pmo, CHAN_VOICE, "*fistgrunt", 1, ATTN_NORM);
	}
	return;		
}

void AFighterPlayer::TweakSpeeds (int &forward, int &side)
{
	if ((unsigned int)(forward + 0x31ff) < 0x63ff)
	{
		forward = forward * 0x1d / 0x19;
	}
	else
	{
		forward = forward * 0x3c / 0x32;
	}
	if ((unsigned int)(side + 0x27ff) < 0x4fff)
	{
		side = side * 0x1b / 0x18;
	}
	else
	{ // The fighter is a very fast strafer when running!
		side = side * 0x3b / 0x28;
	}
	Super::TweakSpeeds (forward, side);
}

fixed_t AFighterPlayer::GetJumpZ ()
{
	return FRACUNIT*39/4;	// ~9.75
}

// Radius armor boost
bool AFighterPlayer::DoHealingRadius (APlayerPawn *other)
{
	bool gotSome = false;

	for (int i = 0; i < 4; ++i)
	{
		AHexenArmor *armor = Spawn<AHexenArmor> (0,0,0);
		armor->health = i;
		armor->Amount = 1;
		if (!armor->TryPickup (player->mo))
		{
			armor->Destroy ();
		}
		else
		{
			gotSome = true;
		}
	}
	if (gotSome)
	{
		S_Sound (other, CHAN_AUTO, "MysticIncant", 1, ATTN_NORM);
		return true;
	}
	return false;
}

// Fighter Weapon Base Class ------------------------------------------------

IMPLEMENT_ABSTRACT_ACTOR (AFighterWeapon)

bool AFighterWeapon::TryPickup (AActor *toucher)
{
	// The Doom and Hexen players are not excluded from pickup in case
	// somebody wants to use these weapons with either of those games.
	if (toucher->IsKindOf (RUNTIME_CLASS(AClericPlayer)) ||
		toucher->IsKindOf (RUNTIME_CLASS(AMagePlayer)))
	{ // Wrong class, but try to pick up for mana
		if (ShouldStay())
		{ // Can't pick up weapons for other classes in coop netplay
			return false;
		}

		bool gaveSome = (NULL != AddAmmo (toucher, AmmoType1, AmmoGive1));
		gaveSome |= (NULL != AddAmmo (toucher, AmmoType2, AmmoGive2));
		if (gaveSome)
		{
			GoAwayAndDie ();
		}
		return gaveSome;
	}
	return Super::TryPickup (toucher);
}
