
// Random spawner ----------------------------------------------------------

class RandomSpawner : Actor
{
	
	const MAX_RANDOMSPAWNERS_RECURSION = 32; // Should be largely more than enough, honestly.
	
	Default
	{
		+NOBLOCKMAP
		+NOSECTOR
		+NOGRAVITY
		+THRUACTORS
	}
	
	virtual void PostSpawn(Actor spawned)
	{}
	
	static bool IsMonster(DropItem di)
	{
		class<Actor> pclass = di.Name;
		if (null == pclass)
		{
			return false;
		}

		return GetDefaultByType(pclass).bIsMonster;
	}

	// Override this to decide what to spawn in some other way.
	// Return the class name, or 'None' to spawn nothing, or 'Unknown' to spawn an error marker.
	virtual Name ChooseSpawn()
	{
		DropItem di;   // di will be our drop item list iterator
		DropItem drop; // while drop stays as the reference point.
		int n = 0;
		bool nomonsters = sv_nomonsters || level.nomonsters;

		drop = di = GetDropItems();
		if (di != null)
		{
			while (di != null)
			{
				bool shouldSkip = (di.Name == 'None') || (nomonsters && IsMonster(di));
				
				if (!shouldSkip)
				{
					int amt = di.Amount;
					if (amt < 0) amt = 1; // default value is -1, we need a positive value.
					n += amt; // this is how we can weight the list.
				}

				di = di.Next;
			}
			if (n == 0)
			{ // Nothing left to spawn. They must have all been monsters, and monsters are disabled.
				return 'None';
			}
			// Then we reset the iterator to the start position...
			di = drop;
			// Take a random number...
			n = random[randomspawn](0, n-1);
			// And iterate in the array up to the random number chosen.
			while (n > -1 && di != null)
			{
				if (di.Name != 'None' &&
					(!nomonsters || !IsMonster(di)))
				{
					int amt = di.Amount;
					if (amt < 0) amt = 1;
					n -= amt;
					if ((di.Next != null) && (n > -1))
						di = di.Next;
					else
						n = -1;
				}
				else
				{
					di = di.Next;
				}
			}
			if (di == null)
			{
				return 'Unknown';
			}
			else if (random[randomspawn]() <= di.Probability)	// prob 255 = always spawn, prob 0 = almost never spawn.
			{
				return di.Name;
			}
			else
			{
				return 'None';
			}
		}
		else
		{
			return 'None';
		}
	}
	
	// To handle "RandomSpawning" missiles, the code has to be split in two parts.
	// If the following code is not done in BeginPlay, missiles will use the
	// random spawner's velocity (0...) instead of their own.
	override void BeginPlay()
	{
		Super.BeginPlay();
		let s = ChooseSpawn();

		if (s == 'Unknown') // Spawn error markers immediately.
		{
			Spawn(s, Pos, NO_REPLACE);
			Destroy();
		}
		else if (s == 'None') // ChooseSpawn chose to spawn nothing.
		{
			Destroy();
		}
		else
		{
			// So now we can spawn the dropped item.
			// Handle replacement here so as to get the proper speed and flags for missiles
			Class<Actor> cls = s;
			if (cls != null)
			{
				Class<Actor> rep = GetReplacement(cls);
				if (rep != null)
				{
					cls = rep;
				}
			}
			if (cls != null)
			{
				Species = Name(cls);
				readonly<Actor> defmobj = GetDefaultByType(cls);
				Speed = defmobj.Speed;
				bMissile |= defmobj.bMissile;
				bSeekerMissile |= defmobj.bSeekerMissile;
				bSpectral |= defmobj.bSpectral;
			}
			else
			{
				A_Log(TEXTCOLOR_RED .. "Unknown item class ".. s .." to drop from a random spawner\n");
				Species = 'None';
			}
		}
	}

