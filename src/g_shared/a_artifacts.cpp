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
#include "p_effect.h"
#include "a_artifacts.h"
#include "sbar.h"
#include "d_player.h"
#include "m_random.h"
#include "v_video.h"
#include "templates.h"
#include "a_morph.h"
#include "g_level.h"
#include "doomstat.h"
#include "v_palette.h"
#include "farchive.h"
#include "r_utility.h"

#include "r_data/colormaps.h"

static FRandom pr_torch ("Torch");

/* Those are no longer needed, except maybe as reference?
 * They're not used anywhere in the code anymore, except
 * MAULATORTICS as redefined in a_minotaur.cpp...
#define	INVULNTICS (30*TICRATE)
#define	INVISTICS (60*TICRATE)
#define	INFRATICS (120*TICRATE)
#define	IRONTICS (60*TICRATE)
#define WPNLEV2TICS (40*TICRATE)
#define FLIGHTTICS (60*TICRATE)
#define SPEEDTICS (45*TICRATE)
#define MAULATORTICS (25*TICRATE)
#define	TIMEFREEZE_TICS	( 12 * TICRATE )
*/

IMPLEMENT_CLASS (APowerup)

// Powerup-Giver -------------------------------------------------------------

IMPLEMENT_CLASS(PClassPowerupGiver)

void PClassPowerupGiver::ReplaceClassRef(PClass *oldclass, PClass *newclass)
{
	Super::ReplaceClassRef(oldclass, newclass);
	APowerupGiver *def = (APowerupGiver*)Defaults;
	if (def != NULL)
	{
		if (def->PowerupType == oldclass) def->PowerupType = static_cast<PClassWeapon *>(newclass);
	}
}

//===========================================================================
//
// APowerupGiver :: Use
//
//===========================================================================

