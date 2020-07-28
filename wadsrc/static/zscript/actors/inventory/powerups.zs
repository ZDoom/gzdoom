class PowerupGiver : Inventory
{
	
	Class<Actor> PowerupType;
	int EffectTics;		// Non-0 to override the powerup's default tics
	color BlendColor;	// Non-0 to override the powerup's default blend
	Name Mode;			// Meaning depends on powerup - used for Invulnerability and Invisibility
	double Strength;	// Meaning depends on powerup - currently used only by Invisibility
	
	property prefix: Powerup;
	property Strength: Strength;
	property Mode: Mode;
	
	Default
	{
		Inventory.DefMaxAmount;
		+INVENTORY.INVBAR
		+INVENTORY.FANCYPICKUPSOUND
		Inventory.PickupSound "misc/p_pkup";
	}
	
	//===========================================================================
	//
	// APowerupGiver :: Use
	//
	//===========================================================================

	override bool Use (bool pickup)
	{
		if (PowerupType == NULL) return true;	// item is useless
		if (Owner == null) return true;

		let power = Powerup(Spawn (PowerupType));

		if (EffectTics != 0)
		{
			power.EffectTics = EffectTics;
		}
		if (BlendColor != 0)
		{
			if (BlendColor != Powerup.SPECIALCOLORMAP_MASK | 65535) power.BlendColor = BlendColor;
			else power.BlendColor = 0;
		}
		if (Mode != 'None')
		{
			power.Mode = Mode;
		}
		if (Strength != 0)
		{
			power.Strength = Strength;
		}

		power.bAlwaysPickup |= bAlwaysPickup;
		power.bAdditiveTime |= bAdditiveTime;
		power.bNoTeleportFreeze |= bNoTeleportFreeze;
		if (power.CallTryPickup (Owner))
		{
			return true;
		}
		power.GoAwayAndDie ();
		return false;
	}
}

class Powerup : Inventory
{
	int EffectTics;	
	color BlendColor;
	Name Mode;			// Meaning depends on powerup - used for Invulnerability and Invisibility
	double Strength;		// Meaning depends on powerup - currently used only by Invisibility
	int Colormap;
	const SPECIALCOLORMAP_MASK =  0x00b60000;

	property Strength: Strength;
	property Mode: Mode;
	
	// Note, that while this is an inventory flag, it only has meaning on an active powerup.
	override bool GetNoTeleportFreeze() 
	{ 
		return bNoTeleportFreeze; 
	}

	//===========================================================================
	//
	// APowerup :: Tick
	//
	//===========================================================================

	override void Tick ()
	{
		// Powerups cannot exist outside an inventory
		if (Owner == NULL)
		{
			Destroy ();
		}
		if (EffectTics == 0 || (EffectTics > 0 && --EffectTics == 0))
		{
			Destroy ();
		}
	}

	//===========================================================================
	//
	// APowerup :: HandlePickup
	//
	//===========================================================================

	override bool HandlePickup (Inventory item)
	{
		if (item.GetClass() == GetClass())
		{
			let power = Powerup(item);
			if (power.EffectTics == 0)
			{
				power.bPickupGood = true;
				return true;
			}
			// Color gets transferred if the new item has an effect.

			// Increase the effect's duration.
			if (power.bAdditiveTime) 
			{
				EffectTics += power.EffectTics;
				BlendColor = power.BlendColor;
			}
			// If it's not blinking yet, you can't replenish the power unless the
			// powerup is required to be picked up.
			else if (EffectTics > BLINKTHRESHOLD && !power.bAlwaysPickup)
			{
				return true;
			}
			// Reset the effect duration.
			else if (power.EffectTics > EffectTics)
			{
				EffectTics = power.EffectTics;
				BlendColor = power.BlendColor;
			}
			power.bPickupGood = true;
			return true;
		}
		return false;
	}

	//===========================================================================
	//
	// APowerup :: CreateCopy
	//
	//===========================================================================

	override Inventory CreateCopy (Actor other)
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
		if (!bCreateCopyMoved)
		{
			Owner = NULL;
		}
		// All done.
		return self;
	}

	//===========================================================================
	//
	// APowerup :: CreateTossable
	//
	// Powerups are never droppable, even without IF_UNDROPPABLE set.
	//
	//===========================================================================

	override Inventory CreateTossable (int amount)
	{
		return NULL;
	}

	//===========================================================================
	//
	// APowerup :: InitEffect
	//
	//===========================================================================

	virtual void InitEffect() 
	{
		// initialize this only once instead of recalculating repeatedly.
		Colormap = ((BlendColor & 0xFFFF0000) == SPECIALCOLORMAP_MASK)? BlendColor & 0xffff : PlayerInfo.NOFIXEDCOLORMAP;
	}

	//===========================================================================
	//
	// APowerup :: DoEffect
	//
	//===========================================================================

	override void DoEffect ()
	{
		if (Owner == NULL || Owner.player == NULL)
		{
			return;
		}

		if (EffectTics > 0)
		{
			if (Colormap != PlayerInfo.NOFIXEDCOLORMAP)
			{
				if (!isBlinking())
				{
					Owner.player.fixedcolormap = Colormap;
				}
				else if (Owner.player.fixedcolormap == Colormap)	
				{
					// only unset if the fixed colormap comes from this item
					Owner.player.fixedcolormap = PlayerInfo.NOFIXEDCOLORMAP;
				}
			}
		}
	}

	//===========================================================================
	//
	// APowerup :: EndEffect
	//
	//===========================================================================

	virtual void EndEffect ()
	{
		if (colormap != PlayerInfo.NOFIXEDCOLORMAP && Owner && Owner.player && Owner.player.fixedcolormap == colormap)
		{ // only unset if the fixed colormap comes from this item
			Owner.player.fixedcolormap = PlayerInfo.NOFIXEDCOLORMAP;
		}
	}

	//===========================================================================
	//
	// APowerup :: Destroy
	//
	//===========================================================================

	override void OnDestroy ()
	{
		EndEffect ();
		Super.OnDestroy();
	}

	//===========================================================================
	//
	// APowerup :: GetBlend
	//
	//===========================================================================

	override color GetBlend ()
	{
		if (Colormap != Player.NOFIXEDCOLORMAP) return 0;
		if (isBlinking()) return 0;
		return BlendColor;
	}

	//===========================================================================
	//
	// Inventory :: GetPowerupIcon
	//
	// Returns the icon that should be drawn for an active powerup.
	//
	//===========================================================================

	virtual clearscope version("2.5") TextureID GetPowerupIcon() const
	{
		return Icon;
	}

	//===========================================================================
	//
	// APowerup :: isBlinking 
	//
	//===========================================================================

	virtual clearscope bool isBlinking() const
	{
		return (EffectTics <= BLINKTHRESHOLD && (EffectTics & 8) && !bNoScreenBlink);
	}

	//===========================================================================
	//
	// APowerup :: OwnerDied
	//
	// Powerups don't last beyond death.
	//
	//===========================================================================

	override void OwnerDied ()
	{
		Destroy ();
	}

	
}

