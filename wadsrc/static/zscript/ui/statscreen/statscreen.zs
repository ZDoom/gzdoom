// Note that the status screen needs to run in 'play' scope!

class InterBackground native ui version("2.5")
{
	native static InterBackground Create(wbstartstruct wbst);
	native virtual bool LoadBackground(bool isenterpic);
	native virtual void updateAnimatedBack();
	native virtual void drawBackground(int CurState, bool drawsplat, bool snl_pointeron);
	native virtual bool IsUsingMusic();
}

// This is obsolete. Hopefully this was never used...
struct PatchInfo ui version("2.5")
{
	Font mFont;
	deprecated("3.8") TextureID mPatch;
	int mColor;

	void Init(GIFont gifont)
	{
		// Replace with the VGA-Unicode font if needed.
		// The default settings for this are marked with a *.
		// If some mod changes this it is assumed that it doesn't provide any localization for the map name in a language not supported by the font.
		String s = gifont.fontname;
		if (s.Left(1) != "*")
			mFont = Font.GetFont(gifont.fontname);
		else if (generic_ui)
			mFont = NewSmallFont;
		else
		{
			s = s.Mid(1);
			mFont = Font.GetFont(s);
		}
		mColor = Font.FindFontColor(gifont.color);
		if (mFont == NULL)
		{
			mFont = BigFont;
		}
	}
};


class StatusScreen : ScreenJob abstract version("2.5")
{
	enum EValues
	{
		// GLOBAL LOCATIONS
		TITLEY = 5,

		// SINGPLE-PLAYER STUFF
		SP_STATSX = 50,
		SP_STATSY = 50,

		SP_TIMEX = 8,
		SP_TIMEY = (200 - 32),

		// NET GAME STUFF
		NG_STATSY = 50,
	};

	enum EState
	{
		NoState = -1,
		StatCount,
		ShowNextLoc,
		LeavingIntermission
	};

	// States for single-player
	enum ESPState
	{
		SP_KILLS = 0,
		SP_ITEMS = 2,
		SP_SECRET = 4,
		SP_FRAGS = 6,
		SP_TIME = 8,
	};

	const SHOWNEXTLOCDELAY = 4;			// in seconds

	InterBackground bg;
	int				acceleratestage;	// used to accelerate or skip a stage
	bool				playerready[MAXPLAYERS];
	int				me;					// wbs.pnum
	int				bcnt;
	int				CurState;				// specifies current CurState
	wbstartstruct	wbs;				// contains information passed into intermission
	wbplayerstruct	Plrs[MAXPLAYERS];				// wbs.plyr[]
	int				otherkills;
	int				cnt;				// used for general timing
	int				cnt_otherkills;
	int				cnt_kills[MAXPLAYERS];
	int				cnt_items[MAXPLAYERS];
	int				cnt_secret[MAXPLAYERS];
	int				cnt_frags[MAXPLAYERS];
	int				cnt_deaths[MAXPLAYERS];
	int				cnt_time;
	int				cnt_total_time;
	int				cnt_par;
	int				cnt_pause;
	int				total_frags;
	int				total_deaths;
	bool			noautostartmap;
	int				dofrags;
	int				ng_state;
	float			shadowalpha;

	PatchInfo 		mapname;
	PatchInfo 		finishedp;
	PatchInfo 		entering;
	PatchInfo		content;
	PatchInfo		author;

	TextureID 		p_secret;
	TextureID 		kills;
	TextureID 		secret;
	TextureID 		items;
	TextureID 		timepic;
	TextureID 		par;
	TextureID 		sucks;
	TextureID		finishedPatch;
	TextureID		enteringPatch;

	// [RH] Info to dynamically generate the level name graphics
	String			lnametexts[2];
	String			authortexts[2];

	bool 			snl_pointeron;

	int 			player_deaths[MAXPLAYERS];
	int  			sp_state;
	
