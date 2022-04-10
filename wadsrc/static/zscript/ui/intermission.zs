

class IntermissionController native
{
    // This is mostly a black box to the native intermission code.
    // May be scriptified later, but right now we do not need it.
    static native IntermissionController Creeate(String music, int musicorder, String flat, String text, int textInLump, int finalePic, int lookupText, bool ending, Name endsequence);
    static native IntermissionController CreateNamed(Name nm);
    native bool Responder(InputEvent ev);
    native bool Ticker();
    native void Drawer();
    native bool NextPage();
}

// Wrappers to play the old intermissions and status screens within a screen job.
class IntermissionScreenJob : ScreenJob
{
    IntermissionController controller;

    void Init(String music, int musicorder, String flat, String text, int textInLump, int finalePic, int lookupText, bool ending, Name endsequence)
	{
		controller = IntermissionController.Create(music, musicorder, flat, text, textInLump, finalePic, lookupText, ending, endsequence);
	}

    void InitNamed(Name nm)
    {
        controller = IntermissionController.CreateNamed(nm);
    }

	override bool OnEvent(InputEvent evt) { return controller.Responder(evt); }
	virtual void OnTick() { if (!controller.Ticker()) jobstate = finished; }
	virtual void Draw(double smoothratio) { controller.Drawer(); }
	virtual void OnSkip() { if (!controller.NextPage()) jobstate = finished; }

	override void OnDestroy()
	{
        controller.Destroy();
        Super.OnDestroy();
	}
}


class StatusScreenJob : ScreenJob
{
    StatusScreen controller;

    void Init(StatusScreen scr)
	{
		controller = scr;
	}

	override bool OnEvent(InputEvent evt) { return controller.Responder(evt); }
	virtual void OnTick() { controller.Ticker(); if (controller.CurState == StatusScreen.LeavingIntermission) jobstate = finished; }
	virtual void Draw(double smoothratio) { controller.Drawer(); }
	virtual void OnSkip() { if (!controller.NextStage()) jobstate = finished; }

	override void OnDestroy()
	{
        controller.Destroy();
        Super.OnDestroy();
	}
}