//===========================================================================
//
// Invulnerable
//
//===========================================================================

class PowerInvulnerable : Powerup
{
	Default
	{
		Powerup.Duration -30;
		inventory.icon "SPSHLD0";
	}

	//===========================================================================
	//
	// APowerInvulnerable :: InitEffect
	//
	//===========================================================================

	override void InitEffect ()
	{
		Super.InitEffect();
		Owner.bRespawnInvul = false;
		Owner.bInvulnerable = true;
		if (Mode == 'None' && Owner is "PlayerPawn")
		{
			Mode = PlayerPawn(Owner).InvulMode;
		}
		if (Mode == 'Reflective')
		{
			Owner.bReflective = true;
		}
	}

	//===========================================================================
	//
	// APowerInvulnerable :: DoEffect
	//
	//===========================================================================

	override void DoEffect ()
	{
		Super.DoEffect ();

		if (Owner == NULL)
		{
			return;
		}
		Owner.bInvulnerable = true;
		if (Mode == 'Reflective')
		{
			Owner.bReflective = true;
		}
		if (Mode == 'Ghost')
		{
			if (!Owner.bShadow)
			{
				// Don't mess with the translucency settings if an
				// invisibility powerup is active.
				let alpha = Owner.Alpha;
				if (!(Level.maptime & 7) && alpha > 0 && alpha < 1)
				{
					if (alpha == HX_SHADOW)
					{
						alpha = HX_ALTSHADOW;
					}
					else
					{
						alpha = 0;
						Owner.bNonShootable = true;
					}
				}
				if (!(Level.maptime & 31))
				{
					if (alpha == 0)
					{
						Owner.bNonShootable = false;
						alpha = HX_ALTSHADOW;
					}
					else
					{
						alpha = HX_SHADOW;
					}
				}
				Owner.A_SetRenderStyle(alpha, STYLE_Translucent);
			}
			else
			{
				Owner.bNonShootable = false;
			}
		}
	}

	//===========================================================================
	//
	// APowerInvulnerable :: EndEffect
	//
	//===========================================================================

	override void EndEffect ()
	{
		Super.EndEffect();

		if (Owner == NULL)
		{
			return;
		}

		Owner.bRespawnInvul = false;
		Owner.bInvulnerable = false;
		if (Mode == 'Ghost')
		{
			Owner.bNonShootable = false;
			if (!bShadow)
			{
				// Don't mess with the translucency settings if an
				// invisibility powerup is active.
				Owner.A_SetRenderStyle(1, STYLE_Normal);
			}
		}
		else if (Mode == 'Reflective')
		{
			Owner.bReflective = false;
		}

		if (Owner.player != NULL)
		{
			Owner.player.fixedcolormap = PlayerInfo.NOFIXEDCOLORMAP;
		}
	}

	//===========================================================================
	//
	// APowerInvulnerable :: AlterWeaponSprite
	//
	//===========================================================================

	override void AlterWeaponSprite (VisStyle vis, in out int changed)
	{
		if (Owner != NULL)
		{
			if (Mode == 'Ghost' && !(Owner.bShadow))
			{
				vis.Alpha = min(0.25 + Owner.Alpha * 0.75, 1.);
			}
		}
	}
}

//===========================================================================
//
// Strength
//
//===========================================================================

class PowerStrength : Powerup
{
	Default
	{
		Powerup.Duration 1;
		Powerup.Color "ff 00 00", 0.5;
		+INVENTORY.HUBPOWER
	}
	
	override bool HandlePickup (Inventory item)
	{
		if (item.GetClass() == GetClass())
		{ // Setting EffectTics to 0 will force Powerup's HandlePickup()
		  // method to reset the tic count so you get the red flash again.
			EffectTics = 0;
		}
		return Super.HandlePickup (item);
	}

	//===========================================================================
	//
	// APowerStrength :: DoEffect
	//
	//===========================================================================

	override void Tick ()
	{
		// Strength counts up to diminish the fade.
		EffectTics += 2;
		Super.Tick();
	}

