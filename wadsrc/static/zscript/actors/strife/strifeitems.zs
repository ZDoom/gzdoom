// Med patch -----------------------------------------------------------------

class MedPatch : HealthPickup
{
	Default
	{
		Health 10;
		+FLOORCLIP
		+INVENTORY.INVBAR
		Inventory.MaxAmount 20;
		Tag "$TAG_MEDPATCH";
		Inventory.Icon "I_STMP";
		Inventory.PickupMessage "$TXT_MEDPATCH";
		HealthPickup.Autouse 3;
	}
	States
	{
	Spawn:
		STMP A -1;
		Stop;
	}
}


// Medical Kit ---------------------------------------------------------------

class MedicalKit : HealthPickup
{
	Default
	{
		Health 25;
		+FLOORCLIP
		+INVENTORY.INVBAR
		Inventory.MaxAmount 15;
		Tag "$TAG_MEDICALKIT";
		Inventory.Icon "I_MDKT";
		Inventory.PickupMessage "$TXT_MEDICALKIT";
		HealthPickup.Autouse 3;
	}
	States
	{
	Spawn:
		MDKT A -1;
		Stop;
	}
}


// Surgery Kit --------------------------------------------------------------

class SurgeryKit : HealthPickup
{
	Default
	{
		+FLOORCLIP
		+INVENTORY.INVBAR
		Health -100;
		Inventory.MaxAmount 5;
		Tag "$TAG_SURGERYKIT";
		Inventory.Icon "I_FULL";
		Inventory.PickupMessage "$TXT_SURGERYKIT";
	}
	States
	{
	Spawn:
		FULL AB 35;
		Loop;
	}
}


// StrifeMap ----------------------------------------------------------------

class StrifeMap : MapRevealer
{
	Default
	{
		+FLOORCLIP
		Inventory.PickupSound "misc/p_pkup";
		Inventory.PickupMessage "$TXT_STRIFEMAP";
	}
	States
	{
	Spawn:
		SMAP AB 6 Bright;
		Loop;
	}
}
		

// Beldin's Ring ------------------------------------------------------------

class BeldinsRing : Inventory
{
	Default
	{
		+NOTDMATCH
		+FLOORCLIP
		+INVENTORY.INVBAR
		Tag "$TAG_BELDINSRING";
		Inventory.Icon "I_RING";
		Inventory.GiveQuest 1;
		Inventory.PickupMessage "$TXT_BELDINSRING";
	}
	States
	{
	Spawn:
		RING A -1;
		Stop;
	}
}


// Offering Chalice ---------------------------------------------------------

class OfferingChalice : Inventory
{
	Default
	{
		+DROPPED
		+FLOORCLIP
		+INVENTORY.INVBAR
		Radius 10;
		Height 16;
		Tag "$TAG_OFFERINGCHALICE";
		Inventory.Icon "I_RELC";
		Inventory.PickupMessage "$TXT_OFFERINGCHALICE";
		Inventory.GiveQuest 2;
	}
	States
	{
	Spawn:
		RELC A -1;
		Stop;
	}
}


// Ear ----------------------------------------------------------------------

class Ear : Inventory
{
	Default
	{
		+FLOORCLIP
		+INVENTORY.INVBAR
		Tag "$TAG_EAR";
		Inventory.Icon "I_EARS";
		Inventory.PickupMessage "$TXT_EAR";
		Inventory.GiveQuest 9;
	}
	States
	{
	Spawn:
		EARS A -1;
		Stop;
	}
}


// Broken Power Coupling ----------------------------------------------------

class BrokenPowerCoupling : Inventory
{
	Default
	{
		Health 40;
		+DROPPED
		+FLOORCLIP
		+INVENTORY.INVBAR
		Radius 16;
		Height 16;
		Tag "$TAG_BROKENCOUPLING";
		Inventory.MaxAmount 1;
		Inventory.Icon "I_COUP";
		Inventory.PickupMessage "$TXT_BROKENCOUPLING";
		Inventory.GiveQuest 8;
	}
	States
	{
	Spawn:
		COUP C -1;
		Stop;
	}
}


// Shadow Armor -------------------------------------------------------------

class ShadowArmor : PowerupGiver
{
	Default
	{
		+FLOORCLIP
		+VISIBILITYPULSE
		+INVENTORY.INVBAR
		-INVENTORY.FANCYPICKUPSOUND
		RenderStyle "Translucent";
		Tag "$TAG_SHADOWARMOR";
		Inventory.MaxAmount 2;
		Powerup.Type "PowerShadow";
		Inventory.Icon "I_SHD1";
		Inventory.PickupSound "misc/i_pkup";
		Inventory.PickupMessage "$TXT_SHADOWARMOR";
	}
	States
	{
	Spawn:
		SHD1 A -1 Bright;
		Stop;
	}
}


