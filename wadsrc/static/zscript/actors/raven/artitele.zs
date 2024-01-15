
// Teleport (self) ----------------------------------------------------------

class ArtiTeleport : Inventory
{
	Default
	{
		+COUNTITEM
		+FLOATBOB
		+INVENTORY.INVBAR
		Inventory.PickupFlash "PickupFlash";
		+INVENTORY.FANCYPICKUPSOUND
		Inventory.DefMaxAmount;
		Inventory.Icon "ARTIATLP";
		Inventory.PickupSound "misc/p_pkup";
		Inventory.PickupMessage "$TXT_ARTITELEPORT";
		Tag "$TAG_ARTITELEPORT";
	}
	States
	{
	Spawn:
		ATLP ABCB 4;
		Loop;
	}
	
	override bool Use(bool pickup)
	{
		Vector3 dest;
		double destAngle;
		if (deathmatch)
			[dest, destAngle] = Level.PickDeathmatchStart();
		else
			[dest, destAngle] = Level.PickPlayerStart(Owner.PlayerNumber());

		if (!Level.UsePlayerStartZ)
			dest.Z = ONFLOORZ;

		Owner.Teleport(dest, destAngle, TELF_SOURCEFOG | TELF_DESTFOG);

		bool canLaugh = Owner.player != null;
		EMorphFlags mStyle = Owner.GetMorphStyle();
		if (Owner.Alternative && (mStyle & MRF_UNDOBYCHAOSDEVICE))
		{
			// Teleporting away will undo any morph effects (pig).
			if (!Owner.Unmorph(Owner, MRF_UNDOBYCHAOSDEVICE) && (mStyle & MRF_FAILNOLAUGH))
				canLaugh = false;
		}

		if (canLaugh)
			Owner.A_StartSound("*evillaugh", CHAN_VOICE, attenuation: ATTN_NONE);

		return true;
	}
	
}


