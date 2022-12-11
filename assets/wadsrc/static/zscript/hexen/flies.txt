
// Buzzy fly ----------------------------------------------------------------

class LittleFly : Actor
{
	Default
	{
		+NOBLOCKMAP +NOGRAVITY
		+CANPASS
		
		Speed 6;
		Radius 5;
		Height 5;
		Mass 2;
		ActiveSound "FlyBuzz";
	}
	
	States
	{
	Spawn:
		TNT1 A 20 A_FlySearch;	// [RH] Invisible when not flying
		Loop;
	Buzz:
		AFLY ABCD 3 A_FlyBuzz;
		Loop;
	}
	
	//===========================================================================
	//
	// FindCorpse
	//
	// Finds a corpse to buzz around. We can't use a blockmap check because
	// corpses generally aren't linked into the blockmap.
	//
	//===========================================================================

	private Actor FindCorpse(sector sec, int recurselimit)
	{
		Actor fallback = null;
		sec.validcount = validcount;

		// Search the current sector
		for (Actor check = sec.thinglist; check != null; check = check.snext)
		{
			if (check == self)
				continue;
			if (!check.bCorpse)
				continue;
			if (!CheckSight(check))
				continue;
			fallback = check;
			if (random[Fly](0,1))	// 50% chance to try to pick a different corpse
				continue;
			return check;
		}
		if (--recurselimit <= 0 || (fallback != null && random[Fly](0,1)))
		{
			return fallback;
		}
		// Try neighboring sectors
		for (int i = 0; i < sec.lines.Size(); ++i)
		{
			line ln = sec.lines[i];
			sector sec2 = (ln.frontsector == sec) ? ln.backsector : ln.frontsector;
			if (sec2 != null && sec2.validcount != validcount)
			{
				Actor neighbor = FindCorpse(sec2, recurselimit);
				if (neighbor != null)
				{
					return neighbor;
				}
			}
		}
		return fallback;
	}

	void A_FlySearch()
	{
		// The version from the retail beta is not so great for general use:
		// 1. Pick one of the first fifty thinkers at random.
		// 2. Starting from that thinker, find one that is an actor, not itself,
		//    and within sight. Give up after 100 sequential thinkers.
		// It's effectively useless if there are more than 150 thinkers on a map.
		//
		// So search the sectors instead. We can't potentially find something all
		// the way on the other side of the map and we can't find invisible corpses,
		// but at least we aren't crippled on maps with lots of stuff going on.
		validcount++;
		Actor other = FindCorpse(CurSector, 5);
		if (other != null)
		{
			target = other;
			SetStateLabel("Buzz");
		}
	}

	void A_FlyBuzz()
	{
		Actor targ = target;

		if (targ == null || !targ.bCorpse || random[Fly]() < 5)
		{
			SetIdle();
			return;
		}

		Angle = AngleTo(targ);
		args[0]++;
		if (!TryMove(Pos.XY + AngleToVector(angle, 6), true))
		{
			SetIdle(true);
			return;
		}
		if (args[0] & 2)
		{
			Vel.X += (random[Fly]() - 128) / 512.;
			Vel.Y += (random[Fly]() - 128) / 512.;
		}
		int zrand = random[Fly]();
		if (targ.pos.Z + 5. < pos.z && zrand > 150)
		{
			zrand = -zrand;
		}
		Vel.Z = zrand / 512.;
		if (random[Fly]() < 40)
		{
			A_PlaySound(ActiveSound, CHAN_VOICE, 0.5f, false, ATTN_STATIC);
		}
	}
}
