#include "info.h"
#include "a_pickups.h"
#include "d_player.h"
#include "p_local.h"
#include "c_dispatch.h"
#include "gi.h"
#include "s_sound.h"
#include "p_local.h"
#include "p_spec.h"
#include "p_lnspec.h"
#include "p_enemy.h"
#include "p_effect.h"
#include "a_artifacts.h"
#include "sbar.h"
#include "d_player.h"
#include "m_random.h"
#include "v_video.h"
#include "templates.h"

static FRandom pr_torch ("Torch");

#define	INVULNTICS (30*TICRATE)
#define	INVISTICS (60*TICRATE)
#define	INFRATICS (120*TICRATE)
#define	IRONTICS (60*TICRATE)
#define WPNLEV2TICS (40*TICRATE)
#define FLIGHTTICS (60*TICRATE)
#define SPEEDTICS (45*TICRATE)
#define MAULATORTICS (25*TICRATE)

EXTERN_CVAR (Bool, r_drawfuzz);

IMPLEMENT_ABSTRACT_ACTOR (APowerup)

// Powerup-Giver -------------------------------------------------------------

//===========================================================================
//
// APowerupGiver :: Use
//
//===========================================================================

bool APowerupGiver::Use ()
{
	APowerup *power = static_cast<APowerup *> (Spawn (PowerupType, 0, 0, 0));

	power->ItemFlags |= ItemFlags & IF_ALWAYSPICKUP;
	if (power->TryPickup (Owner))
	{
		if (EffectTics != 0)
		{
			power->EffectTics = EffectTics;
		}
		if (BlendColor != 0)
		{
			power->BlendColor = BlendColor;
		}
		return true;
	}
	power->GoAwayAndDie ();
	return false;
}

//===========================================================================
//
// APowerupGiver :: Serialize
//
//===========================================================================

void APowerupGiver::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << PowerupType;
	if (SaveVersion >= 223)
	{
		arc << EffectTics << BlendColor;
	}
}

// Powerup -------------------------------------------------------------------

//===========================================================================
//
// APowerup :: Tick
//
//===========================================================================

void APowerup::Tick ()
{
	if (Owner == NULL)
	{
		Destroy ();
	}
	else if (EffectTics > 0)
	{
		DoEffect ();
		if (--EffectTics == 0)
		{
			Destroy ();
		}
	}
}

//===========================================================================
//
// APowerup :: Serialize
//
//===========================================================================

void APowerup::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << EffectTics << BlendColor;
}

//===========================================================================
//
// APowerup :: GetBlend
//
//===========================================================================

PalEntry APowerup::GetBlend ()
{
	if (EffectTics <= 4*32 && !(EffectTics & 8))
		return 0;

	return BlendColor;
}

//===========================================================================
//
// APowerup :: InitEffect
//
//===========================================================================

void APowerup::InitEffect ()
{
}

//===========================================================================
//
// APowerup :: DoEffect
//
//===========================================================================

void APowerup::DoEffect ()
{
}

//===========================================================================
//
// APowerup :: EndEffect
//
//===========================================================================

void APowerup::EndEffect ()
{
}

//===========================================================================
//
// APowerup :: Destroy
//
//===========================================================================

void APowerup::Destroy ()
{
	EndEffect ();
	Super::Destroy ();
}

//===========================================================================
//
// APowerup :: DrawPowerup
//
//===========================================================================

bool APowerup::DrawPowerup (int x, int y)
{
	if (Icon <= 0)
	{
		return false;
	}
	if (EffectTics > BLINKTHRESHOLD || !(EffectTics & 16))
	{
		FTexture *pic = TexMan(Icon);
		screen->DrawTexture (pic, x, y,
			DTA_HUDRules, HUD_Normal,
//			DTA_TopOffset, pic->GetHeight()/2,
//			DTA_LeftOffset, pic->GetWidth()/2,
			TAG_DONE);
	}
	return true;
}

//===========================================================================
//
// APowerup :: HandlePickup
//
//===========================================================================