// Environmental suit -------------------------------------------------------

class EnvironmentalSuit : PowerupGiver
{
	Default
	{
		+FLOORCLIP
		+INVENTORY.INVBAR
		-INVENTORY.FANCYPICKUPSOUND
		Inventory.MaxAmount 5;
		Powerup.Type "PowerMask";
		Tag "$TAG_ENVSUIT";
		Inventory.Icon "I_MASK";
		Inventory.PickupSound "misc/i_pkup";
		Inventory.PickupMessage "$TXT_ENVSUIT";
	}
	States
	{
	Spawn:
		MASK A -1;
		Stop;
	}
}


// Guard Uniform ------------------------------------------------------------

class GuardUniform : Inventory
{
	Default
	{
		+FLOORCLIP
		+INVENTORY.INVBAR
		Tag "$TAG_GUARDUNIFORM";
		Inventory.Icon "I_UNIF";
		Inventory.PickupMessage "$TXT_GUARDUNIFORM";
		Inventory.GiveQuest 15;
	}
	States
	{
	Spawn:
		UNIF A -1;
		Stop;
	}
}


// Officer's Uniform --------------------------------------------------------

class OfficersUniform : Inventory
{
	Default
	{
		+FLOORCLIP
		+INVENTORY.INVBAR
		Tag "$TAG_OFFICERSUNIFORM";
		Inventory.Icon "I_OFIC";
		Inventory.PickupMessage "$TXT_OFFICERSUNIFORM";
	}
	States
	{
	Spawn:
		OFIC A -1;
		Stop;
	}
}
	
	
// Flame Thrower Parts ------------------------------------------------------

class FlameThrowerParts : Inventory 
{
	Default
	{
		+FLOORCLIP
		+INVENTORY.INVBAR
		Inventory.Icon "I_BFLM";
		Tag "$TAG_FTHROWERPARTS";
		Inventory.PickupMessage "$TXT_FTHROWERPARTS";
	}
	States
	{
	Spawn:
		BFLM A -1;
		Stop;
	}
}

// InterrogatorReport -------------------------------------------------------
// SCRIPT32 in strife0.wad has an Acolyte that drops this, but I couldn't
// find that Acolyte in the map. It seems to be totally unused in the
// final game.

class InterrogatorReport : Inventory
{
	Default
	{
		+FLOORCLIP
		Tag "$TAG_REPORT";
		Inventory.PickupMessage "$TXT_REPORT";
	}
	States
	{
	Spawn:
		TOKN A -1;
		Stop;
	}
}


// Info ---------------------------------------------------------------------

class Info : Inventory
{
	Default
	{
		+FLOORCLIP
		+INVENTORY.INVBAR
		Tag "$TAG_INFO";
		Inventory.Icon "I_TOKN";
		Inventory.PickupMessage "$TXT_INFO";
	}
	States
	{
	Spawn:
		TOKN A -1;
		Stop;
	}
}


// Targeter -----------------------------------------------------------------

class Targeter : PowerupGiver
{
	Default
	{
		+FLOORCLIP
		+INVENTORY.INVBAR
		-INVENTORY.FANCYPICKUPSOUND
		Tag "$TAG_TARGETER";
		Powerup.Type "PowerTargeter";
		Inventory.MaxAmount 5;
		Inventory.Icon "I_TARG";
		Inventory.PickupSound "misc/i_pkup";
		Inventory.PickupMessage "$TXT_TARGETER";
	}
	States
	{
	Spawn:
		TARG A -1;
		Stop;
	}
}
	
// Communicator -----------------------------------------------------------------

class Communicator : Inventory
{
	Default
	{
		+NOTDMATCH
		Tag "$TAG_COMMUNICATOR";
		Inventory.Icon "I_COMM";
		Inventory.PickupSound  "misc/p_pkup";
		Inventory.PickupMessage "$TXT_COMMUNICATOR";
	}
	States
	{
	Spawn:
		COMM A -1;
		Stop;
	}
}

// Degnin Ore ---------------------------------------------------------------

