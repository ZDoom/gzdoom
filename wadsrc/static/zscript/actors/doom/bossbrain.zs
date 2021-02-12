
//===========================================================================
//
// Boss Brain
//
//===========================================================================

class BossBrain : Actor
{
	Default
	{
		Health 250;
		Mass 10000000;
		PainChance 255;
		+SOLID +SHOOTABLE
		+NOICEDEATH
		+OLDRADIUSDMG
		PainSound "brain/pain";
		DeathSound "brain/death";
	}
	States
	{
	Spawn:
		BBRN A -1;
		Stop;
	Pain:
		BBRN B 36 A_BrainPain;
		Goto Spawn;
	Death:
		BBRN A 100 A_BrainScream;
		BBRN AA 10;
		BBRN A -1 A_BrainDie;
		Stop;
	}
}


//===========================================================================
//
// Boss Eye
//
//===========================================================================

class BossEye : Actor
{
	Default
	{
		Height 32;
		+NOBLOCKMAP
		+NOSECTOR
	}
	States
	{
	Spawn:
		SSWV A 10 A_Look;
		Loop;
	See:
		SSWV A 181 A_BrainAwake;
		SSWV A 150 A_BrainSpit;
		Wait;
	}
}

//===========================================================================
//
// Boss Target
//
//===========================================================================

class BossTarget : SpecialSpot
{
	Default
	{
		Height 32;
		+NOBLOCKMAP;
		+NOSECTOR;
	}
}

//===========================================================================
//
// Spawn shot
//
//===========================================================================

class SpawnShot : Actor
{
	Default
	{
		Radius 6;
		Height 32;
		Speed 10;
		Damage 3;
		Projectile;
		+NOCLIP
		-ACTIVATEPCROSS
		+RANDOMIZE
		SeeSound "brain/spit";
		DeathSound "brain/cubeboom";
	}
	States
	{
	Spawn:
		BOSF A 3 BRIGHT A_SpawnSound;
		BOSF BCD 3 BRIGHT A_SpawnFly;
		Loop;
	}
}
		
//===========================================================================
//
// Spawn fire
//
//===========================================================================

class SpawnFire : Actor
{
	Default
	{
		Height 78;
		+NOBLOCKMAP
		+NOGRAVITY
		+ZDOOMTRANS
		RenderStyle "Add";
	}
	States
	{
	Spawn:
		FIRE ABCDEFGH 4 Bright A_Fire;
		Stop;
	}
}


//===========================================================================
//
// Code (must be attached to Actor)
//
//===========================================================================

