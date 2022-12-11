
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
		cnt_pause = Thinker.TICRATE;
	
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
			cnt_par = wbs.partime / Thinker.TICRATE;
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

			int psec = wbs.partime / Thinker.TICRATE;
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
				cnt_pause = Thinker.TICRATE;
			}
		}
	}

	override void drawStats (void)
	{
		// line height
		int lh = IntermissionFont.GetHeight() * 3 / 2;

		drawLF();
	
		screen.DrawTexture (Kills, true, SP_STATSX, SP_STATSY, DTA_Clean, true);
		drawPercent (IntermissionFont, 320 - SP_STATSX, SP_STATSY, cnt_kills[0], wbs.maxkills);

		screen.DrawTexture (Items, true, SP_STATSX, SP_STATSY+lh, DTA_Clean, true);
		drawPercent (IntermissionFont, 320 - SP_STATSX, SP_STATSY+lh, cnt_items[0], wbs.maxitems);

		screen.DrawTexture (P_secret, true, SP_STATSX, SP_STATSY+2*lh, DTA_Clean, true);
		drawPercent (IntermissionFont, 320 - SP_STATSX, SP_STATSY+2*lh, cnt_secret[0], wbs.maxsecret);

		screen.DrawTexture (Timepic, true, SP_TIMEX, SP_TIMEY, DTA_Clean, true);
		drawTime (160 - SP_TIMEX, SP_TIMEY, cnt_time);
		if (wi_showtotaltime)
		{
			drawTime (160 - SP_TIMEX, SP_TIMEY + lh, cnt_total_time, true);	// no 'sucks' for total time ever!
		}

		if (wbs.partime)
		{
			screen.DrawTexture (Par, true, 160 + SP_TIMEX, SP_TIMEY, DTA_Clean, true);
			drawTime (320 - SP_TIMEX, SP_TIMEY, cnt_par);
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
	
		screen.DrawText (BigFont, Font.CR_UNTRANSLATED, 50, 65, Stringtable.Localize("$TXT_IMKILLS"), DTA_Clean, true, DTA_Shadow, true);
		screen.DrawText (BigFont, Font.CR_UNTRANSLATED, 50, 90, Stringtable.Localize("$TXT_IMITEMS"), DTA_Clean, true, DTA_Shadow, true);
		screen.DrawText (BigFont, Font.CR_UNTRANSLATED, 50, 115, Stringtable.Localize("$TXT_IMSECRETS"), DTA_Clean, true, DTA_Shadow, true);

		int countpos = gameinfo.gametype==GAME_Strife? 285:270;
		if (sp_state >= 2)
		{
			drawPercent (IntermissionFont, countpos, 65, cnt_kills[0], wbs.maxkills);
		}
		if (sp_state >= 4)
		{
			drawPercent (IntermissionFont, countpos, 90, cnt_items[0], wbs.maxitems);
		}
		if (sp_state >= 6)
		{
			drawPercent (IntermissionFont, countpos, 115, cnt_secret[0], wbs.maxsecret);
		}
		if (sp_state >= 8)
		{
			screen.DrawText (BigFont, Font.CR_UNTRANSLATED, 85, 160, Stringtable.Localize("$TXT_IMTIME"), DTA_Clean, true, DTA_Shadow, true);
			drawTime (249, 160, cnt_time);
			if (wi_showtotaltime)
			{
				drawTime (249, 180, cnt_total_time);
			}
		}
	}
}
