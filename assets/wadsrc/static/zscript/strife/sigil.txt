
// The Almighty Sigil! ------------------------------------------------------

class Sigil : Weapon
{
	// NUmPieces gets stored in 'health', so that it can be quickly accessed by ACS's GetSigilPieces function.
	int downpieces;
	
	Default
	{
		Weapon.Kickback 100;
		Weapon.SelectionOrder 4000;
		Health 1;
		+FLOORCLIP
		+WEAPON.CHEATNOTWEAPON
		Inventory.PickupSound "weapons/sigilcharge";
		Tag "$TAG_SIGIL";
		Inventory.Icon "I_SGL1";
		Inventory.PickupMessage "$TXT_SIGIL";
	}
	
	States(Actor)
	{
	Spawn:
		SIGL A 1;
		SIGL A -1 A_SelectPiece;
		Stop;
		SIGL B -1;
		Stop;
		SIGL C -1;
		Stop;
		SIGL D -1;
		Stop;
		SIGL E -1;
		Stop;
	}
	States(Weapon)
	{
	Ready:
		SIGH A 0 Bright A_SelectSigilView;
		Wait;
		SIGH A 1 Bright A_WeaponReady;
		Wait;
		SIGH B 1 Bright A_WeaponReady;
		Wait;
		SIGH C 1 Bright A_WeaponReady;
		Wait;
		SIGH D 1 Bright A_WeaponReady;
		Wait;
		SIGH E 1 Bright A_WeaponReady;
		Wait;
	Deselect:
		SIGH A 1 Bright A_SelectSigilDown;
		Wait;
		SIGH A 1 Bright A_Lower;
		Wait;
		SIGH B 1 Bright A_Lower;
		Wait;
		SIGH C 1 Bright A_Lower;
		Wait;
		SIGH D 1 Bright A_Lower;
		Wait;
		SIGH E 1 Bright A_Lower;
		Wait;
	Select:
		SIGH A 1 Bright A_SelectSigilView;
		Wait;
		SIGH A 1 Bright A_Raise;
		Wait;
		SIGH B 1 Bright A_Raise;
		Wait;
		SIGH C 1 Bright A_Raise;
		Wait;
		SIGH D 1 Bright A_Raise;
		Wait;
		SIGH E 1 Bright A_Raise;
		Wait;
	
	Fire:
		SIGH A 0 Bright A_SelectSigilAttack;

		SIGH A 18 Bright A_SigilCharge;
		SIGH A 3 Bright A_GunFlash;
		SIGH A 10 A_FireSigil1;
		SIGH A 5;
		Goto Ready;

		SIGH B 18 Bright A_SigilCharge;
		SIGH B 3 Bright A_GunFlash;
		SIGH B 10 A_FireSigil2;
		SIGH B 5;
		Goto Ready;

		SIGH C 18 Bright A_SigilCharge;
		SIGH C 3 Bright A_GunFlash;
		SIGH C 10 A_FireSigil3;
		SIGH C 5;
		Goto Ready;

		SIGH D 18 Bright A_SigilCharge;
		SIGH D 3 Bright A_GunFlash;
		SIGH D 10 A_FireSigil4;
		SIGH D 5;
		Goto Ready;

		SIGH E 18 Bright A_SigilCharge;
		SIGH E 3 Bright A_GunFlash;
		SIGH E 10 A_FireSigil5;
		SIGH E 5;
		Goto Ready;
	Flash:
		SIGF A 4 Bright A_Light2;
		SIGF B 6 Bright A_LightInverse;
		SIGF C 4 Bright A_Light1;
		SIGF C 0 Bright A_Light0;
		Stop;
	}
	

	//============================================================================
	//
	// ASigil :: HandlePickup
	//
	//============================================================================

	override bool HandlePickup (Inventory item)
	{
		if (item is "Sigil")
		{
			int otherPieces = item.health;
			if (otherPieces > health)
			{
				item.bPickupGood = true;
				Icon = item.Icon;
				// If the player is holding the Sigil right now, drop it and bring
				// it back with the new piece(s) in view.
				if (Owner.player != null && Owner.player.ReadyWeapon == self)
				{
					DownPieces = health;
					Owner.player.PendingWeapon = self;
				}
				health = otherPieces;
			}
			return true;
		}
		return false;
	}

	//============================================================================
	//
	// ASigil :: CreateCopy
	//
	//============================================================================

