

class IntermissionController native ui
{
    // This is mostly a black box to the native intermission code.
    // May be scriptified later, but right now we do not need it.
	/*
    static native IntermissionController Create(String music, int musicorder, String flat, String text, int textInLump, int finalePic, int lookupText, bool ending, Name endsequence);
    static native IntermissionController CreateNamed(Name nm);
	*/
    native bool Responder(InputEvent ev);
    native bool Ticker();
    native void Drawer();
    native bool NextPage();
}

// Wrappers to play the old intermissions and status screens within a screen job.
class IntermissionScreenJob : ScreenJob
{
    IntermissionController controller;
	
	ScreenJob Init(IntermissionController ctrl)
	{
		controller = ctrl;
		return self;
	}

	override bool OnEvent(InputEvent evt) { return controller.Responder(evt); }
	override void OnTick() { if (!controller.Ticker()) jobstate = finished; }
	override void Draw(double smoothratio) { controller.Drawer(); }

	override void OnDestroy()
	{
        controller.Destroy();
        Super.OnDestroy();
	}
}


class StatusScreenJob : SkippableScreenJob
{
    StatusScreen controller;

    ScreenJob Init(StatusScreen scr)
	{
		controller = scr;
		return self;
	}

	override void OnTick() { controller.Ticker(); if (controller.CurState == StatusScreen.LeavingIntermission) jobstate = finished; }
	override void Draw(double smoothratio) { controller.Drawer(); }
	override void OnSkip() { controller.NextStage(); } // skipping status screens is asynchronous, so yields no result

	override void OnDestroy()
	{
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
			runner.Append(new("StatusScreenJob").Init(status));
		}
		if (inter)
		{
			runner.Append(new("IntermissionScreenJob").Init(inter));
		}
	}
}
