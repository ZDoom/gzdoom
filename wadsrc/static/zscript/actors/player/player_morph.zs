extend class PlayerPawn
{
	private native void Substitute(PlayerPawn replacement);

	//===========================================================================
	//
	// EndAllPowerupEffects
	//
	// Calls EndEffect() on every Powerup in the inventory list.
	//
	//===========================================================================

	void InitAllPowerupEffects()
	{
		let item = Inv;
		while (item != null)
		{
			let power = Powerup(item);
			if (power != null)
			{
				power.InitEffect();
			}
			item = item.Inv;
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
		let item = Inv;
		while (item != null)
		{
			let power = Powerup(item);
			if (power != null)
			{
				power.EndEffect();
			}
			item = item.Inv;
		}
	}
	
	//===========================================================================
	//
	//
	//
	//===========================================================================

	virtual void ActivateMorphWeapon ()
	{
		class<Weapon> morphweaponcls = MorphWeapon;
		player.PendingWeapon = WP_NOCHANGE;

		if (player.ReadyWeapon != null)
		{
			let psp = player.GetPSprite(PSP_WEAPON);
			if (psp) 
			{
				psp.y = WEAPONTOP;
				player.ReadyWeapon.ResetPSprite(psp);
			}
		}

		if (morphweaponcls == null || !(morphweaponcls is 'Weapon'))
		{ // No weapon at all while morphed!
			player.ReadyWeapon = null;
		}
		else
		{
			player.ReadyWeapon = Weapon(FindInventory (morphweaponcls));
			if (player.ReadyWeapon == null)
			{
				player.ReadyWeapon = Weapon(GiveInventoryType (morphweaponcls));
				if (player.ReadyWeapon != null)
				{
					player.ReadyWeapon.GivenAsMorphWeapon = true; // flag is used only by new beastweap semantics in UndoPlayerMorph
				}
			}
			if (player.ReadyWeapon != null)
			{
				player.SetPsprite(PSP_WEAPON, player.ReadyWeapon.GetReadyState());
			}
		}

		if (player.ReadyWeapon != null)
		{
			player.SetPsprite(PSP_FLASH, null);
		}

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

	virtual bool MorphPlayer(playerinfo activator, Class<PlayerPawn> spawntype, int duration, int style, Class<Actor> enter_flash = null, Class<Actor> exit_flash = null)
	{
		if (bDontMorph)
		{
			return false;
		}
		if (bInvulnerable && (player != activator || !(style & MRF_WHENINVULNERABLE)))
		{ // Immune when invulnerable unless this is a power we activated
			return false;
		}
		if (player.morphTics)
		{ // Player is already a beast
			if ((GetClass() == spawntype) && bCanSuperMorph
				&& (player.morphTics < (((duration) ? duration : DEFMORPHTICS) - TICRATE))
				&& FindInventory('PowerWeaponLevel2', true) == null)
			{ // Make a super chicken
				GiveInventoryType ('PowerWeaponLevel2');
			}
			return false;
		}
		if (health <= 0)
		{ // Dead players cannot morph
			return false;
		}
		if (spawntype == null)
		{
			return false;
		}
		if (!(spawntype is 'PlayerPawn'))
		{
			return false;
		}
		if (spawntype == GetClass())
		{
			return false;
		}

		let morphed = PlayerPawn(Spawn (spawntype, Pos, NO_REPLACE));

		// Use GetClass in the event someone actually allows replacements.
		PreMorph(morphed, false);
		morphed.PreMorph(self, true);

		EndAllPowerupEffects();
		Substitute(morphed);
		if ((style & MRF_TRANSFERTRANSLATION) && !morphed.bDontTranslate)
		{
			morphed.Translation = Translation;
		}
		if (tid != 0 && (style & MRF_NEWTIDBEHAVIOUR))
		{
			morphed.ChangeTid(tid);
			ChangeTid(0);
		}
		morphed.Angle = Angle;
		morphed.target = target;
		morphed.tracer = tracer;
		morphed.alternative = self;
		morphed.FriendPlayer = FriendPlayer;
		morphed.DesignatedTeam = DesignatedTeam;
		morphed.Score = Score;
		player.PremorphWeapon = player.ReadyWeapon;
		
		morphed.special2 = bSolid * 2 + bShootable * 4 + bInvisible * 0x40;	// The factors are for savegame compatibility
		morphed.player = player;

		if (morphed.ViewHeight > player.viewheight && player.deltaviewheight == 0)
		{ // If the new view height is higher than the old one, start moving toward it.
			player.deltaviewheight = player.GetDeltaViewHeight();
		}
		morphed.bShadow |= bShadow;
		morphed.bNoGravity |= bNoGravity;
		morphed.bFly |= bFly;
		morphed.bGhost |= bGhost;

		if (enter_flash == null) enter_flash = 'TeleportFog';
		let eflash = Spawn(enter_flash, Pos + (0, 0, gameinfo.telefogheight), ALLOW_REPLACE);
		let p = player;
		player = null;
		alternative = morphed;
		bSolid = false;
		bShootable = false;
		bUnmorphed = true;
		bInvisible = true;
		
		p.morphTics = (duration) ? duration : DEFMORPHTICS;

		// [MH] Used by SBARINFO to speed up face drawing
		p.MorphedPlayerClass = spawntype;

		p.MorphStyle = style;
		if (exit_flash == null) exit_flash = 'TeleportFog';
		p.MorphExitFlash = exit_flash;
		p.health = morphed.health;
		p.mo = morphed;
		p.vel = (0, 0);
		morphed.ObtainInventory (self);
		// Remove all armor
		for (Inventory item = morphed.Inv; item != null; )
		{
			let next = item.Inv;
			if (item is 'Armor')
			{
				item.DepleteOrDestroy();
			}
			item = next;
		}
		morphed.InitAllPowerupEffects();
		morphed.ActivateMorphWeapon ();
		if (p.camera == self)	// can this happen?
		{
			p.camera = morphed;
		}
		morphed.ClearFOVInterpolation();
		morphed.ScoreIcon = ScoreIcon;	// [GRB]
		if (eflash)	
			eflash.target = morphed;
		PostMorph(morphed, false);		// No longer the current body
		morphed.PostMorph(self, true);	// This is the current body
		return true;
	}
	
	//----------------------------------------------------------------------------
	//
	// FUNC UndoPlayerMorph
	//
	//----------------------------------------------------------------------------

	virtual bool UndoPlayerMorph(playerinfo activator, int unmorphflag = 0, bool force = false)
	{
		if (alternative == null)
		{
			return false;
		}

		let player = self.player;
		bool DeliberateUnmorphIsOkay = !!(MRF_STANDARDUNDOING & unmorphflag);

		if ((bInvulnerable) // If the player is invulnerable
			&& ((player != activator)       // and either did not decide to unmorph,
			|| (!((player.MorphStyle & MRF_WHENINVULNERABLE)  // or the morph style does not allow it
			|| (DeliberateUnmorphIsOkay))))) // (but standard morph styles always allow it),
		{ // Then the player is immune to the unmorph.
			return false;
		}

		let altmo = PlayerPawn(alternative);
		altmo.SetOrigin (Pos, false);
		altmo.bSolid = true;
		bSolid = false;
		if (!force && !altmo.TestMobjLocation())
		{ // Didn't fit
			altmo.bSolid = false;
			bSolid = true;
			player.morphTics = 2*TICRATE;
			return false;
		}

		PreUnmorph(altmo, false);		// This body's about to be left.
		altmo.PreUnmorph(self, true);	// This one's about to become current.

		// No longer using tracer as morph storage. That is what 'alternative' is for. 
		// If the tracer has changed on the morph, change the original too.
		altmo.target = target;
		altmo.tracer = tracer;
		self.player = null;
		altmo.alternative = alternative = null;

		// Remove the morph power if the morph is being undone prematurely.
		for (Inventory item = Inv; item != null;)
		{
			let next = item.Inv;
			if (item is "PowerMorph")
			{
				item.Destroy();
			}
			item = next;
		}
		EndAllPowerupEffects();
		altmo.ObtainInventory (self);
		Substitute(altmo);
		if ((tid != 0) && (player.MorphStyle & MRF_NEWTIDBEHAVIOUR))
		{
			altmo.ChangeTid(tid);
		}
		altmo.Angle = Angle;
		altmo.player = player;
		altmo.reactiontime = 18;
		altmo.bSolid = !!(special2 & 2);
		altmo.bShootable = !!(special2 & 4);
		altmo.bInvisible = !!(special2 & 0x40);
		altmo.Vel = (0, 0, Vel.Z);
		player.Vel = (0, 0);
		altmo.floorz = floorz;
		altmo.bShadow = bShadow;
		altmo.bNoGravity = bNoGravity;
		altmo.bGhost = bGhost;
		altmo.bUnmorphed = false;
		altmo.Score = Score;
		altmo.InitAllPowerupEffects();

		let exit_flash = player.MorphExitFlash;
		bool correctweapon = !!(player.MorphStyle & MRF_LOSEACTUALWEAPON);
		bool undobydeathsaves = !!(player.MorphStyle & MRF_UNDOBYDEATHSAVES);

		player.morphTics = 0;
		player.MorphedPlayerClass = null;
		player.MorphStyle = 0;
		player.MorphExitFlash = null;
		player.viewheight = altmo.ViewHeight;
		Inventory level2 = altmo.FindInventory("PowerWeaponLevel2", true);
		if (level2 != null)
		{
			level2.Destroy ();
		}

		if ((player.health > 0) || undobydeathsaves)
		{
			player.health = altmo.health = altmo.SpawnHealth();
		}
		else // killed when morphed so stay dead
		{
			altmo.health = player.health;
		}

		player.mo = altmo;
		if (player.camera == self)
		{
			player.camera = altmo;
		}
		altmo.ClearFOVInterpolation();

		// [MH]
		// If the player that was morphed is the one
		// taking events, reset up the face, if any;
		// this is only needed for old-skool skins
		// and for the original DOOM status bar.
		if (player == players[consoleplayer])
		{
			if (face != 'None')
			{
				// Assume root-level base skin to begin with
				let skinindex = 0;
				let skin = player.GetSkin();
				// If a custom skin was in use, then reload it
				// or else the base skin for the player class.
				if (skin >= PlayerClasses.Size () && skin < PlayerSkins.Size())
				{
					skinindex = skin;
				}
				else if (PlayerClasses.Size () > 1)
				{
					let whatami = altmo.GetClass();
					for (int i = 0; i < PlayerClasses.Size (); ++i)
					{
						if (PlayerClasses[i].Type == whatami)
						{
							skinindex = i;
							break;
						}
					}
				}
			}
		}

		Actor eflash = null;
		if (exit_flash != null)
		{
			eflash = Spawn(exit_flash, Vec3Angle(20., altmo.Angle, gameinfo.telefogheight), ALLOW_REPLACE);
			if (eflash)	eflash.target = altmo;
		}
		WeaponSlots.SetupWeaponSlots(altmo);		// Use original class's weapon slots.
		let beastweap = player.ReadyWeapon;
		if (player.PremorphWeapon != null)
		{
			player.PremorphWeapon.PostMorphWeapon ();
		}
		else
		{
			player.ReadyWeapon = player.PendingWeapon = null;
		}
		if (correctweapon)
		{ // Better "lose morphed weapon" semantics
			class<Actor> morphweaponcls = MorphWeapon;
			if (morphweaponcls != null && morphweaponcls is 'Weapon')
			{
				let OriginalMorphWeapon = Weapon(altmo.FindInventory (morphweapon));
				if ((OriginalMorphWeapon != null) && (OriginalMorphWeapon.GivenAsMorphWeapon))
				{ // You don't get to keep your morphed weapon.
					if (OriginalMorphWeapon.SisterWeapon != null)
					{
						OriginalMorphWeapon.SisterWeapon.Destroy ();
					}
					OriginalMorphWeapon.Destroy ();
				}
			}
		}
		else // old behaviour (not really useful now)
		{ // Assumptions made here are no longer valid
			if (beastweap != null)
			{ // You don't get to keep your morphed weapon.
				if (beastweap.SisterWeapon != null)
				{
					beastweap.SisterWeapon.Destroy ();
				}
				beastweap.Destroy ();
			}
		}
		PostUnmorph(altmo, false);		// This body is no longer current.
		altmo.PostUnmorph(self, true);	// altmo body is current.
		Destroy ();
		// Restore playerclass armor to its normal amount.
		let hxarmor = HexenArmor(altmo.FindInventory('HexenArmor'));
		if (hxarmor != null)
		{
			hxarmor.Slots[4] = altmo.HexenArmor[0];
		}
		return true;
	}

	//===========================================================================
	//
	//
	//
	//===========================================================================

	override Actor, int, int MorphedDeath()
	{
		// Voodoo dolls should not unmorph the real player here.
		if (player && (player.mo == self) &&
			(player.morphTics) &&
			(player.MorphStyle & MRF_UNDOBYDEATH) &&
			(alternative))
		{
			Actor realme = alternative;
			int realstyle = player.MorphStyle;
			int realhealth = health;
			if (UndoPlayerMorph(player, 0, !!(player.MorphStyle & MRF_UNDOBYDEATHFORCED)))
			{
				return realme, realstyle, realhealth;
			}
		}
		return null, 0, 0;
	}
}

//===========================================================================
//
//
//
//===========================================================================

class MorphProjectile : Actor
{

	Class<PlayerPawn> PlayerClass;
	Class<Actor> MonsterClass, MorphFlash, UnMorphFlash;
	int Duration, MorphStyle;

	Default
	{
		Damage 1;
		Projectile;
		-ACTIVATEIMPACT
		-ACTIVATEPCROSS
	}
	
	override int DoSpecialDamage (Actor target, int damage, Name damagetype)
	{
		if (target.player)
		{
			// Voodoo dolls forward this to the real player
			target.player.mo.MorphPlayer (NULL, PlayerClass, Duration, MorphStyle, MorphFlash, UnMorphFlash);
		}
		else
		{
			target.MorphMonster (MonsterClass, Duration, MorphStyle, MorphFlash, UnMorphFlash);
		}
		return -1;
	}

	
}

//===========================================================================
//
//
//
//===========================================================================

class MorphedMonster : Actor
{
	Actor UnmorphedMe;
	int UnmorphTime, MorphStyle;
	Class<Actor> MorphExitFlash;
	int FlagsSave;

	Default
	{
		Monster;
		-COUNTKILL
		+FLOORCLIP
	}

	private native void Substitute(Actor replacement);

	override void OnDestroy ()
	{
		if (UnmorphedMe != NULL)
		{
			UnmorphedMe.Destroy ();
		}
		Super.OnDestroy();
	}

	override void Die (Actor source, Actor inflictor, int dmgflags, Name MeansOfDeath)
	{
		Super.Die (source, inflictor, dmgflags, MeansOfDeath);
		if (UnmorphedMe != NULL && UnmorphedMe.bUnmorphed)
		{
			UnmorphedMe.health = health;
			UnmorphedMe.Die (source, inflictor, dmgflags, MeansOfDeath);
		}
	}

	override void Tick ()
	{
		if (UnmorphTime > level.time || !UndoMonsterMorph())
		{
			Super.Tick();
		}
	}

	//----------------------------------------------------------------------------
	//
	// FUNC P_UndoMonsterMorph
	//
	// Returns true if the monster unmorphs.
	//
	//----------------------------------------------------------------------------

	virtual bool UndoMonsterMorph(bool force = false)
	{
		if (UnmorphTime == 0 || UnmorphedMe == NULL || bStayMorphed || UnmorphedMe.bStayMorphed)
		{
			return false;
		}
		let unmorphed = UnmorphedMe;
		unmorphed.SetOrigin (Pos, false);
		unmorphed.bSolid = true;
		bSolid = false;
		bool save = bTouchy;
		bTouchy = false;
		if (!force && !unmorphed.TestMobjLocation ())
		{ // Didn't fit
			unmorphed.bSolid = false;
			bSolid = true;
			bTouchy = save;
			UnmorphTime = level.time + 5*TICRATE; // Next try in 5 seconds
			return false;
		}
		PreUnmorph(unmorphed, false);
		unmorphed.PreUnmorph(self, true);
		unmorphed.Angle = Angle;
		unmorphed.target = target;
		unmorphed.bShadow = bShadow;
		unmorphed.bGhost = bGhost;
		unmorphed.bSolid = !!(flagssave & 2);
		unmorphed.bShootable = !!(flagssave & 4);
		unmorphed.bInvisible = !!(flagssave & 0x40);
		unmorphed.health = unmorphed.SpawnHealth();
		unmorphed.Vel = Vel;
		unmorphed.ChangeTid(tid);
		unmorphed.special = special;
		unmorphed.Score = Score;
		unmorphed.args[0] = args[0];
		unmorphed.args[1] = args[1];
		unmorphed.args[2] = args[2];
		unmorphed.args[3] = args[3];
		unmorphed.args[4] = args[4];
		unmorphed.CopyFriendliness (self, true);
		unmorphed.bUnmorphed = false;
		PostUnmorph(unmorphed, false);		// From is false here: Leaving the caller's body.
		unmorphed.PostUnmorph(self, true);	// True here: Entering this body from here.
		UnmorphedMe = NULL;
		Substitute(unmorphed);
		Destroy ();
		let eflash = Spawn(MorphExitFlash, Pos + (0, 0, gameinfo.TELEFOGHEIGHT), ALLOW_REPLACE);
		if (eflash)
			eflash.target = unmorphed;
		return true;
	}

	//===========================================================================
	//
	//
	//
	//===========================================================================

	override Actor, int, int MorphedDeath()
	{
		let realme = UnmorphedMe;
		if (realme != NULL)
		{
			if ((UnmorphTime) &&
				(MorphStyle & MRF_UNDOBYDEATH))
			{
				int realstyle = MorphStyle;
				int realhealth = health;
				if (UndoMonsterMorph(!!(MorphStyle & MRF_UNDOBYDEATHFORCED)))
				{
					return realme, realstyle, realhealth;
				}
			}
			if (realme.bBossDeath)
			{
				realme.health = 0;	// make sure that A_BossDeath considers it dead.
				realme.A_BossDeath();
			}
		}
		return null, 0, 0;
	}

}

