extend class PlayerPawn
{
	//===========================================================================
	//
	// InitAllPowerupEffects
	//
	// Calls InitEffect() on every Powerup in the inventory list. Since these
	// functions can be overridden it's safest to store what's next in the item
	// list before calling it.
	//
	//===========================================================================

	void InitAllPowerupEffects()
	{
		for (Inventory item = Inv; item;)
		{
			Inventory next = item.Inv;
			let power = Powerup(item);
			if (power)
				power.InitEffect();

			item = next;
		}
	}
	
	//===========================================================================
	//
	// EndAllPowerupEffects
	//
	// Calls EndEffect() on every Powerup in the inventory list.
	//
	//===========================================================================

	void EndAllPowerupEffects()
	{
		for (Inventory item = Inv; item;)
		{
			Inventory next = item.Inv;
			let power = Powerup(item);
			if (power)
				power.EndEffect();

			item = next;
		}
	}
	
	//===========================================================================
	//
	//
	//
	//===========================================================================

	virtual void ActivateMorphWeapon()
	{
		if (player.ReadyWeapon)
		{
			let psp = player.GetPSprite(PSP_WEAPON);
			psp.y = WEAPONTOP;
			player.ReadyWeapon.ResetPSprite(psp);
		}
		
		class<Weapon> morphWeapCls = MorphWeapon;
		if (!morphWeapCls)
		{
			player.ReadyWeapon = null;
		}
		else
		{
			player.ReadyWeapon = Weapon(FindInventory(morphWeapCls));
			if (!player.ReadyWeapon)
			{
				player.ReadyWeapon = Weapon(GiveInventoryType(morphWeapCls));
				if (player.ReadyWeapon)
					player.ReadyWeapon.GivenAsMorphWeapon = true; // Flag is used only by new morphWeap semantics in UndoPlayerMorph
			}

			if (player.ReadyWeapon)
				player.SetPSprite(PSP_WEAPON, player.ReadyWeapon.GetReadyState());
		}

		if (player.ReadyWeapon)
			player.SetPSprite(PSP_FLASH, null);

		player.PendingWeapon = WP_NOCHANGE;
	}

	//---------------------------------------------------------------------------
	//
	// MorphPlayer
	//
	// Returns true if the player gets turned into a chicken/pig.
	//
	// TODO: Allow morphed players to receive weapon sets (not just one weapon),
	// since they have their own weapon slots now.
	//
	//---------------------------------------------------------------------------

	virtual bool MorphPlayer(PlayerInfo activator, class<PlayerPawn> spawnType, int duration, EMorphFlags style, class<Actor> enterFlash = "TeleportFog", class<Actor> exitFlash = "TeleportFog")
	{
		if (!player || !spawnType || bDontMorph || player.Health <= 0
			|| (!(style & MRF_IGNOREINVULN) && bInvulnerable && (player != activator || !(style & MRF_WHENINVULNERABLE))))
		{
			return false;
		}

		if (!duration)
			duration = DEFMORPHTICS;

		if (spawnType == GetClass())
		{
			// Player is already a beast.
			if (Alternative && bCanSuperMorph
				&& GetMorphTics() < duration - TICRATE
				&& !FindInventory("PowerWeaponLevel2", true))
			{
				// Make a super chicken.
				GiveInventoryType("PowerWeaponLevel2");
			}

			return false;
		}

		let morphed = PlayerPawn(Spawn(spawnType, Pos, NO_REPLACE));
		if (!MorphInto(morphed))
		{
			if (morphed)
				morphed.Destroy();
				
			return false;
		}

		PreMorph(morphed, false);
		morphed.PreMorph(self, true);

		morphed.EndAllPowerupEffects();

		if ((style & MRF_TRANSFERTRANSLATION) && !morphed.bDontTranslate)
			morphed.Translation = Translation;

		morphed.Angle = Angle;
		morphed.Pitch = Pitch; // Allow pitch here since mouse look in GZDoom is far more common than Heretic/Hexen.
		morphed.Target = Target;
		morphed.Tracer = Tracer;
		morphed.Master = Master;
		morphed.FriendPlayer = FriendPlayer;
		morphed.DesignatedTeam = DesignatedTeam;
		morphed.Score = Score;
		morphed.ScoreIcon = ScoreIcon;
		morphed.Health = morphed.SpawnHealth();
		if (TID && (style & MRF_NEWTIDBEHAVIOUR))
		{
			morphed.ChangeTid(TID);
			ChangeTid(0);
		}
		
		// special2 is no longer used here since Actors now have a proper field for it.
		morphed.PremorphProperties = (bSolid * MPROP_SOLID) | (bShootable * MPROP_SHOOTABLE)
										| (bNoBlockmap * MPROP_NO_BLOCKMAP) | (bNoSector * MPROP_NO_SECTOR)
										| (bNoInteraction * MPROP_NO_INTERACTION) | (bInvisible * MPROP_INVIS);
		
		morphed.bShadow |= bShadow;
		morphed.bNoGravity |= bNoGravity;
		morphed.bFly |= bFly;
		morphed.bGhost |= bGhost;

		// Remove all armor.
		if (!(style & MRF_KEEPARMOR))
		{
			for (Inventory item = morphed.Inv; item;)
			{
				Inventory next = item.Inv;
				if (item is "Armor")
					item.DepleteOrDestroy();

				item = next;
			}
		}

		// Players store their morph behavior into their PlayerInfo unlike regular Actors which use the
		// morph properties. This is needed for backwards compatibility and to give the HUD info.
		let p = morphed.player;
		morphed.SetMorphTics(duration);
		morphed.SetMorphStyle(style);
		morphed.SetMorphExitFlash(exitFlash);
		p.MorphedPlayerClass = spawnType;
		p.PremorphWeapon = p.ReadyWeapon;
		p.Health = morphed.Health;
		p.Vel = (0.0, 0.0);
		// If the new view height is higher than the old one, start moving toward it.
		if (morphed.ViewHeight > p.ViewHeight && !p.DeltaViewHeight)
			p.DeltaViewHeight = p.GetDeltaViewHeight();

		bNoInteraction = true;
		A_ChangeLinkFlags(true, true);

		// Legacy
		bSolid = bShootable = false;
		bInvisible = true;

		morphed.ClearFOVInterpolation();
		morphed.InitAllPowerupEffects();
		morphed.ActivateMorphWeapon();

		PostMorph(morphed, false);		// No longer the current body
		morphed.PostMorph(self, true);	// This is the current body

		if (enterFlash)
		{
			Actor fog = Spawn(enterFlash, morphed.Pos.PlusZ(GameInfo.TelefogHeight), ALLOW_REPLACE);
			if (fog)
				fog.Target = morphed;
		}

		return true;
	}
	
	//----------------------------------------------------------------------------
	//
	// FUNC UndoPlayerMorph
	//
	//----------------------------------------------------------------------------

	virtual bool UndoPlayerMorph(PlayerInfo activator, EMorphFlags unmorphFlags = 0, bool force = false)
	{
		if (!Alternative || bStayMorphed || Alternative.bStayMorphed)
			return false;

		if (!(unmorphFlags & MRF_IGNOREINVULN) && bInvulnerable
			&& (player != activator || (!(player.MorphStyle & MRF_WHENINVULNERABLE) && !(unmorphFlags & MRF_STANDARDUNDOING))))
		{
			return false;
		}

		let alt = PlayerPawn(Alternative);
		alt.SetOrigin(Pos, false);
		// Test if there's room to unmorph.
		if (!force && (PremorphProperties & MPROP_SOLID))
		{
			bool altSolid = alt.bSolid;
			bool isSolid = bSolid;
			bool isTouchy = bTouchy;

			alt.bSolid = true;
			bSolid = bTouchy = false;

			bool res = alt.TestMobjLocation();

			alt.bSolid = altSolid;
			bSolid = isSolid;
			bTouchy = isTouchy;

			if (!res)
			{
				SetMorphTics(2 * TICRATE);
				return false;
			}
		}

		if (!MorphInto(alt))
			return false;

		PreUnmorph(alt, false);		// This body's about to be left.
		alt.PreUnmorph(self, true);	// This one's about to become current.

		alt.EndAllPowerupEffects();

		// Remove the morph power if the morph is being undone prematurely.
		for (Inventory item = alt.Inv; item;)
		{
			Inventory next = item.Inv;
			if (item is "PowerMorph")
				item.Destroy();

			item = next;
		}

		alt.Angle = Angle;
		alt.Pitch = Pitch;
		alt.Target = Target;
		alt.Tracer = Tracer;
		alt.Master = Master;
		alt.FriendPlayer = FriendPlayer;
		alt.DesignatedTeam = DesignatedTeam;
		alt.Score = Score;
		alt.ScoreIcon = ScoreIcon;
		alt.ReactionTime = 18;
		alt.bSolid = (PremorphProperties & MPROP_SOLID);
		alt.bShootable = (PremorphProperties & MPROP_SHOOTABLE);
		alt.bInvisible = (PremorphProperties & MPROP_INVIS);
		alt.bShadow = bShadow;
		alt.bNoGravity = bNoGravity;
		alt.bGhost = bGhost;
		alt.bFly = bFly;
		alt.Vel = (0.0, 0.0, Vel.Z);

		alt.bNoInteraction = (PremorphProperties & MPROP_NO_INTERACTION);
		alt.A_ChangeLinkFlags((PremorphProperties & MPROP_NO_BLOCKMAP), (PremorphProperties & MPROP_NO_SECTOR));

		let p = alt.player;
		class<Actor> exitFlash = alt.GetMorphExitFlash();
		EMorphFlags style = alt.GetMorphStyle();
		Weapon premorphWeap = p.PremorphWeapon;

		if (TID && (style & MRF_NEWTIDBEHAVIOUR))
		{
			alt.ChangeTid(TID);
			ChangeTID(0);
		}

		alt.SetMorphTics(0);
		alt.SetMorphStyle(0);
		alt.SetMorphExitFlash(null);
		p.MorphedPlayerClass = null;
		p.PremorphWeapon = null;
		p.ViewHeight = alt.ViewHeight;
		p.Vel = (0.0, 0.0);
		if (p.Health > 0 || (style & MRF_UNDOBYDEATHSAVES))
			p.Health = alt.Health = alt.SpawnHealth();
		else
			alt.Health = p.Health;

		Inventory level2 = alt.FindInventory("PowerWeaponLevel2", true);
		if (level2)
			level2.Destroy();

		let morphWeap = p.ReadyWeapon;
		if (premorphWeap)
		{
			premorphWeap.PostMorphWeapon();
		}
		else
		{
			p.ReadyWeapon = null;
			p.PendingWeapon = WP_NOCHANGE;
			p.Refire = 0;
		}

		if (style & MRF_LOSEACTUALWEAPON)
		{
			// Improved "lose morph weapon" semantics.
			class<Weapon> morphWeapCls = MorphWeapon;
			if (morphWeapCls)
			{
				let originalMorphWeapon = Weapon(alt.FindInventory(morphWeapCls));
				if (originalMorphWeapon && originalMorphWeapon.GivenAsMorphWeapon)
					originalMorphWeapon.Destroy();
			}
		}
		else if (morphWeap) // Old behaviour (not really useful now).
		{
			morphWeap.Destroy();
		}

		// Reset the base AC of the player's Hexen armor back to its default.
		let hexArmor = HexenArmor(alt.FindInventory("HexenArmor"));
		if (hexArmor)
			hexArmor.Slots[4] = alt.HexenArmor[0];

		alt.ClearFOVInterpolation();
		alt.InitAllPowerupEffects();

		PostUnmorph(alt, false);		// This body is no longer current.
		alt.PostUnmorph(self, true);	// altmo body is current.

		if (exitFlash)
		{
			Actor fog = Spawn(exitFlash, alt.Vec3Angle(20.0, alt.Angle, GameInfo.TelefogHeight), ALLOW_REPLACE);
			if (fog)
				fog.Target = alt;
		}

		Destroy();
		return true;
	}
}
