class DoomBuilderCamera : Actor
{
	States
	{
	Spawn:
		TNT1 A 1;
		Stop;
	}
}


class SecurityCamera : Actor
{
	default
	{
		+NOBLOCKMAP 
		+NOGRAVITY
		+DONTSPLASH
		RenderStyle "None";
		CameraHeight 0;
	}

	double Center;
	double Acc;
	double Delta;
	double Range;
	
	override void PostBeginPlay ()
	{
		Super.PostBeginPlay ();
		Center = Angle;
		if (args[2])
			Delta = 360. / (args[2] * TICRATE / 8);
		else
			Delta = 0.;
		if (args[1])
			Delta /= 2;
		Acc = 0.;
		int arg = (args[0] << 24) >> 24;	// make sure the value has the intended sign.
		Pitch = clamp(arg, -89, 89);
		Range = args[1];
	}

	override void Tick ()
	{
		Acc += Delta;
		if (Range != 0)
			Angle = Center + Range * sin(Acc);
		else if (Delta != 0)
			Angle = Acc;
	}

	
}

class AimingCamera : SecurityCamera
{
	double MaxPitchChange;

	override void PostBeginPlay ()
	{
		int changepitch = args[2];

		args[2] = 0;
		Super.PostBeginPlay ();
		MaxPitchChange = double(changepitch) / TICRATE;
		Range /= TICRATE;

		ActorIterator it = Level.CreateActorIterator(args[3]);
		tracer = it.Next ();
		if (tracer == NULL)
		{
			if (args[3] != 0)
			{
				console.Printf ("AimingCamera %d: Can't find TID %d\n", tid, args[3]);
			}
		}
		else
		{ // Don't try for a new target upon losing this one.
			args[3] = 0;
		}
	}

	override void Tick ()
	{
		if (tracer == NULL && args[3] != 0)
		{ // Recheck, in case something with this TID was created since the last time.
			ActorIterator it = Level.CreateActorIterator(args[3]);
			tracer = it.Next ();
		}
		if (tracer != NULL)
		{
			double dir = deltaangle(angle, AngleTo(tracer));
			double delta = abs(dir);
			if (delta > Range)
			{
				delta = Range;
			}
			if (dir > 0)
			{
				Angle += delta;
			}
			else
			{
				Angle -= delta;
			}
			if (MaxPitchChange != 0)
			{ // Aim camera's pitch; use floats for precision
				Vector2 vect = tracer.Vec2To(self);
				double dz = pos.z - tracer.pos.z - tracer.Height/2;
				double dist = vect.Length();
				double desiredPitch = dist != 0.f ? VectorAngle(dist, dz) : 0.;
				double diff = deltaangle(pitch, desiredPitch);
				if (abs (diff) < MaxPitchChange)
				{
					pitch = desiredPitch;
				}
				else if (diff < 0)
				{
					pitch -= MaxPitchChange;
				}
				else
				{
					pitch += MaxPitchChange;
				}
			}
		}
	}

}

class SpectatorCamera : Actor
{

	bool chasingtracer;
	double lagdistance; // camera gives chase (lazy follow) if tracer gets > lagdistance away from camera.pos
	int chasemode; // 0: chase until tracer centered, 1: same but only when tracer is moving, 2: stop chase if tracer within lagdistance
	property lagdistance : lagdistance;
	property chasingtracer : chasingtracer;
	property chasemode : chasemode;

	default
	{
		+NOBLOCKMAP
		+NOGRAVITY
		+NOINTERACTION
		RenderStyle "None";
		CameraHeight 0;
		SpectatorCamera.chasingtracer false;
		SpectatorCamera.lagdistance 0.0;
		SpectatorCamera.chasemode 0;
	}

	void Init(double dist, double yaw, double inpitch, int inflags)
	{

		double zshift = 0.0;
		if(tracer != NULL)
		{
			if(player != NULL) zshift = -0.25*tracer.height;
			else zshift = 0.5*tracer.height;
		}
		else if (player != NULL && player.mo != NULL) zshift = -0.5*player.mo.height;

		SetViewPos((-dist*Cos(yaw)*Cos(inpitch), -dist*Sin(yaw)*Cos(inpitch), dist*Sin(inpitch)+zshift), inflags);
		LookAtSelf(inpitch);
	}

	void LookAtSelf(double inpitch)
	{
		if(ViewPos.Offset.length() > 0.)
		{
			Vector3 negviewpos = (-1.0) * ViewPos.Offset;
			angle = negviewpos.Angle();
			pitch = inpitch;
		}
	}

	override void Tick()
	{
		if(tracer != NULL)
		{
			Vector3 distvec = tracer.pos - pos;
			double dist = distvec.length();
			if((dist <= 4 && chasingtracer) || lagdistance <= 0.0) // Keep tracer centered on screen
			{
				SetOrigin(tracer.pos, true);
				chasingtracer = false;
			}
			else // Lazy follow tracer
			{
				if(dist >= 2*lagdistance)
				{
					SetOrigin(tracer.pos, true);
					chasingtracer = false;
				}
				else if(dist > lagdistance && !chasingtracer) chasingtracer = true;

				if(chasingtracer)
				{
					speed = tracer.vel.xy.length()/dist;
					if((speed == 0.0) && (chasemode == 0)) speed = tracer.speed/dist;
					SetOrigin(pos + 2*distvec*speed, true);
					if(chasemode > 1) chasingtracer = false;
				}
			}
		}
		else if(player != NULL && player.mo != NULL)
		{
			cameraFOV = player.FOV;
			SetOrigin(player.mo.pos, true);
		}
	}
}

Class OrthographicCamera : Actor
{
	Default
	{
		+NOBLOCKMAP
		+NOINTERACTION
		CameraHeight 0;
		RenderStyle "None";
	}

	override void PostBeginPlay()
	{
		Super.PostBeginPlay();
		UpdateViewPos();
	}
	
	override void Tick()
	{
		if (current != args[0])
			UpdateViewPos();
		
		Super.Tick();
	}

	protected int current;
	protected void UpdateViewPos()
	{
		current = args[0];
		SetViewPos((-abs(max(1.0, double(current))), 0, 0), VPSF_ORTHOGRAPHIC|VPSF_ALLOWOUTOFBOUNDS);
	}
}
