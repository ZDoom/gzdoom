// Same as Doom weapons, but the obituaries are removed.

class Bootspoon : Fist
{
	Default
	{
		obituary "$OB_MPSPOON";
		Tag "$TAG_SPOON";
	}
}

class SuperBootspork : Chainsaw
{
	Default
	{
		obituary "$OB_MPBOOTSPORK";
		Inventory.PickupMessage "$GOTSUPERBOOTSPORK";
		Tag "$TAG_SPORK";
	}
}

class MiniZorcher : Pistol
{
	Default
	{
		obituary "$OB_MPZORCH";
		inventory.pickupmessage "$GOTMINIZORCHER";
		Tag "$TAG_MINIZORCHER";
	}
	States
	{
		Spawn:
			MINZ A -1;
			Stop;
	}
}

class LargeZorcher : Shotgun
{
	Default
	{
		obituary "$OB_MPZORCH";
		inventory.pickupmessage "$GOTLARGEZORCHER";
		Tag "$TAG_LARGEZORCHER";
	}
}

class SuperLargeZorcher : SuperShotgun
{
	Default
	{
		obituary "$OB_MPMEGAZORCH";
		inventory.pickupmessage "$GOTSUPERLARGEZORCHER";
		Tag "$TAG_SUPERLARGEZORCHER";
	}
}

class RapidZorcher : Chaingun
{
	Default
	{
		obituary "$OB_MPRAPIDZORCH";
		inventory.pickupmessage "$GOTRAPIDZORCHER";
		Tag "$TAG_RAPIDZORCHER";
	}
}

class ZorchPropulsor : RocketLauncher
{
	Default
	{
		obituary "";
		inventory.pickupmessage "$GOTZORCHPROPULSOR";
		Tag "$TAG_ZORCHPROPULSOR";
	}
	States
	{
	Fire:
		MISG B 8 A_GunFlash;
		MISG B 12 A_FireProjectile("PropulsorMissile");
		MISG B 0 A_ReFire;
		Goto Ready;
	}
}

class PropulsorMissile : Rocket
{
	Default
	{
		-ROCKETTRAIL
		-DEHEXPLOSION
		RenderStyle "Translucent";
		Obituary "$OB_MPPROPULSOR";
		Alpha 0.75;
	}
}	

class PhasingZorcher : PlasmaRifle
{
	Default
	{
		obituary "";
		inventory.pickupmessage "$GOTPHASINGZORCHER";
		Tag "$TAG_PHASINGZORCHER";
	}
	States
	{
	Fire:
		PLSG A 0 A_GunFlash;
		PLSG A 3 A_FireProjectile("PhaseZorchMissile");
		PLSG B 20 A_ReFire;
		Goto Ready;
	Flash:
		PLSF A 0 A_Jump(128, "Flash2");
		PLSF A 3 Bright A_Light1;
		Goto LightDone;
	Flash2:
		PLSF B 3 Bright A_Light1;
		Goto LightDone;
	}
}

class PhaseZorchMissile : PlasmaBall
{
	Default
	{
		RenderStyle "Translucent";
		Obituary "$OB_MPPHASEZORCH";
		Alpha 0.75;
	}
}	

class LAZDevice : BFG9000
{
	Default
	{
		obituary "";
		inventory.pickupmessage "$GOTLAZDEVICE";
		Tag "$TAG_LAZDEVICE";
	}
	States
	{
	Fire:
		BFGG A 20 A_BFGsound;
		BFGG B 10 A_GunFlash;
		BFGG B 10 A_FireProjectile("LAZBall");
		BFGG B 20 A_ReFire;
		Goto Ready;
	}
}

class LAZBall : BFGBall
{
	Default
	{
		RenderStyle "Translucent";
		Obituary "$OB_MPLAZ_BOOM";
		Alpha 0.75;
	}
}