	//===========================================================================
	//
	// APowerStrength :: GetBlend
	//
	//===========================================================================

	override color GetBlend ()
	{
		// slowly fade the berserk out
		int cnt = 128 - (EffectTics>>3);

		if (cnt > 0)
		{
			return Color(BlendColor.a*cnt/256,
				BlendColor.r, BlendColor.g, BlendColor.b);
		}
		return 0;
	}
	
}

//===========================================================================
//
// Invisibility
//
//===========================================================================

class PowerInvisibility : Powerup
{
	Default
	{
		+SHADOW;
		Powerup.Duration -60;
		Powerup.Strength 80;
		Powerup.Mode "Fuzzy";
	}
	
	//===========================================================================
	//
	// APowerInvisibility :: InitEffect
	//
	//===========================================================================

	override void InitEffect ()
	{
		Super.InitEffect();

		let Owner = self.Owner;
		if (Owner != NULL)
		{
			let savedShadow = Owner.bShadow;
			let savedGhost = Owner.bGhost;
			let savedCantSeek = Owner.bCantSeek;
			Owner.bShadow = bShadow;
			Owner.bGhost = bGhost;
			Owner.bCantSeek = bCantSeek;
			bShadow = savedShadow;
			bGhost = savedGhost;
			bCantSeek = savedCantSeek;
			DoEffect();
		}
	}

	//===========================================================================
	//
	// APowerInvisibility :: DoEffect
	//
	//===========================================================================
	
	override void DoEffect ()
	{
		Super.DoEffect();
		// Due to potential interference with other PowerInvisibility items
		// the effect has to be refreshed each tic.
		double ts = (Strength / 100.) * (special1 + 1);
		
		if (ts > 1.) ts = 1.;
		let newAlpha = clamp((1. - ts), 0., 1.);
		int newStyle;
		switch (Mode)
		{
		case 'Fuzzy':
			newStyle = STYLE_OptFuzzy;
			break;
		case 'Opaque':
			newStyle = STYLE_Normal;
			break;
		case 'Additive':
			newStyle = STYLE_Add;
			break;
		case 'Stencil':
			newStyle = STYLE_Stencil;
			break;
		case 'AddStencil' :
			newStyle = STYLE_AddStencil;
			break;
		case 'TranslucentStencil':
			newStyle = STYLE_TranslucentStencil;
			break;
		case 'None' :
		case 'Cumulative':
		case 'Translucent':
			newStyle = STYLE_Translucent;
			break;
		default: // Something's wrong
			newStyle = STYLE_Normal;
			newAlpha = 1.;
			break;
		}
		Owner.A_SetRenderStyle(newAlpha, newStyle);
	}

	//===========================================================================
	//
	// APowerInvisibility :: EndEffect
	//
	//===========================================================================

	override void EndEffect ()
	{
		Super.EndEffect();
		if (Owner != NULL)
		{
			Owner.bShadow = bShadow;
			Owner.bGhost = bGhost;
			Owner.bCantSeek = bCantSeek;

			Owner.A_SetRenderStyle(1, STYLE_Normal);

			// Check whether there are other invisibility items and refresh their effect.
			// If this isn't done there will be one incorrectly drawn frame when this
			// item expires.
			for(let item = Owner.Inv; item != null; item = item.Inv)
			{
				if (item != self && item is 'PowerInvisibility')
				{
					item.DoEffect();
				}
			}
		}
	}

	//===========================================================================
	//
	// APowerInvisibility :: AlterWeaponSprite
	//
	//===========================================================================

	override void AlterWeaponSprite (VisStyle vis, in out int changed)
	{
		// Blink if the powerup is wearing off
		if (changed == 0 && EffectTics < 4*32 && !(EffectTics & 8))
		{
			vis.RenderStyle = STYLE_Normal;
			vis.Alpha = 1.f;
			changed = 1;
			return;
		}
		else if (changed == 1)
		{
			// something else set the weapon sprite back to opaque but this item is still active.
			float ts = float((Strength / 100) * (special1 + 1));
			vis.Alpha = clamp((1. - ts), 0., 1.);
			switch (Mode)
			{
			case 'Fuzzy':
				vis.RenderStyle = STYLE_OptFuzzy;
				break;
			case 'Opaque':
				vis.RenderStyle = STYLE_Normal;
				break;
			case 'Additive':
				vis.RenderStyle = STYLE_Add;
				break;
			case 'Stencil':
				vis.RenderStyle = STYLE_Stencil;
				break;
			case 'TranslucentStencil':
				vis.RenderStyle = STYLE_TranslucentStencil;
				break;
			case 'AddStencil':
				vis.RenderStyle = STYLE_AddStencil;
				break;
			case 'None':
			case 'Cumulative':
			case 'Translucent':
			default:
				vis.RenderStyle = STYLE_Translucent;
				break;
			}
		}
		// Handling of Strife-like cumulative invisibility powerups, the weapon itself shouldn't become invisible
		if ((vis.Alpha < 0.25f && special1 > 0) || (vis.Alpha == 0))
		{
			vis.Alpha = clamp((1. - Strength/100.), 0., 1.);
			vis.invert = true;
		}
		changed = -1;	// This item is valid so another one shouldn't reset the translucency
	}

	//===========================================================================
	//
	// APowerInvisibility :: HandlePickup
	//
	// If the player already has the first stage of a cumulative powerup, getting 
	// it again increases the player's alpha. (But shouldn't this be in Use()?)
	//
	//===========================================================================