bool APowerup::HandlePickup (AInventory *item)
{
	if (item->GetClass() == GetClass())
	{
		APowerup *power = static_cast<APowerup*>(item);
		if (power->EffectTics == 0)
		{
			power->ItemFlags |= IF_PICKUPGOOD;
			return true;
		}
		// If it's not blinking yet, you can't replenish the power unless the
		// powerup is required to be picked up.
		if (EffectTics > BLINKTHRESHOLD && !(power->ItemFlags & IF_ALWAYSPICKUP))
		{
			return true;
		}
		// Only increase the EffectTics, not decrease it.
		EffectTics = MAX (EffectTics, power->EffectTics);
		power->ItemFlags |= IF_PICKUPGOOD;
		return true;
	}
	if (Inventory != NULL)
	{
		return Inventory->HandlePickup (item);
	}
	return false;
}

//===========================================================================
//
// APowerup :: CreateCopy
//
//===========================================================================

AInventory *APowerup::CreateCopy (AActor *other)
{
	EffectTics = abs (EffectTics);
	Owner = other;
	InitEffect ();
	Owner = NULL;
	return this;
}

//===========================================================================
//
// APowerup :: CreateTossable
//
// Powerups are never droppable, even without IF_UNDROPPABLE set.
//
//===========================================================================

AInventory *APowerup::CreateTossable ()
{
	return NULL;
}

// Invulnerability Powerup ---------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerInvulnerable, Any, -1, 0)
	PROP_Powerup_EffectTics (INVULNTICS)
	PROP_Inventory_Icon ("SPSHLD0")
END_DEFAULTS

//===========================================================================
//
// APowerInvulnerable :: InitEffect
//
//===========================================================================

void APowerInvulnerable::InitEffect ()
{
	Owner->effects &= ~FX_RESPAWNINVUL;
	Owner->flags2 |= MF2_INVULNERABLE;
	Owner->player->mo->SpecialInvulnerabilityHandling (APlayerPawn::INVUL_Start);
}

//===========================================================================
//
// APowerInvulnerable :: DoEffect
//
//===========================================================================

void APowerInvulnerable::DoEffect ()
{
	Owner->player->mo->SpecialInvulnerabilityHandling (APlayerPawn::INVUL_Active);
	if (gameinfo.gametype != GAME_Hexen)
	{
		if ((EffectTics > BLINKTHRESHOLD || (EffectTics & 8)) &&
			!(Owner->player->mo->effects & FX_RESPAWNINVUL) &&
			0 == BlendColor.a)	// [RH] No special colormap if there is a blend
		{
			Owner->player->fixedcolormap = NUMCOLORMAPS;
		}
		else
		{
			Owner->player->fixedcolormap = 0;
		}
	}
}

//===========================================================================
//
// APowerInvulnerable :: EndEffect
//
//===========================================================================

void APowerInvulnerable::EndEffect ()
{
	if (Owner == NULL)
	{
		return;
	}

	Owner->flags2 &= ~MF2_INVULNERABLE;
	Owner->effects &= ~FX_RESPAWNINVUL;
	if (Owner->player != NULL)
	{
		if (Owner->player->mo != NULL)
		{
			Owner->player->mo->SpecialInvulnerabilityHandling (APlayerPawn::INVUL_Stop);
		}
		Owner->player->fixedcolormap = 0;
	}
}

// Strength (aka Berserk) Powerup --------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerStrength, Any, -1, 0)
	PROP_Powerup_EffectTics (1)
	PROP_Powerup_Color (128, 255, 0, 0)
	PROP_Inventory_FlagsSet (IF_HUBPOWER)
END_DEFAULTS

//===========================================================================
//
// APowerStrength :: InitEffect
//
//===========================================================================

void APowerStrength::InitEffect ()
{
	P_GiveBody (Owner->player, 100);
}

//===========================================================================
//
// APowerStrength :: DoEffect
//
//===========================================================================