bool APowerupGiver::Use (bool pickup)
{
	if (PowerupType == NULL) return true;	// item is useless

	APowerup *power = static_cast<APowerup *> (Spawn (PowerupType));

	if (EffectTics != 0)
	{
		power->EffectTics = EffectTics;
	}
	if (BlendColor != 0)
	{
		if (BlendColor != MakeSpecialColormap(65535)) power->BlendColor = BlendColor;
		else power->BlendColor = 0;
	}
	if (Mode != NAME_None)
	{
		power->Mode = Mode;
	}
	if (Strength != 0)
	{
		power->Strength = Strength;
	}

	power->ItemFlags |= ItemFlags & (IF_ALWAYSPICKUP|IF_ADDITIVETIME|IF_NOTELEPORTFREEZE);
	if (power->CallTryPickup (Owner))
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
	arc << EffectTics << BlendColor << Mode;
	arc << Strength;
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
	arc << EffectTics << BlendColor << Mode;
	arc << Strength;
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

	if (IsSpecialColormap(BlendColor)) return 0;
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
		int Colormap = GetSpecialColormap(BlendColor);

		if (Colormap != NOFIXEDCOLORMAP)
		{
			if (EffectTics > BLINKTHRESHOLD || (EffectTics & 8))
			{
				Owner->player->fixedcolormap = Colormap;
			}
			else if (Owner->player->fixedcolormap == Colormap)	
			{
				// only unset if the fixed colormap comes from this item
				Owner->player->fixedcolormap = NOFIXEDCOLORMAP;
			}
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
	int colormap = GetSpecialColormap(BlendColor);

	if (colormap != NOFIXEDCOLORMAP && Owner && Owner->player && Owner->player->fixedcolormap == colormap)
	{ // only unset if the fixed colormap comes from this item
		Owner->player->fixedcolormap = NOFIXEDCOLORMAP;
	}
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
	if (!Icon.isValid())
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
		// Color gets transferred if the new item has an effect.

		// Increase the effect's duration.
		if (power->ItemFlags & IF_ADDITIVETIME) 
		{
			EffectTics += power->EffectTics;
			BlendColor = power->BlendColor;
		}
		// If it's not blinking yet, you can't replenish the power unless the
		// powerup is required to be picked up.
		else if (EffectTics > BLINKTHRESHOLD && !(power->ItemFlags & IF_ALWAYSPICKUP))
		{
			return true;
		}
		// Reset the effect duration.
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
	// Get the effective effect time.
	EffectTics = abs (EffectTics);
	// Abuse the Owner field to tell the
	// InitEffect method who started it;
	// this should be cleared afterwards,
	// as this powerup instance is not
	// properly attached to anything yet.
	Owner = other;
	// Actually activate the powerup.
	InitEffect ();
	// Clear the Owner field, unless it was
	// changed by the activation, for example,
	// if this instance is a morph powerup;
	// the flag tells the caller that the
	// ownership has changed so that they
	// can properly handle the situation.
	if (!(ItemFlags & IF_CREATECOPYMOVED))
	{
		Owner = NULL;
	}
	// All done.
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

//===========================================================================
//
// AInventory :: GetNoTeleportFreeze
//
//===========================================================================

bool APowerup::GetNoTeleportFreeze ()
{
	if (ItemFlags & IF_NOTELEPORTFREEZE) return true;
	return Super::GetNoTeleportFreeze();
}

// Invulnerability Powerup ---------------------------------------------------

IMPLEMENT_CLASS (APowerInvulnerable)

//===========================================================================
//
// APowerInvulnerable :: InitEffect
//
//===========================================================================

void APowerInvulnerable::InitEffect ()
{
	Super::InitEffect();
	Owner->effects &= ~FX_RESPAWNINVUL;
	Owner->flags2 |= MF2_INVULNERABLE;
	if (Mode == NAME_None && Owner->IsKindOf(RUNTIME_CLASS(APlayerPawn)))
	{
		Mode = static_cast<PClassPlayerPawn *>(Owner->GetClass())->InvulMode;
	}
	if (Mode == NAME_Reflective)
	{
		Owner->flags2 |= MF2_REFLECTIVE;
	}
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

	if (Mode == NAME_Ghost)
	{
		if (!(Owner->flags & MF_SHADOW))
		{
			// Don't mess with the translucency settings if an
			// invisibility powerup is active.
			Owner->RenderStyle = STYLE_Translucent;
			if (!(level.time & 7) && Owner->Alpha > 0 && Owner->Alpha < 1)
			{
				if (Owner->Alpha == HX_SHADOW)
				{
					Owner->Alpha = HX_ALTSHADOW;
				}
				else
				{
					Owner->Alpha = 0;
					Owner->flags2 |= MF2_NONSHOOTABLE;
				}
			}
			if (!(level.time & 31))
			{
				if (Owner->Alpha == 0)
				{
					Owner->flags2 &= ~MF2_NONSHOOTABLE;
					Owner->Alpha = HX_ALTSHADOW;
				}
				else
				{
					Owner->Alpha = HX_SHADOW;
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
	Super::EndEffect();

	if (Owner == NULL)
	{
		return;
	}

	Owner->flags2 &= ~MF2_INVULNERABLE;
	Owner->effects &= ~FX_RESPAWNINVUL;
	if (Mode == NAME_Ghost)
	{
		Owner->flags2 &= ~MF2_NONSHOOTABLE;
		if (!(Owner->flags & MF_SHADOW))
		{
			// Don't mess with the translucency settings if an
			// invisibility powerup is active.
			Owner->RenderStyle = STYLE_Normal;
			Owner->Alpha = 1.;
		}
	}
	else if (Mode == NAME_Reflective)
	{
		Owner->flags2 &= ~MF2_REFLECTIVE;
	}

	if (Owner->player != NULL)
	{
		Owner->player->fixedcolormap = NOFIXEDCOLORMAP;
	}
}

//===========================================================================
//
// APowerInvulnerable :: AlterWeaponSprite
//
//===========================================================================

int APowerInvulnerable::AlterWeaponSprite (visstyle_t *vis)
{
	int changed = Inventory == NULL ? false : Inventory->AlterWeaponSprite(vis);
	if (Owner != NULL)
	{
		if (Mode == NAME_Ghost && !(Owner->flags & MF_SHADOW))
		{
			vis->Alpha = MIN<float>(0.25f + (float)Owner->Alpha*0.75f, 1.f);
		}
	}
	return changed;
}

// Strength (aka Berserk) Powerup --------------------------------------------

IMPLEMENT_CLASS (APowerStrength)

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
	Super::InitEffect();
}

//===========================================================================
//
// APowerStrength :: DoEffect
//
//===========================================================================

void APowerStrength::Tick ()
{
	// Strength counts up to diminish the fade.
	assert(EffectTics < (INT_MAX - 1)); // I can't see a game lasting nearly two years, but...
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
	// slowly fade the berserk out
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

IMPLEMENT_CLASS (APowerInvisibility)

// Invisibility flag combos
#define INVISIBILITY_FLAGS1	(MF_SHADOW)
#define INVISIBILITY_FLAGS3	(MF3_GHOST)
#define INVISIBILITY_FLAGS5	(MF5_CANTSEEK)

//===========================================================================
//
// APowerInvisibility :: InitEffect
//
//===========================================================================

void APowerInvisibility::InitEffect ()
{
	Super::InitEffect();
	// This used to call CommonInit(), which used to contain all the code that's repeated every
	// tic, plus the following code that needs to happen once and only once.
	// The CommonInit() code has been moved to DoEffect(), so this now ends with a call to DoEffect(),
	// and DoEffect() no longer needs to call InitEffect(). CommonInit() has been removed for being redundant.
	if (Owner != NULL)
	{
		flags &= ~(Owner->flags  & INVISIBILITY_FLAGS1);
		Owner->flags  |= flags & INVISIBILITY_FLAGS1;
		flags3 &= ~(Owner->flags3 & INVISIBILITY_FLAGS3);
		Owner->flags3 |= flags3 & INVISIBILITY_FLAGS3;
		flags5 &= ~(Owner->flags5 & INVISIBILITY_FLAGS5);
		Owner->flags5 |= flags5 & INVISIBILITY_FLAGS5;

		DoEffect();
	}
}

//===========================================================================
//
// APowerInvisibility :: DoEffect
//
//===========================================================================
void APowerInvisibility::DoEffect ()
{
	Super::DoEffect();
	// Due to potential interference with other PowerInvisibility items
	// the effect has to be refreshed each tic.
	double ts = (Strength / 100) * (special1 + 1);
	
	if (ts > 1.) ts = 1.;
	Owner->Alpha = clamp((1. - ts), 0., 1.);
	switch (Mode)
	{
	case (NAME_Fuzzy):
		Owner->RenderStyle = STYLE_OptFuzzy;
		break;
	case (NAME_Opaque):
		Owner->RenderStyle = STYLE_Normal;
		break;
	case (NAME_Additive):
		Owner->RenderStyle = STYLE_Add;
		break;
	case (NAME_Stencil):
		Owner->RenderStyle = STYLE_Stencil;
		break;
	case (NAME_AddStencil) :
		Owner->RenderStyle = STYLE_AddStencil;
		break;
	case (NAME_TranslucentStencil) :
		Owner->RenderStyle = STYLE_TranslucentStencil;
		break;
	case (NAME_None) :
	case (NAME_Cumulative):
	case (NAME_Translucent):
		Owner->RenderStyle = STYLE_Translucent;
		break;
	default: // Something's wrong
		Owner->RenderStyle = STYLE_Normal;
		Owner->Alpha = 1.;
		break;
	}
}

//===========================================================================
//
// APowerInvisibility :: EndEffect
//
//===========================================================================

void APowerInvisibility::EndEffect ()
{
	Super::EndEffect();
	if (Owner != NULL)
	{
		Owner->flags  &= ~(flags  & INVISIBILITY_FLAGS1);
		Owner->flags3 &= ~(flags3 & INVISIBILITY_FLAGS3);
		Owner->flags5 &= ~(flags5 & INVISIBILITY_FLAGS5);

		Owner->RenderStyle = STYLE_Normal;
		Owner->Alpha = 1.;

		// Check whether there are other invisibility items and refresh their effect.
		// If this isn't done there will be one incorrectly drawn frame when this
		// item expires.
		AInventory *item = Owner->Inventory;
		while (item != NULL)
		{
			if (item->IsKindOf(RUNTIME_CLASS(APowerInvisibility)) && item != this)
			{
				static_cast<APowerInvisibility*>(item)->DoEffect();
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

int APowerInvisibility::AlterWeaponSprite (visstyle_t *vis)
{
	int changed = Inventory == NULL ? false : Inventory->AlterWeaponSprite(vis);
	// Blink if the powerup is wearing off
	if (changed == 0 && EffectTics < 4*32 && !(EffectTics & 8))
	{
		vis->RenderStyle = STYLE_Normal;
		vis->Alpha = 1.f;
		return 1;
	}
	else if (changed == 1)
	{
		// something else set the weapon sprite back to opaque but this item is still active.
		float ts = float((Strength / 100) * (special1 + 1));
		vis->Alpha = clamp<>((1.f - ts), 0.f, 1.f);
		switch (Mode)
		{
		case (NAME_Fuzzy):
			vis->RenderStyle = STYLE_OptFuzzy;
			break;
		case (NAME_Opaque):
			vis->RenderStyle = STYLE_Normal;
			break;
		case (NAME_Additive):
			vis->RenderStyle = STYLE_Add;
			break;
		case (NAME_Stencil):
			vis->RenderStyle = STYLE_Stencil;
			break;
		case (NAME_TranslucentStencil) :
			vis->RenderStyle = STYLE_TranslucentStencil;
			break;
		case (NAME_AddStencil) :
			vis->RenderStyle = STYLE_AddStencil;
			break;
		case (NAME_None) :
		case (NAME_Cumulative):
		case (NAME_Translucent):
		default:
			vis->RenderStyle = STYLE_Translucent;
			break;
		}
	}
	// Handling of Strife-like cumulative invisibility powerups, the weapon itself shouldn't become invisible
	if ((vis->Alpha < 0.25f && special1 > 0) || (vis->Alpha == 0))
	{
		vis->Alpha = clamp((1.f - float(Strength/100)), 0.f, 1.f);
		vis->colormap = SpecialColormaps[INVERSECOLORMAP].Colormap;
	}
	return -1;	// This item is valid so another one shouldn't reset the translucency
}

//===========================================================================
//
// APowerInvisibility :: HandlePickup
//
// If the player already has the first stage of a cumulative powerup, getting 
// it again increases the player's alpha. (But shouldn't this be in Use()?)
//
//===========================================================================

bool APowerInvisibility::HandlePickup (AInventory *item)
{
	if (Mode == NAME_Cumulative && ((Strength * special1) < 1.) && item->GetClass() == GetClass())
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
		special1++;	// increases power
		power->ItemFlags |= IF_PICKUPGOOD;
		return true;
	}
	return Super::HandlePickup (item);
}

// Ironfeet Powerup ----------------------------------------------------------

IMPLEMENT_CLASS (APowerIronFeet)

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
	}
	else if (Inventory != NULL)
	{
		Inventory->AbsorbDamage (damage, damageType, newdamage);
	}
}

//===========================================================================
//
// APowerIronFeet :: DoEffect
//
//===========================================================================

void APowerIronFeet::DoEffect ()
{
	if (Owner->player != NULL)
	{
		Owner->player->mo->ResetAirSupply ();
	}
}


// Strife Environment Suit Powerup -------------------------------------------

IMPLEMENT_CLASS (APowerMask)

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

IMPLEMENT_CLASS (APowerLightAmp)

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
			Owner->player->fixedlightlevel = 1;
		}
		else
		{
			Owner->player->fixedlightlevel = -1;
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
	Super::EndEffect();
	if (Owner != NULL && Owner->player != NULL && Owner->player->fixedcolormap < NUMCOLORMAPS)
	{
		Owner->player->fixedlightlevel = -1;
	}
}

// Torch Powerup -------------------------------------------------------------

IMPLEMENT_CLASS (APowerTorch)

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
				if (Owner->player->fixedlightlevel + NewTorchDelta > 7
					|| Owner->player->fixedlightlevel + NewTorchDelta < 0
					|| NewTorch == Owner->player->fixedlightlevel)
				{
					NewTorch = 0;
				}
				else
				{
					Owner->player->fixedlightlevel += NewTorchDelta;
				}
			}
			else
			{
				NewTorch = (pr_torch() & 7) + 1;
				NewTorchDelta = (NewTorch == Owner->player->fixedlightlevel) ?
					0 : ((NewTorch > Owner->player->fixedlightlevel) ? 1 : -1);
			}
		}
	}
}

// Flight (aka Wings of Wrath) powerup ---------------------------------------

IMPLEMENT_CLASS (APowerFlight)

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
	Super::InitEffect();
	Owner->flags2 |= MF2_FLY;
	Owner->flags |= MF_NOGRAVITY;
	if (Owner->Z() <= Owner->floorz)
	{
		Owner->Vel.Z = 4;;	// thrust the player in the air a bit
	}
	if (Owner->Vel.Z <= -35)
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
	if (!multiplayer && (level.flags2 & LEVEL2_INFINITE_FLIGHT))
	{
		assert(EffectTics < INT_MAX); // I can't see a game lasting nearly two years, but...
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
	Super::EndEffect();
	if (Owner == NULL || Owner->player == NULL)
	{
		return;
	}

	if (!(Owner->flags7 & MF7_FLYCHEAT))
	{
		if (Owner->Z() != Owner->floorz)
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
	// If this item got a valid icon use that instead of the default spinning wings.
	if (Icon.isValid())
	{
		return Super::DrawPowerup(x, y);
	}

	if (EffectTics > BLINKTHRESHOLD || !(EffectTics & 16))
	{
		FTextureID picnum = TexMan.CheckForTexture ("SPFLY0", FTexture::TEX_MiscPatch);
		int frame = (level.time/3) & 15;

		if (!picnum.isValid())
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

IMPLEMENT_CLASS (APowerWeaponLevel2)

//===========================================================================
//
// APowerWeaponLevel2 :: InitEffect
//
//===========================================================================

void APowerWeaponLevel2::InitEffect ()
{
	AWeapon *weapon, *sister;

	Super::InitEffect();

	if (Owner->player == nullptr)
		return;

	weapon = Owner->player->ReadyWeapon;

	if (weapon == nullptr)
		return;

	sister = weapon->SisterWeapon;

	if (sister == nullptr)
		return;

	if (!(sister->WeaponFlags & WIF_POWERED_UP))
		return;

	assert (sister->SisterWeapon == weapon);


	if (weapon->GetReadyState() != sister->GetReadyState())
	{
		Owner->player->ReadyWeapon = sister;
		P_SetPsprite(Owner->player, PSP_WEAPON, sister->GetReadyState());
	}
	else
	{
		DPSprite *psp = Owner->player->FindPSprite(PSP_WEAPON);
		if (psp != nullptr && psp->GetCaller() == Owner->player->ReadyWeapon)
		{
			// If the weapon changes but the state does not, we have to manually change the PSprite's caller here.
			psp->SetCaller(sister);
			Owner->player->ReadyWeapon = sister;
		}
		else
		{
			// Something went wrong. Initiate a regular weapon change.
			Owner->player->PendingWeapon = sister;
		}
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

	Super::EndEffect();
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
	DECLARE_CLASS (APlayerSpeedTrail, AActor)
public:
	void Tick ();
};

IMPLEMENT_CLASS (APlayerSpeedTrail)

//===========================================================================
//
// APlayerSpeedTrail :: Tick
//
//===========================================================================

void APlayerSpeedTrail::Tick ()
{
	const double fade = .6 / 8;
	if (Alpha <= fade)
	{
		Destroy ();
	}
	else
	{
		Alpha -= fade;
	}
}

// Speed Powerup -------------------------------------------------------------

IMPLEMENT_CLASS (APowerSpeed)

//===========================================================================
//
// APowerSpeed :: Serialize
//
//===========================================================================

void APowerSpeed::Serialize(FArchive &arc)
{
	Super::Serialize (arc);
	arc << SpeedFlags;
}

//===========================================================================
//
// APowerSpeed :: GetSpeedFactor
//
//===========================================================================

double APowerSpeed ::GetSpeedFactor ()
{
	if (Inventory != NULL)
		return Speed * Inventory->GetSpeedFactor();
	else
		return Speed;
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

	if (SpeedFlags & PSF_NOTRAIL)
		return;

	if (level.time & 1)
		return;

	// Check if another speed item is present to avoid multiple drawing of the speed trail.
	// Only the last PowerSpeed without PSF_NOTRAIL set will actually draw the trail.
	for (AInventory *item = Inventory; item != NULL; item = item->Inventory)
	{
		if (item->IsKindOf(RUNTIME_CLASS(APowerSpeed)) &&
			!(static_cast<APowerSpeed *>(item)->SpeedFlags & PSF_NOTRAIL))
		{
			return;
		}
	}

	if (Owner->Vel.LengthSquared() <= 12*12)
		return;

	AActor *speedMo = Spawn<APlayerSpeedTrail> (Owner->Pos(), NO_REPLACE);
	if (speedMo)
	{
		speedMo->Angles.Yaw = Owner->Angles.Yaw;
		speedMo->Translation = Owner->Translation;
		speedMo->target = Owner;
		speedMo->sprite = Owner->sprite;
		speedMo->frame = Owner->frame;
		speedMo->Floorclip = Owner->Floorclip;

		// [BC] Also get the scale from the owner.
		speedMo->Scale = Owner->Scale;

		if (Owner == players[consoleplayer].camera &&
			!(Owner->player->cheats & CF_CHASECAM))
		{
			speedMo->renderflags |= RF_INVISIBLE;
		}
	}
}

// Minotaur (aka Dark Servant) powerup ---------------------------------------

IMPLEMENT_CLASS (APowerMinotaur)

// Targeter powerup ---------------------------------------------------------

IMPLEMENT_CLASS (APowerTargeter)

void APowerTargeter::Travelled ()
{
	InitEffect ();
}

void APowerTargeter::InitEffect ()
{
	// Why is this called when the inventory isn't even attached yet
	// in APowerup::CreateCopy?
	if (!Owner->FindInventory(GetClass(), true))
		return;

	player_t *player;

	Super::InitEffect();

	if ((player = Owner->player) == nullptr)
		return;

	FState *state = FindState("Targeter");

	if (state != nullptr)
	{
		P_SetPsprite(player, PSP_TARGETCENTER,  state + 0);
		P_SetPsprite(player, PSP_TARGETLEFT,  state + 1);
		P_SetPsprite(player, PSP_TARGETRIGHT, state + 2);
	}

	player->GetPSprite(PSP_TARGETCENTER)->x = (160-3);
	player->GetPSprite(PSP_TARGETCENTER)->y =
		player->GetPSprite(PSP_TARGETLEFT)->y =
		player->GetPSprite(PSP_TARGETRIGHT)->y = (100-3);
	PositionAccuracy ();
}

void APowerTargeter::AttachToOwner(AActor *other)
{
	Super::AttachToOwner(other);

	// Let's actually properly call this for the targeters.
	InitEffect();
}

bool APowerTargeter::HandlePickup(AInventory *item)
{
	if (Super::HandlePickup(item))
	{
		InitEffect();	// reset the HUD sprites
		return true;
	}
	return false;
}

void APowerTargeter::DoEffect ()
{
	Super::DoEffect ();

	if (Owner != nullptr && Owner->player != nullptr)
	{
		player_t *player = Owner->player;

		PositionAccuracy ();
		if (EffectTics < 5*TICRATE)
		{
			FState *state = FindState("Targeter");

			if (state != nullptr)
			{
				if (EffectTics & 32)
				{
					P_SetPsprite(player, PSP_TARGETRIGHT, nullptr);
					P_SetPsprite(player, PSP_TARGETLEFT,  state + 1);
				}
				else if (EffectTics & 16)
				{
					P_SetPsprite(player, PSP_TARGETRIGHT, state + 2);
					P_SetPsprite(player, PSP_TARGETLEFT,  nullptr);
				}
			}
		}
	}
}

void APowerTargeter::EndEffect ()
{
	Super::EndEffect();
	if (Owner != nullptr && Owner->player != nullptr)
	{
		// Calling GetPSprite here could crash if we're creating a new game.
		// This is because P_SetupLevel nulls the player's mo before destroying
		// every DThinker which in turn ends up calling this.
		// However P_SetupLevel is only called after G_NewInit which calls
		// every player's dtor which destroys all their psprites.
		DPSprite *pspr;
		if ((pspr = Owner->player->FindPSprite(PSP_TARGETCENTER)) != nullptr) pspr->SetState(nullptr);
		if ((pspr = Owner->player->FindPSprite(PSP_TARGETLEFT)) != nullptr) pspr->SetState(nullptr);
		if ((pspr = Owner->player->FindPSprite(PSP_TARGETRIGHT)) != nullptr) pspr->SetState(nullptr);
	}
}

void APowerTargeter::PositionAccuracy ()
{
	player_t *player = Owner->player;

	if (player != nullptr)
	{
		player->GetPSprite(PSP_TARGETLEFT)->x = (160-3) - ((100 - player->mo->accuracy));
		player->GetPSprite(PSP_TARGETRIGHT)->x = (160-3)+ ((100 - player->mo->accuracy));
	}
}

// Frightener Powerup --------------------------------

IMPLEMENT_CLASS (APowerFrightener)

//===========================================================================
//
// APowerFrightener :: InitEffect
//
//===========================================================================

void APowerFrightener::InitEffect ()
{
	Super::InitEffect();

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
	Super::EndEffect();

	if (Owner== NULL || Owner->player == NULL)
		return;

	Owner->player->cheats &= ~CF_FRIGHTENING;
}

// Buddha Powerup --------------------------------

IMPLEMENT_CLASS (APowerBuddha)

//===========================================================================
//
// APowerBuddha :: InitEffect
//
//===========================================================================

void APowerBuddha::InitEffect ()
{
	Super::InitEffect();

	if (Owner== NULL || Owner->player == NULL)
		return;

	Owner->player->cheats |= CF_BUDDHA;
}

//===========================================================================
//
// APowerBuddha :: EndEffect
//
//===========================================================================

void APowerBuddha::EndEffect ()
{
	Super::EndEffect();

	if (Owner== NULL || Owner->player == NULL)
		return;

	Owner->player->cheats &= ~CF_BUDDHA;
}

// Scanner powerup ----------------------------------------------------------

IMPLEMENT_CLASS (APowerScanner)

// Time freezer powerup -----------------------------------------------------

IMPLEMENT_CLASS( APowerTimeFreezer)

//===========================================================================
//
// APowerTimeFreezer :: InitEffect
//
//===========================================================================

void APowerTimeFreezer::InitEffect()
{
	int freezemask;

	Super::InitEffect();

	if (Owner == NULL || Owner->player == NULL)
		return;

	// When this powerup is in effect, pause the music.
	S_PauseSound(false, false);

	// Give the player and his teammates the power to move when time is frozen.
	freezemask = 1 << (Owner->player - players);
	Owner->player->timefreezer |= freezemask;
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] &&
			players[i].mo != NULL &&
			players[i].mo->IsTeammate(Owner)
		   )
		{
			players[i].timefreezer |= freezemask;
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
		level.flags2 |= LEVEL2_FROZEN;
	}
	else
	{
		// Compensate for skipped tic, but beware of overflow.
		if(EffectTics < INT_MAX)
			EffectTics++;
	}
}

//===========================================================================
//
// APowerTimeFreezer :: DoEffect
//
//===========================================================================

void APowerTimeFreezer::DoEffect()
{
	Super::DoEffect();
	// [RH] Do not change LEVEL_FROZEN on odd tics, or the Revenant's tracer
	// will get thrown off.
	// [ED850] Don't change it if the player is predicted either.
	if (level.time & 1 || (Owner != NULL && Owner->player != NULL && Owner->player->cheats & CF_PREDICTING))
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
		level.flags2 |= LEVEL2_FROZEN;
	else
		level.flags2 &= ~LEVEL2_FROZEN;
}

//===========================================================================
//
// APowerTimeFreezer :: EndEffect
//
//===========================================================================

void APowerTimeFreezer::EndEffect()
{
	int	i;

	Super::EndEffect();

	// If there is an owner, remove the timefreeze flag corresponding to
	// her from all players.
	if (Owner != NULL && Owner->player != NULL)
	{
		int freezemask = ~(1 << (Owner->player - players));
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			players[i].timefreezer &= freezemask;
		}
	}

	// Are there any players who still have timefreezer bits set?
	for (i = 0; i < MAXPLAYERS; ++i)
	{
		if (playeringame[i] && players[i].timefreezer != 0)
		{
			break;
		}
	}

	if (i == MAXPLAYERS)
	{
		// No, so allow other actors to move about freely once again.
		level.flags2 &= ~LEVEL2_FROZEN;

		// Also, turn the music back on.
		S_ResumeSound(false);
	}
}

// Damage powerup ------------------------------------------------------

IMPLEMENT_CLASS(APowerDamage)

//===========================================================================
//
// APowerDamage :: InitEffect
//
//===========================================================================

void APowerDamage::InitEffect( )
{
	Super::InitEffect();

	// Use sound channel 5 to avoid interference with other actions.
	if (Owner != NULL) S_Sound(Owner, 5, SeeSound, 1.0f, ATTN_NONE);
}

//===========================================================================
//
// APowerDamage :: EndEffect
//
//===========================================================================

void APowerDamage::EndEffect( )
{
	Super::EndEffect();
	// Use sound channel 5 to avoid interference with other actions.
	if (Owner != NULL) S_Sound(Owner, 5, DeathSound, 1.0f, ATTN_NONE);
}

//===========================================================================
//
// APowerDamage :: ModifyDamage
//
//===========================================================================

void APowerDamage::ModifyDamage(int damage, FName damageType, int &newdamage, bool passive)
{
	if (!passive && damage > 0)
	{
		int newdam;
		DmgFactors *df = GetClass()->DamageFactors;
		if (df != NULL && df->CountUsed() != 0)
		{
			newdam = MAX(1, df->Apply(damageType, damage));// don't allow zero damage as result of an underflow
		}
		else
		{
			newdam = damage * 4;
		}
		if (Owner != NULL && newdam > damage) S_Sound(Owner, 5, ActiveSound, 1.0f, ATTN_NONE);
		newdamage = newdam;
	}
	if (Inventory != NULL) Inventory->ModifyDamage(damage, damageType, newdamage, passive);
}

// Quarter damage powerup ------------------------------------------------------

IMPLEMENT_CLASS(APowerProtection)

#define PROTECTION_FLAGS3	(MF3_NORADIUSDMG | MF3_DONTMORPH | MF3_DONTSQUASH | MF3_DONTBLAST | MF3_NOTELEOTHER)
#define PROTECTION_FLAGS5	(MF5_NOPAIN | MF5_DONTRIP)

//===========================================================================
//
// APowerProtection :: InitEffect
//
//===========================================================================

void APowerProtection::InitEffect( )
{
	Super::InitEffect();

	if (Owner != NULL)
	{
		S_Sound(Owner, CHAN_AUTO, SeeSound, 1.0f, ATTN_NONE);

		// Transfer various protection flags if owner does not already have them.
		// If the owner already has the flag, clear it from the powerup.
		// If the powerup still has a flag set, add it to the owner.
		flags3 &= ~(Owner->flags3 & PROTECTION_FLAGS3);
		Owner->flags3 |= flags3 & PROTECTION_FLAGS3;

		flags5 &= ~(Owner->flags5 & PROTECTION_FLAGS5);
		Owner->flags5 |= flags5 & PROTECTION_FLAGS5;
	}
}

//===========================================================================
//
// APowerProtection :: EndEffect
//
//===========================================================================

void APowerProtection::EndEffect( )
{
	Super::EndEffect();
	if (Owner != NULL)
	{
		S_Sound(Owner, CHAN_AUTO, DeathSound, 1.0f, ATTN_NONE);
		Owner->flags3 &= ~(flags3 & PROTECTION_FLAGS3);
		Owner->flags5 &= ~(flags5 & PROTECTION_FLAGS5);
	}
}

//===========================================================================
//
// APowerProtection :: AbsorbDamage
//
//===========================================================================

void APowerProtection::ModifyDamage(int damage, FName damageType, int &newdamage, bool passive)
{
	if (passive && damage > 0)
	{
		int newdam;
		DmgFactors *df = GetClass()->DamageFactors;
		if (df != NULL && df->CountUsed() != 0)
		{
			newdam = MAX(0, df->Apply(damageType, damage));
		}
		else
		{
			newdam = damage / 4;
		}
		if (Owner != NULL && newdam < damage) S_Sound(Owner, CHAN_AUTO, ActiveSound, 1.0f, ATTN_NONE);
		newdamage = newdam;
	}
	if (Inventory != NULL)
	{
		Inventory->ModifyDamage(damage, damageType, newdamage, passive);
	}
}

// Drain rune -------------------------------------------------------

IMPLEMENT_CLASS(APowerDrain)

//===========================================================================
//
// ARuneDrain :: InitEffect
//
//===========================================================================

void APowerDrain::InitEffect( )
{
	Super::InitEffect();

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
	Super::EndEffect();

	// Nothing to do if there's no owner.
	if (Owner != NULL && Owner->player != NULL)
	{
		// Take away the drain power.
		Owner->player->cheats &= ~CF_DRAIN;
	}
}


// Regeneration rune -------------------------------------------------------

IMPLEMENT_CLASS(APowerRegeneration)

//===========================================================================
//
// APowerRegeneration :: DoEffect
//
//===========================================================================

void APowerRegeneration::DoEffect()
{
	Super::DoEffect();
	if (Owner != NULL && Owner->health > 0 && (level.time & 31) == 0)
	{
		if (P_GiveBody(Owner, int(Strength)))
		{
			S_Sound(Owner, CHAN_ITEM, "*regenerate", 1, ATTN_NORM );
		}
	}
}

// High jump rune -------------------------------------------------------

IMPLEMENT_CLASS(APowerHighJump)

//===========================================================================
//
// ARuneHighJump :: InitEffect
//
//===========================================================================

void APowerHighJump::InitEffect( )
{
	Super::InitEffect();

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
	Super::EndEffect();
	// Nothing to do if there's no owner.
	if (Owner != NULL && Owner->player != NULL)
	{
		// Take away the high jump power.
		Owner->player->cheats &= ~CF_HIGHJUMP;
	}
}

// Double firing speed rune ---------------------------------------------

IMPLEMENT_CLASS(APowerDoubleFiringSpeed)

//===========================================================================
//
// APowerDoubleFiringSpeed :: InitEffect
//
//===========================================================================

void APowerDoubleFiringSpeed::InitEffect( )
{
	Super::InitEffect();

	if (Owner== NULL || Owner->player == NULL)
		return;

	// Give the player the power to shoot twice as fast.
	Owner->player->cheats |= CF_DOUBLEFIRINGSPEED;
}

//===========================================================================
//
// APowerDoubleFiringSpeed :: EndEffect
//
//===========================================================================

void APowerDoubleFiringSpeed::EndEffect( )
{
	Super::EndEffect();
	// Nothing to do if there's no owner.
	if (Owner != NULL && Owner->player != NULL)
	{
		// Take away the shooting twice as fast power.
		Owner->player->cheats &= ~CF_DOUBLEFIRINGSPEED;
	}
}

// Morph powerup ------------------------------------------------------

IMPLEMENT_CLASS(APowerMorph)

//===========================================================================
//
// APowerMorph :: Serialize
//
//===========================================================================

void APowerMorph::Serialize (FArchive &arc)
{
	Super::Serialize (arc);
	arc << PlayerClass << MorphStyle << MorphFlash << UnMorphFlash;
	arc << Player;
}

//===========================================================================
//
// APowerMorph :: InitEffect
//
//===========================================================================

void APowerMorph::InitEffect( )
{
	Super::InitEffect();

	if (Owner != NULL && Owner->player != NULL && PlayerClass != NAME_None)
	{
		player_t *realplayer = Owner->player;	// Remember the identity of the player
		PClassActor *morph_flash = PClass::FindActor(MorphFlash);
		PClassActor *unmorph_flash = PClass::FindActor(UnMorphFlash);
		PClassPlayerPawn *player_class = dyn_cast<PClassPlayerPawn>(PClass::FindClass (PlayerClass));
		if (P_MorphPlayer(realplayer, realplayer, player_class, -1/*INDEFINITELY*/, MorphStyle, morph_flash, unmorph_flash))
		{
			Owner = realplayer->mo;				// Replace the new owner in our owner; safe because we are not attached to anything yet
			ItemFlags |= IF_CREATECOPYMOVED;	// Let the caller know the "real" owner has changed (to the morphed actor)
			Player = realplayer;				// Store the player identity (morphing clears the unmorphed actor's "player" field)
		}
		else // morph failed - give the caller an opportunity to fail the pickup completely
		{
			ItemFlags |= IF_INITEFFECTFAILED;	// Let the caller know that the activation failed (can fail the pickup if appropriate)
		}
	}
}

//===========================================================================
//
// APowerMorph :: EndEffect
//
//===========================================================================

void APowerMorph::EndEffect( )
{
	Super::EndEffect();

	// Abort if owner already destroyed
	if (Owner == NULL)
	{
		assert(Player == NULL);
		return;
	}
	
	// Abort if owner already unmorphed
	if (Player == NULL)
	{
		return;
	}

	// Abort if owner is dead; their Die() method will
	// take care of any required unmorphing on death.
	if (Player->health <= 0)
	{
		return;
	}

	// Unmorph if possible
	if (!bNoCallUndoMorph)
	{
		int savedMorphTics = Player->morphTics;
		P_UndoPlayerMorph (Player, Player, 0, !!(Player->MorphStyle & MORPH_UNDOALWAYS));

		// Abort if unmorph failed; in that case,
		// set the usual retry timer and return.
		if (Player != NULL && Player->morphTics)
		{
			// Transfer retry timeout
			// to the powerup's timer.
			EffectTics = Player->morphTics;
			// Reload negative morph tics;
			// use actual value; it may
			// be in use for animation.
			Player->morphTics = savedMorphTics;
			// Try again some time later
			return;
		}
	}
	// Unmorph suceeded
	Player = NULL;
}

// Infinite Ammo Powerup -----------------------------------------------------

IMPLEMENT_CLASS(APowerInfiniteAmmo)

//===========================================================================
//
// APowerInfiniteAmmo :: InitEffect
//
//===========================================================================

void APowerInfiniteAmmo::InitEffect( )
{
	Super::InitEffect();

	if (Owner== NULL || Owner->player == NULL)
		return;

	// Give the player infinite ammo
	Owner->player->cheats |= CF_INFINITEAMMO;
}

//===========================================================================
//
// APowerInfiniteAmmo :: EndEffect
//
//===========================================================================

void APowerInfiniteAmmo::EndEffect( )
{
	Super::EndEffect();

	// Nothing to do if there's no owner.
	if (Owner != NULL && Owner->player != NULL)
	{
		// Take away the limitless ammo
		Owner->player->cheats &= ~CF_INFINITEAMMO;
	}
}