	override bool HandlePickup (Inventory item)
	{
		if (Mode == 'Cumulative' && ((Strength * special1) < 1.) && item.GetClass() == GetClass())
		{
			let power = Powerup(item);
			if (power.EffectTics == 0)
			{
				power.bPickupGood = true;
				return true;
			}
			// Only increase the EffectTics, not decrease it.
			// Color also gets transferred only when the new item has an effect.
			if (power.EffectTics > EffectTics)
			{
				EffectTics = power.EffectTics;
				BlendColor = power.BlendColor;
			}
			special1++;	// increases power
			power.bPickupGood = true;
			return true;
		}
		return Super.HandlePickup (item);
	}
}

class PowerGhost : PowerInvisibility
{
	Default
	{
		+GHOST;
		Powerup.Duration -60;
		Powerup.Strength 60;
		Powerup.Mode "None";
	}
}

class PowerShadow : PowerInvisibility
{
	Default
	{
		+INVENTORY.HUBPOWER
		Powerup.Duration -55;
		Powerup.Strength 75;
		Powerup.Mode "Cumulative";
	}
}

//===========================================================================
//
// IronFeet
//
//===========================================================================

class PowerIronFeet : Powerup
{
	Default
	{
		Powerup.Duration -60;
		Powerup.Color "00 ff 00", 0.125;
	}
	
	override void AbsorbDamage (int damage, Name damageType, out int newdamage, Actor inflictor, Actor source, int flags)
	{
		if (damageType == 'Drowning')
		{
			newdamage = 0;
		}
	}

	override void DoEffect ()
	{
		if (Owner.player != NULL)
		{
			Owner.player.mo.ResetAirSupply ();
		}
	}
	
}

//===========================================================================
//
// Mask
//
//===========================================================================

class PowerMask : PowerIronFeet
{
	Default
	{
		Powerup.Duration -80;
		Powerup.Color "00 00 00", 0;
		+INVENTORY.HUBPOWER
		Inventory.Icon "I_MASK";
	}
	
	override void AbsorbDamage (int damage, Name damageType, out int newdamage, Actor inflictor, Actor source, int flags)
	{
		if (damageType == 'Fire' || damageType == 'Drowning')
		{
			newdamage = 0;
		}
	}

	override void DoEffect ()
	{
		Super.DoEffect ();
		if (!(Level.maptime & 0x3f))
		{
			Owner.A_StartSound ("misc/mask", CHAN_AUTO);
		}
	}
	
}

//===========================================================================
//
// LightAmp
//
//===========================================================================

class PowerLightAmp : Powerup
{
	Default
	{
		Powerup.Duration -120;
	}
	
	//===========================================================================
	//
	// APowerLightAmp :: DoEffect
	//
	//===========================================================================

	override void DoEffect ()
	{
		Super.DoEffect ();

		let player = Owner.player;
		if (player != NULL && player.fixedcolormap < PlayerInfo.NUMCOLORMAPS)
		{
			if (!isBlinking())
			{	
				player.fixedlightlevel = 1;
			}
			else
			{
				player.fixedlightlevel = -1;
			}
		}
	}

	//===========================================================================
	//
	// APowerLightAmp :: EndEffect
	//
	//===========================================================================

	override void EndEffect ()
	{
		Super.EndEffect();
		if (Owner != NULL && Owner.player != NULL && Owner.player.fixedcolormap < PlayerInfo.NUMCOLORMAPS)
		{
			Owner.player.fixedlightlevel = -1;
		}
	}
	
}

//===========================================================================
//
// Torch
//
//===========================================================================

class PowerTorch : PowerLightAmp
{
	int NewTorch, NewTorchDelta;
	
	override void DoEffect ()
	{
		if (Owner == NULL || Owner.player == NULL)
		{
			return;
		}

		let player = Owner.player;
		if (EffectTics <= BLINKTHRESHOLD || player.fixedcolormap >= PlayerInfo.NUMCOLORMAPS)
		{
			Super.DoEffect ();
		}
		else 
		{
			Powerup.DoEffect ();

			if (!(Level.maptime & 16) && Owner.player != NULL)
			{
				if (NewTorch != 0)
				{
					if (player.fixedlightlevel + NewTorchDelta > 7
						|| player.fixedlightlevel + NewTorchDelta < 0
						|| NewTorch == player.fixedlightlevel)
					{
						NewTorch = 0;
					}
					else
					{
						player.fixedlightlevel += NewTorchDelta;
					}
				}
				else
				{
					NewTorch = random[torch](1, 8);
					NewTorchDelta = (NewTorch == Owner.player.fixedlightlevel) ?
						0 : ((NewTorch > player.fixedlightlevel) ? 1 : -1);
				}
			}
		}
	}
	
}

//===========================================================================
//
// Flight
//
//===========================================================================

class PowerFlight : Powerup
{
	Default
	{
		Powerup.Duration -60;
		+INVENTORY.HUBPOWER
	}

	clearscope bool HitCenterFrame;

	//===========================================================================
	//
	// APowerFlight :: InitEffect
	//
	//===========================================================================

	override void InitEffect ()
	{
		Super.InitEffect();
		Owner.bFly = true;
		Owner.bNoGravity = true;
		if (Owner.pos.Z <= Owner.floorz)
		{
			Owner.Vel.Z = 4;;	// thrust the player in the air a bit
		}
		if (Owner.Vel.Z <= -35)
		{ // stop falling scream
			Owner.A_StopSound (CHAN_VOICE);
		}
	}

