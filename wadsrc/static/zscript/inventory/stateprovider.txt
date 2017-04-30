
class StateProvider : Inventory native
{
	action native state A_JumpIfNoAmmo(statelabel label);
	action native void A_CustomPunch(int damage, bool norandom = false, int flags = CPF_USEAMMO, class<Actor> pufftype = "BulletPuff", double range = 0, double lifesteal = 0, int lifestealmax = 0, class<BasicArmorBonus> armorbonustype = "ArmorBonus", sound MeleeSound = 0, sound MissSound = "");
	action native void A_FireBullets(double spread_xy, double spread_z, int numbullets, int damageperbullet, class<Actor> pufftype = "BulletPuff", int flags = 1, double range = 0, class<Actor> missile = null, double Spawnheight = 32, double Spawnofs_xy = 0);
	action native Actor A_FireProjectile(class<Actor> missiletype, double angle = 0, bool useammo = true, double spawnofs_xy = 0, double spawnheight = 0, int flags = 0, double pitch = 0);
	action native void A_RailAttack(int damage, int spawnofs_xy = 0, bool useammo = true, color color1 = 0, color color2 = 0, int flags = 0, double maxdiff = 0, class<Actor> pufftype = "BulletPuff", double spread_xy = 0, double spread_z = 0, double range = 0, int duration = 0, double sparsity = 1.0, double driftspeed = 1.0, class<Actor> spawnclass = "none", double spawnofs_z = 0, int spiraloffset = 270, int limit = 0);
	action native void A_WeaponReady(int flags = 0);

	//---------------------------------------------------------------------------
	//
	// PROC A_ReFire
	//
	// The player can re-fire the weapon without lowering it entirely.
	//
	//---------------------------------------------------------------------------

	action void A_ReFire(statelabel flash = null)
	{
		let player = self.player;
		bool pending;

		if (NULL == player)
		{
			return;
		}
		pending = player.PendingWeapon != WP_NOCHANGE && (player.WeaponState & WF_REFIRESWITCHOK);
		if ((player.cmd.buttons & BT_ATTACK)
			&& !player.ReadyWeapon.bAltFire && !pending && player.health > 0)
		{
			player.refire++;
			player.mo.FireWeapon(ResolveState(flash));
		}
		else if ((player.cmd.buttons & BT_ALTATTACK)
			&& player.ReadyWeapon.bAltFire && !pending && player.health > 0)
		{
			player.refire++;
			player.mo.FireWeaponAlt(ResolveState(flash));
		}
		else
		{
			player.refire = 0;
			player.ReadyWeapon.CheckAmmo (player.ReadyWeapon.bAltFire? Weapon.AltFire : Weapon.PrimaryFire, true);
		}
	}
	
	action native state A_CheckForReload(int counter, statelabel label, bool dontincrement = false);
	action native void A_ResetReloadCounter();

	action void A_ClearReFire()
	{
		if (NULL != player)	player.refire = 0;
	}
}

class CustomInventory : StateProvider
{
	Default
	{
		DefaultStateUsage SUF_ACTOR|SUF_OVERLAY|SUF_ITEM;
	}
	
	//---------------------------------------------------------------------------
	//
	//
	//---------------------------------------------------------------------------

	// This is only here, because these functions were originally exported on Inventory, despite only working for weapons, so this is here to satisfy some potential old mods having called it through CustomInventory.
	deprecated("2.3") action void A_GunFlash(statelabel flash = null, int flags = 0) {}
	deprecated("2.3") action void A_Lower() {}
	deprecated("2.3") action void A_Raise() {}
	deprecated("2.3") action void A_CheckReload() {}
	native bool CallStateChain (Actor actor, State state);
		
	//===========================================================================
	//
	// ACustomInventory :: SpecialDropAction
	//
	//===========================================================================

	override bool SpecialDropAction (Actor dropper)
	{
		return CallStateChain (dropper, FindState('Drop'));
	}

	//===========================================================================
	//
	// ACustomInventory :: Use
	//
	//===========================================================================

	override bool Use (bool pickup)
	{
		return CallStateChain (Owner, FindState('Use'));
	}

	//===========================================================================
	//
	// ACustomInventory :: TryPickup
	//
	//===========================================================================

	override bool TryPickup (in out Actor toucher)
	{
		let pickupstate = FindState('Pickup');
		bool useok = CallStateChain (toucher, pickupstate);
		if ((useok || pickupstate == NULL) && FindState('Use') != NULL)
		{
			useok = Super.TryPickup (toucher);
		}
		else if (useok)
		{
			GoAwayAndDie();
		}
		return useok;
	}
}
