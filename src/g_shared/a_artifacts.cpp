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
#define	TIMEFREEZE_TICS	( 12 * TICRATE )

EXTERN_CVAR (Bool, r_drawfuzz);

IMPLEMENT_ABSTRACT_ACTOR (APowerup)

// Powerup-Giver -------------------------------------------------------------

//===========================================================================
//
// APowerupGiver :: Use
//
//===========================================================================

bool APowerupGiver::Use (bool pickup)
{
	APowerup *power = static_cast<APowerup *> (Spawn (PowerupType, 0, 0, 0, NO_REPLACE));

	if (EffectTics != 0)
	{
		power->EffectTics = EffectTics;
	}
	if (BlendColor != 0)
	{
		power->BlendColor = BlendColor;
	}
	if (mode != NAME_None)
	{
		power->mode = mode;
	}

	power->ItemFlags |= ItemFlags & (IF_ALWAYSPICKUP|IF_ADDITIVETIME);
	if (power->TryPickup (Owner))
	{
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
	arc << EffectTics << BlendColor << mode;
}

// Powerup -------------------------------------------------------------------

//===========================================================================
//
// APowerup :: Tick
//
//===========================================================================

void APowerup::Tick ()
{
	// Powerups cannot exist outside an inventory
	if (Owner == NULL)
	{
		Destroy ();
	}
	if (EffectTics > 0 && --EffectTics == 0)
	{
		Destroy ();
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
	arc << EffectTics << BlendColor << mode;
}

//===========================================================================
//
// APowerup :: GetBlend
//
//===========================================================================

PalEntry APowerup::GetBlend ()
{
	if (EffectTics <= BLINKTHRESHOLD && !(EffectTics & 8))
		return 0;

	if (BlendColor == INVERSECOLOR ||
		BlendColor == GOLDCOLOR ||
		// [BC] HAX!
		BlendColor == REDCOLOR ||
		BlendColor == GREENCOLOR) 
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
	if (Owner == NULL || Owner->player == NULL)
	{
		return;
	}

	if (EffectTics > 0)
	{
		int oldcolormap = Owner->player->fixedcolormap;
		if (EffectTics > BLINKTHRESHOLD || (EffectTics & 8))
		{
			if (BlendColor == INVERSECOLOR)
			{
				Owner->player->fixedcolormap = INVERSECOLORMAP;
			}
			else if (BlendColor == GOLDCOLOR)
			{
				Owner->player->fixedcolormap = GOLDCOLORMAP;
			}
			else if (BlendColor == REDCOLOR)
			{
				Owner->player->fixedcolormap = REDCOLORMAP;
			}
			else if (BlendColor == GREENCOLOR)
			{
				Owner->player->fixedcolormap = GREENCOLORMAP;
			}
		}
		else if ((BlendColor == INVERSECOLOR && Owner->player->fixedcolormap == INVERSECOLORMAP) || 
				 (BlendColor == GOLDCOLOR && Owner->player->fixedcolormap == GOLDCOLORMAP) ||
				 (BlendColor == REDCOLOR && Owner->player->fixedcolormap == REDCOLORMAP) ||
				 (BlendColor == GREENCOLOR && Owner->player->fixedcolormap == GREENCOLORMAP))
		{
			Owner->player->fixedcolormap = 0;
		}
	}
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
		// Color also gets transferred only when the new item has an effect.
		if (power->ItemFlags & IF_ADDITIVETIME) 
		{
			EffectTics += power->EffectTics;
			BlendColor = power->BlendColor;
		}
		else if (power->EffectTics > EffectTics)
		{
			EffectTics = power->EffectTics;
			BlendColor = power->BlendColor;
		}
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

//===========================================================================
//
// APowerup :: OwnerDied
//
// Powerups don't last beyond death.
//
//===========================================================================

void APowerup::OwnerDied ()
{
	Destroy ();
}

// Invulnerability Powerup ---------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerInvulnerable, Any, -1, 0)
	PROP_Powerup_EffectTics (INVULNTICS)
	PROP_Inventory_Icon ("SPSHLD0")
END_DEFAULTS

// Need to set the default for each game here
AT_GAME_SET(PowerInvulnerable)
{
	APowerInvulnerable * invul = GetDefault<APowerInvulnerable>();
	switch (gameinfo.gametype)
	{
	case GAME_Doom:
	case GAME_Strife:
		invul->BlendColor = INVERSECOLOR;
		break;

	case GAME_Heretic:
		invul->BlendColor = GOLDCOLOR;
		break;

	default:
		break;
	}
}
//===========================================================================
//
// APowerInvulnerable :: InitEffect
//
//===========================================================================

void APowerInvulnerable::InitEffect ()
{
	Owner->effects &= ~FX_RESPAWNINVUL;
	Owner->flags2 |= MF2_INVULNERABLE;
	if (mode == NAME_None)
	{
		mode = (ENamedName)RUNTIME_TYPE(Owner)->Meta.GetMetaInt(APMETA_InvulMode);
	}
	if (mode == NAME_Reflective) Owner->flags2 |= MF2_REFLECTIVE;
}

//===========================================================================
//
// APowerInvulnerable :: DoEffect
//
//===========================================================================

void APowerInvulnerable::DoEffect ()
{
	Super::DoEffect ();

	if (Owner == NULL)
	{
		return;
	}

	if (mode == NAME_Ghost)
	{
		if (!(Owner->flags & MF_SHADOW))
		{
			// Don't mess with the translucency settings if an
			// invisibility powerup is active.
			Owner->RenderStyle = STYLE_Translucent;
			if (!(level.time & 7) && Owner->alpha > 0 && Owner->alpha < OPAQUE)
			{
				if (Owner->alpha == HX_SHADOW)
				{
					Owner->alpha = HX_ALTSHADOW;
				}
				else
				{
					Owner->alpha = 0;
					Owner->flags2 |= MF2_NONSHOOTABLE;
				}
			}
			if (!(level.time & 31))
			{
				if (Owner->alpha == 0)
				{
					Owner->flags2 &= ~MF2_NONSHOOTABLE;
					Owner->alpha = HX_ALTSHADOW;
				}
				else
				{
					Owner->alpha = HX_SHADOW;
				}
			}
		}
		else
		{
			Owner->flags2 &= ~MF2_NONSHOOTABLE;
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
	if (mode == NAME_Ghost)
	{
		Owner->flags2 &= ~MF2_NONSHOOTABLE;
		if (!(Owner->flags & MF_SHADOW))
		{
			// Don't mess with the translucency settings if an
			// invisibility powerup is active.
			Owner->RenderStyle = STYLE_Normal;
			Owner->alpha = OPAQUE;
		}
	}
	else if (mode == NAME_Reflective)
	{
		Owner->flags2 &= ~MF2_REFLECTIVE;
	}

	if (Owner->player != NULL)
	{
		Owner->player->fixedcolormap = 0;
	}
}

//===========================================================================
//
// APowerInvulnerable :: AlterWeaponSprite
//
//===========================================================================

int APowerInvulnerable::AlterWeaponSprite (vissprite_t *vis)
{
	int changed = Inventory == NULL ? false : Inventory->AlterWeaponSprite(vis);
	if (Owner != NULL)
	{
		if (mode == NAME_Ghost && !(Owner->flags & MF_SHADOW))
		{
			fixed_t wp_alpha = MIN<fixed_t>(FRACUNIT/4 + Owner->alpha*3/4, FRACUNIT);
			if (wp_alpha != FIXED_MAX) vis->alpha = wp_alpha;
		}
	}
	return changed;
}

// Strength (aka Berserk) Powerup --------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerStrength, Any, -1, 0)
	PROP_Powerup_EffectTics (1)
	PROP_Powerup_Color (128, 255, 0, 0)
	PROP_Inventory_FlagsSet (IF_HUBPOWER)
END_DEFAULTS

//===========================================================================
//
// APowerStrength :: HandlePickup
//
//===========================================================================

bool APowerStrength::HandlePickup (AInventory *item)
{
	if (item->GetClass() == GetClass())
	{ // Setting EffectTics to 0 will force Powerup's HandlePickup()
	  // method to reset the tic count so you get the red flash again.
		EffectTics = 0;
	}
	return Super::HandlePickup (item);
}

//===========================================================================
//
// APowerStrength :: InitEffect
//
//===========================================================================

void APowerStrength::InitEffect ()
{
}

//===========================================================================
//
// APowerStrength :: DoEffect
//
//===========================================================================

void APowerStrength::Tick ()
{
	// Strength counts up to diminish the fade.
	EffectTics += 2;
	Super::Tick();
}

//===========================================================================
//
// APowerStrength :: GetBlend
//
//===========================================================================

PalEntry APowerStrength::GetBlend ()
{
	// slowly fade the berzerk out
	int cnt = 12 - (EffectTics >> 6);

	if (cnt > 0)
	{
		cnt = (cnt + 7) >> 3;
		return PalEntry (BlendColor.a*cnt*255/9,
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

void APowerInvisibility::DoEffect ()
{
	Super::DoEffect();
	// Due to potential interference with other PowerInvisibility items
	// the effect has to be refreshed each tic.
	InitEffect();
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

		// Check whether there are other invisibility items and refresh their effect.
		// If this isn't done there will be one incorrectly drawn frame when this
		// item expires.
		AInventory *item = Owner->Inventory;
		while (item != NULL)
		{
			if (item->IsKindOf(RUNTIME_CLASS(APowerInvisibility)) && item != this)
			{
				static_cast<APowerInvisibility*>(item)->InitEffect();
			}
			item = item->Inventory;
		}
	}
}

//===========================================================================
//
// APowerInvisibility :: AlterWeaponSprite
//
//===========================================================================

int APowerInvisibility::AlterWeaponSprite (vissprite_t *vis)
{
	int changed = Inventory == NULL ? false : Inventory->AlterWeaponSprite(vis);

	// Blink if the powerup is wearing off
	if (changed == 0 && EffectTics < 4*32 && !(EffectTics & 8))
	{
		vis->RenderStyle = STYLE_Normal;
		return 1;
	}
	else if (changed == 1)
	{
		// something else set the weapon sprite back to opaque but this item is still active.
		vis->alpha = FRACUNIT/5;
		vis->RenderStyle = STYLE_OptFuzzy;
	}
	return -1;	// This item is valid so another one shouldn't reset the translucency
}

// Ghost Powerup (Heretic's version of invisibility) -------------------------

IMPLEMENT_STATELESS_ACTOR (APowerGhost, Any, -1, 0)
	PROP_Powerup_EffectTics (INVISTICS)
END_DEFAULTS

//===========================================================================
//
// APowerGhost :: InitEffect
//
//===========================================================================

void APowerGhost::InitEffect ()
{
	Owner->flags |= MF_SHADOW;
	Owner->flags3 |= MF3_GHOST;
	Owner->alpha = HR_SHADOW;
	Owner->RenderStyle = STYLE_Translucent;
}

//===========================================================================
//
// APowerGhost :: AlterWeaponSprite
//
//===========================================================================

int APowerGhost::AlterWeaponSprite (vissprite_t *vis)
{
	int changed = Inventory == NULL ? false : Inventory->AlterWeaponSprite(vis);

	// Blink if the powerup is wearing off
	if (changed == 0 && EffectTics < 4*32 && !(EffectTics & 8))
	{
		vis->RenderStyle = STYLE_Normal;
		return 1;
	}
	else if (changed == 1)
	{
		// something else set the weapon sprite back to opaque but this item is still active.
		vis->alpha = HR_SHADOW;
		vis->RenderStyle = STYLE_Translucent;
	}
	return -1;	// This item is valid so another one shouldn't reset the translucency
}

// Shadow Powerup (Strife's version of invisibility) -------------------------

IMPLEMENT_STATELESS_ACTOR (APowerShadow, Any, -1, 0)
	PROP_Powerup_EffectTics (55*TICRATE)
	PROP_Inventory_FlagsSet (IF_HUBPOWER)
END_DEFAULTS

//===========================================================================
//
// APowerShadow :: HandlePickup
//
// If the player already has the first stage of the powerup, getting it
// again makes them completely invisible. Special1 tracks which stage we
// are in, initially 0.
//
//===========================================================================

bool APowerShadow::HandlePickup (AInventory *item)
{
	if (special1 == 0 && item->GetClass() == GetClass())
	{
		APowerup *power = static_cast<APowerup *>(item);
		if (power->EffectTics == 0)
		{
			power->ItemFlags |= IF_PICKUPGOOD;
			return true;
		}
		// Only increase the EffectTics, not decrease it.
		// Color also gets transferred only when the new item has an effect.
		if (power->EffectTics > EffectTics)
		{
			EffectTics = power->EffectTics;
			BlendColor = power->BlendColor;
		}
		special1 = 1;	// Go to stage 2.
		power->ItemFlags |= IF_PICKUPGOOD;
		return true;
	}
	return Super::HandlePickup (item);
}

//===========================================================================
//
// APowerShadow :: InitEffect
//
//===========================================================================

void APowerShadow::InitEffect ()
{
	Owner->flags |= MF_SHADOW;
	Owner->alpha = special1 == 0 ? TRANSLUC25 : 0;
	Owner->RenderStyle = STYLE_Translucent;
}

//===========================================================================
//
// APowerShadow :: AlterWeaponSprite
//
//===========================================================================

int APowerShadow::AlterWeaponSprite (vissprite_t *vis)
{
	int changed = Inventory == NULL ? false : Inventory->AlterWeaponSprite(vis);

	// Blink if the powerup is wearing off
	if (changed == 0 && EffectTics < 4*32 && !(EffectTics & 8))
	{
		vis->RenderStyle = STYLE_Normal;
		return 1;
	}
	else if (changed == 1)
	{
		// something else set the weapon sprite back to opaque but this item is still active.
		vis->alpha = TRANSLUC25;
		vis->RenderStyle = STYLE_Translucent;
	}
	if (special1 == 1)
	{
		vis->alpha = TRANSLUC25;
		vis->colormap = InverseColormap;
	}
	return -1;	// This item is valid so another one shouldn't reset the translucency
}

// Ironfeet Powerup ----------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerIronFeet, Any, -1, 0)
	PROP_Powerup_EffectTics (IRONTICS)
	PROP_Powerup_Color (32, 0, 255, 0)
END_DEFAULTS

//===========================================================================
//
// APowerIronFeet :: AbsorbDamage
//
//===========================================================================

void APowerIronFeet::AbsorbDamage (int damage, FName damageType, int &newdamage)
{
	if (damageType == NAME_Drowning)
	{
		newdamage = 0;
		if (Owner->player != NULL)
		{
			Owner->player->mo->ResetAirSupply ();
		}
	}
	else if (Inventory != NULL)
	{
		Inventory->AbsorbDamage (damage, damageType, newdamage);
	}
}

// Strife Environment Suit Powerup -------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerMask, Any, -1, 0)
	PROP_Powerup_EffectTics (80*TICRATE)
	PROP_Powerup_Color (0, 0, 0, 0)
	PROP_Inventory_FlagsSet (IF_HUBPOWER)
	PROP_Inventory_Icon ("I_MASK")
END_DEFAULTS

//===========================================================================
//
// APowerMask :: AbsorbDamage
//
//===========================================================================

void APowerMask::AbsorbDamage (int damage, FName damageType, int &newdamage)
{
	if (damageType == NAME_Fire)
	{
		newdamage = 0;
	}
	else
	{
		Super::AbsorbDamage (damage, damageType, newdamage);
	}
}

//===========================================================================
//
// APowerMask :: DoEffect
//
//===========================================================================

void APowerMask::DoEffect ()
{
	Super::DoEffect ();
	if (!(level.time & 0x3f))
	{
		S_Sound (Owner, CHAN_AUTO, "misc/mask", 1, ATTN_STATIC);
	}
}

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
	Super::DoEffect ();

	if (Owner->player != NULL && Owner->player->fixedcolormap < NUMCOLORMAPS)
	{
		if (EffectTics > BLINKTHRESHOLD || (EffectTics & 8))
		{	
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
	if (Owner != NULL && Owner->player != NULL && Owner->player->fixedcolormap < NUMCOLORMAPS)
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
	if (Owner == NULL || Owner->player == NULL)
	{
		return;
	}

	if (EffectTics <= BLINKTHRESHOLD || Owner->player->fixedcolormap >= NUMCOLORMAPS)
	{
		Super::DoEffect ();
	}
	else 
	{
		APowerup::DoEffect ();

		if (!(level.time & 16) && Owner->player != NULL)
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

void APowerFlight::Tick ()
{
	// The Wings of Wrath only expire in multiplayer and non-hub games
	if (!multiplayer && (level.flags & LEVEL_INFINITE_FLIGHT))
	{
		EffectTics++;
	}

	Super::Tick ();

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
		int picnum = TexMan.CheckForTexture ("SPFLY0", FTexture::TEX_MiscPatch);
		int frame = (level.time/3) & 15;

		if (picnum <= 0)
		{
			return false;
		}
		if (Owner->flags & MF_NOGRAVITY)
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

	if (player != NULL)
	{

		if (player->ReadyWeapon != NULL &&
			player->ReadyWeapon->WeaponFlags & WIF_POWERED_UP)
		{
			player->ReadyWeapon->EndPowerup ();
		}
		if (player->PendingWeapon != NULL && player->PendingWeapon != WP_NOCHANGE &&
			player->PendingWeapon->WeaponFlags & WIF_POWERED_UP &&
			player->PendingWeapon->SisterWeapon != NULL)
		{
			player->PendingWeapon = player->PendingWeapon->SisterWeapon;
		}
	}
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
	PROP_SpeedLong(3*FRACUNIT/2)
	PROP_Powerup_EffectTics (SPEEDTICS)
	PROP_Inventory_Icon ("SPBOOT0")
END_DEFAULTS

//===========================================================================
//
// APowerSpeed :: GetSpeedFactor
//
//===========================================================================

fixed_t APowerSpeed ::GetSpeedFactor ()
{
	if (Inventory != NULL) return FixedMul(Speed, Inventory->GetSpeedFactor());
	else return Speed;
}

//===========================================================================
//
// APowerSpeed :: DoEffect
//
//===========================================================================

void APowerSpeed::DoEffect ()
{
	Super::DoEffect ();
	
	if (Owner == NULL || Owner->player == NULL)
		return;

	if (Owner->player->cheats & CF_PREDICTING)
		return;

	if (level.time & 1)
		return;

	// check if another speed item is present to avoid multiple drawing of the speed trail.
	if (Inventory != NULL && Inventory->GetSpeedFactor() > FRACUNIT)
		return;

	if (P_AproxDistance (Owner->momx, Owner->momy) <= 12*FRACUNIT)
		return;

	AActor *speedMo = Spawn<APlayerSpeedTrail> (Owner->x, Owner->y, Owner->z, NO_REPLACE);
	if (speedMo)
	{
		speedMo->angle = Owner->angle;
		speedMo->Translation = Owner->Translation;
		speedMo->target = Owner;
		speedMo->sprite = Owner->sprite;
		speedMo->frame = Owner->frame;
		speedMo->floorclip = Owner->floorclip;

		// [BC] Also get the scale from the owner.
		speedMo->scaleX = Owner->scaleX;
		speedMo->scaleY = Owner->scaleY;

		if (Owner == players[consoleplayer].camera &&
			!(Owner->player->cheats & CF_CHASECAM))
		{
			speedMo->renderflags |= RF_INVISIBLE;
		}
	}
}

// Minotaur (aka Dark Servant) powerup ---------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerMinotaur, Any, -1, 0)
	PROP_Powerup_EffectTics (MAULATORTICS)
	PROP_Inventory_Icon ("SPMINO0")
END_DEFAULTS

// Targeter powerup ---------------------------------------------------------

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

void APowerTargeter::Travelled ()
{
	InitEffect ();
}

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
	Super::DoEffect ();

	if (Owner != NULL && Owner->player != NULL)
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

	if (player != NULL)
	{
		player->psprites[ps_targetleft].sx = (160-3)*FRACUNIT - ((100 - player->accuracy) << FRACBITS);
		player->psprites[ps_targetright].sx = (160-3)*FRACUNIT + ((100 - player->accuracy) << FRACBITS);
	}
}

// Frightener Powerup --------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerFrightener, Any, -1, 0)
	PROP_Powerup_EffectTics (60*TICRATE)
END_DEFAULTS

//===========================================================================
//
// APowerFrightener :: InitEffect
//
//===========================================================================

void APowerFrightener::InitEffect ()
{
	if (Owner== NULL || Owner->player == NULL)
		return;

	Owner->player->cheats |= CF_FRIGHTENING;
}

//===========================================================================
//
// APowerFrightener :: EndEffect
//
//===========================================================================

void APowerFrightener::EndEffect ()
{
	if (Owner== NULL || Owner->player == NULL)
		return;

	Owner->player->cheats &= ~CF_FRIGHTENING;
}

// Scanner powerup ----------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR (APowerScanner, Any, -1, 0)
	PROP_Powerup_EffectTics (80*TICRATE)
	PROP_Inventory_FlagsSet (IF_HUBPOWER)
END_DEFAULTS


// Time freezer powerup -----------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( APowerTimeFreezer, Any, -1, 0 )
	PROP_Powerup_EffectTics( TIMEFREEZE_TICS )
END_DEFAULTS

//===========================================================================
//
// APowerTimeFreezer :: InitEffect
//
//===========================================================================

void APowerTimeFreezer::InitEffect( )
{
	int	ulIdx;

	if (Owner== NULL || Owner->player == NULL)
		return;

	// When this powerup is in effect, pause the music.
	S_PauseSound( false );

	// Give the player and his teammates the power to move when time is frozen.
	Owner->player->cheats |= CF_TIMEFREEZE;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] &&
			 players[ulIdx].mo != NULL &&
			 players[ulIdx].mo->IsTeammate( Owner )
		   )
		{
			players[ulIdx].cheats |= CF_TIMEFREEZE;
		}
	}

	// [RH] The effect ends one tic after the counter hits zero, so make
	// sure we start at an odd count.
	EffectTics += !(EffectTics & 1);
	if ((EffectTics & 1) == 0)
	{
		EffectTics++;
	}
	// Make sure the effect starts and ends on an even tic.
	if ((level.time & 1) == 0)
	{
		level.flags |= LEVEL_FROZEN;
	}
	else
	{
		EffectTics++;
	}
}

//===========================================================================
//
// APowerTimeFreezer :: DoEffect
//
//===========================================================================

void APowerTimeFreezer::DoEffect( )
{
	Super::DoEffect();
	// [RH] Do not change LEVEL_FROZEN on odd tics, or the Revenant's tracer
	// will get thrown off.
	if (level.time & 1)
	{
		return;
	}
	// [RH] The "blinking" can't check against EffectTics exactly or it will
	// never happen, because InitEffect ensures that EffectTics will always
	// be odd when level.time is even.
	if ( EffectTics > 4*32 
		|| (( EffectTics > 3*32 && EffectTics <= 4*32 ) && ((EffectTics + 1) & 15) != 0 )
		|| (( EffectTics > 2*32 && EffectTics <= 3*32 ) && ((EffectTics + 1) & 7) != 0 )
		|| (( EffectTics >   32 && EffectTics <= 2*32 ) && ((EffectTics + 1) & 3) != 0 )
		|| (( EffectTics >    0 && EffectTics <= 1*32 ) && ((EffectTics + 1) & 1) != 0 ))
		level.flags |= LEVEL_FROZEN;
	else
		level.flags &= ~LEVEL_FROZEN;
}

//===========================================================================
//
// APowerTimeFreezer :: EndEffect
//
//===========================================================================

void APowerTimeFreezer::EndEffect( )
{
	int	ulIdx;

	// Allow other actors to move about freely once again.
	level.flags &= ~LEVEL_FROZEN;

	// Also, turn the music back on.
	S_ResumeSound( );

	// Nothing more to do if there's no owner.
	if (( Owner == NULL ) || ( Owner->player == NULL ))
	{
		return;
	}

	// Take away the time freeze power, and his teammates.
	Owner->player->cheats &= ~CF_TIMEFREEZE;
	for ( ulIdx = 0; ulIdx < MAXPLAYERS; ulIdx++ )
	{
		if ( playeringame[ulIdx] &&
			 players[ulIdx].mo != NULL &&
			 players[ulIdx].mo->IsTeammate( Owner )
		   )
		{
			players[ulIdx].cheats &= ~CF_TIMEFREEZE;
		}
	}
}

// Damage powerup ------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( APowerDamage, Any, -1, 0 )
	PROP_Powerup_EffectTics( 25*TICRATE )
END_DEFAULTS

//===========================================================================
//
// APowerDamage :: InitEffect
//
//===========================================================================

void APowerDamage::InitEffect( )
{
	// Use sound channel 5 to avoid interference with other actions.
	if (Owner != NULL) S_SoundID(Owner, 5, SeeSound, 1.0f, ATTN_NONE);
}

//===========================================================================
//
// APowerDamage :: EndEffect
//
//===========================================================================

void APowerDamage::EndEffect( )
{
	// Use sound channel 5 to avoid interference with other actions.
	if (Owner != NULL) S_SoundID(Owner, 5, DeathSound, 1.0f, ATTN_NONE);
}

//===========================================================================
//
// APowerDamage :: AbsorbDamage
//
//===========================================================================

void APowerDamage::ModifyDamage(int damage, FName damageType, int &newdamage, bool passive)
{
	static const fixed_t def = 4*FRACUNIT;
	if (!passive && damage > 0)
	{
		DmgFactors * df = GetClass()->ActorInfo->DamageFactors;
		if (df != NULL)
		{
			const fixed_t * pdf = df->CheckKey(damageType);
			if (pdf== NULL && damageType != NAME_None) pdf = df->CheckKey(NAME_None);
			if (pdf == NULL) pdf = &def;

			damage = newdamage = FixedMul(damage, *pdf);
			if (*pdf > 0 && damage == 0) damage = newdamage = 1;	// don't allow zero damage as result of an underflow
			if (Owner != NULL && *pdf > FRACUNIT) S_SoundID(Owner, 5, ActiveSound, 1.0f, ATTN_NONE);
		}
	}
	if (Inventory != NULL) Inventory->ModifyDamage(damage, damageType, newdamage, passive);
}

// Quarter damage powerup ------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( APowerProtection, Any, -1, 0 )
	PROP_Powerup_EffectTics( 25*TICRATE )
END_DEFAULTS

//===========================================================================
//
// APowerProtection :: InitEffect
//
//===========================================================================

void APowerProtection::InitEffect( )
{
	// Use sound channel 5 to avoid interference with other actions.
	if (Owner != NULL) S_SoundID(Owner, 5, SeeSound, 1.0f, ATTN_NONE);
}

//===========================================================================
//
// APowerProtection :: EndEffect
//
//===========================================================================

void APowerProtection::EndEffect( )
{
	// Use sound channel 5 to avoid interference with other actions.
	if (Owner != NULL) S_SoundID(Owner, 5, DeathSound, 1.0f, ATTN_NONE);
}

//===========================================================================
//
// APowerProtection :: AbsorbDamage
//
//===========================================================================

void APowerProtection::ModifyDamage(int damage, FName damageType, int &newdamage, bool passive)
{
	static const fixed_t def = FRACUNIT/4;
	if (passive && damage > 0)
	{
		DmgFactors * df = GetClass()->ActorInfo->DamageFactors;
		if (df != NULL)
		{
			const fixed_t * pdf = df->CheckKey(damageType);
			if (pdf== NULL && damageType != NAME_None) pdf = df->CheckKey(NAME_None);
			if (pdf == NULL) pdf = &def;

			damage = newdamage = FixedMul(damage, *pdf);
			if (Owner != NULL && *pdf < FRACUNIT) S_SoundID(Owner, 5, ActiveSound, 1.0f, ATTN_NONE);
		}
	}
	if (Inventory != NULL) Inventory->ModifyDamage(damage, damageType, newdamage, passive);
}

// Drain rune -------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( APowerDrain, Any, -1, 0 )
	PROP_Powerup_EffectTics( 60*TICRATE )
END_DEFAULTS

//===========================================================================
//
// ARuneDrain :: InitEffect
//
//===========================================================================

void APowerDrain::InitEffect( )
{
	if (Owner== NULL || Owner->player == NULL)
		return;

	// Give the player the power to drain life from opponents when he damages them.
	Owner->player->cheats |= CF_DRAIN;
}

//===========================================================================
//
// ARuneDrain :: EndEffect
//
//===========================================================================

void APowerDrain::EndEffect( )
{
	// Nothing to do if there's no owner.
	if (Owner != NULL && Owner->player != NULL)
	{
		// Take away the drain power.
		Owner->player->cheats &= ~CF_DRAIN;
	}
}


// Regeneration rune -------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( APowerRegeneration, Any, -1, 0 )
	PROP_Powerup_EffectTics( 120*TICRATE )
END_DEFAULTS

//===========================================================================
//
// ARuneRegeneration :: InitEffect
//
//===========================================================================

void APowerRegeneration::InitEffect( )
{
	if (Owner== NULL || Owner->player == NULL)
		return;

	// Give the player the power to regnerate lost life.
	Owner->player->cheats |= CF_REGENERATION;
}

//===========================================================================
//
// ARuneRegeneration :: EndEffect
//
//===========================================================================

void APowerRegeneration::EndEffect( )
{
	// Nothing to do if there's no owner.
	if (Owner != NULL && Owner->player != NULL)
	{
		// Take away the regeneration power.
		Owner->player->cheats &= ~CF_REGENERATION;
	}
}

// High jump rune -------------------------------------------------------

IMPLEMENT_STATELESS_ACTOR( APowerHighJump, Any, -1, 0 )
END_DEFAULTS

//===========================================================================
//
// ARuneHighJump :: InitEffect
//
//===========================================================================

void APowerHighJump::InitEffect( )
{
	if (Owner== NULL || Owner->player == NULL)
		return;

	// Give the player the power to jump much higher.
	Owner->player->cheats |= CF_HIGHJUMP;
}

//===========================================================================
//
// ARuneHighJump :: EndEffect
//
//===========================================================================

void APowerHighJump::EndEffect( )
{
	// Nothing to do if there's no owner.
	if (Owner != NULL && Owner->player != NULL)
	{
		// Take away the high jump power.
		Owner->player->cheats &= ~CF_HIGHJUMP;
	}
}