	//===========================================================================
	//
	// APowerFlight :: DoEffect
	//
	//===========================================================================

	override void Tick ()
	{
		// The Wings of Wrath only expire in multiplayer and non-hub games
		if (!multiplayer && level.infinite_flight)
		{
			EffectTics++;
		}
		Super.Tick ();
	}

	//===========================================================================
	//
	// APowerFlight :: EndEffect
	//
	//===========================================================================

	override void EndEffect ()
	{
		Super.EndEffect();
		if (Owner == NULL || Owner.player == NULL)
		{
			return;
		}

		if (!(Owner.bFlyCheat))
		{
			if (Owner.pos.Z != Owner.floorz)
			{
				Owner.player.centering = true;
			}
			Owner.bFly = false;
			Owner.bNoGravity = false;
		}
	}

	//===========================================================================
	//
	// APowerFlight :: DrawPowerup
	//
	//===========================================================================

	override TextureID GetPowerupIcon ()
	{
		// If this item got a valid icon use that instead of the default spinning wings.
		if (Icon.isValid())
		{
			return Icon;
		}

		TextureID picnum = TexMan.CheckForTexture ("SPFLY0", TexMan.Type_MiscPatch);
		int frame = (Level.maptime/3) & 15;

		if (!picnum.isValid())
		{
			return picnum;
		}
		if (Owner.bNoGravity)
		{
			if (HitCenterFrame && (frame != 15 && frame != 0))
			{
				return picnum + 15;
			}
			else
			{
				HitCenterFrame = false;
				return picnum + frame;
			}
		}
		else
		{
			if (!HitCenterFrame && (frame != 15 && frame != 0))
			{
				HitCenterFrame = false;
				return picnum + frame;
			}
			else
			{
				HitCenterFrame = true;
				return picnum+15;
			}
		}
	}

	
}

//===========================================================================
//
// WeaponLevel2
//
//===========================================================================

class PowerWeaponLevel2 : Powerup
{
	Default
	{
		Powerup.Duration -40;
		Inventory.Icon "SPINBK0";
		+INVENTORY.NOTELEPORTFREEZE
	}
	
	//===========================================================================
	//
	// APowerWeaponLevel2 :: InitEffect
	//
	//===========================================================================

	override void InitEffect ()
	{
		
		Super.InitEffect();

		let player = Owner.player;

		if (player == null)
			return;

		let weap = player.ReadyWeapon;

		if (weap == null)
			return;

		let sister = weap.SisterWeapon;

		if (sister == null)
			return;

		if (!sister.bPowered_Up)
			return;

		let ready = sister.GetReadyState();
		if (weap.GetReadyState() != ready)
		{
			player.ReadyWeapon = sister;
			player.SetPsprite(PSP_WEAPON, ready);
		}
		else
		{
			PSprite psp = player.FindPSprite(PSprite.WEAPON);
			if (psp != null && psp.Caller == player.ReadyWeapon)
			{
				// If the weapon changes but the state does not, we have to manually change the PSprite's caller here.
				psp.Caller = sister;
				player.ReadyWeapon = sister;
			}
			else
			{
				// Something went wrong. Initiate a regular weapon change.
				player.PendingWeapon = sister;
			}
		}
	}

	//===========================================================================
	//
	// APowerWeaponLevel2 :: EndEffect
	//
	//===========================================================================

	override void EndEffect ()
	{
		Super.EndEffect();
		if (Owner == null) return;
		let player = Owner.player;
		if (player != NULL)
		{
			if (player.ReadyWeapon != NULL && player.ReadyWeapon.bPowered_Up)
			{
				player.ReadyWeapon.EndPowerup ();
			}
			if (player.PendingWeapon != NULL && player.PendingWeapon != WP_NOCHANGE &&
				player.PendingWeapon.bPowered_Up &&
				player.PendingWeapon.SisterWeapon != NULL)
			{
				player.PendingWeapon = player.PendingWeapon.SisterWeapon;
			}
		}
	}

	
}

//===========================================================================
//
// Speed
//
//===========================================================================

class PowerSpeed : Powerup
{
	int NoTrail;

	Property NoTrail: NoTrail;
	FlagDef NoTrail: NoTrail, 0;	// This was once a flag, not a property.
	
	Default
	{
		Powerup.Duration -45;
		Speed 1.5;
		Inventory.Icon "SPBOOT0";
		+INVENTORY.NOTELEPORTFREEZE
	}
	
	override double GetSpeedFactor() 
	{ 
		return Speed; 
	}
	
	//===========================================================================
	//
	// APowerSpeed :: DoEffect
	//
	//===========================================================================

	override void DoEffect ()
	{
		Super.DoEffect ();
		
		if (Owner == NULL || Owner.player == NULL)
			return;

		if (Owner.player.cheats & CF_PREDICTING)
			return;

		if (NoTrail)
			return;

		if (Level.maptime & 1)
			return;

		// Check if another speed item is present to avoid multiple drawing of the speed trail.
		// Only the last PowerSpeed without PSF_NOTRAIL set will actually draw the trail.
		for (Inventory item = Inv; item != NULL; item = item.Inv)
		{
			let sitem = PowerSpeed(item);
			if (sitem != null && !NoTrail)
			{
				return;
			}
		}

		if (Owner.Vel.Length() <= 12)
			return;

		Actor speedMo = Spawn("PlayerSpeedTrail", Owner.Pos, NO_REPLACE);
		if (speedMo)
		{
			speedMo.Angle = Owner.Angle;
			speedMo.Translation = Owner.Translation;
			speedMo.target = Owner;
			speedMo.sprite = Owner.sprite;
			speedMo.frame = Owner.frame;
			speedMo.Floorclip = Owner.Floorclip;

			// [BC] Also get the scale from the owner.
			speedMo.Scale = Owner.Scale;

			if (Owner == players[consoleplayer].camera &&
				!(Owner.player.cheats & CF_CHASECAM))
			{
				speedMo.bInvisible = true;
			}
		}
	}
}

