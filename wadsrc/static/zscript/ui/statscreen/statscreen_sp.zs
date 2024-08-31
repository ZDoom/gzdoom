
class DoomStatusScreen : StatusScreen
{
	int intermissioncounter;
	
	override void initStats ()
	{
		intermissioncounter = gameinfo.intermissioncounter;
		CurState = StatCount;
		acceleratestage = 0;
		sp_state = 1;
		cnt_kills[0] = cnt_items[0] = cnt_secret[0] = -1;
		cnt_time = cnt_par = -1;
		cnt_pause = GameTicRate;
	
		cnt_total_time = -1;
	}

	override void updateStats ()
	{
		if (acceleratestage && sp_state != 10)
		{
			acceleratestage = 0;
			sp_state = 10;
			PlaySound("intermission/nextstage");

			cnt_kills[0] = Plrs[me].skills;
			cnt_items[0] = Plrs[me].sitems;
			cnt_secret[0] = Plrs[me].ssecret;
			cnt_time = Thinker.Tics2Seconds(Plrs[me].stime);
			cnt_par = wbs.partime / GameTicRate;
			cnt_total_time = Thinker.Tics2Seconds(wbs.totaltime);
		}

		if (sp_state == 2)
		{
			if (intermissioncounter)
			{
				cnt_kills[0] += 2;

				if (!(bcnt&3))
					PlaySound("intermission/tick");
			}
			if (!intermissioncounter || cnt_kills[0] >= Plrs[me].skills)
			{
				cnt_kills[0] = Plrs[me].skills;
				PlaySound("intermission/nextstage");
				sp_state++;
			}
		}
		else if (sp_state == 4)
		{
			if (intermissioncounter)
			{
				cnt_items[0] += 2;

				if (!(bcnt&3))
					PlaySound("intermission/tick");
			}
			if (!intermissioncounter || cnt_items[0] >= Plrs[me].sitems)
			{
				cnt_items[0] = Plrs[me].sitems;
				PlaySound("intermission/nextstage");
				sp_state++;
			}
		}
		else if (sp_state == 6)
		{
			if (intermissioncounter)
			{
				cnt_secret[0] += 2;

				if (!(bcnt&3))
					PlaySound("intermission/tick");
			}
			if (!intermissioncounter || cnt_secret[0] >= Plrs[me].ssecret)
			{
				cnt_secret[0] = Plrs[me].ssecret;
				PlaySound("intermission/nextstage");
				sp_state++;
			}
		}
		else if (sp_state == 8)
		{
			if (intermissioncounter)
			{
				if (!(bcnt&3))
					PlaySound("intermission/tick");

				cnt_time += 3;
				cnt_par += 3;
				cnt_total_time += 3;
			}

			int sec = Thinker.Tics2Seconds(Plrs[me].stime);
			if (!intermissioncounter || cnt_time >= sec)
				cnt_time = sec;

			int tsec = Thinker.Tics2Seconds(wbs.totaltime);
			if (!intermissioncounter || cnt_total_time >= tsec)
				cnt_total_time = tsec;

			int psec = wbs.partime / GameTicRate;
			if (!intermissioncounter || cnt_par >= psec)
			{
				cnt_par = psec;

				if (cnt_time >= sec)
				{
					cnt_total_time = tsec;
					PlaySound("intermission/nextstage");
					sp_state++;
				}
			}
		}
		else if (sp_state == 10)
		{
			if (acceleratestage)
			{
				PlaySound("intermission/paststats");
				initShowNextLoc();
			}
		}
		else if (sp_state & 1)
		{
			if (!--cnt_pause)
			{
				sp_state++;
				cnt_pause = GameTicRate;
			}
		}
	}