	override Inventory CreateCopy (Actor other)
	{
		Sigil copy = Sigil(Spawn("Sigil"));
		copy.Amount = Amount;
		copy.MaxAmount = MaxAmount;
		copy.health = health;
		copy.Icon = Icon;
		GoAwayAndDie ();
		return copy;
	}

	//============================================================================
	//
	// A_SelectPiece
	//
	// Decide which sprite frame self Sigil should use as an item, based on how
	// many pieces it represents.
	//
	//============================================================================

	void A_SelectPiece ()
	{
		int pieces = min (health, 5);

		if (pieces > 1)
		{
			SetState (FindState("Spawn") + pieces);
		}
	}

	//============================================================================
	//
	// A_SelectSigilView
	//
	// Decide which first-person frame self Sigil should show, based on how many
	// pieces it represents. Strife did self by selecting a flash that looked like
	// the Sigil whenever you switched to it and at the end of an attack. I have
	// chosen to make the weapon sprite choose the correct frame and let the flash
	// be a regular flash. It means I need to use more states, but I think it's
	// worth it.
	//
	//============================================================================

	action void A_SelectSigilView ()
	{
		if (player == null)
		{
			return;
		}
		PSprite pspr = player.GetPSprite(PSP_WEAPON);
		pspr.SetState(pspr.CurState + invoker.health);
		invoker.downpieces = 0;
	}

	//============================================================================
	//
	// A_SelectSigilDown
	//
	// Same as A_SelectSigilView, except it uses DownPieces. self is so that when
	// you pick up a Sigil, the old one will drop and *then* change to the new
	// one.
	//
	//============================================================================

	action void A_SelectSigilDown ()
	{
		if (player == null)
		{
			return;
		}
		PSprite pspr = player.GetPSprite(PSP_WEAPON);
		int pieces = invoker.downpieces;
		if (pieces < 1 || pieces > 5) pieces = invoker.health;
		pspr.SetState(pspr.CurState + pieces);
	}

	//============================================================================
	//
	// A_SelectSigilAttack
	//
	// Same as A_SelectSigilView, but used just before attacking.
	//
	//============================================================================

	action void A_SelectSigilAttack ()
	{
		if (player == null)
		{
			return;
		}
		PSprite pspr = player.GetPSprite(PSP_WEAPON);
		pspr.SetState(pspr.CurState + (4 * invoker.health - 3));
	}

	//============================================================================
	//
	// A_SigilCharge
	//
	//============================================================================

	action void A_SigilCharge ()
	{
		A_PlaySound ("weapons/sigilcharge", CHAN_WEAPON);
		if (player != null)
		{
			player.extralight = 2;
		}
	}

	//============================================================================
	//
	// A_FireSigil1
	//
	//============================================================================

	action void A_FireSigil1 ()
	{
		Actor spot = null;
		FTranslatedLineTarget t;

		if (player == null || player.ReadyWeapon == null)
			return;

		DamageMobj (self, null, 1*4, 'Sigil', DMG_NO_ARMOR);
		A_PlaySound ("weapons/sigilcharge", CHAN_WEAPON);

		BulletSlope (t, ALF_PORTALRESTRICT);
		if (t.linetarget != null)
		{
			spot = Spawn("SpectralLightningSpot", (t.linetarget.pos.xy, t.linetarget.floorz), ALLOW_REPLACE);
			if (spot != null)
			{
				spot.tracer = t.linetarget;
			}
		}
		else
		{
			spot = Spawn("SpectralLightningSpot", Pos, ALLOW_REPLACE);
			if (spot != null)
			{
				spot.VelFromAngle(28., angle);
			}
		}
		if (spot != null)
		{
			spot.SetFriendPlayer(player);
			spot.target = self;
		}
	}

	//============================================================================
	//
	// A_FireSigil2
	//
	//============================================================================

	action void A_FireSigil2 ()
	{
		if (player == null || player.ReadyWeapon == null)
			return;

		DamageMobj (self, null, 2*4, 'Sigil', DMG_NO_ARMOR);
		A_PlaySound ("weapons/sigilcharge", CHAN_WEAPON);
		SpawnPlayerMissile ("SpectralLightningH1");
	}

	//============================================================================
	//
	// A_FireSigil3
	//
	//============================================================================

	action void A_FireSigil3 ()
	{
		if (player == null || player.ReadyWeapon == null)
			return;

		DamageMobj (self, null, 3*4, 'Sigil', DMG_NO_ARMOR);
		A_PlaySound ("weapons/sigilcharge", CHAN_WEAPON);

		angle -= 90.;
		for (int i = 0; i < 20; ++i)
		{
			angle += 9.;
			Actor spot = SpawnSubMissile ("SpectralLightningBall1", self);
			if (spot != null)
			{
				spot.SetZ(pos.z + 32);
			}
		}
		angle -= 90.;
	}