// Player Speed Trail (used by the Speed Powerup) ----------------------------

class PlayerSpeedTrail : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		Alpha 0.6;
		RenderStyle "Translucent";
	}
	
	override void Tick()
	{
		Alpha -= .6 / 8;
		if (Alpha <= 0)
		{
			Destroy ();
		}
	}
}

//===========================================================================
//
// Minotaur
//
//===========================================================================

class PowerMinotaur : Powerup
{
	Default
	{
		Powerup.Duration -25;
		Inventory.Icon "SPMINO0";
	}
}

//===========================================================================
//
// Targeter
//
//===========================================================================

class PowerTargeter : Powerup
{
	Default
	{
		Powerup.Duration -160;
		+INVENTORY.HUBPOWER
	}
	States
	{
	Targeter:
		TRGT A -1;
		Stop;
		TRGT B -1;
		Stop;
		TRGT C -1;
		Stop;
	}
	
	override void Travelled ()
	{
		InitEffect ();
	}

	const POS_X = 160 - 3;
	const POS_Y = 100 - 3;

	override void InitEffect ()
	{
		// Why is this called when the inventory isn't even attached yet
		// in APowerup.CreateCopy?
		if (!Owner.FindInventory(GetClass(), true))
			return;

		let player = Owner.player;

		Super.InitEffect();

		if (player == null)
			return;

		let stat = FindState("Targeter");

		if (stat != null)
		{
			player.SetPsprite(PSprite.TARGETCENTER,  stat);
			player.SetPsprite(PSprite.TARGETLEFT,  stat + 1);
			player.SetPsprite(PSprite.TARGETRIGHT, stat + 2);
		}

		PSprite center = player.GetPSprite(PSprite.TARGETCENTER);
		if (center)
		{
			center.x = POS_X;
			center.y = POS_Y;
		}
		PositionAccuracy ();
	}

	override void AttachToOwner(Actor other)
	{
		Super.AttachToOwner(other);

		// Let's actually properly call this for the targeters.
		InitEffect();
	}

	override bool HandlePickup(Inventory item)
	{
		if (Super.HandlePickup(item))
		{
			InitEffect();	// reset the HUD sprites
			return true;
		}
		return false;
	}

	override void DoEffect ()
	{
		Super.DoEffect ();

		if (Owner != null && Owner.player != null)
		{
			let player = Owner.player;

			PositionAccuracy ();
			if (EffectTics < 5*TICRATE)
			{
				let stat = FindState("Targeter");

				if (stat != null)
				{
					if (EffectTics & 32)
					{
						player.SetPsprite(PSprite.TARGETRIGHT, null);
						player.SetPsprite(PSprite.TARGETLEFT,  stat + 1);
					}
					else if (EffectTics & 16)
					{
						player.SetPsprite(PSprite.TARGETRIGHT, stat + 2);
						player.SetPsprite(PSprite.TARGETLEFT,  null);
					}
				}
			}
		}
	}

	override void EndEffect ()
	{
		Super.EndEffect();
		if (Owner != null && Owner.player != null)
		{
			// Calling GetPSprite here could crash if we're creating a new game.
			// This is because P_SetupLevel nulls the player's mo before destroying
			// every DThinker which in turn ends up calling this.
			// However P_SetupLevel is only called after G_NewInit which calls
			// every player's dtor which destroys all their psprites.
			let player = Owner.player;
			PSprite pspr;
			if ((pspr = player.FindPSprite(PSprite.TARGETCENTER)) != null) pspr.SetState(null);
			if ((pspr = player.FindPSprite(PSprite.TARGETLEFT)) != null) pspr.SetState(null);
			if ((pspr = player.FindPSprite(PSprite.TARGETRIGHT)) != null) pspr.SetState(null);
		}
	}

	private void PositionAccuracy ()
	{
		let player = Owner.player;

		if (player != null)
		{
			PSprite left = player.GetPSprite(PSprite.TARGETLEFT);
			if (left)
			{
				left.x = POS_X - (100 - player.mo.accuracy);
				left.y = POS_Y;
			}

			PSprite right = player.GetPSprite(PSprite.TARGETRIGHT);
			if (right)
			{
				right.x = POS_X + (100 - player.mo.accuracy);
				right.y = POS_Y;
			}
		}
	}
	
}

//===========================================================================
//
// Frightener
//
//===========================================================================

class PowerFrightener : Powerup
{
	Default
	{
		Powerup.Duration -60;
	}
	
	override void InitEffect ()
	{
		Super.InitEffect();

		if (Owner== null || Owner.player == null)
			return;

		Owner.player.cheats |= CF_FRIGHTENING;
	}

	override void EndEffect ()
	{
		Super.EndEffect();

		if (Owner== null || Owner.player == null)
			return;

		Owner.player.cheats &= ~CF_FRIGHTENING;
	}
}

//===========================================================================
//
// Buddha
//
//===========================================================================

class PowerBuddha : Powerup
{
	Default
	{
		Powerup.Duration -60;
	}
}

//===========================================================================
//
// Scanner (this is active just by being present)
//
//===========================================================================