	override void drawStats (void)
	{
		// line height
		int lh = IntermissionFont.GetHeight() * 3 / 2;

		drawLF();
		
	
		// For visual consistency, only use the patches here if all are present.
		bool useGfx = TexMan.OkForLocalization(Kills, "$TXT_IMKILLS")
			&& TexMan.OkForLocalization(Items, "$TXT_IMITEMS")
			&& TexMan.OkForLocalization(P_secret, "$TXT_IMSECRETS")
			&& TexMan.OkForLocalization(Timepic, "$TXT_IMTIME")
			&& (!wbs.partime || TexMan.OkForLocalization(Par, "$TXT_IMPAR"));

		// The font color may only be used when the entire screen is printed as text.
		// Otherwise the text based parts should not be translated to match the other graphics patches.
		let tcolor = useGfx? Font.CR_UNTRANSLATED : content.mColor;

		Font printFont;
		Font textFont = generic_ui? NewSmallFont : content.mFont;
		int statsx = SP_STATSX;

		int timey = SP_TIMEY;
		if (wi_showtotaltime)
			timey = min(SP_TIMEY, 200 - 2 * lh);

		if (useGfx)
		{
			printFont = IntermissionFont;
			DrawTexture (Kills, statsx, SP_STATSY);
			DrawTexture (Items, statsx, SP_STATSY+lh);
			DrawTexture (P_secret, statsx, SP_STATSY+2*lh);
			DrawTexture (Timepic, SP_TIMEX, timey);
			if (wbs.partime) DrawTexture (Par, 160 + SP_TIMEX, timey);
		}
		else
		{
			// Check if everything fits on the screen.
			String percentage = wi_percents? " 0000%" : " 0000/0000";
			int perc_width = textFont.StringWidth(percentage);
			int k_width = textFont.StringWidth("$TXT_IMKILLS");
			int i_width = textFont.StringWidth("$TXT_IMITEMS");
			int s_width = textFont.StringWidth("$TXT_IMSECRETS");
			int allwidth = max(k_width, i_width, s_width) + perc_width;
			if ((SP_STATSX*2 + allwidth) > 320)	// The content does not fit so adjust the position a bit.
			{
				statsx = max(0, (320 - allwidth) / 2);
			}

			printFont = generic_ui? IntermissionFont : content.mFont;
			DrawText (textFont, tcolor, statsx, SP_STATSY, "$TXT_IMKILLS");
			DrawText (textFont, tcolor, statsx, SP_STATSY+lh, "$TXT_IMITEMS");
			DrawText (textFont, tcolor, statsx, SP_STATSY+2*lh, "$TXT_IMSECRETS");
			DrawText (textFont, tcolor, SP_TIMEX, timey, "$TXT_IMTIME");
			if (wbs.partime) DrawText (textFont, tcolor, 160 + SP_TIMEX, timey, "$TXT_IMPAR");
		}
			 
		drawPercent (printFont, 320 - statsx, SP_STATSY, cnt_kills[0], wbs.maxkills, true, tcolor);
		drawPercent (printFont, 320 - statsx, SP_STATSY+lh, cnt_items[0], wbs.maxitems, true, tcolor);
		drawPercent (printFont, 320 - statsx, SP_STATSY+2*lh, cnt_secret[0], wbs.maxsecret, true, tcolor);
		drawTimeFont (printFont, 160 - SP_TIMEX, timey, cnt_time, tcolor);
			 
		// This really sucks - not just by its message - and should have been removed long ago!
		// To avoid problems here, the "sucks" text only gets printed if the lump is present, this even applies to the text replacement.
			 
		if (cnt_time >= wbs.sucktime * 60 * 60 && wbs.sucktime > 0 && Sucks.IsValid())
		{ // "sucks"
			int x = 160 - SP_TIMEX;
			int y = timey;
			if (useGfx && TexMan.OkForLocalization(Sucks, "$TXT_IMSUCKS"))
			{
				let size = TexMan.GetScaledSize(Sucks);
				DrawTexture (Sucks, x - size.X, y - size.Y - 2);
			}
			else
			{
				DrawText (textFont, tColor, x  - printFont.StringWidth("$TXT_IMSUCKS"), y - printFont.GetHeight() - 2,	"$TXT_IMSUCKS");
			}
		}

		if (wi_showtotaltime)
		{
			 drawTimeFont (printFont, 160 - SP_TIMEX, timey + lh, cnt_total_time, tcolor);
		}

		if (wbs.partime)
		{
			drawTimeFont (printFont, 320 - SP_TIMEX, timey, cnt_par, tcolor);
		}
	}
}

class RavenStatusScreen : DoomStatusScreen
{
	override void drawStats (void)
	{
		// line height
		int lh = IntermissionFont.GetHeight() * 3 / 2;

		drawLF();

		Font textFont = generic_ui? NewSmallFont : content.mFont;
		let tcolor = content.mColor;

		DrawText (textFont, tcolor, 50, 65, "$TXT_IMKILLS", shadow:true);
		DrawText (textFont, tcolor, 50, 90, "$TXT_IMITEMS", shadow:true);
		DrawText (textFont, tcolor, 50, 115, "$TXT_IMSECRETS", shadow:true);

		int countpos = gameinfo.gametype==GAME_Strife? 285:270;
		if (sp_state >= 2)
		{
			drawPercent (textFont, countpos, 65, cnt_kills[0], wbs.maxkills, true, tcolor);
		}
		if (sp_state >= 4)
		{
			drawPercent (textFont, countpos, 90, cnt_items[0], wbs.maxitems, true, tcolor);
		}
		if (sp_state >= 6)
		{
			drawPercent (textFont, countpos, 115, cnt_secret[0], wbs.maxsecret, true, tcolor);
		}
		if (sp_state >= 8)
		{
			DrawText (textFont, tcolor, 85, 160, "$TXT_IMTIME", shadow:true);
			drawTimeFont (textFont, 249, 160, cnt_time, tcolor);
			if (wi_showtotaltime)
			{
				drawTimeFont (textFont, 249, 180, cnt_total_time, tcolor);
			}
		}
	}
}