class DegninOre : Inventory
{
	Default
	{	
		Health 10;
		Radius 16;
		Height 16;
		Inventory.MaxAmount 10;
		+SOLID
		+SHOOTABLE
		+NOBLOOD
		+FLOORCLIP
		+INCOMBAT
		+INVENTORY.INVBAR
		Tag "$TAG_DEGNINORE";
		DeathSound "ore/explode";
		Inventory.Icon "I_XPRK";
		Inventory.PickupMessage "$TXT_DEGNINORE";
	}
	States
	{
	Spawn:
		XPRK A -1;
		Stop;
	Death:
		XPRK A 1 A_RemoveForceField;
		BNG3 A 0 A_SetRenderStyle(1, STYLE_Normal);
		BNG3 A 0 A_Scream;
		BNG3 A 3 Bright A_Explode(192, 192, alert:true);
		BNG3 BCDEFGH 3 Bright;
		Stop;
	}
	
	override bool Use (bool pickup)
	{
		if (pickup)
		{
			return false;
		}
		else
		{
			Inventory drop;

			// Increase the amount by one so that when DropInventory decrements it,
			// the actor will have the same number of beacons that he started with.
			// When we return to UseInventory, it will take care of decrementing
			// Amount again and disposing of this item if there are no more.
			Amount++;
			drop = Owner.DropInventory (self);
			if (drop == NULL)
			{
				Amount--;
				return false;
			}
			return true;
		}
	}
	
}

// Gun Training -------------------------------------------------------------

class GunTraining : Inventory
{
	Default
	{
		+FLOORCLIP
		+INVENTORY.INVBAR
		+INVENTORY.UNDROPPABLE
		Inventory.MaxAmount 100;
		Tag "$TAG_GUNTRAINING";
		Inventory.Icon "I_GUNT";
	}
	States
	{
	Spawn:
		GUNT A -1;
		Stop;
	}
}

// Health Training ----------------------------------------------------------

class HealthTraining : Inventory
{
	Default
	{
		+FLOORCLIP
		+INVENTORY.INVBAR
		+INVENTORY.UNDROPPABLE
		Inventory.MaxAmount 100;
		Tag "$TAG_HEALTHTRAINING";
		Inventory.Icon "I_HELT";
	}
	States
	{
	Spawn:
		HELT A -1;
		Stop;
	}
	
	override bool TryPickup (in out Actor toucher)
	{
		if (Super.TryPickup(toucher))
		{
			toucher.GiveInventoryType ("GunTraining");
			toucher.A_GiveInventory("Coin", toucher.player.mo.accuracy*5 + 300);
			return true;
		}
		return false;
	}
	
}

// Scanner ------------------------------------------------------------------

class Scanner : PowerupGiver 
{
	Default
	{
		+FLOORCLIP
		+INVENTORY.FANCYPICKUPSOUND
		Inventory.MaxAmount 1;
		Tag "$TAG_SCANNER";
		Inventory.Icon "I_PMUP";
		Powerup.Type "PowerScanner";
		Inventory.PickupSound "misc/i_pkup";
		Inventory.PickupMessage "$TXT_SCANNER";
	}
	States
	{
	Spawn:
		PMUP AB 6;
		Loop;
	}
	
	override bool Use (bool pickup)
	{
		if (!level.AllMap)
		{
			if (Owner.CheckLocalView())
			{
				Console.MidPrint(null, "$TXT_NEEDMAP");
			}
			return false;
		}
		return Super.Use (pickup);
	}
	
}

// Prison Pass --------------------------------------------------------------

class PrisonPass : Key
{
	Default
	{
		Inventory.Icon "I_TOKN";
		Tag "$TAG_PRISONPASS";
		Inventory.PickupMessage "$TXT_PRISONPASS";
	}
	States
	{
	Spawn:
		TOKN A -1;
		Stop;
	}
	
	override bool TryPickup (in out Actor toucher)
	{
		Super.TryPickup (toucher);
		Door_Open(223, 16);
		toucher.GiveInventoryType ("QuestItem10");
		return true;
	}

	//============================================================================
	//
	// APrisonPass :: SpecialDropAction
	//
	// Trying to make a monster that drops a prison pass turns it into an
	// OpenDoor223 item instead. That means the only way to get it in Strife
	// is through dialog, which is why it doesn't have its own sprite.
	//
	//============================================================================

	override bool SpecialDropAction (Actor dropper)
	{
		Door_Open(223, 16);
		Destroy ();
		return true;
	}
	
}

//---------------------------------------------------------------------------
// Dummy items. They are just used by Strife to perform ---------------------
// actions and cannot be held. ----------------------------------------------
//---------------------------------------------------------------------------

class DummyStrifeItem : Inventory
{
	States
	{
	Spawn:
		TOKN A -1;
		Stop;
	}
}

// Sound the alarm! ---------------------------------------------------------

class RaiseAlarm : DummyStrifeItem
{
	Default
	{	
		Tag "$TAG_ALARM";
	}
	