	// The second half of random spawning. Now that the spawner is initialized, the
	// real actor can be created. If the following code were in BeginPlay instead,
	// missiles would not have yet obtained certain information that is absolutely
	// necessary to them -- such as their source and destination.
	override void PostBeginPlay()
	{
		Super.PostBeginPlay();

		if (bouncecount >= MAX_RANDOMSPAWNERS_RECURSION)	// Prevents infinite recursions
		{
			Spawn("Unknown", Pos, NO_REPLACE);		// Show that there's a problem.
			Destroy();
			return;
		}

		Actor newmobj = null;
		bool boss = false;

		if (Species == 'None')
		{
			Destroy();
			return;
		}
		Class<Actor> cls = Species;
		if (bMissile && target && target.target) // Attempting to spawn a missile.
		{
			if ((tracer == null) && bSeekerMissile)
			{
				tracer = target.target;
			}
			newmobj = target.SpawnMissileXYZ(Pos, target.target, cls, false);
		}
		else 
		{		
			newmobj = Spawn(cls, Pos, NO_REPLACE);
		}
		if (newmobj != null)
		{
			// copy everything relevant
			newmobj.SpawnAngle = SpawnAngle;
			newmobj.Angle		= Angle;
			newmobj.Pitch		= Pitch;
			newmobj.Roll		= Roll;
			newmobj.SpawnPoint = SpawnPoint;
			newmobj.special    = special;
			newmobj.args[0]    = args[0];
			newmobj.args[1]    = args[1];
			newmobj.args[2]    = args[2];
			newmobj.args[3]    = args[3];
			newmobj.args[4]    = args[4];
			newmobj.special1   = special1;
			newmobj.special2   = special2;
			newmobj.SpawnFlags = SpawnFlags & ~MTF_SECRET;	// MTF_SECRET needs special treatment to avoid incrementing the secret counter twice. It had already been processed for the spawner itself.
			newmobj.HandleSpawnFlags();
			newmobj.SpawnFlags = SpawnFlags;
			newmobj.bCountSecret = SpawnFlags & MTF_SECRET;	// "Transfer" count secret flag to spawned actor
			newmobj.ChangeTid(tid);
			newmobj.Vel	= Vel;
			newmobj.master = master;	// For things such as DamageMaster/DamageChildren, transfer mastery.
			newmobj.target = target;
			newmobj.tracer = tracer;
			newmobj.CopyFriendliness(self, false);
			// This handles things such as projectiles with the MF4_SPECTRAL flag that have
			// a health set to -2 after spawning, for internal reasons.
			if (health != SpawnHealth()) newmobj.health = health;
			if (!bDropped) newmobj.bDropped = false;
			// Handle special altitude flags
			if (newmobj.bSpawnCeiling)
			{
				newmobj.SetZ(newmobj.ceilingz - newmobj.Height - SpawnPoint.Z);
			}
			else if (newmobj.bSpawnFloat) 
			{
				double space = newmobj.ceilingz - newmobj.Height - newmobj.floorz;
				if (space > 48)
				{
					space -= 40;
					newmobj.SetZ((space * random[randomspawn]()) / 256. + newmobj.floorz + 40);
				}
				newmobj.AddZ(SpawnPoint.Z);
			}
			if (newmobj.bMissile && !(newmobj is 'RandomSpawner'))
				newmobj.CheckMissileSpawn(0);
			// Bouncecount is used to count how many recursions we're in.
			if (newmobj is 'RandomSpawner')
				newmobj.bouncecount = ++bouncecount;
			// If the spawned actor has either of those flags, it's a boss.
			if (newmobj.bBossDeath || newmobj.bBoss)
				boss = true;
			// If a replaced actor has either of those same flags, it's also a boss.
			readonly<Actor> rep = GetDefaultByType(GetReplacee(GetClass()));
			if (rep && (rep.bBossDeath || rep.bBoss))
				boss = true;

			PostSpawn(newmobj);
		}
		if (boss)
			tracer = newmobj;
		else	// "else" because a boss-replacing spawner must wait until it can call A_BossDeath.
			Destroy();
	}

	override void Tick()	// This function is needed for handling boss replacers
	{
		Super.Tick();
		if (tracer == null || tracer.health <= 0)
		{
			A_BossDeath();
			Destroy();
		}
	}

}
