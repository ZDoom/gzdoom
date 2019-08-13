/*
** conversationmenu.txt
** The Strife dialogue display
**
**---------------------------------------------------------------------------
** Copyright 2010-2017 Christoph Oelckers
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

struct StrifeDialogueNode native version("2.4")
{
	native Class<Actor> DropType;
	native int ThisNodeNum;
	native int ItemCheckNode;

	native Class<Actor> SpeakerType;
	native String SpeakerName;
	native Sound SpeakerVoice;
	native String Backdrop;
	native String Dialogue;
	native String Goodbye;

	native StrifeDialogueReply Children;
	native Name MenuClassName;
	native String UserData;
}

// FStrifeDialogueReply holds responses the player can give to the NPC
struct StrifeDialogueReply native version("2.4")
{
	native StrifeDialogueReply Next;
	native Class<Actor> GiveType;
	native int ActionSpecial;
	native int Args[5];
	native int PrintAmount;
	native String Reply;
	native String QuickYes;
	native String QuickNo;
	native String LogString;
	native int NextNode;	// index into StrifeDialogues
	native int LogNumber;
	native bool NeedsGold;
	
	native bool ShouldSkipReply(PlayerInfo player);
}


class ConversationMenu : Menu
{
	String mSpeaker;
	BrokenLines mDialogueLines;
	Array<String> mResponseLines;
	Array<uint> mResponses;
	bool mShowGold;
	bool mHasBackdrop;
	bool mConfineTextToBackdrop;
	StrifeDialogueNode mCurNode;
	int mYpos;
	PlayerInfo mPlayer;
	int mSelection;
	int ConversationPauseTic;
	int LineHeight;
	int ReplyLineHeight;
	Font displayFont;
	int speechDisplayWidth;
	int displayWidth;
	int displayHeight;
	int fontScale;
	int refwidth;
	int refheight;
	double fontfactor;
	
	int SpeechWidth;
	int ReplyWidth;
	
	native static void SendConversationReply(int node, int reply);
	
	const NUM_RANDOM_LINES = 10;
	const NUM_RANDOM_GOODBYES = 3;

	//=============================================================================
	//
	// returns the y position of the replies box for positioning the terminal response.
	//
	//=============================================================================

	virtual int Init(StrifeDialogueNode CurNode, PlayerInfo player, int activereply)
	{
		mCurNode = CurNode;
		mPlayer = player;
		mShowGold = false;
		ConversationPauseTic = gametic + 20;
		DontDim = true;
		
		let tex = TexMan.CheckForTexture (CurNode.Backdrop, TexMan.Type_MiscPatch);
		mHasBackdrop = tex.isValid();
		DontBlur = !mHasBackdrop;
		
		if (!generic_ui && !dlg_vgafont)
		{
			displayFont = SmallFont;
			displayWidth = CleanWidth;
			displayHeight = CleanHeight;
			fontScale = CleanXfac;
			fontFactor = 1;
			refwidth = 320;
			refheight = 200;
			ReplyWidth = 320-50-10;
			SpeechWidth = screen.GetWidth()/CleanXfac - 24*2;
			ReplyLineHeight = LineHeight = displayFont.GetHeight();
			mConfineTextToBackdrop = false;
			speechDisplayWidth = displayWidth;
		}
		else
		{
			displayFont = NewSmallFont;
			fontScale = (CleanXfac+1) / 2;
			fontFactor = double(CleanXfac) / fontScale;
			refwidth = 640;
			refheight = 400;
			ReplyWidth = 640-100-20;
			displayWidth = screen.GetWidth() / fontScale;
			displayHeight = screen.GetHeight() / fontScale;
			let aspect = Screen.GetAspectRatio();
			if (!mHasBackdrop || aspect <= 1.3334)
			{
				SpeechWidth = screen.GetWidth()/fontScale - (24*3 * CleanXfac / fontScale);
				mConfineTextToBackdrop = false;
				speechDisplayWidth = displayWidth;
			}
			else
			{
				speechDisplayWidth = int(Screen.GetHeight() * 1.3333 / fontScale);
				SpeechWidth = speechDisplayWidth - (24*3 * CleanXfac / fontScale);
				mConfineTextToBackdrop = true;
			}
			
			LineHeight = displayFont.GetHeight() + 2;
			ReplyLineHeight = LineHeight * fontScale / CleanYfac;
		}
		

		FormatSpeakerMessage();
		return FormatReplies(activereply);
	}
	
	//=============================================================================
	//
	//
	//
	//=============================================================================
	
	virtual int FormatReplies(int activereply)
	{
		mSelection = -1;

		StrifeDialogueReply reply;
		int r = -1;
		int i = 1,j;
		for (reply = mCurNode.Children; reply != NULL; reply = reply.Next)
		{
			r++;
			if (reply.ShouldSkipReply(mPlayer))
			{
				continue;
			}
			if (activereply == r) mSelection = i - 1;

			mShowGold |= reply.NeedsGold;

			let ReplyText = Stringtable.Localize(reply.Reply);
			if (reply.NeedsGold)
			{
				ReplyText.AppendFormat(" %s", Stringtable.Localize("$TXT_TRADE"));
				let amount = String.Format("%u", reply.PrintAmount);
				ReplyText.Replace("%u", amount);
			}
			let ReplyLines = displayFont.BreakLines (ReplyText, ReplyWidth);

			mResponses.Push(mResponseLines.Size());
			for (j = 0; j < ReplyLines.Count(); ++j)
			{
				mResponseLines.Push(ReplyLines.StringAt(j));
			}
			
			++i;
			ReplyLines.Destroy();
		}
		if (mSelection == -1)
		{
			mSelection = r < activereply ? r + 1 : 0;
		}
		let goodbyestr = mCurNode.Goodbye;
		if (goodbyestr.Length() == 0)
		{
			goodbyestr = String.Format("$TXT_RANDOMGOODBYE_%d", Random[RandomSpeech](1, NUM_RANDOM_GOODBYES));
		}
		else if (goodbyestr.Left(7) == "RANDOM_")
		{
			goodbyestr = String.Format("$TXT_%s_%02d", goodbyestr, Random[RandomSpeech](1, NUM_RANDOM_LINES));
		}
		goodbyestr = Stringtable.Localize(goodbyestr);
		if (goodbyestr.Length() == 0 || goodbyestr.Left(1) == "$") goodbyestr = "Bye.";
		mResponses.Push(mResponseLines.Size());
		mResponseLines.Push(goodbyestr);

		// Determine where the top of the reply list should be positioned.
		mYpos = MIN (140, 192 - mResponseLines.Size() * ReplyLineHeight);
		i = 44 + mResponseLines.Size() * ReplyLineHeight;
		if (mYpos - 100 < i - screen.GetHeight() / CleanYfac / 2)
		{
			mYpos = i - screen.GetHeight() / CleanYfac / 2 + 100;
		}

		if (mSelection >= mResponses.Size())
		{
			mSelection = mResponses.Size() - 1;
		}
		return mYpos;
	}
	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	virtual void FormatSpeakerMessage()
	{
		// Format the speaker's message.
		String toSay = mCurNode.Dialogue;
		if (toSay.Left(7) == "RANDOM_")
		{
			let dlgtext = String.Format("$TXT_%s_%02d", toSay, random[RandomSpeech](1, NUM_RANDOM_LINES));
			toSay = Stringtable.Localize(dlgtext);
			if (toSay.Left(1) == "$") toSay = Stringtable.Localize("$TXT_GOAWAY");
		}
		else
		{
			// handle string table replacement
			toSay = Stringtable.Localize(toSay);
		}
		if (toSay.Length() == 0)
		{
			toSay = ".";
		}
		mDialogueLines = displayFont.BreakLines(toSay, SpeechWidth);
	}
	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void OnDestroy()
	{
		mDialogueLines.Destroy();
		SetMusicVolume (1);
		Super.OnDestroy();
	}

	protected int GetReplyNum()
	{
		// This is needed because mSelection represents the replies currently being displayed which will
		// not match up with what's supposed to be selected if there are any hidden/skipped replies. [FishyClockwork]
		let reply = mCurNode.Children;
		int replynum = mSelection;
		for (int i = 0; i <= mSelection && reply != null; reply = reply.Next)
		{
			if (reply.ShouldSkipReply(mPlayer))
				replynum++;
			else
				i++;
		}
		return replynum;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MenuEvent(int mkey, bool fromcontroller)
	{
		if (demoplayback)
		{ // During demo playback, don't let the user do anything besides close this menu.
			if (mkey == MKEY_Back)
			{
				Close();
				return true;
			}
			return false;
		}
		if (mkey == MKEY_Up)
		{
			if (--mSelection < 0) mSelection = mResponses.Size() - 1;
			return true;
		}
		else if (mkey == MKEY_Down)
		{
			if (++mSelection >= mResponses.Size()) mSelection = 0;
			return true;
		}
		else if (mkey == MKEY_Back)
		{
			SendConversationReply(-1, GetReplyNum());
			Close();
			return true;
		}
		else if (mkey == MKEY_Enter)
		{
			int replynum = GetReplyNum();
			if (mSelection >= mResponses.Size())
			{
				SendConversationReply(-2, replynum);
			}
			else
			{
				// Send dialogue and reply numbers across the wire.
				SendConversationReply(mCurNode.ThisNodeNum, replynum);
			}
			Close();
			return true;
		}
		return false;
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MouseEvent(int type, int x, int y)
	{
		int sel = -1;
		int fh = LineHeight;

		// convert x/y from screen to virtual coordinates, according to CleanX/Yfac use in DrawTexture
		x = ((x - (screen.GetWidth() / 2)) / fontScale) + refWidth/2;
		y = ((y - (screen.GetHeight() / 2)) / fontScale) + refHeight/2;
		
		int ypos = int(mYpos * FontFactor);

		if (x >= 24 && x <= refWidth-24 && y >= ypos && y < ypos + fh * mResponseLines.Size())
		{
			sel = (y - ypos) / fh;
			for(int i = 0; i < mResponses.Size(); i++)
			{
				if (mResponses[i] > sel)
				{
					sel = i-1;
					break;
				}
			}
		}
		if (sel != -1 && sel != mSelection)
		{
			//S_Sound (CHAN_VOICE | CHAN_UI, "menu/cursor", snd_menuvolume, ATTN_NONE);
		}
		mSelection = sel;
		if (type == MOUSE_Release)
		{
			return MenuEvent(MKEY_Enter, true);
		}
		return true;
	}


	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool OnUIEvent(UIEvent ev)
	{
		if (demoplayback)
		{ // No interaction during demo playback
			return false;
		}
		if (ev.type == UIEvent.Type_Char && ev.KeyChar >= 48 && ev.KeyChar <= 57)
		{ // Activate an item of type numberedmore (dialogue only)
			mSelection = ev.KeyChar == 48 ? 9 : ev.KeyChar - 49;
			return MenuEvent(MKEY_Enter, false);
		}
		return Super.OnUIEvent(ev);
	}
	
	//============================================================================
	//
	// Draw the backdrop, returns true if the text background should be dimmed
	//
	//============================================================================

	virtual bool DrawBackdrop()
	{
		let tex = TexMan.CheckForTexture (mCurNode.Backdrop, TexMan.Type_MiscPatch);
		if (tex.isValid())
		{
			screen.DrawTexture(tex, false, 0, 0, DTA_320x200, true);
			return false;
		}
		return true;
	}

	//============================================================================
	//
	// Draw the speaker text
	//
	//============================================================================

	virtual void DrawSpeakerText(bool dimbg)
	{
		String speakerName;
		int linesize = LineHeight * fontScale;
		int cnt = mDialogueLines.Count();

		// Who is talking to you?
		if (mCurNode.SpeakerName.Length() > 0)
		{
			speakerName = Stringtable.Localize(mCurNode.SpeakerName);
		}
		else
		{
			speakerName = players[consoleplayer].ConversationNPC.GetTag("$TXT_PERSON");
		}

		
		// Dim the screen behind the dialogue (but only if there is no backdrop).
		if (dimbg)
		{
			int x = 14 * screen.GetWidth() / 320;
			int y = 13 * screen.GetHeight() / 200;
			int w = 294 * screen.GetWidth() / 320;
			int h = linesize * cnt + 6 * CleanYfac;
			if (speakerName.Length() > 0) h += linesize * 3 / 2;
			screen.Dim(0, 0.45f, x, y, w, h);
		}

		int x = 16 * screen.GetWidth() / 320;
		int y = 16 * screen.GetHeight() / 200;

		if (speakerName.Length() > 0)
		{
			screen.DrawText(displayFont, Font.CR_WHITE, x / fontScale, y / fontScale, speakerName, DTA_KeepRatio, !mConfineTextToBackdrop, DTA_VirtualWidth, speechDisplayWidth, DTA_VirtualHeight, displayHeight);
			y += linesize * 3 / 2;
		}
		x = 24 * screen.GetWidth() / 320;
		for (int i = 0; i < cnt; ++i)
		{
			screen.DrawText(displayFont, Font.CR_UNTRANSLATED, x / fontScale, y / fontScale, mDialogueLines.StringAt(i), DTA_KeepRatio, !mConfineTextToBackdrop, DTA_VirtualWidth, speechDisplayWidth, DTA_VirtualHeight, displayHeight);
			y += linesize;
		}
	}


	//============================================================================
	//
	// Draw the replies
	//
	//============================================================================

	virtual void DrawReplies()
	{
		// Dim the screen behind the PC's choices.
		screen.Dim(0, 0.45, (24 - 160) * CleanXfac + screen.GetWidth() / 2, (mYpos - 2 - 100) * CleanYfac + screen.GetHeight() / 2,
			272 * CleanXfac, MIN(mResponseLines.Size() * ReplyLineHeight + 4, 200 - mYpos) * CleanYfac);


		int y = mYpos;

		int response = 0;
		for (int i = 0; i < mResponseLines.Size(); i++)
		{
			int width = displayFont.StringWidth(mResponseLines[i]);
			int x = 64;

			double sx = (x - 160.0) * CleanXfac + (screen.GetWidth() * 0.5);
			double sy = (y - 100.0) * CleanYfac + (screen.GetHeight() * 0.5);


			screen.DrawText(displayFont, Font.CR_GREEN, sx / fontScale, sy / fontScale, mResponseLines[i], DTA_KeepRatio, true, DTA_VirtualWidth, displayWidth, DTA_VirtualHeight, displayHeight);

			if (i == mResponses[response])
			{
				String tbuf;

				response++;
				tbuf = String.Format("%d.", response);
				x = 50 - displayFont.StringWidth(tbuf);
				sx = (x - 160.0) * CleanXfac + (screen.GetWidth() * 0.5);
				screen.DrawText(displayFont, Font.CR_GREY, sx / fontScale, sy / fontScale, tbuf, DTA_KeepRatio, true, DTA_VirtualWidth, displayWidth, DTA_VirtualHeight, displayHeight);

				if (response == mSelection + 1)
				{
					int colr = ((MenuTime() % 8) < 4) || GetCurrentMenu() != self ? Font.CR_RED : Font.CR_GREY;

					x = (50 + 3 - 160) * CleanXfac + screen.GetWidth() / 2;
					int yy = (y + ReplyLineHeight / 2 - 5 - 100) * CleanYfac + screen.GetHeight() / 2;
					screen.DrawText(ConFont, colr, x, yy, "\xd", DTA_CellX, 8 * CleanXfac, DTA_CellY, 8 * CleanYfac);
				}
			}
			y += ReplyLineHeight;
		}
	}

	virtual void DrawGold()
	{
		if (mShowGold)
		{
			let coin = players[consoleplayer].ConversationPC.FindInventory("Coin");
			let icon = GetDefaultByType("Coin").Icon;
			let goldstr = String.Format("%d", coin != NULL ? coin.Amount : 0);
			screen.DrawText(SmallFont, Font.CR_GRAY, 21, 191, goldstr, DTA_320x200, true, DTA_FillColor, 0, DTA_Alpha, HR_SHADOW);
			screen.DrawTexture(icon, false, 3, 190, DTA_320x200, true, DTA_FillColor, 0, DTA_Alpha, HR_SHADOW);
			screen.DrawText(SmallFont, Font.CR_GRAY, 20, 190, goldstr, DTA_320x200, true);
			screen.DrawTexture(icon, false, 2, 189, DTA_320x200, true);
		}

	}

	//============================================================================
	//
	// DrawConversationMenu
	//
	//============================================================================

	override void Drawer()
	{
		if (mCurNode == NULL)
		{
			Close ();
			return;
		}

		bool dimbg = DrawBackdrop();
		DrawSpeakerText(dimbg);
		DrawReplies();
		DrawGold();
	}
	
	
	//============================================================================
	//
	//
	//
	//============================================================================

	override void Ticker()
	{
		// [CW] Freeze the game depending on MAPINFO options.
		if (ConversationPauseTic < gametic && !multiplayer && !Level.no_dlg_freeze)
		{
			menuactive = Menu.On;
		}
	}
	
}