	override bool TryPickup (in out Actor toucher)
	{
		toucher.SoundAlert (toucher);

		ThinkerIterator it = ThinkerIterator.Create("AlienSpectre3");
		Actor spectre = Actor(it.Next());

		if (spectre != NULL && spectre.health > 0 && toucher != spectre)
		{
			spectre.CurSector.SoundTarget = spectre.LastHeard = toucher;
			spectre.target = toucher;
			spectre.SetState (spectre.SeeState);
		}
		GoAwayAndDie ();
		return true;
	}

	override bool SpecialDropAction (Actor dropper)
	{
		if (dropper.target != null)
		{
			dropper.target.SoundAlert(dropper.target);
			if (dropper.target.CheckLocalView())
			{
				Console.MidPrint(null, "$TXT_YOUFOOL");
			}
		}
		Destroy ();
		return true;
	}
	
}

// Open door tag 222 --------------------------------------------------------

class OpenDoor222 : DummyStrifeItem
{
	override bool TryPickup (in out Actor toucher)
	{
		Door_Open(222, 16);
		GoAwayAndDie ();
		return true;
	}
	
}

// Close door tag 222 -------------------------------------------------------

class CloseDoor222 : DummyStrifeItem
{
	override bool TryPickup (in out Actor toucher)
	{
		Door_Close(222, 16);
		GoAwayAndDie ();
		return true;
	}

	override bool SpecialDropAction (Actor dropper)
	{
		Door_Close(222, 16);
		if (dropper.target != null)
		{
			if (dropper.target.CheckLocalView())
			{
				Console.MidPrint(null, "$TXT_YOUREDEAD");
			}
			dropper.target.SoundAlert(dropper.target);
		}
		Destroy ();
		return true;
	}
	
}

// Open door tag 224 --------------------------------------------------------

class OpenDoor224 : DummyStrifeItem 
{
	override bool TryPickup (in out Actor toucher)
	{
		Door_Open(224, 16);
		GoAwayAndDie ();
		return true;
	}
	
	override bool SpecialDropAction (Actor dropper)
	{
		Door_Open(224, 16);
		Destroy ();
		return true;
	}
	
}

// Ammo ---------------------------------------------------------------------

class AmmoFillup : DummyStrifeItem
{
	Default
	{
		Tag "$TAG_AMMOFILLUP";
	}
	
	override bool TryPickup (in out Actor toucher)
	{
		Inventory item = toucher.FindInventory("ClipOfBullets");
		if (item == NULL)
		{
			item = toucher.GiveInventoryType ("ClipOfBullets");
			if (item != NULL)
			{
				item.Amount = 50;
			}
		}
		else if (item.Amount < 50)
		{
			item.Amount = 50;
		}
		else
		{
			return false;
		}
		GoAwayAndDie ();
		return true;
	}
	
}

// Health -------------------------------------------------------------------

class HealthFillup : DummyStrifeItem
{
	Default
	{
		Tag "$TAG_HEALTHFILLUP";
	}
	
	override bool TryPickup (in out Actor toucher)
	{
		static const int skillhealths[] = { -100, -75, -50, -50, -100 };

		int index = clamp(skill, 0,4);
		if (!toucher.GiveBody (skillhealths[index]))
		{
			return false;
		}
		GoAwayAndDie ();
		return true;
	}
	
}

// Upgrade Stamina ----------------------------------------------------------

class UpgradeStamina : DummyStrifeItem
{
	Default
	{
		Inventory.Amount 10;
		Inventory.MaxAmount 100;
	}
	
	override bool TryPickup (in out Actor toucher)
	{
		if (toucher.player == NULL)
			return false;
			
		toucher.player.mo.stamina += Amount;
		if (toucher.player.mo.stamina >= MaxAmount)
			toucher.player.mo.stamina = MaxAmount;
			
		toucher.GiveBody (-100);
		GoAwayAndDie ();
		return true;
	}
	
}

// Upgrade Accuracy ---------------------------------------------------------

class UpgradeAccuracy : DummyStrifeItem 
{
	override bool TryPickup (in out Actor toucher)
	{
		if (toucher.player == NULL || toucher.player.mo.accuracy >= 100)
			return false;
		toucher.player.mo.accuracy += 10;
		GoAwayAndDie ();
		return true;
	}
	
}

// Start a slideshow --------------------------------------------------------

class SlideshowStarter : DummyStrifeItem
{
	override bool TryPickup (in out Actor toucher)
	{
		Level.StartSlideshow();
		if (level.levelnum == 10)
		{
			toucher.GiveInventoryType ("QuestItem17");
		}
		GoAwayAndDie ();
		return true;
	}
	
}