void APowerStrength::DoEffect ()
{
	// Strength counts up to diminish the fade.
	EffectTics += 2;
}

//===========================================================================
//
// APowerStrength :: GetBlend
//
//===========================================================================

PalEntry APowerStrength::GetBlend ()
{
	// slowly fade the berzerk out
	int cnt = 128 - ((EffectTics>>3) & (~0x1f));

	if (cnt > 0)
	{
		return PalEntry (BlendColor.a*cnt/128,
			BlendColor.r, BlendColor.g, BlendColor.b);
	}
	return 0;
}

// Invisibility Powerup ------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerInvisibility, Any, -1, 0)
	PROP_Powerup_EffectTics (INVISTICS)
END_DEFAULTS

//===========================================================================
//
// APowerInvisibility :: InitEffect
//
//===========================================================================

void APowerInvisibility::InitEffect ()
{
	Owner->flags |= MF_SHADOW;
	Owner->alpha = FRACUNIT/5;
	Owner->RenderStyle = STYLE_OptFuzzy;
}

//===========================================================================
//
// APowerInvisibility :: EndEffect
//
//===========================================================================

void APowerInvisibility::EndEffect ()
{
	if (Owner != NULL)
	{
		Owner->flags &= ~MF_SHADOW;
		Owner->flags3 &= ~MF3_GHOST;
		Owner->RenderStyle = STYLE_Normal;
		Owner->alpha = OPAQUE;
	}
}

//===========================================================================
//
// APowerInvisibility :: AlterWeaponSprite
//
//===========================================================================

void APowerInvisibility::AlterWeaponSprite (vissprite_t *vis)
{
	// Blink if the powerup is wearing off
	if (EffectTics < 4*32 && !(EffectTics & 8))
	{
		vis->RenderStyle = STYLE_Normal;
	}
	if (Inventory != NULL)
	{
		Inventory->AlterWeaponSprite (vis);
	}
}

// Ghost Powerup (Heretic's version of invisibility) -------------------------

IMPLEMENT_STATELESS_ACTOR (APowerGhost, Any, -1, 0)
	PROP_Powerup_EffectTics (INVISTICS)
END_DEFAULTS

//===========================================================================
//
// APowerInvisibility :: InitEffect
//
//===========================================================================

void APowerGhost::InitEffect ()
{
	Owner->flags |= MF_SHADOW;
	Owner->flags3 |= MF3_GHOST;
	Owner->alpha = HR_SHADOW;
	Owner->RenderStyle = STYLE_Translucent;
}

// Ironfeet Powerup ----------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerIronFeet, Any, -1, 0)
	PROP_Powerup_EffectTics (IRONTICS)
	PROP_Powerup_Color (32, 0, 255, 0)
END_DEFAULTS

// Strife Environment Suit Powerup -------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerMask, Any, -1, 0)
	PROP_Powerup_EffectTics (80*TICRATE)
	PROP_Powerup_Color (0, 0, 0, 0)
	PROP_Inventory_Icon ("I_MASK")
END_DEFAULTS

// Light-Amp Powerup ---------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerLightAmp, Any, -1, 0)
	PROP_Powerup_EffectTics (INFRATICS)
END_DEFAULTS

//===========================================================================
//
// APowerLightAmp :: DoEffect
//
//===========================================================================

void APowerLightAmp::DoEffect ()
{
	if (Owner->player != NULL)
	{
		if (EffectTics > BLINKTHRESHOLD || (EffectTics & 8))
		{	// almost full bright
			Owner->player->fixedcolormap = 1;
		}
		else
		{
			Owner->player->fixedcolormap = 0;
		}
	}
}

//===========================================================================
//
// APowerLightAmp :: EndEffect
//
//===========================================================================

void APowerLightAmp::EndEffect ()
{
	if (Owner != NULL && Owner->player != NULL)
	{
		Owner->player->fixedcolormap = 0;
	}
}

// Torch Powerup -------------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerTorch, Any, -1, 0)
	PROP_Powerup_EffectTics (INFRATICS)