extend class Actor
{

	void A_BrainAwake()
	{
		A_StartSound("brain/sight", CHAN_VOICE, CHANF_DEFAULT, 1, ATTN_NONE);
	}
	
	void A_BrainPain()
	{
		A_StartSound("brain/pain", CHAN_VOICE, CHANF_DEFAULT, 1, ATTN_NONE);
	}

	private static void BrainishExplosion(vector3 pos)
	{
		Actor boom = Actor.Spawn("Rocket", pos, NO_REPLACE);
		if (boom)
		{
			boom.DeathSound = "misc/brainexplode";
			boom.Vel.z = random[BrainScream](0, 255)/128.;

			boom.SetStateLabel ("Brainexplode");
			boom.bRocketTrail = false;
			boom.SetDamage(0);	// disables collision detection which is not wanted here
			boom.tics -= random[BrainScream](0, 7);
			if (boom.tics < 1) boom.tics = 1;
		}
	}
	
	void A_BrainScream()
	{
		for (double x = -196; x < +320; x += 8)
		{
			// (1 / 512.) is actually what the original value of 128 did, even though it probably meant 128 map units.
			BrainishExplosion(Vec2OffsetZ(x, -320, (1 / 512.) + random[BrainExplode](0, 255) * 2));
		}
		A_StartSound("brain/death", CHAN_VOICE, CHANF_DEFAULT, 1., ATTN_NONE);
	}
	
	void A_BrainExplode()
	{
		double x = random2[BrainExplode]() / 32.;
		Vector3 pos = Vec2OffsetZ(x, 0, 1 / 512. + random[BrainExplode]() * 2);
		BrainishExplosion(pos);
	}

	void A_BrainDie()
	{
		// [RH] If noexit, then don't end the level.
		if ((deathmatch || alwaysapplydmflags) && sv_noexit)
			return;

		// New dmflag: Kill all boss spawned monsters before ending the level.
		if (sv_killbossmonst)
		{
			int count;	// Repeat until we have no more boss-spawned monsters.
			ThinkerIterator it = ThinkerIterator.Create("Actor");
			do			// (e.g. Pain Elementals can spawn more to kill upon death.)
			{
				Actor mo;
				it.Reinit();
				count = 0;
				while (mo = Actor(it.Next()))
				{
					if (mo.health > 0 && mo.bBossSpawned)
					{
						mo.DamageMobj(self, self, mo.health, "None", DMG_NO_ARMOR|DMG_FORCED|DMG_THRUSTLESS|DMG_NO_FACTOR);

						// [Blue Shadow] If 'mo' is a RandomSpawner or another actor which can't be killed,
						// it could cause this code to loop indefinitely. So only let it trigger a loop if it
						// has been actually killed.
						if (mo.bKilled) count++;
					}
				}
			} while (count != 0);
		}
		Level.ExitLevel(0, false);
	}

	void A_BrainSpit(class<Actor> spawntype = null)
	{
		SpotState spstate = Level.GetSpotState();
		Actor targ;
		Actor spit;
		bool isdefault = false;

		// shoot a cube at current target
		targ = spstate.GetNextInList("BossTarget", G_SkillPropertyInt(SKILLP_EasyBossBrain));

		if (targ)
		{
			if (spawntype == null) 
			{
				spawntype = "SpawnShot";
				isdefault = true;
			}

			// spawn brain missile
			spit = SpawnMissile (targ, spawntype);

			if (spit)
			{
				// Boss cubes should move freely to their destination so it's
				// probably best to disable all collision detection for them.
				spit.bNoInteraction = spit.bNoClip;
		
				spit.target = targ;
				spit.master = self;
				// [RH] Do this correctly for any trajectory. Doom would divide by 0
				// if the target had the same y coordinate as the spitter.
				if (spit.Vel.xy == (0, 0))
				{
					spit.special2 = 0;
				}
				else if (abs(spit.Vel.y) > abs(spit.Vel.x))
				{
					spit.special2 = int((targ.pos.y - pos.y) / spit.Vel.y);
				}
				else
				{
					spit.special2 = int((targ.pos.x - pos.x) / spit.Vel.x);
				}
				// [GZ] Calculates when the projectile will have reached destination
				spit.special2 += level.maptime;
				spit.bBossCube = true;
			}

			if (!isdefault)
			{
				A_StartSound(self.AttackSound, CHAN_WEAPON, CHANF_DEFAULT, 1., ATTN_NONE);
			}
			else
			{
				// compatibility fallback
				A_StartSound("brain/spit", CHAN_WEAPON, CHANF_DEFAULT, 1., ATTN_NONE);
			}
		}
	}
	
	private void SpawnFly(class<Actor> spawntype, sound snd)
	{
		Actor newmobj;
		Actor fog;
		Actor eye = master; // The eye is the spawnshot's master, not the target!
		Actor targ = target; // Unlike other projectiles, the target is the intended destination.
		int r;

		if (targ == null)
		{
			Destroy();
			return;
		}
			
		// [GZ] Should be more viable than a countdown...
		if (special2 != 0)
		{
			if (special2 > level.maptime)
				return;		// still flying
		}
		else
		{
			if (reactiontime == 0 || --reactiontime != 0)
				return;		// still flying
		}
		
		if (spawntype)
		{
			fog = Spawn (spawntype, targ.pos, ALLOW_REPLACE);
			if (fog) A_StartSound(snd, CHAN_BODY);
		}

		class<Actor> SpawnName = null;

		DropItem di;   // di will be our drop item list iterator
		DropItem drop; // while drop stays as the reference point.
		int n = 0;

		// First see if this cube has its own actor list
		drop = GetDropItems();

		// If not, then default back to its master's list
		if (drop == null && eye != null)
			drop = eye.GetDropItems();

		if (drop != null)
		{
			for (di = drop; di != null; di = di.Next)
			{
				if (di.Name != 'None')
				{
					int amt = di.Amount;
					if (amt < 0)
					{
						amt = 1; // default value is -1, we need a positive value.
					}
					n += amt; // this is how we can weight the list.
				}
			}
			di = drop;
			n = random[pr_spawnfly](0, n);
			while (n >= 0)
			{
				if (di.Name != 'none')
				{
					int amt = di.Amount;
					if (amt < 0)
					{
						amt = 1;
					}
					n -= amt;
				}
				if ((di.Next != null) && (n >= 0))
				{
					di = di.Next;
				}
				else
				{
					n = -1;
				}
			}
			SpawnName = di.Name;
		}
		if (SpawnName == null)
		{
			// Randomly select monster to spawn.
			r = random[pr_spawnfly](0, 255);

			// Probability distribution (kind of :),
			// decreasing likelihood.
				 if (r < 50)  SpawnName = "DoomImp";
			else if (r < 90)  SpawnName = "Demon";
			else if (r < 120) SpawnName = "Spectre";
			else if (r < 130) SpawnName = "PainElemental";
			else if (r < 160) SpawnName = "Cacodemon";
			else if (r < 162) SpawnName = "Archvile";
			else if (r < 172) SpawnName = "Revenant";
			else if (r < 192) SpawnName = "Arachnotron";
			else if (r < 222) SpawnName = "Fatso";
			else if (r < 246) SpawnName = "HellKnight";
			else			  SpawnName = "BaronOfHell";
		}
		if (spawnname != null)
		{
			newmobj = Spawn (spawnname, targ.pos, ALLOW_REPLACE);
			if (newmobj != null)
			{
				// Make the new monster hate what the boss eye hates
				if (eye != null)
				{
					newmobj.CopyFriendliness (eye, false);
				}
				// Make it act as if it was around when the player first made noise
				// (if the player has made noise).
				newmobj.LastHeard = newmobj.CurSector.SoundTarget;

				if (newmobj.SeeState != null && newmobj.LookForPlayers (true))
				{
					newmobj.SetState (newmobj.SeeState);
				}
				if (!newmobj.bDestroyed)
				{
					// telefrag anything in this spot
					newmobj.TeleportMove (newmobj.pos, true);
				}
				newmobj.bBossSpawned = true;
			}
		}

		// remove self (i.e., cube).
		Destroy ();
	}

	void A_SpawnFly(class<Actor> spawntype = null)
	{
		sound snd; 
		if (spawntype != null) 
		{
			snd = GetDefaultByType(spawntype).SeeSound;
		}
		else
		{
			spawntype = "SpawnFire";
			snd = "brain/spawn";
		}
		SpawnFly(spawntype, snd);
	}

		void A_SpawnSound()
	{
		// travelling cube sound
		A_StartSound("brain/cube", CHAN_BODY);
		SpawnFly("SpawnFire", "brain/spawn");
	}
}
