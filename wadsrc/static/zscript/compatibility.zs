// This file contains compatibility wrappers for DECORATE functions with bad parameters or other things that were refactored since the first release.

extend class Object
{
	deprecated("2.4", "Use gameinfo.gametype instead") static int GameType()
	{ 
		return gameinfo.gametype; 
	}
	
	deprecated("2.4", "Use Console.MidPrint() instead") static void C_MidPrint(string fontname, string textlabel, bool bold = false)
	{
		let f = Font.GetFont(fontname);
		if (f == null) return;
		Console.MidPrint(f, textlabel, bold);
	}

}

extend class Actor
{
	//==========================================================================
	//
	// CheckClass
	//
	// NON-ACTION function to check a pointer's class.
	// deprecated because functionality is directly accessible.
	//
	//==========================================================================

	deprecated("3.7", "Use GetClass(), type cast, or 'is' operator instead") bool CheckClass(class<Actor> checkclass, int ptr_select = AAPTR_DEFAULT, bool match_superclass = false)
	{
		let check = GetPointer(ptr_select);
		if (check == null || checkclass == null)
		{
			return false;
		}
		else if (match_superclass)
		{
			return check is checkclass;
		}
		else
		{
			return check.GetClass() == checkclass;
		}
	}

	//==========================================================================
	//
	// GetDistance
	//
	// NON-ACTION function to get the distance in double.
	// deprecated because it requires AAPTR to work.
	//
	//==========================================================================
	deprecated("3.7", "Use Actor.Distance2D() or Actor.Distance3D() instead") double GetDistance(bool checkz, int ptr = AAPTR_TARGET) const
	{
		let target = GetPointer(ptr);

		if (!target || target == self)
		{
			return 0;
		}
		else
		{
			let diff = Vec3To(target);
			if (checkz)
				diff.Z += (target.Height - self.Height) / 2;
			else
				diff.Z = 0.;

			return diff.Length();
		}
	}

	//==========================================================================
	//
	// GetAngle
	//
	// NON-ACTION function to get the angle in degrees (normalized to -180..180)
	// deprecated because it requires AAPTR to work.
	//
	//==========================================================================

	deprecated("3.7", "Use Actor.AngleTo() instead") double GetAngle(int flags, int ptr = AAPTR_TARGET)
	{
		let target = GetPointer(ptr);

		if (!target || target == self)
		{
			return 0;
		}
		else
		{
			let angto = (flags & GAF_SWITCH) ? target.AngleTo(self) : self.AngleTo(target);
			let yaw = (flags & GAF_SWITCH) ? target.Angle : self.Angle;
			if (flags & GAF_RELATIVE) angto = deltaangle(yaw, angto);
			return angto;
		}
	}

	//==========================================================================
	//
	// GetSpriteAngle
	//
	// NON-ACTION function returns the sprite angle of a pointer.
	// deprecated because direct access to the data is now possible.
	//
	//==========================================================================
	
	deprecated("3.7", "Use Actor.SpriteAngle instead") double GetSpriteAngle(int ptr = AAPTR_DEFAULT)
	{
		let target = GetPointer(ptr);
		return target? target.SpriteAngle : 0;
	}

	//==========================================================================
	//
	// GetSpriteRotation
	//
	// NON-ACTION function returns the sprite rotation of a pointer.
	// deprecated because direct access to the data is now possible.
	//
	//==========================================================================

	deprecated("3.7", "Use Actor.SpriteRotation instead") double GetSpriteRotation(int ptr = AAPTR_DEFAULT)
	{
		let target = GetPointer(ptr);
		return target? target.SpriteRotation : 0;
	}

	deprecated("2.3", "Use A_SpawnProjectile() instead") void A_CustomMissile(class<Actor> missiletype, double spawnheight = 32, double spawnofs_xy = 0, double angle = 0, int flags = 0, double pitch = 0, int ptr = AAPTR_TARGET)
	{
		A_SpawnProjectile(missiletype, spawnheight, spawnofs_xy, angle, flags|CMF_BADPITCH, pitch, ptr);
	}
}

extend class StateProvider
{
	deprecated("2.3", "Use A_FireProjectile() instead") action void A_FireCustomMissile(class<Actor> missiletype, double angle = 0, bool useammo = true, double spawnofs_xy = 0, double spawnheight = 0, int flags = 0, double pitch = 0)
	{
		A_FireProjectile(missiletype, angle, useammo, spawnofs_xy, spawnheight, flags, -pitch);
	}
}