END_DEFAULTS

//===========================================================================
//
// APowerTorch :: Serialize
//
//===========================================================================

void APowerTorch::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << NewTorch << NewTorchDelta;
}

//===========================================================================
//
// APowerTorch :: DoEffect
//
//===========================================================================

void APowerTorch::DoEffect ()
{
	if (EffectTics <= BLINKTHRESHOLD)
	{
		Super::DoEffect ();
	}
	else if (!(level.time & 16) && Owner->player != NULL)
	{
		if (NewTorch != 0)
		{
			if (Owner->player->fixedcolormap + NewTorchDelta > 7
				|| Owner->player->fixedcolormap + NewTorchDelta < 1
				|| NewTorch == Owner->player->fixedcolormap)
			{
				NewTorch = 0;
			}
			else
			{
				Owner->player->fixedcolormap += NewTorchDelta;
			}
		}
		else
		{
			NewTorch = (pr_torch() & 7) + 1;
			NewTorchDelta = (NewTorch == Owner->player->fixedcolormap) ?
				0 : ((NewTorch > Owner->player->fixedcolormap) ? 1 : -1);
		}
	}
}

// Flight (aka Wings of Wrath) powerup ---------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerFlight, Any, -1, 0)
	PROP_Powerup_EffectTics (FLIGHTTICS)
	PROP_Inventory_FlagsSet (IF_HUBPOWER)
END_DEFAULTS

//===========================================================================
//
// APowerFlight :: Serialize
//
//===========================================================================

void APowerFlight::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << HitCenterFrame;
}

//===========================================================================
//
// APowerFlight :: InitEffect
//
//===========================================================================

void APowerFlight::InitEffect ()
{
	Owner->flags2 |= MF2_FLY;
	Owner->flags |= MF_NOGRAVITY;
	if (Owner->z <= Owner->floorz)
	{
		Owner->momz = 4*FRACUNIT;	// thrust the player in the air a bit
	}
	if (Owner->momz <= -35*FRACUNIT)
	{ // stop falling scream
		S_StopSound (Owner, CHAN_VOICE);
	}
}

//===========================================================================
//
// APowerFlight :: DoEffect
//
//===========================================================================

void APowerFlight::DoEffect ()
{
	// The Wings of Wrath only expire in multiplayer and non-hub games
	if (!multiplayer && (level.clusterflags & CLUSTER_HUB))
	{
		EffectTics++;
	}
//	Owner->flags |= MF_NOGRAVITY;
//	Owner->flags2 |= MF2_FLY;
}

//===========================================================================
//
// APowerFlight :: EndEffect
//
//===========================================================================

void APowerFlight::EndEffect ()
{
	if (Owner == NULL || Owner->player == NULL)
	{
		return;
	}
	if (!(Owner->player->cheats & CF_FLY))
	{
		if (Owner->z != Owner->floorz)
		{
			Owner->player->centering = true;
		}
		Owner->flags2 &= ~MF2_FLY;
		Owner->flags &= ~MF_NOGRAVITY;
	}
//	BorderTopRefresh = screen->GetPageCount (); //make sure the sprite's cleared out
}

//===========================================================================
//
// APowerFlight :: DrawPowerup
//
//===========================================================================

bool APowerFlight::DrawPowerup (int x, int y)
{
	if (EffectTics > BLINKTHRESHOLD || !(EffectTics & 16))
	{
		int picnum = TexMan.GetTexture ("SPFLY0", FTexture::TEX_MiscPatch);
		int frame = (level.time/3) & 15;

		if (Owner->flags2 & MF2_FLY)
		{
			if (HitCenterFrame && (frame != 15 && frame != 0))
			{
				screen->DrawTexture (TexMan[picnum+15], x, y,
					DTA_HUDRules, HUD_Normal, TAG_DONE);
			}
			else
			{
				screen->DrawTexture (TexMan[picnum+frame], x, y,
					DTA_HUDRules, HUD_Normal, TAG_DONE);
				HitCenterFrame = false;
			}
		}
		else
		{
			if (!HitCenterFrame && (frame != 15 && frame != 0))
			{
				screen->DrawTexture (TexMan[picnum+frame], x, y,
					DTA_HUDRules, HUD_Normal, TAG_DONE);
				HitCenterFrame = false;
			}
			else
			{
				screen->DrawTexture (TexMan[picnum+15], x, y,
					DTA_HUDRules, HUD_Normal, TAG_DONE);
				HitCenterFrame = true;
			}
		}
	}
	return true;
}

