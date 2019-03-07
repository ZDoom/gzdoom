class Weapon : StateProvider
{
	enum EFireMode
	{
		PrimaryFire,
		AltFire,
		EitherFire
	};

	const ZOOM_INSTANT = 1;
	const ZOOM_NOSCALETURNING = 2;
	
	deprecated("3.7") uint WeaponFlags;		// not to be used directly.
	class<Ammo> AmmoType1, AmmoType2;		// Types of ammo used by self weapon
	int AmmoGive1, AmmoGive2;				// Amount of each ammo to get when picking up weapon
	deprecated("3.7") int MinAmmo1, MinAmmo2;	// not used anywhere and thus deprecated.
	int AmmoUse1, AmmoUse2;					// How much ammo to use with each shot
	int Kickback;
	double YAdjust;							// For viewing the weapon fullscreen
	sound UpSound, ReadySound;				// Sounds when coming up and idle
	class<Weapon> SisterWeaponType;			// Another weapon to pick up with self one
	int SelectionOrder;						// Lower-numbered weapons get picked first
	int MinSelAmmo1, MinSelAmmo2;			// Ignore in BestWeapon() if inadequate ammo
	int ReloadCounter;						// For A_CheckForReload
	int BobStyle;							// [XA] Bobbing style. Defines type of bobbing (e.g. Normal, Alpha)  (visual only so no need to be a double)
	float BobSpeed;							// [XA] Bobbing speed. Defines how quickly a weapon bobs.
	float BobRangeX, BobRangeY;				// [XA] Bobbing range. Defines how far a weapon bobs in either direction.
	Ammo Ammo1, Ammo2;						// In-inventory instance variables
	Weapon SisterWeapon;
	double FOVScale;
	double LookScale;						// Multiplier for look sensitivity (like FOV scaling but without the zooming)
	int Crosshair;							// 0 to use player's crosshair
	bool GivenAsMorphWeapon;
	bool bAltFire;							// Set when this weapon's alternate fire is used.
	readonly bool bDehAmmo;					// Uses Doom's original amount of ammo for the respective attack functions so that old DEHACKED patches work as intended.
											// AmmoUse1 will be set to the first attack's ammo use so that checking for empty weapons still works
	meta int SlotNumber;
	meta double SlotPriority;
	
	property AmmoGive: AmmoGive1;
	property AmmoGive1: AmmoGive1;
	property AmmoGive2: AmmoGive2;
	property AmmoUse: AmmoUse1;
	property AmmoUse1: AmmoUse1;
	property AmmoUse2: AmmoUse2;
	property AmmoType: AmmoType1;
	property AmmoType1: AmmoType1;
	property AmmoType2: AmmoType2;
	property Kickback: Kickback;
	property ReadySound: ReadySound;
	property SelectionOrder: SelectionOrder;
	property MinSelectionAmmo1: MinSelAmmo1;
	property MinSelectionAmmo2: MinSelAmmo2;
	property SisterWeapon: SisterWeaponType;
	property UpSound: UpSound;
	property YAdjust: YAdjust;
	property BobSpeed: BobSpeed;
	property BobRangeX: BobRangeX;
	property BobRangeY: BobRangeY;
	property SlotNumber: SlotNumber;
	property SlotPriority: SlotPriority;
	property LookScale: LookScale;

	flagdef NoAutoFire: WeaponFlags, 0;			// weapon does not autofire
	flagdef ReadySndHalf: WeaponFlags, 1;		// ready sound is played ~1/2 the time
	flagdef DontBob: WeaponFlags, 2;			// don't bob the weapon
	flagdef AxeBlood: WeaponFlags, 3;			// weapon makes axe blood on impact
	flagdef NoAlert: WeaponFlags, 4;			// weapon does not alert monsters
	flagdef Ammo_Optional: WeaponFlags, 5;		// weapon can use ammo but does not require it
	flagdef Alt_Ammo_Optional: WeaponFlags, 6;	// alternate fire can use ammo but does not require it
	flagdef Primary_Uses_Both: WeaponFlags, 7;	// primary fire uses both ammo
	flagdef Alt_Uses_Both: WeaponFlags, 8;		// alternate fire uses both ammo
	flagdef Wimpy_Weapon:WeaponFlags, 9;		// change away when ammo for another weapon is replenished
	flagdef Powered_Up: WeaponFlags, 10;		// this is a tome-of-power'ed version of its sister
	flagdef Ammo_CheckBoth: WeaponFlags, 11;	// check for both primary and secondary fire before switching it off
	flagdef No_Auto_Switch: WeaponFlags, 12;	// never switch to this weapon when it's picked up
	flagdef Staff2_Kickback: WeaponFlags, 13;	// the powered-up Heretic staff has special kickback
	flagdef NoAutoaim: WeaponFlags, 14;			// this weapon never uses autoaim (useful for ballistic projectiles)
	flagdef MeleeWeapon: WeaponFlags, 15;		// melee weapon. Used by monster AI with AVOIDMELEE.
	flagdef NoDeathDeselect: WeaponFlags, 16;	// Don't jump to the Deselect state when the player dies
	flagdef NoDeathInput: WeaponFlags, 17;		// The weapon cannot be fired/reloaded/whatever when the player is dead
	flagdef CheatNotWeapon: WeaponFlags, 18;	// Give cheat considers this not a weapon (used by Sigil)

	// no-op flags
	flagdef NoLMS: none, 0;
	flagdef Allow_With_Respawn_Invul: none, 0;
	flagdef BFG: none, 0;
	flagdef Explosive: none, 0;

	Default
	{
		Inventory.PickupSound "misc/w_pkup";
		Weapon.DefaultKickback;
		Weapon.BobSpeed 1.0;
		Weapon.BobRangeX 1.0;
		Weapon.BobRangeY 1.0;
		Weapon.SlotNumber -1;
		Weapon.SlotPriority 32767;
		+WEAPONSPAWN
		DefaultStateUsage SUF_ACTOR|SUF_OVERLAY|SUF_WEAPON;
	}
	States
	{
	LightDone:
		SHTG E 0 A_Light0;
		Stop;
	}

	//===========================================================================
	//
	// Weapon :: MarkPrecacheSounds
	//
	//===========================================================================

	override void MarkPrecacheSounds()
	{
		Super.MarkPrecacheSounds();
		MarkSound(UpSound);
		MarkSound(ReadySound);
	}
	
	virtual int, int CheckAddToSlots()
	{
		if (GetReplacement(GetClass()) == GetClass() && !bPowered_Up)
		{
			return SlotNumber, int(SlotPriority*65536);
		}
		return -1, 0;
	}
	
	virtual State GetReadyState ()
	{
		return FindState('Ready');
	}
	
	virtual State GetUpState ()
	{
		return FindState('Select');
	}

	virtual State GetDownState ()
	{
		return FindState('Deselect');
	}

	virtual State GetAtkState (bool hold)
	{
		State s = null;
		if (hold) s = FindState('Hold');
		if (s == null) s = FindState('Fire');
		return s;
	}
	
	virtual State GetAltAtkState (bool hold)
	{
		State s = null;
		if (hold) s = FindState('AltHold');
		if (s == null) s = FindState('AltFire');
		return s;
	}
	
	virtual void PlayUpSound(Actor origin)
	{
		if (UpSound)
		{
			origin.A_PlaySound(UpSound, CHAN_WEAPON);
		}
	}
	
	override String GetObituary(Actor victim, Actor inflictor, Name mod, bool playerattack)
	{
		// Weapons may never return HitObituary by default. Override this if it is needed.
		return Obituary;
	}
	
	action void A_GunFlash(statelabel flashlabel = null, int flags = 0)
	{
		let player = player;

		if (null == player || player.ReadyWeapon == null)
		{
			return;
		}
		if (!(flags & GFF_NOEXTCHANGE))
		{
			player.mo.PlayAttacking2 ();
		}

		Weapon weapon = player.ReadyWeapon;
		state flashstate = null;

		if (flashlabel == null)
		{
			if (weapon.bAltFire)
			{
				flashstate = weapon.FindState('AltFlash');
			}
			if (flashstate == null)
			{
				flashstate = weapon.FindState('Flash');
			}
		}
		else
		{
			flashstate = weapon.FindState(flashlabel);
		}
		player.SetPsprite(PSP_FLASH, flashstate);
	}
	
	//---------------------------------------------------------------------------
	//
	// PROC A_Lower
	//
	//---------------------------------------------------------------------------

	action void A_Lower(int lowerspeed = 6)
	{
		let player = player;

		if (null == player)
		{
			return;
		}
		if (null == player.ReadyWeapon)
		{
			player.mo.BringUpWeapon();
			return;
		}
		let psp = player.GetPSprite(PSP_WEAPON);
		if (player.morphTics || player.cheats & CF_INSTANTWEAPSWITCH)
		{
			psp.y = WEAPONBOTTOM;
		}
		else
		{
			psp.y += lowerspeed;
		}
		if (psp.y < WEAPONBOTTOM)
		{ // Not lowered all the way yet
			return;
		}
		if (player.playerstate == PST_DEAD)
		{ // Player is dead, so don't bring up a pending weapon
			// Player is dead, so keep the weapon off screen
			player.SetPsprite(PSP_FLASH, null);
			psp.SetState(player.ReadyWeapon.FindState('DeadLowered'));
			return;
		}
		// [RH] Clear the flash state. Only needed for Strife.
		player.SetPsprite(PSP_FLASH, null);
		player.mo.BringUpWeapon ();
		return;
	}

	//---------------------------------------------------------------------------
	//
	// PROC A_Raise
	//
	//---------------------------------------------------------------------------

	action void A_Raise(int raisespeed = 6)
	{
		let player = player;

		if (null == player)
		{
			return;
		}
		if (player.PendingWeapon != WP_NOCHANGE)
		{
			player.mo.DropWeapon();
			return;
		}
		if (player.ReadyWeapon == null)
		{
			return;
		}
		let psp = player.GetPSprite(PSP_WEAPON);
		psp.y -= raisespeed;
		if (psp.y > WEAPONTOP)
		{ // Not raised all the way yet
			return;
		}
		psp.y = WEAPONTOP;
		psp.SetState(player.ReadyWeapon.GetReadyState());
		return;
	}

	//============================================================================
	//
	// PROC A_WeaponReady
	//
	// Readies a weapon for firing or bobbing with its three ancillary functions,
	// DoReadyWeaponToSwitch(), DoReadyWeaponToFire() and DoReadyWeaponToBob().
	// [XA] Added DoReadyWeaponToReload() and DoReadyWeaponToZoom()
	//
	//============================================================================

	static void DoReadyWeaponToSwitch (PlayerInfo player, bool switchable)
	{
		// Prepare for switching action.
		if (switchable)
		{
			player.WeaponState |= WF_WEAPONSWITCHOK | WF_REFIRESWITCHOK;
		}
		else
		{
			// WF_WEAPONSWITCHOK is automatically cleared every tic by P_SetPsprite().
			player.WeaponState &= ~WF_REFIRESWITCHOK;
		}
	}

	static void DoReadyWeaponDisableSwitch (PlayerInfo player, int disable)
	{
		// Discard all switch attempts?
		if (disable)
		{
			player.WeaponState |= WF_DISABLESWITCH;
			player.WeaponState &= ~WF_REFIRESWITCHOK;
		}
		else
		{
			player.WeaponState &= ~WF_DISABLESWITCH;
		}
	}

	static void DoReadyWeaponToFire (PlayerPawn pawn, bool prim, bool alt)
	{
		let player = pawn.player;
		let weapon = player.ReadyWeapon;
		if (!weapon)
		{
			return;
		}

		// Change player from attack state
		if (pawn.InStateSequence(pawn.curstate, pawn.MissileState) ||
			pawn.InStateSequence(pawn.curstate, pawn.MeleeState))
		{
			pawn.PlayIdle ();
		}

		// Play ready sound, if any.
		if (weapon.ReadySound && player.GetPSprite(PSP_WEAPON).curState == weapon.FindState('Ready'))
		{
			if (!weapon.bReadySndHalf || random[WpnReadySnd]() < 128)
			{
				pawn.A_PlaySound(weapon.ReadySound, CHAN_WEAPON);
			}
		}

		// Prepare for firing action.
		player.WeaponState |= ((prim ? WF_WEAPONREADY : 0) | (alt ? WF_WEAPONREADYALT : 0));
		return;
	}

	static void DoReadyWeaponToBob (PlayerInfo player)
	{
		if (player.ReadyWeapon)
		{
			// Prepare for bobbing action.
			player.WeaponState |= WF_WEAPONBOBBING;
			let pspr = player.GetPSprite(PSP_WEAPON);
			pspr.x = 0;
			pspr.y = WEAPONTOP;
		}
	}

	static int GetButtonStateFlags(int flags)
	{
		// Rewritten for efficiency and clarity
		int outflags = 0;
		if (flags & WRF_AllowZoom) outflags |= WF_WEAPONZOOMOK;
		if (flags & WRF_AllowReload) outflags |= WF_WEAPONRELOADOK;
		if (flags & WRF_AllowUser1) outflags |= WF_USER1OK;
		if (flags & WRF_AllowUser2) outflags |= WF_USER2OK;
		if (flags & WRF_AllowUser3) outflags |= WF_USER3OK;
		if (flags & WRF_AllowUser4) outflags |= WF_USER4OK;
		return outflags;
	}
	
	action void A_WeaponReady(int flags = 0)
	{
		if (!player) return;
														DoReadyWeaponToSwitch(player, !(flags & WRF_NoSwitch));
		if ((flags & WRF_NoFire) != WRF_NoFire)			DoReadyWeaponToFire(player.mo, !(flags & WRF_NoPrimary), !(flags & WRF_NoSecondary));
		if (!(flags & WRF_NoBob))						DoReadyWeaponToBob(player);

		player.WeaponState |= GetButtonStateFlags(flags);														
		DoReadyWeaponDisableSwitch(player, flags & WRF_DisableSwitch);
	}

	//---------------------------------------------------------------------------
	//
	// PROC A_CheckReload
	//
	// Present in Doom, but unused. Also present in Strife, and actually used.
	//
	//---------------------------------------------------------------------------

	action void A_CheckReload()
	{
		let player = self.player;
		if (player != NULL)
		{
			player.ReadyWeapon.CheckAmmo (player.ReadyWeapon.bAltFire ? Weapon.AltFire : Weapon.PrimaryFire, true);
		}
	}
		
	//===========================================================================
	//
	// A_ZoomFactor
	//
	//===========================================================================

	action void A_ZoomFactor(double zoom = 1, int flags = 0)
	{
		let player = self.player;
		if (player != NULL && player.ReadyWeapon != NULL)
		{
			zoom = 1 / clamp(zoom, 0.1, 50.0);
			if (flags & 1)
			{ // Make the zoom instant.
				player.FOV = player.DesiredFOV * zoom;
			}
			if (flags & 2)
			{ // Disable pitch/yaw scaling.
				zoom = -zoom;
			}
			player.ReadyWeapon.FOVScale = zoom;
		}
	}

	//===========================================================================
	//
	// A_SetCrosshair
	//
	//===========================================================================

	action void A_SetCrosshair(int xhair)
	{
		let player = self.player;
		if (player != NULL && player.ReadyWeapon != NULL)
		{
			player.ReadyWeapon.Crosshair = xhair;
		}
	}
	
	//===========================================================================
	//
	// Weapon :: TryPickup
	//
	// If you can't see the weapon when it's active, then you can't pick it up.
	//
	//===========================================================================

	override bool TryPickupRestricted (in out Actor toucher)
	{
		// Wrong class, but try to pick up for ammo
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

	//===========================================================================
	//
	// Weapon :: TryPickup
	//
	//===========================================================================

	override bool TryPickup (in out Actor toucher)
	{
		State ReadyState = FindState('Ready');
		if (ReadyState != NULL && ReadyState.ValidateSpriteFrame())
		{
			return Super.TryPickup (toucher);
		}
		return false;
	}

	//===========================================================================
	//
	// Weapon :: Use
	//
	// Make the player switch to self weapon.
	//
	//===========================================================================

	override bool Use (bool pickup)
	{
		Weapon useweap = self;

		// Powered up weapons cannot be used directly.
		if (bPowered_Up) return false;

		// If the player is powered-up, use the alternate version of the
		// weapon, if one exists.
		if (SisterWeapon != NULL &&
			SisterWeapon.bPowered_Up &&
			Owner.FindInventory ("PowerWeaponLevel2", true))
		{
			useweap = SisterWeapon;
		}
		if (Owner.player != NULL && Owner.player.ReadyWeapon != useweap)
		{
			Owner.player.PendingWeapon = useweap;
		}
		// Return false so that the weapon is not removed from the inventory.
		return false;
	}

	//===========================================================================
	//
	// Weapon :: Destroy
	//
	//===========================================================================

	override void OnDestroy()
	{
		let sister = SisterWeapon;

		if (sister != NULL)
		{
			// avoid recursion
			sister.SisterWeapon = NULL;
			if (sister != self)
			{ // In case we are our own sister, don't crash.
				sister.Destroy();
			}
		}
		Super.OnDestroy();
	}


	//===========================================================================
	//
	// Weapon :: HandlePickup
	//
	// Try to leach ammo from the weapon if you have it already.
	//
	//===========================================================================

	override bool HandlePickup (Inventory item)
	{
		if (item.GetClass() == GetClass())
		{
			if (Weapon(item).PickupForAmmo (self))
			{
				item.bPickupGood = true;
			}
			if (MaxAmount > 1) //[SP] If amount<maxamount do another pickup test of the weapon itself!
			{
				return Super.HandlePickup (item);
			}
			return true;
		}
		return false;
	}

	//===========================================================================
	//
	// Weapon :: PickupForAmmo
	//
	// The player already has self weapon, so try to pick it up for ammo.
	//
	//===========================================================================

	protected bool PickupForAmmo (Weapon ownedWeapon)
	{
		bool gotstuff = false;

		// Don't take ammo if the weapon sticks around.
		if (!ShouldStay ())
		{
			int oldamount1 = 0;
			int oldamount2 = 0;
			if (ownedWeapon.Ammo1 != NULL) oldamount1 = ownedWeapon.Ammo1.Amount;
			if (ownedWeapon.Ammo2 != NULL) oldamount2 = ownedWeapon.Ammo2.Amount;

			if (AmmoGive1 > 0) gotstuff = AddExistingAmmo (ownedWeapon.Ammo1, AmmoGive1);
			if (AmmoGive2 > 0) gotstuff |= AddExistingAmmo (ownedWeapon.Ammo2, AmmoGive2);

			let Owner = ownedWeapon.Owner;
			if (gotstuff && Owner != NULL && Owner.player != NULL)
			{
				if (ownedWeapon.Ammo1 != NULL && oldamount1 == 0)
				{
					PlayerPawn(Owner).CheckWeaponSwitch(ownedWeapon.Ammo1.GetClass());
				}
				else if (ownedWeapon.Ammo2 != NULL && oldamount2 == 0)
				{
					PlayerPawn(Owner).CheckWeaponSwitch(ownedWeapon.Ammo2.GetClass());
				}
			}
		}
		return gotstuff;
	}

	//===========================================================================
	//
	// Weapon :: CreateCopy
	//
	//===========================================================================

	override Inventory CreateCopy (Actor other)
	{
		let copy = Weapon(Super.CreateCopy (other));
		if (copy != self && copy != null)
		{
			copy.AmmoGive1 = AmmoGive1;
			copy.AmmoGive2 = AmmoGive2;
		}
		return copy;
	}

	//===========================================================================
	//
	// Weapon :: CreateTossable
	//
	// A weapon that's tossed out should contain no ammo, so you can't cheat
	// by dropping it and then picking it back up.
	//
	//===========================================================================

	override Inventory CreateTossable (int amt)
	{
		// Only drop the weapon that is meant to be placed in a level. That is,
		// only drop the weapon that normally gives you ammo.
		if (SisterWeapon != NULL && 
			Default.AmmoGive1 == 0 && Default.AmmoGive2 == 0 &&
			(SisterWeapon.Default.AmmoGive1 > 0 || SisterWeapon.Default.AmmoGive2 > 0))
		{
			return SisterWeapon.CreateTossable (amt);
		}
		let copy = Weapon(Super.CreateTossable (-1));

		if (copy != NULL)
		{
			// If self weapon has a sister, remove it from the inventory too.
			if (SisterWeapon != NULL)
			{
				SisterWeapon.SisterWeapon = NULL;
				SisterWeapon.Destroy ();
			}
			// To avoid exploits, the tossed weapon must not have any ammo.
			copy.AmmoGive1 = 0;
			copy.AmmoGive2 = 0;
		}
		return copy;
	}

	//===========================================================================
	//
	// Weapon :: AttachToOwner
	//
	//===========================================================================

	override void AttachToOwner (Actor other)
	{
		Super.AttachToOwner (other);

		Ammo1 = AddAmmo (Owner, AmmoType1, AmmoGive1);
		Ammo2 = AddAmmo (Owner, AmmoType2, AmmoGive2);
		SisterWeapon = AddWeapon (SisterWeaponType);
		if (Owner.player != NULL)
		{
			if (!Owner.player.GetNeverSwitch() && !bNo_Auto_Switch)
			{
				Owner.player.PendingWeapon = self;
			}
			if (Owner.player.mo == players[consoleplayer].camera)
			{
				StatusBar.ReceivedWeapon (self);
			}
		}
		GivenAsMorphWeapon = false; // will be set explicitly by morphing code
	}

	//===========================================================================
	//
	// Weapon :: AddAmmo
	//
	// Give some ammo to the owner, even if it's just 0.
	//
	//===========================================================================

	protected Ammo AddAmmo (Actor other, Class<Ammo> ammotype, int amount)
	{
		Ammo ammoitem;

		if (ammotype == NULL)
		{
			return NULL;
		}

		// [BC] This behavior is from the original Doom. Give 5/2 times as much ammoitem when
		// we pick up a weapon in deathmatch.
		if (( deathmatch ) && ( gameinfo.gametype & GAME_DoomChex ))
			amount = amount * 5 / 2;

		// extra ammoitem in baby mode and nightmare mode
		if (!bIgnoreSkill)
		{
			amount = int(amount * G_SkillPropertyFloat(SKILLP_AmmoFactor));
		}
		ammoitem = Ammo(other.FindInventory (ammotype));
		if (ammoitem == NULL)
		{
			ammoitem = Ammo(Spawn (ammotype));
			ammoitem.Amount = MIN (amount, ammoitem.MaxAmount);
			ammoitem.AttachToOwner (other);
		}
		else if (ammoitem.Amount < ammoitem.MaxAmount || sv_unlimited_pickup)
		{
			ammoitem.Amount += amount;
			if (ammoitem.Amount > ammoitem.MaxAmount && !sv_unlimited_pickup)
			{
				ammoitem.Amount = ammoitem.MaxAmount;
			}
		}
		return ammoitem;
	}

	//===========================================================================
	//
	// Weapon :: AddExistingAmmo
	//
	// Give the owner some more ammo he already has.
	//
	//===========================================================================

	protected bool AddExistingAmmo (Inventory ammo, int amount)
	{
		if (ammo != NULL && (ammo.Amount < ammo.MaxAmount || sv_unlimited_pickup))
		{
			// extra ammo in baby mode and nightmare mode
			if (!bIgnoreSkill)
			{
				amount = int(amount * G_SkillPropertyFloat(SKILLP_AmmoFactor));
			}
			ammo.Amount += amount;
			if (ammo.Amount > ammo.MaxAmount && !sv_unlimited_pickup)
			{
				ammo.Amount = ammo.MaxAmount;
			}
			return true;
		}
		return false;
	}

	//===========================================================================
	//
	// Weapon :: AddWeapon
	//
	// Give the owner a weapon if they don't have it already.
	//
	//===========================================================================

	protected Weapon AddWeapon (Class<Weapon> weapontype)
	{
		Weapon weap;

		if (weapontype == NULL)
		{
			return NULL;
		}
		weap = Weapon(Owner.FindInventory (weapontype));
		if (weap == NULL)
		{
			weap = Weapon(Spawn (weapontype));
			weap.AttachToOwner (Owner);
		}
		return weap;
	}

	//===========================================================================
	//
	// Weapon :: ShouldStay
	//
	//===========================================================================

	override bool ShouldStay ()
	{
		if (((multiplayer &&
			(!deathmatch && !alwaysapplydmflags)) || sv_weaponstay) &&
			!bDropped)
		{
			return true;
		}
		return false;
	}


	//===========================================================================
	//
	// Weapon :: EndPowerUp
	//
	// The Tome of Power just expired.
	//
	//===========================================================================

	virtual void EndPowerup ()
	{
		let player = Owner.player;
		if (SisterWeapon != NULL && bPowered_Up)
		{
			let ready = GetReadyState();
			if (ready != SisterWeapon.GetReadyState())
			{
				if (player.PendingWeapon == NULL ||	player.PendingWeapon == WP_NOCHANGE)
				{
					player.PendingWeapon = SisterWeapon;
				}
				player.WeaponState |= WF_REFIRESWITCHOK;
			}
			else
			{
				let psp = player.FindPSprite(PSP_WEAPON);
				if (psp != null && psp.Caller == player.ReadyWeapon && psp.CurState.InStateSequence(ready))
				{
					// If the weapon changes but the state does not, we have to manually change the PSprite's caller here.
					psp.Caller = SisterWeapon;
					player.ReadyWeapon = SisterWeapon;
				}
				else 
				{
					if (player.PendingWeapon == NULL || player.PendingWeapon == WP_NOCHANGE)
					{
						// Something went wrong. Initiate a regular weapon change.
						player.PendingWeapon = SisterWeapon;
					}
					player.WeaponState |= WF_REFIRESWITCHOK;
				}
			}
		}
	}

	
	//===========================================================================
	//
	// Weapon :: PostMorphWeapon
	//
	// Bring this weapon up after a player unmorphs.
	//
	//===========================================================================

	void PostMorphWeapon ()
	{
		if (Owner == null)
		{
			return;
		}
		let p = owner.player;
		p.PendingWeapon = WP_NOCHANGE;
		p.ReadyWeapon = self;
		p.refire = 0;

		let pspr = p.GetPSprite(PSP_WEAPON);
		pspr.y = WEAPONBOTTOM;
		pspr.ResetInterpolation();
		pspr.SetState(GetUpState());
	}

	//===========================================================================
	//
	// Weapon :: CheckAmmo
	//
	// Returns true if there is enough ammo to shoot.  If not, selects the
	// next weapon to use.
	//
	//===========================================================================
	
	virtual bool CheckAmmo(int fireMode, bool autoSwitch, bool requireAmmo = false, int ammocount = -1)
	{
		int count1, count2;
		int enough, enoughmask;
		int lAmmoUse1;

		if (sv_infiniteammo || (Owner.FindInventory ('PowerInfiniteAmmo', true) != null))
		{
			return true;
		}
		if (fireMode == EitherFire)
		{
			bool gotSome = CheckAmmo (PrimaryFire, false) || CheckAmmo (AltFire, false);
			if (!gotSome && autoSwitch)
			{
				PlayerPawn(Owner).PickNewWeapon (null);
			}
			return gotSome;
		}
		let altFire = (fireMode == AltFire);
		let optional = (altFire? bAlt_Ammo_Optional : bAmmo_Optional);
		let useboth = (altFire? bAlt_Uses_Both : bPrimary_Uses_Both);

		if (!requireAmmo && optional)
		{
			return true;
		}
		count1 = (Ammo1 != null) ? Ammo1.Amount : 0;
		count2 = (Ammo2 != null) ? Ammo2.Amount : 0;

		if (bDehAmmo && Ammo1 == null)
		{
			lAmmoUse1 = 0;
		}
		else if (ammocount >= 0 && bDehAmmo)
		{
			lAmmoUse1 = ammocount;
		}
		else
		{
			lAmmoUse1 = AmmoUse1;
		}

		enough = (count1 >= lAmmoUse1) | ((count2 >= AmmoUse2) << 1);
		if (useboth)
		{
			enoughmask = 3;
		}
		else
		{
			enoughmask = 1 << altFire;
		}
		if (altFire && FindState('AltFire') == null)
		{ // If this weapon has no alternate fire, then there is never enough ammo for it
			enough &= 1;
		}
		if (((enough & enoughmask) == enoughmask) || (enough && bAmmo_CheckBoth))
		{
			return true;
		}
		// out of ammo, pick a weapon to change to
		if (autoSwitch)
		{
			PlayerPawn(Owner).PickNewWeapon (null);
		}
		return false;
	}

		
	//===========================================================================
	//
	// Weapon :: DepleteAmmo
	//
	// Use up some of the weapon's ammo. Returns true if the ammo was successfully
	// depleted. If checkEnough is false, then the ammo will always be depleted,
	// even if it drops below zero.
	//
	//===========================================================================

	virtual bool DepleteAmmo(bool altFire, bool checkEnough = true, int ammouse = -1)
	{
		if (!(sv_infiniteammo || (Owner.FindInventory ('PowerInfiniteAmmo', true) != null)))
		{
			if (checkEnough && !CheckAmmo (altFire ? AltFire : PrimaryFire, false, false, ammouse))
			{
				return false;
			}
			if (!altFire)
			{
				if (Ammo1 != null)
				{
					if (ammouse >= 0 && bDehAmmo)
					{
						Ammo1.Amount -= ammouse;
					}
					else
					{
						Ammo1.Amount -= AmmoUse1;
					}
				}
				if (bPRIMARY_USES_BOTH && Ammo2 != null)
				{
					Ammo2.Amount -= AmmoUse2;
				}
			}
			else
			{
				if (Ammo2 != null)
				{
					Ammo2.Amount -= AmmoUse2;
				}
				if (bALT_USES_BOTH && Ammo1 != null)
				{
					Ammo1.Amount -= AmmoUse1;
				}
			}
			if (Ammo1 != null && Ammo1.Amount < 0)
				Ammo1.Amount = 0;
			if (Ammo2 != null && Ammo2.Amount < 0)
				Ammo2.Amount = 0;
		}
		return true;
	}


	//---------------------------------------------------------------------------
	//
	// Modifies the drop amount of this item according to the current skill's
	// settings (also called by ADehackedPickup::TryPickup)
	//
	//---------------------------------------------------------------------------
	override void ModifyDropAmount(int dropamount)
	{
		bool ignoreskill = true;
		double dropammofactor = G_SkillPropertyFloat(SKILLP_DropAmmoFactor);
		// Default drop amount is half of regular amount * regular ammo multiplication
		if (dropammofactor == -1) 
		{
			dropammofactor = 0.5;
			ignoreskill = false;
		}

		if (dropamount > 0)
		{
			self.Amount = dropamount;
		}
		// Adjust the ammo given by this weapon
		AmmoGive1 = int(AmmoGive1 * dropammofactor);
		AmmoGive2 = int(AmmoGive2 * dropammofactor);
		bIgnoreSkill = ignoreskill;
	}
	
}

class WeaponGiver : Weapon
{
	double AmmoFactor;
	
	Default
	{
		Weapon.AmmoGive1 -1;
		Weapon.AmmoGive2 -1;
	}
	
	override bool TryPickup(in out Actor toucher)
	{
		DropItem di = GetDropItems();
		Weapon weap;

		if (di != NULL)
		{
			Class<Weapon> ti = di.Name;
			if (ti != NULL)
			{
				if (master == NULL)
				{
					// save the spawned weapon in 'master' to avoid constant respawning if it cannot be picked up.
					master = weap = Weapon(Spawn(di.Name));
					if (weap != NULL)
					{
						weap.bAlwaysPickup = false;	// use the flag of self item only.
						weap.bDropped = bDropped;

						// If our ammo gives are non-negative, transfer them to the real weapon.
						if (AmmoGive1 >= 0) weap.AmmoGive1 = AmmoGive1;
						if (AmmoGive2 >= 0) weap.AmmoGive2 = AmmoGive2;

						// If AmmoFactor is non-negative, modify the given ammo amounts.
						if (AmmoFactor > 0)
						{
							weap.AmmoGive1 = int(weap.AmmoGive1 * AmmoFactor);
							weap.AmmoGive2 = int(weap.AmmoGive2 * AmmoFactor);
						}
						weap.BecomeItem();
					}
					else return false;
				}

				weap = Weapon(master);
				bool res = false;
				if (weap != null)
				{
					res = weap.CallTryPickup(toucher);
					if (res)
					{
						GoAwayAndDie();
						master = NULL;
					}
				}
				return res;
			}
		}
		return false;
	}
	
	//---------------------------------------------------------------------------
	//
	// Modifies the drop amount of this item according to the current skill's
	// settings (also called by ADehackedPickup::TryPickup)
	//
	//---------------------------------------------------------------------------

	override void ModifyDropAmount(int dropamount)
	{
		bool ignoreskill = true;
		double dropammofactor = G_SkillPropertyFloat(SKILLP_DropAmmoFactor);
		// Default drop amount is half of regular amount * regular ammo multiplication
		if (dropammofactor == -1) 
		{
			dropammofactor = 0.5;
			ignoreskill = false;
		}

		AmmoFactor = dropammofactor;
		bIgnoreSkill = ignoreskill;
	}
	
	
}

struct WeaponSlots native
{
	native bool, int, int LocateWeapon(class<Weapon> weap) const;
	native static void SetupWeaponSlots(PlayerPawn pp);
	native class<Weapon> GetWeapon(int slot, int index) const;
	native int SlotSize(int slot) const;
}
