// The Doom and Heretic players are not excluded from pickup in case
// somebody wants to use these weapons with either of those games.

class FighterWeapon : Weapon
{
	Default
	{
		Weapon.Kickback 150;
		Inventory.ForbiddenTo "ClericPlayer", "MagePlayer";
	}
}

class ClericWeapon : Weapon
{
	Default
	{
		Weapon.Kickback 150;
		Inventory.ForbiddenTo "FighterPlayer", "MagePlayer";
	}
}

class MageWeapon : Weapon
{
	Default
	{
		Weapon.Kickback 150;
		Inventory.ForbiddenTo "FighterPlayer", "ClericPlayer";
	}
}

extend class Actor
{
	//============================================================================
	//
	//	AdjustPlayerAngle
	//
	//============================================================================

	const MAX_ANGLE_ADJUST = (5.);

	void AdjustPlayerAngle(FTranslatedLineTarget t)
	{
		double difference = deltaangle(angle, t.angleFromSource);
		if (abs(difference) > MAX_ANGLE_ADJUST)
		{
			if (difference > 0)
			{
				angle += MAX_ANGLE_ADJUST;
			}
			else
			{
				angle -= MAX_ANGLE_ADJUST;
			}
		}
		else
		{
			angle = t.angleFromSource;
		}
	}
	
	//============================================================================
	//
	// A_DropQuietusPieces
	//
	//============================================================================

	void A_DropWeaponPieces(class<Actor> p1, class<Actor> p2, class<Actor> p3)
	{
		for (int i = 0, j = 0; i < 3; ++i)
		{
			Actor piece = Spawn (j == 0 ?  p1 : j == 1 ? p2 : p3, Pos, ALLOW_REPLACE);
			if (piece != null)
			{
				piece.Vel = self.Vel + AngleToVector(i * 120., 1);
				piece.bDropped = true;
				j = (j == 0) ? random[PieceDrop](1, 2) : 3-j;
			}
		}
	}	
}
