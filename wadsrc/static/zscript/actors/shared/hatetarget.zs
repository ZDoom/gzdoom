

// Hate Target --------------------------------------------------------------

class HateTarget : Actor
{
	default
	{
		Radius 20;
		Height 56;
		+SHOOTABLE
		+NOGRAVITY
		+NOBLOOD
		+DONTSPLASH
		Mass 0x7fffffff;
	}
	States
	{
	Spawn:
		TNT1 A -1;
	}
	
	override void BeginPlay()
	{
		Super.BeginPlay();
		if (SpawnAngle != 0)
		{	// Each degree translates into 10 units of health
			health = SpawnAngle * 10;
		}
		else
		{
			special2 = 1;
			health = 1000001;
		}
	}

	override int TakeSpecialDamage(Actor inflictor, Actor source, int damage, Name damagetype)
	{
		if (special2 != 0)
		{
			return 0;
		}
		else
		{
			return damage;
		}
	}

	
}