	//============================================================================
	//
	// A_FireSigil4
	//
	//============================================================================

	action void A_FireSigil4 ()
	{
		FTranslatedLineTarget t;
		
		if (player == null || player.ReadyWeapon == null)
			return;

		DamageMobj (self, null, 4*4, 'Sigil', DMG_NO_ARMOR);
		A_PlaySound ("weapons/sigilcharge", CHAN_WEAPON);

		BulletSlope (t, ALF_PORTALRESTRICT);
		if (t.linetarget != null)
		{
			Actor spot = SpawnPlayerMissile ("SpectralLightningBigV1", angle, pLineTarget: t, aimFlags: ALF_PORTALRESTRICT);
			if (spot != null)
			{
				spot.tracer = t.linetarget;
			}
		}
		else
		{
			Actor spot = SpawnPlayerMissile ("SpectralLightningBigV1");
			if (spot != null)
			{
				spot.VelFromAngle(spot.Speed, angle);
			}
		}
	}

	//============================================================================
	//
	// A_FireSigil5
	//
	//============================================================================

	action void A_FireSigil5 ()
	{
		if (player == null || player.ReadyWeapon == null)
			return;

		DamageMobj (self, null, 5*4, 'Sigil', DMG_NO_ARMOR);
		A_PlaySound ("weapons/sigilcharge", CHAN_WEAPON);

		SpawnPlayerMissile ("SpectralLightningBigBall1");
	}

	//============================================================================
	//
	// ASigil :: SpecialDropAction
	//
	// Monsters don't drop Sigil pieces. The Sigil pieces grab hold of the person
	// who killed the dropper and automatically enter their inventory. That's the
	// way it works if you believe Macil, anyway...
	//
	//============================================================================

	override bool SpecialDropAction (Actor dropper)
	{
		// Give a Sigil piece to every player in the game
		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && players[i].mo != null)
			{
				GiveSigilPiece (players[i].mo);
				Destroy ();
			}
		}
		return true;
	}

	//============================================================================
	//
	// ASigil :: GiveSigilPiece
	//
	// Gives the actor another Sigil piece, up to 5. Returns the number of Sigil
	// pieces the actor previously held.
	//
	//============================================================================

	static int GiveSigilPiece (Actor receiver)
	{
		Sigil sigl = Sigil(receiver.FindInventory("Sigil"));
		if (sigl == null)
		{
			sigl = Sigil(Spawn("Sigil1"));
			if (!sigl.CallTryPickup (receiver))
			{
				sigl.Destroy ();
			}
			return 0;
		}
		else if (sigl.health < 5)
		{
			++sigl.health;
			static const class<Sigil> sigils[] =
			{
				"Sigil1", "Sigil2", "Sigil3", "Sigil4", "Sigil5"
			};
			sigl.Icon = GetDefaultByType(sigils[clamp(sigl.health, 1, 5)-1]).Icon;
			// If the player has the Sigil out, drop it and bring it back up.
			if (sigl.Owner.player != null && sigl.Owner.player.ReadyWeapon == sigl)
			{
				sigl.Owner.player.PendingWeapon = sigl;
				sigl.DownPieces = sigl.health - 1;
			}
			return sigl.health - 1;
		}
		else
		{
			return 5;
		}
	}
}

// Sigil 1 ------------------------------------------------------------------

class Sigil1 : Sigil
{
	Default
	{
		Inventory.Icon "I_SGL1";
		Health 1;
	}
}

// Sigil 2 ------------------------------------------------------------------

class Sigil2 : Sigil
{
	Default
	{
		Inventory.Icon "I_SGL2";
		Health 2;
	}
}

// Sigil 3 ------------------------------------------------------------------

class Sigil3 : Sigil
{
	Default
	{
		Inventory.Icon "I_SGL3";
		Health 3;
	}
}

// Sigil 4 ------------------------------------------------------------------

class Sigil4 : Sigil
{
	Default
	{
		Inventory.Icon "I_SGL4";
		Health 4;
	}
}

// Sigil 5 ------------------------------------------------------------------

class Sigil5 : Sigil
{
	Default
	{
		Inventory.Icon "I_SGL5";
		Health 5;
	}
}
