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
