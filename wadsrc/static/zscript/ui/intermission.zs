

class IntermissionController native ui
{
    // This is mostly a black box to the native intermission code.
    // May be scriptified later, but right now we do not need it.

    native void Start();
	native bool Responder(InputEvent ev);
    native bool Ticker();
    native void Drawer();
    native bool NextPage();
}

// Wrapper to play the native intermissions within a screen job.
class IntermissionScreenJob : ScreenJob
{
    IntermissionController controller;
	
	ScreenJob Init(IntermissionController ctrl, bool allowwipe)
	{
		Super.Init();
		if (allowwipe && wipetype != 0) flags = wipetype << ScreenJob.transition_shift;
		controller = ctrl;
		return self;
	}

	override void Start() { controller.Start(); }
	override bool OnEvent(InputEvent evt) { return controller.Responder(evt); }
	override void OnTick() { if (!controller.Ticker()) jobstate = finished; }
	override void Draw(double smoothratio) { controller.Drawer(); }

	override void OnDestroy()
	{
        if (controller)
            controller.Destroy();
        Super.OnDestroy();
	}
}


class DoomCutscenes ui
{
	//---------------------------------------------------------------------------
	//
	//
	//
	//---------------------------------------------------------------------------

	static void BuildMapTransition(ScreenJobRunner runner, IntermissionController inter, StatusScreen status)
	{
		if (status)
		{
			runner.Append(status);
		}
		if (inter)
		{
			runner.Append(new("IntermissionScreenJob").Init(inter, status != null));
		}
	}
}
