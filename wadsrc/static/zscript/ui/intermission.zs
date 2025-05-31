
extend class ScreenJobRunner
{
	protected native static void ReadyPlayer();
	protected native static void ResetReadyTimer();

	native static int GetReadyTimer();
	native static bool IsPlayerReady(int pNum);

	bool ConsumedInput(InputEvent evt)
	{
		if (netgame && evt.type == InputEvent.Type_KeyDown
			&& (evt.KeyScan == InputEvent.Key_Space || evt.KeyScan == InputEvent.Key_Mouse1
				|| evt.KeyScan == InputEvent.Key_Pad_A || evt.KeyScan == InputEvent.Key_Joy1))
		{
			ReadyPlayer();
			return true;
		}

		return false;
	}

	void DrawReadiedPlayers(double smoothratio)
	{
		if (!netgame || GetSkipType() == ST_UNSKIPPABLE)
			return;
		
		if (net_cutscenereadytype == 0)
		{
			int totalClients, readyClients;
			for (int i; i < MAXPLAYERS; ++i)
			{
				if (!playerInGame[i] || players[i].Bot)
					continue;

				++totalClients;
				readyClients += IsPlayerReady(i);
			}

			if (totalClients > 1)
			{
				TextureID readyico = TexMan.CheckForTexture("READYICO", TexMan.Type_MiscPatch);
				Vector2 readysize = TexMan.GetScaledSize(readyico);
				
				if (IsPlayerReady(consoleplayer))
					Screen.DrawTexture(readyico, true, 0, 0, DTA_CleanNoMove, true, DTA_TopLeft, true);

				Screen.DrawText(ConFont, Font.CR_UNTRANSLATED, (int(readysize.X) + 4) * CleanXFac, CleanYFac, String.Format("%d/%d", readyClients, totalClients), DTA_CleanNoMove, true);
				int startTimer = GetReadyTimer();
				if (startTimer > 0)
				{
					int col = Font.CR_UNTRANSLATED;
					if (startTimer <= GameTicRate * 5)
						col = Font.CR_RED;

					Screen.DrawText(ConFont, col, 0, int(readysize.Y) * CleanYFac + CleanYFac, SystemTime.Format("%M:%S", int(ceil(double(startTimer) / GameTicRate))), DTA_CleanNoMove, true);
				}
			}
		}

		if (net_cutscenereadytype != 2 || consoleplayer == Net_Arbitrator)
		{
			string contType;
			switch (GetLastInputType())
			{
				case INP_KEYBOARD_MOUSE:
					contType = "$NET_CONTINUE_MKB";
					break;
				case INP_CONTROLLER:
					contType = "$NET_CONTINUE_CONTROLLER";
					break;
				case INP_JOYSTICK:
					contType = "$NET_CONTINUE_JOYSTICK";
					break;
			}

			string contTxt = StringTable.Localize(contType);
			int xOfs = (Screen.GetWidth() - ConFont.StringWidth(contTxt) * CleanXFac) / 2;
			int yOfs = Screen.GetHeight() - ConFont.GetHeight() * CleanYFac - CleanYFac;
			Screen.DrawText(ConFont, Font.CR_GREEN, xOfs, yOfs, contTxt, DTA_CleanNoMove, true);
		}
	}
}

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