// Weapon Level 2 (aka Tome of Power) Powerup --------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerWeaponLevel2, Any, -1, 0)
	PROP_Powerup_EffectTics (WPNLEV2TICS)
	PROP_Inventory_Icon ("SPINBK0")
END_DEFAULTS

//===========================================================================
//
// APowerWeaponLevel2 :: InitEffect
//
//===========================================================================

void APowerWeaponLevel2::InitEffect ()
{
	AWeapon *weapon, *sister;

	if (Owner->player == NULL)
		return;

	weapon = Owner->player->ReadyWeapon;

	if (weapon == NULL)
		return;

	sister = weapon->SisterWeapon;

	if (sister == NULL)
		return;

	if (!(sister->WeaponFlags & WIF_POWERED_UP))
		return;

	Owner->player->ReadyWeapon = sister;

	if (weapon->GetReadyState() != sister->GetReadyState())
	{
		P_SetPsprite (Owner->player, ps_weapon, sister->GetReadyState());
	}
}

//===========================================================================
//
// APowerWeaponLevel2 :: EndEffect
//
//===========================================================================

void APowerWeaponLevel2::EndEffect ()
{
	player_t *player = Owner != NULL ? Owner->player : NULL;

	if (player != NULL &&
		player->ReadyWeapon != NULL &&
		player->ReadyWeapon->WeaponFlags & WIF_POWERED_UP)
	{
		player->ReadyWeapon->EndPowerup ();
	}
//	BorderTopRefresh = screen->GetPageCount ();
}

// Player Speed Trail (used by the Speed Powerup) ----------------------------

class APlayerSpeedTrail : public AActor
{
	DECLARE_STATELESS_ACTOR (APlayerSpeedTrail, AActor)
public:
	void Tick ();
};

IMPLEMENT_STATELESS_ACTOR (APlayerSpeedTrail, Any, -1, 0)
	PROP_Flags (MF_NOBLOCKMAP|MF_NOGRAVITY)
	PROP_Alpha (FRACUNIT*6/10)
	PROP_RenderStyle (STYLE_Translucent)
END_DEFAULTS

//===========================================================================
//
// APlayerSpeedTrail :: Tick
//
//===========================================================================

void APlayerSpeedTrail::Tick ()
{
	const int fade = OPAQUE*6/10/8;
	if (alpha <= fade)
	{
		Destroy ();
	}
	else
	{
		alpha -= fade;
	}
}

// Speed Powerup -------------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerSpeed, Any, -1, 0)
	PROP_Powerup_EffectTics (SPEEDTICS)
	PROP_Inventory_Icon ("SPBOOT0")
END_DEFAULTS

//===========================================================================
//
// APowerSpeed :: InitEffect
//
//===========================================================================

void APowerSpeed::InitEffect ()
{
	Owner->player->Powers |= PW_SPEED;
}

//===========================================================================
//
// APowerSpeed :: DoEffect
//
//===========================================================================

void APowerSpeed::DoEffect ()
{
	if (Owner->player->cheats & CF_PREDICTING)
		return;

	if (level.time & 1)
		return;

	if (P_AproxDistance (Owner->momx, Owner->momy) <= 12*FRACUNIT)
		return;

	AActor *speedMo = Spawn<APlayerSpeedTrail> (Owner->x, Owner->y, Owner->z);
	if (speedMo)
	{
		speedMo->angle = Owner->angle;
		speedMo->Translation = Owner->Translation;
		speedMo->target = Owner;
		speedMo->sprite = Owner->sprite;
		speedMo->frame = Owner->frame;
		speedMo->floorclip = Owner->floorclip;
		if (Owner == players[consoleplayer].camera &&
			!(Owner->player->cheats & CF_CHASECAM))
		{
			speedMo->renderflags |= RF_INVISIBLE;
		}
	}
}

