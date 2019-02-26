extend class Actor
{
	
	virtual void ApplyKickback(Actor inflictor, Actor source, int damage, double angle, Name mod, int flags)
	{
		double ang;
		int kickback;
		double thrust;

		if (inflictor && inflictor.projectileKickback)
			kickback = inflictor.projectileKickback;
		else if (!source || !source.player || !source.player.ReadyWeapon)
			kickback = gameinfo.defKickback;
		else
			kickback = source.player.ReadyWeapon.Kickback;

		kickback = int(kickback * G_SkillPropertyFloat(SKILLP_KickbackFactor));
		if (kickback)
		{
			Actor origin = (source && (flags & DMG_INFLICTOR_IS_PUFF)) ? source : inflictor;

			if (flags & DMG_USEANGLE)
			{
				ang = angle;
			}
			else if (origin.pos.xy == pos.xy)
			{
				// If the origin and target are in exactly the same spot, choose a random direction.
				// (Most likely cause is from telefragging somebody during spawning because they
				// haven't moved from their spawn spot at all.)
				ang = frandom[Kickback](0., 360.);
			}
			else
			{
				ang = origin.AngleTo(self);
			}

			thrust = mod == 'MDK' ? 10 : 32;
			if (Mass > 0)
			{
				thrust = clamp((damage * 0.125 * kickback) / Mass, 0., thrust);
			}

			// Don't apply ultra-small damage thrust
			if (thrust < 0.01) thrust = 0;

			// make fall forwards sometimes
			if ((damage < 40) && (damage > health)
				&& (pos.Z - origin.pos.Z > 64)
				&& random[Kickback](0, 1)
				// [RH] But only if not too fast and not flying
				&& thrust < 10
				&& !bNoGravity
				&& !bNoForwardFall
				&& (inflictor == NULL || !inflictor.bNoForwardFall)
				)
			{
				ang += 180.;
				thrust *= 4;
			}
			if (source && source.player && (flags & DMG_INFLICTOR_IS_PUFF)
				&& source.player.ReadyWeapon != NULL &&	(source.player.ReadyWeapon.bSTAFF2_KICKBACK))
			{
				// Staff power level 2
				Thrust(10, ang);
				if (!bNoGravity)
				{
					Vel.Z += 5.;
				}
			}
			else
			{
				Thrust(thrust, ang);
			}
		}
	}

	
}
