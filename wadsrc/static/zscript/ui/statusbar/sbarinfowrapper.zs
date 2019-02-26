
struct SBarInfo native ui
{
	native void Destroy();
	native void AttachToPlayer(PlayerInfo player);
	native void Draw(int state);
	native void NewGame();
	native bool MustDrawLog(int state);
	native void Tick();
	native void ShowPop(int popnum);
	native int GetProtrusion(double scalefac);
}


// The sole purpose of this wrapper is to eliminate the native dependencies of the status bar object
// because those would seriously impede the script conversion of the base class.

class SBarInfoWrapper : BaseStatusBar
{
	private clearscope SBarInfo core;

	override void OnDestroy()
	{
		if (core != null) core.Destroy();	// note that the core is not a GC'd object!
		Super.OnDestroy();
	}

	override void AttachToPlayer(PlayerInfo player)
	{
		Super.AttachToPlayer(player);
		core.AttachToPlayer(player);
	}

	override void Draw(int state, double TicFrac)
	{
		Super.Draw(state, TicFrac);
		core.Draw(state);
	}

	override void NewGame()
	{
		Super.NewGame();
		if (CPlayer != NULL)
		{
			core.NewGame();
		}
	}

	override bool MustDrawLog(int state)
	{
		return core.MustDrawLog(state);
	}

	override void Tick()
	{
		Super.Tick();
		core.Tick();
	}

	override void ShowPop(int popnum)
	{
		Super.ShowPop(popnum);
		core.ShowPop(popnum);
	}

	override int GetProtrusion(double scaleratio) const
	{
		return core.GetProtrusion(scaleratio);
	}


}