//===========================================================================
//
// APowerSpeed :: EndEffect
//
//===========================================================================

void APowerSpeed::EndEffect ()
{
	if (Owner != NULL && Owner->player != NULL)
	{
		Owner->player->Powers &= ~PW_SPEED;
	}
}

// Minotaur (aka Dark Servant) powerup ---------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerMinotaur, Any, -1, 0)
	PROP_Powerup_EffectTics (MAULATORTICS)
	PROP_Inventory_Icon ("SPMINO0")
END_DEFAULTS

// Targeter powerup ---------------------------------------------------------

class APowerTargeter : public APowerup
{
	DECLARE_ACTOR (APowerTargeter, APowerup)
protected:
	void InitEffect ();
	void DoEffect ();
	void EndEffect ();
	void PositionAccuracy ();
};

FState APowerTargeter::States[] =
{
	S_NORMAL (TRGT, 'A', -1, NULL, NULL),
	S_NORMAL (TRGT, 'B', -1, NULL, NULL),
	S_NORMAL (TRGT, 'C', -1, NULL, NULL)
};

IMPLEMENT_ACTOR (APowerTargeter, Any, -1, 0)
	PROP_Powerup_EffectTics (160*TICRATE)
	PROP_Inventory_FlagsSet (IF_HUBPOWER)
END_DEFAULTS

void APowerTargeter::InitEffect ()
{
	player_t *player;

	if ((player = Owner->player) == NULL)
		return;

	P_SetPsprite (player, ps_targetcenter, &States[0]);
	P_SetPsprite (player, ps_targetleft, &States[1]);
	P_SetPsprite (player, ps_targetright, &States[2]);

	player->psprites[ps_targetcenter].sx = (160-3)*FRACUNIT;
	player->psprites[ps_targetcenter].sy =
		player->psprites[ps_targetleft].sy =
		player->psprites[ps_targetright].sy = (100-3)*FRACUNIT;
	PositionAccuracy ();
}

void APowerTargeter::DoEffect ()
{
	if (Owner == NULL)
	{
		Destroy ();
	}
	else if (Owner->player != NULL)
	{
		player_t *player = Owner->player;

		PositionAccuracy ();
		if (EffectTics < 5*TICRATE)
		{
			if (EffectTics & 32)
			{
				P_SetPsprite (player, ps_targetright, NULL);
				P_SetPsprite (player, ps_targetleft, &States[1]);
			}
			else if (EffectTics & 16)
			{
				P_SetPsprite (player, ps_targetright, &States[2]);
				P_SetPsprite (player, ps_targetleft, NULL);
			}
		}
	}
}

void APowerTargeter::EndEffect ()
{
	if (Owner != NULL && Owner->player != NULL)
	{
		P_SetPsprite (Owner->player, ps_targetcenter, NULL);
		P_SetPsprite (Owner->player, ps_targetleft, NULL);
		P_SetPsprite (Owner->player, ps_targetright, NULL);
	}
}

void APowerTargeter::PositionAccuracy ()
{
	player_t *player = Owner->player;

	player->psprites[ps_targetleft].sx = (160-3)*FRACUNIT - ((100 - player->accuracy) << FRACBITS);
	player->psprites[ps_targetright].sx = (160-3)*FRACUNIT + ((100 - player->accuracy) << FRACBITS);
}

// Scanner powerup ----------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerScanner, Any, -1, 0)
	PROP_Powerup_EffectTics (80*TICRATE)
	PROP_Inventory_FlagsSet (IF_HUBPOWER)
END_DEFAULTS
