
// Rocks --------------------------------------------------------------------

class Rock1 : Actor
{
	Default
	{
		+NOBLOCKMAP
		+DROPOFF
		+MISSILE
		+NOTELEPORT
	}
	States
	{
	Spawn:
		ROKK A 20;
		Loop;
	Death:
		ROKK A 10;
		Stop;
	}
}
	
class Rock2 : Actor
{
	Default
	{
		+NOBLOCKMAP
		+DROPOFF
		+MISSILE
		+NOTELEPORT
	}
	States
	{
	Spawn:
		ROKK B 20;
		Loop;
	Death:
		ROKK B 10;
		Stop;
	}
}


class Rock3 : Actor
{
	Default
	{
		+NOBLOCKMAP
		+DROPOFF
		+MISSILE
		+NOTELEPORT
	}
	States
	{
	Spawn:
		ROKK C 20;
		Loop;
	Death:
		ROKK C 10;
		Stop;
	}
}


// Dirt --------------------------------------------------------------------

class Dirt1 : Actor
{
	Default
	{
		+NOBLOCKMAP
		+DROPOFF
		+MISSILE
		+NOTELEPORT
	}
	States
	{
	Spawn:
		ROKK D 20;
		Loop;
	Death:
		ROKK D 10;
		Stop;
	}
}

class Dirt2 : Actor
{
	Default
	{
		+NOBLOCKMAP
		+DROPOFF
		+MISSILE
		+NOTELEPORT
	}
	States
	{
	Spawn:
		ROKK E 20;
		Loop;
	Death:
		ROKK E 10;
		Stop;
	}
}

class Dirt3 : Actor
{
	Default
	{
		+NOBLOCKMAP
		+DROPOFF
		+MISSILE
		+NOTELEPORT
	}
	States
	{
	Spawn:
		ROKK F 20;
		Loop;
	Death:
		ROKK F 10;
		Stop;
	}
}

class Dirt4 : Actor
{
	Default
	{
		+NOBLOCKMAP
		+DROPOFF
		+MISSILE
		+NOTELEPORT
	}
	States
	{
	Spawn:
		ROKK G 20;
		Loop;
	Death:
		ROKK G 10;
		Stop;
	}
}

class Dirt5 : Actor
{
	Default
	{
		+NOBLOCKMAP
		+DROPOFF
		+MISSILE
		+NOTELEPORT
	}
	States
	{
	Spawn:
		ROKK H 20;
		Loop;
	Death:
		ROKK H 10;
		Stop;
	}
}

class Dirt6 : Actor
{
	Default
	{
		+NOBLOCKMAP
		+DROPOFF
		+MISSILE
		+NOTELEPORT
	}
	States
	{
	Spawn:
		ROKK I 20;
		Loop;
	Death:
		ROKK I 10;
		Stop;
	}
}

// Stained glass ------------------------------------------------------------

class GlassShard : Actor
{
	Default
	{
		Radius 5;
		Mass 5;
		Projectile;
		-ACTIVATEPCROSS
		-ACTIVATEIMPACT
		BounceType "HexenCompat";
		BounceFactor 0.3;
	}
	
	override void Tick()
	{
		Super.Tick();
		if (Vel.Z > 0 && Vel.Z < 0.5 && pos.z < floorz + 1)
		{
			Destroy ();
		}
	}
	
}

class SGShard1 : GlassShard
{
	States
	{
	Spawn:
		SGSA ABCDE 4;
		Loop;
	Death:
		SGSA E 30;
		Stop;
	}
}

class SGShard2 : GlassShard
{
	States
	{
	Spawn:
		SGSA FGHIJ 4;
		Loop;
	Death:
		SGSA J 30;
		Stop;
	}
}

class SGShard3 : GlassShard
{
	States
	{
	Spawn:
		SGSA KLMNO 4;
		Loop;
	Death:
		SGSA O 30;
		Stop;
	}
}

class SGShard4 : GlassShard
{
	States
	{
	Spawn:
		SGSA PQRST 4;
		Loop;
	Death:
		SGSA T 30;
		Stop;
	}
}

class SGShard5 : GlassShard
{
	States
	{
	Spawn:
		SGSA UVWXY 4;
		Loop;
	Death:
		SGSA Y 30;
		Stop;
	}
}

class SGShard6 : GlassShard
{
	States
	{
	Spawn:
		SGSB A 4;
		Loop;
	Death:
		SGSB A 30;
		Stop;
	}
}

class SGShard7 : GlassShard
{
	States
	{
	Spawn:
		SGSB B 4;
		Loop;
	Death:
		SGSB B 30;
		Stop;
	}
}

class SGShard8 : GlassShard
{
	States
	{
	Spawn:
		SGSB C 4;
		Loop;
	Death:
		SGSB C 30;
		Stop;
	}
}

class SGShard9 : GlassShard
{
	States
	{
	Spawn:
		SGSB D 4;
		Loop;
	Death:
		SGSB D 30;
		Stop;
	}
}

class SGShard0 : GlassShard
{
	States
	{
	Spawn:
		SGSB E 4;
		Loop;
	Death:
		SGSB E 30;
		Stop;
	}
}

class GlassJunk : Actor
{
	Default
	{
		+NOCLIP
		+NOBLOCKMAP
		RenderStyle "Translucent";
		Alpha 0.4;
		Health 3;	// Number of different shards
	}
	States
	{
		// Are the first three frames used anywhere?
		SHAR A 128;
		Goto Death;
		SHAR B 128;
		Goto Death;
		SHAR C 128;
		Goto Death;
	Spawn:
		SHAR D 128;
		Goto Death;
		SHAR E 128;
		Goto Death;
		SHAR F 128;
		Goto Death;
	Death:
		"----" A 1 A_FadeOut(0.03);
		Wait;
	}
}