class PowerScanner : Powerup
{
	Default
	{
		Powerup.Duration -80;
		+INVENTORY.HUBPOWER
	}
}

//===========================================================================
//
// TimeFreezer
//
//===========================================================================

class PowerTimeFreezer : Powerup
{
	Default
	{
		Powerup.Duration -12;
	}
	
	//===========================================================================
	//
	// InitEffect
	//
	//===========================================================================

	override void InitEffect()
	{
		int freezemask;

		Super.InitEffect();

		if (Owner == null || Owner.player == null)
			return;

		// When this powerup is in effect, pause the music.
		S_PauseSound(false, false);

		// Give the player and his teammates the power to move when time is frozen.
		freezemask = 1 << Owner.PlayerNumber();
		Owner.player.timefreezer |= freezemask;
		for (int i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i] &&
				players[i].mo != null &&
				players[i].mo.IsTeammate(Owner)
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
		if ((Level.maptime & 1) == 0)
		{
			Level.SetFrozen(true);
		}
		else
		{
			// Compensate for skipped tic, but beware of overflow.
			if(EffectTics < 0x7fffffff)
				EffectTics++;
		}
	}

	//===========================================================================
	//
	// APowerTimeFreezer :: DoEffect
	//
	//===========================================================================

	override void DoEffect()
	{
		Super.DoEffect();
		// [RH] Do not change LEVEL_FROZEN on odd tics, or the Revenant's tracer
		// will get thrown off.
		// [ED850] Don't change it if the player is predicted either.
		if (Level.maptime & 1 || (Owner != null && Owner.player != null && Owner.player.cheats & CF_PREDICTING))
		{
			return;
		}
		// [RH] The "blinking" can't check against EffectTics exactly or it will
		// never happen, because InitEffect ensures that EffectTics will always
		// be odd when Level.maptime is even.
		Level.SetFrozen ( EffectTics > 4*32 
			|| (( EffectTics > 3*32 && EffectTics <= 4*32 ) && ((EffectTics + 1) & 15) != 0 )
			|| (( EffectTics > 2*32 && EffectTics <= 3*32 ) && ((EffectTics + 1) & 7) != 0 )
			|| (( EffectTics >   32 && EffectTics <= 2*32 ) && ((EffectTics + 1) & 3) != 0 )
			|| (( EffectTics >    0 && EffectTics <= 1*32 ) && ((EffectTics + 1) & 1) != 0 ));
	}

	//===========================================================================
	//
	// APowerTimeFreezer :: EndEffect
	//
	//===========================================================================

	override void EndEffect()
	{
		Super.EndEffect();

		// If there is an owner, remove the timefreeze flag corresponding to
		// her from all players.
		if (Owner != null && Owner.player != null)
		{
			int freezemask = ~(1 << Owner.PlayerNumber());
			for (int i = 0; i < MAXPLAYERS; ++i)
			{
				players[i].timefreezer &= freezemask;
			}
		}

		// Are there any players who still have timefreezer bits set?
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && players[i].timefreezer != 0)
			{
				return;
			}
		}

		// No, so allow other actors to move about freely once again.
		Level.SetFrozen(false);

		// Also, turn the music back on.
		S_ResumeSound(false);
	}
}

//===========================================================================
//
// Damage
//
//===========================================================================

class PowerDamage : Powerup
{
	Default
	{
		Powerup.Duration -25;
	}
	
	//===========================================================================
	//
	// InitEffect
	//
	//===========================================================================

	override void InitEffect()
	{
		Super.InitEffect();

		if (Owner != null)
		{
			Owner.A_StartSound(SeeSound, CHAN_5, CHANF_DEFAULT, 1.0, ATTN_NONE);
		}
	}

	//===========================================================================
	//
	// EndEffect
	//
	//===========================================================================

	override void EndEffect()
	{
		Super.EndEffect();
		if (Owner != null)
		{
			Owner.A_StartSound(DeathSound, CHAN_5, CHANF_DEFAULT, 1.0, ATTN_NONE);
		}
	}

	//===========================================================================
	//
	// ModifyDamage
	//
	//===========================================================================

	override void ModifyDamage(int damage, Name damageType, out int newdamage, bool passive, Actor inflictor, Actor source, int flags)
	{
		if (!passive && damage > 0)
		{
			newdamage = max(1, ApplyDamageFactors(GetClass(), damageType, damage, damage * 4));
			if (Owner != null && newdamage > damage) Owner.A_StartSound(ActiveSound, CHAN_AUTO, CHANF_DEFAULT, 1.0, ATTN_NONE);
		}
	}
}

//===========================================================================
//
// Protection
//
//===========================================================================

class PowerProtection : Powerup
{
	Default
	{
		Powerup.Duration -25;
	}
	
	//===========================================================================
	//
	// InitEffect
	//
	//===========================================================================

	override void InitEffect()
	{
		Super.InitEffect();

		let o = Owner;	// copy to a local variable for quicker access.
		if (o != null)
		{
			o.A_StartSound(SeeSound, CHAN_AUTO, CHANF_DEFAULT, 1.0, ATTN_NONE);

			// Transfer various protection flags if owner does not already have them.
			// If the owner already has the flag, clear it from the powerup.
			// If the powerup still has a flag set, add it to the owner.
			bNoRadiusDmg &= !o.bNoRadiusDmg;
			o.bNoRadiusDmg |= bNoRadiusDmg;

			bDontMorph &= !o.bDontMorph;
			o.bDontMorph |= bDontMorph;
			
			bDontSquash &= !o.bDontSquash;
			o.bDontSquash |= bDontSquash;

			bDontBlast &= !o.bDontBlast;
			o.bDontBlast |= bDontBlast;
			
			bNoTeleOther &= !o.bNoTeleOther;
			o.bNoTeleOther |= bNoTeleOther;
			
			bNoPain &= !o.bNoPain;
			o.bNoPain |= bNoPain;

			bDontRip &= !o.bDontRip;
			o.bDontRip |= bDontRip;
		}
	}