	int cWidth, cHeight;	// size of the canvas
	int scalemode;
	int wrapwidth;	// size used to word wrap level names
	int scaleFactorX, scaleFactorY;


	//====================================================================
	//
	// Set fixed size mode.
	//
	//====================================================================

	void SetSize(int width, int height, int wrapw = -1, int scalemode = FSMode_ScaleToFit43)
	{
		cwidth = width;
		cheight = height;
		scalemode = FSMode_ScaleToFit43;
		scalefactorx = 1;
		scalefactory = 1;
		wrapwidth = wrapw == -1 ? width : wrapw;
	}

	//====================================================================
	//
	// Draws a single character with a shadow
	//
	//====================================================================

	int DrawCharPatch(Font fnt, int charcode, int x, int y, int translation = Font.CR_UNTRANSLATED, bool nomove = false)
	{
		int width = fnt.GetCharWidth(charcode);
		if (scalemode == -1) screen.DrawChar(fnt, translation, x, y, charcode, nomove ? DTA_CleanNoMove : DTA_Clean, true);
		else screen.DrawChar(fnt, translation, x, y, charcode, DTA_FullscreenScale, scalemode, DTA_VirtualWidth, cwidth, DTA_VirtualHeight, cheight);
		return x - width;
	}
	
	//====================================================================
	//
	//
	//
	//====================================================================

