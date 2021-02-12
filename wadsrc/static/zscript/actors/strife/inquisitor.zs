
// Inquisitor ---------------------------------------------------------------

class Inquisitor : Actor
{
	Default
	{
		Health 1000;
		Speed 12;
		Radius 40;
		Height 110;
		Mass 0x7fffffff;
		Monster;
		+DROPOFF
		+NOBLOOD
		+BOSS
		+FLOORCLIP
		+DONTMORPH
		+NORADIUSDMG
		MaxDropOffHeight 32;
		MinMissileChance 150;
		SeeSound "inquisitor/sight";
		DeathSound "inquisitor/death";
		ActiveSound "inquisitor/active";
		Obituary "$OB_INQUISITOR";
	}

	States
	{
	Spawn:
		ROB3 AB 10 A_Look;
		Loop;
	See:
		ROB3 B 3 A_InquisitorWalk;
		ROB3 B 3 A_Chase;
		ROB3 CCDD 4 A_Chase;
		ROB3 E 3 A_InquisitorWalk;
		ROB3 E 3 A_InquisitorDecide;
		Loop;
	Missile:
		ROB3 A 2 A_InquisitorDecide;
		ROB3 F 6 A_FaceTarget;
		ROB3 G 8 Bright A_ReaverRanged;
		ROB3 G 8 A_ReaverRanged;
		Goto See;
	Grenade:
		ROB3 K 12 A_FaceTarget;
		ROB3 J 6 Bright A_InquisitorAttack;
		ROB3 K 12;
		Goto See;
	Jump:
		ROB3 H 8 Bright A_InquisitorJump;
		ROB3 I 4 Bright A_InquisitorCheckLand;
		ROB3 H 4 Bright A_InquisitorCheckLand;
		Goto Jump+1;
	Death:
		ROB3 L 0 A_StopSound(CHAN_ITEM);
		ROB3 L 4 A_TossGib;
		ROB3 M 4 A_Scream;
		ROB3 N 4 A_TossGib;
		ROB3 O 4 Bright A_Explode(128, 128, alert:true);
		ROB3 P 4 Bright A_TossGib;
		ROB3 Q 4 Bright A_NoBlocking;
		ROB3 RSTUV 4 A_TossGib;
		ROB3 W 4 Bright A_Explode(128, 128, alert:true);
		ROB3 XY 4 Bright A_TossGib;
		ROB3 Z 4 A_TossGib;
		ROB3 [ 4 A_TossGib;
		ROB3 \ 3 A_TossGib;
		ROB3 ] 3 Bright A_Explode(128, 128, alert:true);
		RBB3 A 3 Bright A_TossArm;
		RBB3 B 3 Bright A_TossGib;
		RBB3 CD 3 A_TossGib;
		RBB3 E -1;
		Stop;
	}
	
	
	// Inquisitor ---------------------------------------------------------------

	void A_InquisitorWalk ()
	{
		A_StartSound ("inquisitor/walk", CHAN_BODY);
		A_Chase ();
	}

	private bool InquisitorCheckDistance ()
	{
		if (reactiontime == 0 && CheckSight (target))
		{
			return Distance2D (target) < 264.;
		}
		return false;
	}

	void A_InquisitorDecide ()
	{
		if (target == null)
			return;

		A_FaceTarget ();
		if (!InquisitorCheckDistance ())
		{
			SetStateLabel("Grenade");
		}
		if (target.pos.z != pos.z)
		{
			if (pos.z + height + 54 < ceilingz)
			{
				SetStateLabel("Jump");
			}
		}
	}

	void A_InquisitorAttack ()
	{
		if (target == null)
			return;

		A_FaceTarget ();

		AddZ(32);
		angle -= 45./32;
		Actor proj = SpawnMissileZAimed (pos.z, target, "InquisitorShot");
		if (proj != null)
		{
			proj.Vel.Z += 9;
		}
		angle += 45./16;
		proj = SpawnMissileZAimed (pos.z, target, "InquisitorShot");
		if (proj != null)
		{
			proj.Vel.Z += 16;
		}
		AddZ(-32);
	}

	void A_InquisitorJump ()
	{
		if (target == null)
			return;

		A_StartSound ("inquisitor/jump", CHAN_ITEM, CHANF_LOOPING);
		AddZ(64);
		A_FaceTarget ();
		let localspeed = Speed * (2./3);
		VelFromAngle(localspeed);
		double dist = DistanceBySpeed(target, localspeed);
		Vel.Z = (target.pos.z - pos.z) / dist;
		reactiontime = 60;
		bNoGravity = true;
	}

	void A_InquisitorCheckLand ()
	{
		reactiontime--;
		if (reactiontime < 0 ||
			Vel.X == 0 ||
			Vel.Y == 0 ||
			pos.z <= floorz)
		{
			SetState (SeeState);
			reactiontime = 0;
			bNoGravity = false;
			A_StopSound (CHAN_ITEM);
			return;
		}
		A_StartSound ("inquisitor/jump", CHAN_ITEM, CHANF_LOOPING);
	}

	void A_TossArm ()
	{
		Actor foo = Spawn("InquisitorArm", Pos + (0,0,24), ALLOW_REPLACE);
		if (foo != null)
		{
			foo.angle = angle - 90. + Random2[Inquisitor]() * (360./1024.);
			foo.VelFromAngle(foo.Speed / 8);
			foo.Vel.Z = random[Inquisitor]() / 64.;
		}
	}

	
}

// Inquisitor Shot ----------------------------------------------------------

class InquisitorShot : Actor
{
	Default
	{
		ReactionTime 15;
		Speed 25;
		Radius 13;
		Height 13;
		Mass 15;
		Projectile;
		-ACTIVATEIMPACT
		-ACTIVATEPCROSS
		-NOGRAVITY
		+STRIFEDAMAGE
		MaxStepHeight 4;
		SeeSound "inquisitor/attack";
		DeathSound "inquisitor/atkexplode";
	}
	States
	{
	Spawn:
		UBAM AB 3 A_Countdown;
		Loop;
	Death:
		BNG2 A 0 Bright A_SetRenderStyle(1, STYLE_Normal);
		BNG2 A 4 Bright A_Explode(192, 192, alert:true);
		BNG2 B 4 Bright;
		BNG2 C 4 Bright;
		BNG2 D 4 Bright;
		BNG2 E 4 Bright;
		BNG2 F 4 Bright;
		BNG2 G 4 Bright;
		BNG2 H 4 Bright;
		BNG2 I 4 Bright;
		Stop;
	}

}


// The Dead Inquisitor's Detached Arm ---------------------------------------

class InquisitorArm : Actor
{
	Default
	{
		Speed 25;
		+NOBLOCKMAP
		+NOCLIP
		+NOBLOOD
	}
	States
	{
	Spawn:
		RBB3 FG 5 Bright;
		RBB3 H -1;
		Stop;
	}
}



