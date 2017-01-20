class MorphProjectile : Actor
{

	Class<PlayerPawn> PlayerClass;
	Class<Actor> MonsterClass, MorphFlash, UnMorphFlash;
	int Duration, MorphStyle;

	Default
	{
		Damage 1;
		Projectile;
		-ACTIVATEIMPACT
		-ACTIVATEPCROSS
	}
	
	override int DoSpecialDamage (Actor target, int damage, Name damagetype)
	{
		if (target.player)
		{
			target.player.MorphPlayer (NULL, PlayerClass, Duration, MorphStyle, MorphFlash, UnMorphFlash);
		}
		else
		{
			target.MorphMonster (MonsterClass, Duration, MorphStyle, MorphFlash, UnMorphFlash);
		}
		return -1;
	}

	
}

class MorphedMonster : Actor native
{
	native Actor UnmorphedMe;
	native int UnmorphTime, MorphStyle;
	native Class<Actor> MorphExitFlash;

	Default
	{
		Monster;
		-COUNTKILL
		+FLOORCLIP
	}
}

