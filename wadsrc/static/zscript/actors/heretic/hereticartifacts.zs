// Super map ----------------------------------------------------------------

Class SuperMap : MapRevealer
{
	Default
	{
		+COUNTITEM
		+INVENTORY.ALWAYSPICKUP
		+FLOATBOB
		Inventory.MaxAmount 0;
		Inventory.PickupMessage "$TXT_ITEMSUPERMAP";
	}
	States
	{
	Spawn:
		SPMP A -1;
		Stop;
	}
}


// Invisibility -------------------------------------------------------------

Class ArtiInvisibility : PowerupGiver
{
	Default
	{
		+COUNTITEM
		+FLOATBOB
		Inventory.PickupFlash "PickupFlash";
		RenderStyle "Translucent";
		Alpha 0.4;
		Inventory.RespawnTics 4230;
		Inventory.Icon "ARTIINVS";
		Powerup.Type "PowerGhost";
		Inventory.PickupMessage "$TXT_ARTIINVISIBILITY";
		Tag "$TAG_ARTIINVISIBILITY";
	}
	States
	{
	Spawn:
		INVS A 350 Bright;
		Loop;
	}
}

 
// Tome of power ------------------------------------------------------------

Class ArtiTomeOfPower : PowerupGiver
{
	Default
	{
		+COUNTITEM
		+FLOATBOB
		Inventory.PickupFlash "PickupFlash";
		Inventory.Icon "ARTIPWBK";
		Powerup.Type "PowerWeaponlevel2";
		Inventory.PickupMessage "$TXT_ARTITOMEOFPOWER";
		Tag "$TAG_ARTITOMEOFPOWER";
	}
	States
	{
	Spawn:
		PWBK A 350;
		Loop;
	}
	
	override bool Use(bool pickup)
	{
		EMorphFlags mStyle = Owner.GetMorphStyle();
		if (Owner.Alternative && (mStyle & MRF_UNDOBYTOMEOFPOWER))
		{
			// Attempt to undo chicken.
			if (!Owner.Unmorph(Owner, MRF_UNDOBYTOMEOFPOWER))
			{
				if (!(mStyle & MRF_FAILNOTELEFRAG))
					Owner.DamageMobj(null, null, TELEFRAG_DAMAGE, 'Telefrag');
			}
			else if (Owner.player)
			{
				Owner.A_StartSound("*evillaugh", CHAN_VOICE);
			}

			return true;
		}

		return Super.Use(pickup);
	}
	
}


// Time bomb ----------------------------------------------------------------

Class ActivatedTimeBomb : Actor
{
	Default
	{
		+NOGRAVITY
		RenderStyle "Translucent";
		Alpha 0.4;
		DeathSound "misc/timebomb";
	}
		
	States
	{
	Spawn:
		FBMB ABCD 10;
		FBMB E 6 A_Scream;
		XPL1 A 4 BRIGHT A_Timebomb;
		XPL1 BCDEF 4 BRIGHT;
		Stop;
	}

	void A_TimeBomb()
	{
		AddZ(32, false);
		A_SetRenderStyle(1., STYLE_Add);
		A_Explode();
	}
}


Class ArtiTimeBomb : Inventory
{
	Default
	{
		+COUNTITEM
		+FLOATBOB
		Inventory.PickupFlash "PickupFlash";
		+INVENTORY.INVBAR
		+INVENTORY.FANCYPICKUPSOUND
		Inventory.Icon "ARTIFBMB";
		Inventory.PickupSound "misc/p_pkup";
		Inventory.PickupMessage "$TXT_ARTIFIREBOMB";
		Tag "$TAG_ARTIFIREBOMB";
		Inventory.DefMaxAmount;
	}
	States
	{
	Spawn:
		FBMB E 350;
		Loop;
	}
	
	override bool Use (bool pickup)
	{
		Actor mo = Spawn("ActivatedTimeBomb", Owner.Vec3Angle(24., Owner.angle, - Owner.Floorclip), ALLOW_REPLACE);
		if (mo != null) mo.target = Owner;
		return true;
	}
	
}
