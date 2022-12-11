
// Oracle -------------------------------------------------------------------

class Oracle : Actor
{
	Default
	{
		Health 1;
		Radius 15;
		Height 56;
		Monster;
		+NOTDMATCH
		+NOBLOOD
		+NEVERRESPAWN
		DamageFactor "Fire", 0.5;
		DamageFactor "SpectralLow", 0;
		MaxDropoffHeight 32;
		Tag "$TAG_ORACLE";
		DropItem "Meat";
	}

	States
	{
	Spawn:
		ORCL A -1;
		Stop;
	Death:
		ORCL BCDEFGHIJK 5;
		ORCL L 5 A_NoBlocking;
		ORCL M 5;
		ORCL N 5 A_WakeOracleSpectre;
		ORCL OP 5;
		ORCL Q -1;
		Stop;
	}
	
	void A_WakeOracleSpectre ()
	{
		ThinkerIterator it = ThinkerIterator.Create("AlienSpectre3");
		Actor spectre = Actor(it.Next());

		if (spectre != NULL && spectre.health > 0 && self.target != spectre)
		{
			spectre.CurSector.SoundTarget = spectre.LastHeard = self.LastHeard;
			spectre.target = self.target;
			spectre.SetState (spectre.SeeState);
		}
	}

	
}
