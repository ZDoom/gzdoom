//===========================================================================
//
// Pain Elemental
//
//===========================================================================
class PainElemental : Actor
{
	Default
	{
		Health 400;
		Radius 31;
		Height 56;
		Mass 400;
		Speed 8;
		PainChance 128;
		Monster;
		+FLOAT 
		+NOGRAVITY
		SeeSound "pain/sight";
		PainSound "pain/pain";
		DeathSound "pain/death";
		ActiveSound "pain/active";
		Tag "$FN_PAIN";
	}
	States
	{
	Spawn:
		PAIN A 10 A_Look;
		Loop;
	See:
		PAIN AABBCC 3 A_Chase;
		Loop;
	Missile:
		PAIN D 5 A_FaceTarget;
		PAIN E 5 A_FaceTarget;
		PAIN F 5 BRIGHT A_FaceTarget;
		PAIN F 0 BRIGHT A_PainAttack;
		Goto See;
	Pain:
		PAIN G 6;
		PAIN G 6 A_Pain;
		Goto See;
	Death:
		PAIN H 8 BRIGHT;
		PAIN I 8 BRIGHT A_Scream;
		PAIN JK 8 BRIGHT;
		PAIN L 8 BRIGHT A_PainDie;
		PAIN M 8 BRIGHT;
		Stop;
	Raise:
		PAIN MLKJIH 8;
		Goto See;
	}
}


//===========================================================================
//
// Code (must be attached to Actor)
//
//===========================================================================

extend class Actor
{
	//
	// A_PainShootSkull
	// Spawn a lost soul and launch it at the target
	//
	void A_PainShootSkull(Class<Actor> spawntype, double angle, int flags = 0, int limit = -1)
	{
		// Don't spawn if we get massacred.
		if (DamageType == 'Massacre') return;

		if (spawntype == null) spawntype = "LostSoul";

		// [RH] check to make sure it's not too close to the ceiling
		if (pos.z + height + 8 > ceilingz)
		{
			if (bFloat)
			{
				Vel.Z -= 2;
				bInFloat = true;
				bVFriction = true;
			}
			return;
		}

		// [RH] make this optional
		if (limit < 0 && (Level.compatflags & COMPATF_LIMITPAIN))
			limit = 21;

		if (limit > 0)
		{
			// count total number of skulls currently on the level
			// if there are already 21 skulls on the level, don't spit another one
			int count = limit;
			ThinkerIterator it = ThinkerIterator.Create(spawntype);
			Thinker othink;

			while ( (othink = it.Next ()) )
			{
				if (--count == 0)
					return;
			}
		}

		// okay, there's room for another one
		double otherradius = GetDefaultByType(spawntype).radius;
		double prestep = 4 + (radius + otherradius) * 1.5;

		Vector2 move = AngleToVector(angle, prestep);
		Vector3 spawnpos = pos + (0,0,8);
		Vector3 destpos = spawnpos + move;

		Actor other = Spawn(spawntype, spawnpos, ALLOW_REPLACE);

		// Now check if the spawn is legal. Unlike Boom's hopeless attempt at fixing it, let's do it the same way
		// P_XYMovement solves the line skipping: Spawn the Lost Soul near the PE's center and then use multiple
		// smaller steps to get it to its intended position. This will also result in proper clipping, but
		// it will avoid all the problems of the Boom method, which checked too many lines that weren't even touched
		// and despite some adjustments never worked with portals.

		if (other != null)
		{
			double maxmove = other.radius - 1;

			if (maxmove <= 0) maxmove = 16;

			double xspeed = abs(move.X);
			double yspeed = abs(move.Y);

			int steps = 1;

			if (xspeed > yspeed)
			{
				if (xspeed > maxmove)
				{
					steps = int(1 + xspeed / maxmove);
				}
			}
			else
			{
				if (yspeed > maxmove)
				{
					steps = int(1 + yspeed / maxmove);
				}
			}

			Vector2 stepmove = move / steps;
			bool savedsolid = bSolid;
			bool savednoteleport = other.bNoTeleport;
			
			// make the PE nonsolid for the check and the LS non-teleporting so that P_TryMove doesn't do unwanted things.
			bSolid = false;
			other.bNoTeleport = true;
			for (int i = 0; i < steps; i++)
			{
				Vector2 ptry = other.pos.xy + stepmove;
				double oldangle = other.angle;
				if (!other.TryMove(ptry, 0))
				{
					// kill it immediately
					other.ClearCounters();
					other.DamageMobj(self, self, TELEFRAG_DAMAGE, 'None');
					bSolid = savedsolid;
					other.bNoTeleport = savednoteleport;
					return;
				}

				if (other.pos.xy != ptry)
				{
					// If the new position does not match the desired position, the player
					// must have gone through a portal.
					// For that we need to adjust the movement vector for the following steps.
					double anglediff = deltaangle(oldangle, other.angle);

					if (anglediff != 0)
					{
						stepmove = RotateVector(stepmove, anglediff);
					}
				}

			}
			bSolid = savedsolid;
			other.bNoTeleport = savednoteleport;

			// [RH] Lost souls hate the same things as their pain elementals
			other.CopyFriendliness (self, !(flags & PAF_NOTARGET));

			if (!(flags & PAF_NOSKULLATTACK))
			{
				other.A_SkullAttack();
			}
		}
	}

	
	void A_PainAttack(class<Actor> spawntype = "LostSoul", double addangle = 0, int flags = 0, int limit = -1)
	{
		if (target)
		{
			A_FaceTarget();
			A_PainShootSkull(spawntype, angle + addangle, flags, limit);
		}
	}
	
	void A_DualPainAttack(class<Actor> spawntype = "LostSoul")
	{
		if (target)
		{
			A_FaceTarget();
			A_PainShootSkull(spawntype, angle + 45);
			A_PainShootSkull(spawntype, angle - 45);
		}
	}
	
	void A_PainDie(class<Actor> spawntype = "LostSoul")
	{
		if (target && IsFriend(target))
		{ // And I thought you were my friend!
			bFriendly = false;
		}
		A_NoBlocking();
		A_PainShootSkull(spawntype, angle + 90);
		A_PainShootSkull(spawntype, angle + 180);
		A_PainShootSkull(spawntype, angle + 270);
	}
}