	void DrawTexture(TextureID tex, double x, double y, bool nomove = false)
	{
		if (scalemode == -1) screen.DrawTexture(tex, true, x, y, nomove ? DTA_CleanNoMove : DTA_Clean, true);
		else screen.DrawTexture(tex, true, x, y, DTA_FullscreenScale, scalemode, DTA_VirtualWidth, cwidth, DTA_VirtualHeight, cheight);
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	void DrawText(Font fnt, int color, double x, double y, String str, bool nomove = false, bool shadow = false)
	{
		if (scalemode == -1) screen.DrawText(fnt, color, x, y, str, nomove ? DTA_CleanNoMove : DTA_Clean, true, DTA_Shadow, shadow);
		else screen.DrawText(fnt, color, x, y, str, DTA_FullscreenScale, scalemode, DTA_VirtualWidth, cwidth, DTA_VirtualHeight, cheight, DTA_Shadow, shadow);
	}

	//====================================================================
	//
	// Draws a level name with the big font
	//
	// x is no longer passed as a parameter because the text is now broken into several lines
	// if it is too long
	//
	//====================================================================

	int DrawName(int y, TextureID tex, String levelname)
	{
		// draw <LevelName> 
		if (tex.isValid())
		{
			let size = TexMan.GetScaledSize(tex);
			DrawTexture(tex, (cwidth - size.X * scaleFactorX) /2, y, true);
			if (size.Y > 50)
			{ // Fix for Deus Vult II and similar wads that decide to make these hugely tall
			  // patches with vast amounts of empty space at the bottom.
				size.Y = TexMan.CheckRealHeight(tex);
			}
			return y + int(Size.Y) * scaleFactorY;
		}
		else if (levelname.Length() > 0)
		{
			int h = 0;
			int lumph = mapname.mFont.GetHeight() * scaleFactorY;

			BrokenLines lines = mapname.mFont.BreakLines(levelname, wrapwidth / scaleFactorX);

			int count = lines.Count();
			for (int i = 0; i < count; i++)
			{
				DrawText(mapname.mFont, mapname.mColor, (cwidth - lines.StringWidth(i) * scaleFactorX) / 2, y + h, lines.StringAt(i), true);
				h += lumph;
			}
			return y + h;
		}
		return 0;
	}

	//====================================================================
	//
	// Draws a level author's name with the given font
	//
	//====================================================================
	
	int DrawAuthor(int y, String levelname)
	{
		if (levelname.Length() > 0)
		{
			int h = 0;
			int lumph = author.mFont.GetHeight() * scaleFactorY;
			
			BrokenLines lines = author.mFont.BreakLines(levelname, wrapwidth / scaleFactorX);
			
			int count = lines.Count();
			for (int i = 0; i < count; i++)
			{
				DrawText(author.mFont, author.mColor, (cwidth - lines.StringWidth(i) * scaleFactorX) / 2, y + h, lines.StringAt(i), true);
				h += lumph;
			}
			return y + h;
		}
		return y;
	}
	
	//====================================================================
	//
	// Only kept so that mods that were accessing it continue to compile
	//
	//====================================================================

	deprecated("3.8") int DrawPatchText(int y, PatchInfo pinfo, String stringname)
	{
		String string = Stringtable.Localize(stringname);
		int midx = screen.GetWidth() / 2;

		screen.DrawText(pinfo.mFont, pinfo.mColor, midx - pinfo.mFont.StringWidth(string) * CleanXfac/2, y, string, DTA_CleanNoMove, true);
		return y + pinfo.mFont.GetHeight() * CleanYfac;
	}

	//====================================================================
	//
	// Draws a text, either as patch or as string from the string table
	//
	//====================================================================
	
	int DrawPatchOrText(int y, PatchInfo pinfo, TextureID patch, String stringname)
	{
		String string = Stringtable.Localize(stringname);
		int midx = cwidth / 2;
		
		if (TexMan.OkForLocalization(patch, stringname))
		{
			let size = TexMan.GetScaledSize(patch);
			DrawTexture(patch, midx - size.X * scaleFactorX/2, y, true);
			return y + int(size.Y * scaleFactorY);
		}
		else
		{
			DrawText(pinfo.mFont, pinfo.mColor, midx - pinfo.mFont.StringWidth(string) * scaleFactorX/2, y, string, true);
			return y + pinfo.mFont.GetHeight() * scaleFactorY;
		}
	}
	
	//====================================================================
	//
	// Draws "<Levelname> Finished!"
	//
	// Either uses the specified patch or the big font
	// A level name patch can be specified for all games now, not just Doom.
	//
	//====================================================================

	virtual int drawLF ()
	{
		bool ispatch = wbs.LName0.isValid();
		int oldy = TITLEY * scaleFactorY;
		int h;
		
		if (!ispatch)
		{
			let asc = mapname.mFont.GetMaxAscender(lnametexts[1]);
			if (asc > TITLEY - 2)
			{
				oldy = (asc+2) * scaleFactorY;
			}
		}
		
		int y = DrawName(oldy, wbs.LName0, lnametexts[0]);

		// If the displayed info is made of patches we need some additional offsetting here.
		if (ispatch) 
		{
			int disp = 0;
			// The offset getting applied here must at least be as tall as the largest ascender in the following text to avoid overlaps.
			if (authortexts[0].length() == 0)
			{
				int h1 = BigFont.GetHeight() - BigFont.GetDisplacement();
				int h2 = (y - oldy) / scaleFactorY / 4;
				disp = min(h1, h2);
				
				if (!TexMan.OkForLocalization(finishedPatch, "$WI_FINISHED"))
				{
					disp += finishedp.mFont.GetMaxAscender("$WI_FINISHED");
				}
			}
			else
			{
					disp += author.mFont.GetMaxAscender(authortexts[0]);
			}
			y += disp * scaleFactorY;
		}
		
		y = DrawAuthor(y, authortexts[0]);
		
		// draw "Finished!"

		int statsy = multiplayer? NG_STATSY : SP_STATSY * scaleFactorY;
		if (y < (statsy - finishedp.mFont.GetHeight()*3/4) * scaleFactorY)
		{
			// don't draw 'finished' if the level name is too tall
			y = DrawPatchOrText(y, finishedp, finishedPatch, "$WI_FINISHED");
		}
		return y;
	}

	//====================================================================
	//
	// Draws "Entering <LevelName>"
	//
	// Either uses the specified patch or the big font
	// A level name patch can be specified for all games now, not just Doom.
	//
	//====================================================================

	virtual void drawEL ()
	{
		bool ispatch = TexMan.OkForLocalization(enteringPatch, "$WI_ENTERING");
		int oldy = TITLEY * scaleFactorY;

		if (!ispatch)
		{
			let asc = entering.mFont.GetMaxAscender("$WI_ENTERING");
			if (asc > TITLEY - 2)
			{
				oldy = (asc+2) * scaleFactorY;
			}
		}

		int y = DrawPatchOrText(oldy, entering, enteringPatch, "$WI_ENTERING");
		
		// If the displayed info is made of patches we need some additional offsetting here.
		
		if (ispatch)
		{
			int h1 = BigFont.GetHeight() - BigFont.GetDisplacement();
			let size = TexMan.GetScaledSize(enteringPatch);
			int h2 = int(size.Y);
			let disp = min(h1, h2) / 4;
			// The offset getting applied here must at least be as tall as the largest ascender in the following text to avoid overlaps.
			if (!wbs.LName1.isValid())
			{
				disp += mapname.mFont.GetMaxAscender(lnametexts[1]);
			}
			y += disp * scaleFactorY;
		}

		y = DrawName(y, wbs.LName1, lnametexts[1]);

		if (wbs.LName1.isValid() && authortexts[1].length() > 0) 
		{
			// Consdider the ascender height of the following text.
			y += author.mFont.GetMaxAscender(authortexts[1]) * scaleFactorY;
		}
			
		DrawAuthor(y, authortexts[1]);

	}

	//====================================================================
	//
	// Draws a number.
	// If digits > 0, then use that many digits minimum,
	//	otherwise only use as many as necessary.
	// x is the right edge of the number.
	// Returns new x position, that is, the left edge of the number.
	//
	//====================================================================
	int drawNum (Font fnt, int x, int y, int n, int digits, bool leadingzeros = true, int translation = Font.CR_UNTRANSLATED, bool nomove = false)
	{
		int fntwidth = fnt.StringWidth("3");
		String text;
		int len;

		if (nomove && scalemode == -1)
		{
			fntwidth *= scaleFactorX;
		}
		text = String.Format("%d", n);
		len = text.Length();
		if (leadingzeros)
		{
			int filldigits = digits - len;
			for(int i = 0; i < filldigits; i++)
			{
				text = "0" .. text;
			}
			len = text.Length();
		}
		
		for(int text_p = len-1; text_p >= 0; text_p--)
		{
			// Digits are centered in a box the width of the '3' character.
			// Other characters (specifically, '-') are right-aligned in their cell.
			int c = text.ByteAt(text_p);
			if (c >= "0" && c <= "9")
			{
				x -= fntwidth;
				DrawCharPatch(fnt, c, x + (fntwidth - fnt.GetCharWidth(c)) / 2, y, translation, nomove);
			}
			else
			{
				DrawCharPatch(fnt, c, x - fnt.GetCharWidth(c), y, translation, nomove);
				x -= fntwidth;
			}
		}
		if (len < digits)
		{
			x -= fntwidth * (digits - len);
		}
		return x;
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	void drawPercent (Font fnt, int x, int y, int p, int b, bool show_total = true, int color = Font.CR_UNTRANSLATED, bool nomove = false)
	{
		if (p < 0)
			return;

		if (wi_percents)
		{
			if (nomove && scalemode == -1)
			{
				x -= fnt.StringWidth("%") * scaleFactorX;
			}
			else
			{
				x -= fnt.StringWidth("%");
			}
			DrawText(fnt, color, x, y, "%", nomove);
			if (nomove)
			{
				x -= 2*CleanXfac;
			}
			drawNum(fnt, x, y, b == 0 ? 100 : p * 100 / b, -1, false, color, nomove);
		}
		else
		{
			if (show_total)
			{
				x = drawNum(fnt, x, y, b, 2, false, color, nomove);
				x -= fnt.StringWidth("/");
				DrawText (fnt, color, x, y, "/", nomove);
			}
			drawNum (fnt, x, y, p, -1, false, color, nomove);
		}
	}


	//====================================================================
	//
	// Display level completion time and par, or "sucks" message if overflow.
	//
	//====================================================================

	void drawTimeFont (Font printFont, int x, int y, int t, int color)
	{
		bool sucky;

		if (t < 0)
			return;

		int hours = t / 3600;
		t -= hours * 3600;
		int minutes = t / 60;
		t -= minutes * 60;
		int seconds = t;

		// Why were these offsets hard coded? Half the WADs with custom patches
		// I tested screwed up miserably in this function!
		int num_spacing = printFont.GetCharWidth("3");
		int colon_spacing = printFont.GetCharWidth(":");

		x = drawNum (printFont, x, y, seconds, 2, true, color) - 1;
		DrawCharPatch (printFont, ":", x -= colon_spacing, y, color);
		x = drawNum (printFont, x, y, minutes, 2, hours!=0, color);
		if (hours)
		{
			DrawCharPatch (printFont, ":", x -= colon_spacing, y, color);
			drawNum (printFont, x, y, hours, 2, false, color);
		}
	}

	void drawTime (int x, int y, int t, bool no_sucks=false)
	{
		drawTimeFont(IntermissionFont, x, y, t, Font.CR_UNTRANSLATED);
	}

	//====================================================================
	//
	// the 'scaled' drawers are for the multiplayer scoreboard
	//
	//====================================================================

	void drawTextScaled (Font fnt, double x, double y, String text, double scale, int translation = Font.CR_UNTRANSLATED)
	{
		screen.DrawText(fnt, translation, x / scale, y / scale, text, DTA_VirtualWidthF, screen.GetWidth() / scale, DTA_VirtualHeightF, screen.GetHeight() / scale);
	}

	//====================================================================
	//
	//
	//====================================================================

	void drawNumScaled (Font fnt, int x, int y, double scale, int n, int digits, int translation = Font.CR_UNTRANSLATED)
	{
		String s = String.Format("%d", n);
		drawTextScaled(fnt, x - fnt.StringWidth(s) * scale, y, s, scale, translation);
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	void drawPercentScaled (Font fnt, int x, int y, int p, int b, double scale, bool show_total = true, int color = Font.CR_UNTRANSLATED)
	{
		if (p < 0) return;

		String s;
		if (wi_percents)
		{
			s = String.Format("%d%%", b == 0 ? 100 : p * 100 / b);
		}
		else if (show_total)
		{
			s = String.Format("%d/%3d", p, b);
		}
		else
		{
			s = String.Format("%d", p);
		}
		drawTextScaled(fnt, x - fnt.StringWidth(s) * scale, y, s, scale, color);
	}

	//====================================================================
	//
	// Display the completed time scaled
	//
	//====================================================================

	void drawTimeScaled (Font fnt, int x, int y, int t, double scale, int color = Font.CR_UNTRANSLATED)
	{
		if (t < 0)
			return;

		int hours = t / 3600;
		t -= hours * 3600;
		int minutes = t / 60;
		t -= minutes * 60;
		int seconds = t;

		String s = (hours > 0 ? String.Format("%d:", hours) : "") .. String.Format("%02d:%02d", minutes, seconds);

		drawTextScaled(fnt, x - fnt.StringWidth(s) * scale, y, s, scale, color);
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	virtual void End ()
	{
		CurState = LeavingIntermission;
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	bool autoSkip()
	{
		return wi_autoadvance > 0 && bcnt > (wi_autoadvance * GameTicRate);
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	protected virtual void initNoState ()
	{
		CurState = NoState;
		acceleratestage = 0;
		cnt = 10;
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	protected virtual void updateNoState ()
	{
		if (acceleratestage)
		{
			cnt = 0;
		}
		else
		{
			bool noauto = noautostartmap;

			for (int i = 0; !noauto && i < MAXPLAYERS; ++i)
			{
				if (playeringame[i])
				{
					noauto |= players[i].GetNoAutostartMap();
				}
			}
			if (!noauto || autoSkip())
			{
				cnt--;
			}
		}

		if (cnt == 0)
		{
			End();
		}
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	protected virtual void initShowNextLoc ()
	{
		if (wbs.next == "") 
		{
			// Last map in episode - there is no next location!
			jobstate = finished;
			return;
		}

		CurState = ShowNextLoc;
		acceleratestage = 0;
		cnt = SHOWNEXTLOCDELAY * GameTicRate;
		noautostartmap = bg.LoadBackground(true);
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	protected virtual void updateShowNextLoc ()
	{
		if (!--cnt || acceleratestage)
			initNoState();
		else
			snl_pointeron = (cnt & 31) < 20;
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	protected virtual void drawShowNextLoc(void)
	{
		bg.drawBackground(CurState, true, snl_pointeron);

		// draws which level you are entering..
		drawEL ();  

	}

	//====================================================================
	//
	//
	//
	//====================================================================

	protected virtual void drawNoState ()
	{
		snl_pointeron = true;
		drawShowNextLoc();
	}
	
	//====================================================================
	//
	//
	//
	//====================================================================

	protected int fragSum (int playernum)
	{
		int i;
		int frags = 0;
	
		for (i = 0; i < MAXPLAYERS; i++)
		{
			if (playeringame[i]
				&& i!=playernum)
			{
				frags += Plrs[playernum].frags[i];
			}
		}
		
		// JDC hack - negative frags.
		frags -= Plrs[playernum].frags[playernum];

		return frags;
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	static void PlaySound(Sound snd)
	{
		S_StartSound(snd, CHAN_VOICE, CHANF_MAYBE_LOCAL|CHANF_UI, 1, ATTN_NONE);
	}
	
	
	// ====================================================================
	//
	// Purpose: See if the player has hit either the attack or use key
	//          or mouse button.  If so we set acceleratestage to 1 and
	//          all those display routines above jump right to the end.
	// Args:    none
	// Returns: void
	//
	// ====================================================================

	override bool OnEvent(InputEvent evt)
	{
		if (evt.type == InputEvent.Type_KeyDown)
		{
			accelerateStage = 1;
			return true;
		}
		return false;
	}

	void nextStage()
	{
		accelerateStage = 1;
	}

	// this one is no longer used, but still needed for old content referencing them.
	deprecated("4.8") void checkForAccelerate()
	{
	}
	
	// ====================================================================
	// Ticker
	// Purpose: Do various updates every gametic, for stats, animation,
	//          checking that intermission music is running, etc.
	// Args:    none
	// Returns: void
	//
	// ====================================================================
	
	virtual void StartMusic()
	{
		if (!bg.IsUsingMusic())
			Level.SetInterMusic(wbs.next);
	}

	//====================================================================
	//
	// Two stage interface to allow redefining this class as a screen job
	//
	//====================================================================

	protected virtual void Ticker()
	{
		// counter for general background animation
		bcnt++;  
	
		if (bcnt == 1)
		{
			StartMusic();
		}
	
		bg.updateAnimatedBack();
	
		switch (CurState)
		{
		case StatCount:
			updateStats();
			break;
		
		case ShowNextLoc:
			updateShowNextLoc();
			break;
		
		case NoState:
			updateNoState();
			break;

		case LeavingIntermission:
			break;
		}
	}
	
	override void OnTick()
	{
		Ticker();
		if (CurState == StatusScreen.LeavingIntermission) jobstate = finished;
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	protected virtual void Drawer()
	{
		switch (CurState)
		{
		case StatCount:
			// draw animated background
			bg.drawBackground(CurState, false, false);
			drawStats();
			break;
	
		case ShowNextLoc:
		case LeavingIntermission:	// this must still draw the screen once more for the wipe code to pick up.
			drawShowNextLoc();
			break;
	
		default:
			drawNoState();
			break;
		}
	}

	override void Draw(double smoothratio)
	{
		Drawer();
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	virtual void Start (wbstartstruct wbstartstruct)
	{
		wbs = wbstartstruct;
		acceleratestage = 0;
		cnt = bcnt = 0;
		me = wbs.pnum;
		otherkills = wbs.totalkills;
		for (int i = 0; i < MAXPLAYERS; i++)
		{
			Plrs[i] = wbs.plyr[i];
			otherkills -= Plrs[i].skills;
		}

		if (gameinfo.mHideParTimes)
		{
			// par time and suck time are not displayed if zero.
			wbs.partime = 0;
			wbs.sucktime = 0;
		}
		
		entering.Init(gameinfo.mStatscreenEnteringFont);
		finishedp.Init(gameinfo.mStatscreenFinishedFont);
		mapname.Init(gameinfo.mStatscreenMapNameFont);
		content.Init(gameinfo.mStatscreenContentFont);
		author.Init(gameinfo.mStatscreenAuthorFont);

		Kills = TexMan.CheckForTexture("WIOSTK", TexMan.Type_MiscPatch);			// "kills"
		Secret = TexMan.CheckForTexture("WIOSTS", TexMan.Type_MiscPatch);		// "scrt", not used
		P_secret = TexMan.CheckForTexture("WISCRT2", TexMan.Type_MiscPatch);		// "secret"
		Items = TexMan.CheckForTexture("WIOSTI", TexMan.Type_MiscPatch);			// "items"
		Timepic = TexMan.CheckForTexture("WITIME", TexMan.Type_MiscPatch);		// "time"
		Sucks = TexMan.CheckForTexture("WISUCKS", TexMan.Type_MiscPatch);		// "sucks"
		Par = TexMan.CheckForTexture("WIPAR", TexMan.Type_MiscPatch);			// "par"
		enteringPatch = TexMan.CheckForTexture("WIENTER", TexMan.Type_MiscPatch);	// "entering"
		finishedPatch = TexMan.CheckForTexture("WIF", TexMan.Type_MiscPatch);			// "finished"

		lnametexts[0] = StringTable.Localize(wbstartstruct.thisname);		
		lnametexts[1] = StringTable.Localize(wbstartstruct.nextname);
		authortexts[0] = StringTable.Localize(wbstartstruct.thisauthor);
		authortexts[1] = StringTable.Localize(wbstartstruct.nextauthor);

		bg = InterBackground.Create(wbs);
		noautostartmap = bg.LoadBackground(false);
		initStats();
		
		wrapwidth = cwidth = screen.GetWidth();
		cheight = screen.GetHeight();
		scalemode = -1;
		scaleFactorX = CleanXfac;
		scaleFactorY = CleanYfac;
	}
	
	protected virtual void initStats() {}
	protected virtual void updateStats() {}
	protected virtual void drawStats() {}

	static int, int, int GetPlayerWidths()
	{
		int maxNameWidth;
		int maxScoreWidth;
		int maxIconHeight;

		StatusBar.Scoreboard_GetPlayerWidths(maxNameWidth, maxScoreWidth, maxIconHeight);

		return maxNameWidth, maxScoreWidth, maxIconHeight;
	}

	static Color GetRowColor(PlayerInfo player, bool highlight)
	{
		return StatusBar.Scoreboard_GetRowColor(player, highlight);
	}

	static void GetSortedPlayers(in out Array<int> sorted, bool teamplay)
	{
		sorted.clear();
		for(int i = 0; i < MAXPLAYERS; i++)
		{
			if(playeringame[i])
			{
				sorted.Push(i);
			}
		}

		if(teamplay)
		{
			StatusBar.Scoreboard_SortPlayers(sorted, BaseStatusBar.Scoreboard_CompareByTeams);
		}
		else
		{
			StatusBar.Scoreboard_SortPlayers(sorted, BaseStatusBar.Scoreboard_CompareByPoints);
		}
	}
}