	//===========================================================================
	//
	// EndEffect
	//
	//===========================================================================

	override void EndEffect()
	{
		Super.EndEffect();
		let o = Owner;	// copy to a local variable for quicker access.
		if (o != null)
		{
			o.A_StartSound(DeathSound, CHAN_AUTO, CHANF_DEFAULT, 1.0, ATTN_NONE);
			
			o.bNoRadiusDmg &= !bNoRadiusDmg;
			o.bDontMorph &= !bDontMorph;
			o.bDontSquash &= !bDontSquash;
			o.bDontBlast &= !bDontBlast;
			o.bNoTeleOther &= !bNoTeleOther;
			o.bNoPain &= !bNoPain;
			o.bDontRip &= !bDontRip;
		}
	}

	//===========================================================================
	//
	// AbsorbDamage
	//
	//===========================================================================

	override void ModifyDamage(int damage, Name damageType, out int newdamage, bool passive, Actor inflictor, Actor source, int flags)
	{
		if (passive && damage > 0)
		{
			newdamage = max(0, ApplyDamageFactors(GetClass(), damageType, damage, damage / 4));
			if (Owner != null && newdamage < damage) Owner.A_StartSound(ActiveSound, CHAN_AUTO, CHANF_DEFAULT, 1.0, ATTN_NONE);
		}
	}
}

//===========================================================================
//
// Drain
//
//===========================================================================

class PowerDrain : Powerup
{
	Default
	{
		Powerup.Strength 0.5;
		Powerup.Duration -60;
	}
}

//===========================================================================
//
// Regeneration
//
//===========================================================================

class PowerRegeneration : Powerup
{
	Default
	{
		Powerup.Duration -120;
		Powerup.Strength 5;
	}
	
	override void DoEffect()
	{
		Super.DoEffect();
		if (Owner != null && Owner.health > 0 && (Level.maptime & 31) == 0)
		{
			if (Owner.GiveBody(int(Strength)))
			{
				Owner.A_StartSound("*regenerate", CHAN_ITEM);
			}
		}
	}
}

//===========================================================================
//
// HighJump
//
//===========================================================================

class PowerHighJump : Powerup
{
	Default
	{
		Powerup.Strength 2;
	}
}

//===========================================================================
//
// DoubleFiringSpeed
//
//===========================================================================

class PowerDoubleFiringSpeed : Powerup
{
	Default
	{
		Powerup.Duration -40;
	}
}

//===========================================================================
//
// InfiniteAmmo
//
//===========================================================================

class PowerInfiniteAmmo : Powerup
{
	Default
	{
		Powerup.Duration -30;
	}
}


//===========================================================================
//
// InfiniteAmmo
//
//===========================================================================

class PowerReflection : Powerup
{
	// if 1, reflects the damage type as well.
	bool ReflectType;
	property ReflectType : ReflectType;
	
	Default
	{
		Powerup.Duration -60;
		DamageFactor 0.5;
	}
}

//===========================================================================
//
// PowerMorph
//
//===========================================================================

class PowerMorph : Powerup
{
	Class<PlayerPawn> PlayerClass;
	Class<Actor> MorphFlash, UnMorphFlash;
	int MorphStyle;
	PlayerInfo MorphedPlayer;
	
	Default
	{
		Powerup.Duration -40;
	}
	
	//===========================================================================
	//
	// InitEffect
	//
	//===========================================================================

	override void InitEffect()
	{
		Super.InitEffect();

		if (Owner != null && Owner.player != null && PlayerClass != null)
		{
			let realplayer = Owner.player;	// Remember the identity of the player
			if (realplayer.mo.MorphPlayer(realplayer, PlayerClass, 0x7fffffff/*INDEFINITELY*/, MorphStyle, MorphFlash, UnMorphFlash))
			{
				Owner = realplayer.mo;				// Replace the new owner in our owner; safe because we are not attached to anything yet
				bCreateCopyMoved = true;			// Let the caller know the "real" owner has changed (to the morphed actor)
				MorphedPlayer = realplayer;			// Store the player identity (morphing clears the unmorphed actor's "player" field)
			}
			else // morph failed - give the caller an opportunity to fail the pickup completely
			{
				bInitEffectFailed = true;			// Let the caller know that the activation failed (can fail the pickup if appropriate)
			}
		}
	}

	//===========================================================================
	//
	// EndEffect
	//
	//===========================================================================

	override void EndEffect()
	{
		Super.EndEffect();

		// Abort if owner already destroyed or unmorphed
		if (Owner == null || MorphedPlayer == null || Owner.alternative == null)
		{
			return;
		}
		
		// Abort if owner is dead; their Die() method will
		// take care of any required unmorphing on death.
		if (MorphedPlayer.health <= 0)
		{
			return;
		}

		int savedMorphTics = MorphedPlayer.morphTics;
		MorphedPlayer.mo.UndoPlayerMorph (MorphedPlayer, 0, !!(MorphedPlayer.MorphStyle & MRF_UNDOALWAYS));
		MorphedPlayer = null;
	}

	
}

