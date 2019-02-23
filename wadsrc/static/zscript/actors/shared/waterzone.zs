class WaterZone : Actor
{
	default
	{
		+NOSECTOR
		+NOBLOCKMAP
		+NOGRAVITY
		+DONTSPLASH
	}
	
	override void PostBeginPlay ()
	{
		Super.PostBeginPlay ();
		CurSector.MoreFlags |= Sector.SECMF_UNDERWATER;
		Destroy ();
	}

	
}
