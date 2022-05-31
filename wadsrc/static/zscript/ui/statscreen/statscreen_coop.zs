
class CoopStatusScreen : StatusScreen
{
	int textcolor;
	double FontScale;
	int RowHeight;
	Font displayFont;

	//====================================================================
	//
	//
	//
	//====================================================================

	override void initStats ()
	{
		textcolor = Font.CR_GRAY;

		CurState = StatCount;
		acceleratestage = 0;
		ng_state = 1;
		displayFont = NewSmallFont;
		FontScale = max(screen.GetHeight() / 400, 1);
		RowHeight = int(max((displayFont.GetHeight() + 1) * FontScale, 1));

		cnt_pause = GameTicRate;

		for (int i = 0; i < MAXPLAYERS; i++)
		{
			playerready[i] = false;
			cnt_kills[i] = cnt_items[i] = cnt_secret[i] = cnt_frags[i] = 0;

			if (!playeringame[i])
				continue;

			dofrags += fragSum (i);
		}

		cnt_otherkills = 0;

		dofrags = !!dofrags;
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	override void updateStats ()
	{

		int i;
		int fsum;
		bool stillticking;
		bool autoskip = autoSkip();

		if ((acceleratestage || autoskip) && ng_state != 12)
		{
			acceleratestage = 0;

			for (i=0 ; i<MAXPLAYERS ; i++)
			{
				if (!playeringame[i])
					continue;

				cnt_kills[i] = Plrs[i].skills;
				cnt_items[i] = Plrs[i].sitems;
				cnt_secret[i] = Plrs[i].ssecret;

				if (dofrags)
					cnt_frags[i] = fragSum (i);
			}
			cnt_otherkills = otherkills;

			cnt_time = Thinker.Tics2Seconds(Plrs[me].stime);
			cnt_total_time = Thinker.Tics2Seconds(wbs.totaltime);

			PlaySound("intermission/nextstage");
			ng_state = 12;
		}

		if (ng_state == 2)
		{
			if (!(bcnt&3))
				PlaySound("intermission/tick");

			stillticking = false;

			for (i=0 ; i<MAXPLAYERS ; i++)
			{
				if (!playeringame[i])
					continue;

				cnt_kills[i] += 2;

				if (cnt_kills[i] > Plrs[i].skills)
					cnt_kills[i] = Plrs[i].skills;
				else
					stillticking = true;
			}

			cnt_otherkills += 2;

			if (cnt_otherkills > otherkills)
				cnt_otherkills = otherkills;
			else
				stillticking = true;

			if (!stillticking)
			{
				PlaySound("intermission/nextstage");
				ng_state++;
			}
		}
		else if (ng_state == 4)
		{
			if (!(bcnt&3))
				PlaySound("intermission/tick");

			stillticking = false;

			for (i=0 ; i<MAXPLAYERS ; i++)
			{
				if (!playeringame[i])
					continue;

				cnt_items[i] += 2;
				if (cnt_items[i] > Plrs[i].sitems)
					cnt_items[i] = Plrs[i].sitems;
				else
					stillticking = true;
			}
			if (!stillticking)
			{
				PlaySound("intermission/nextstage");
				ng_state++;
			}
		}
		else if (ng_state == 6)
		{
			if (!(bcnt&3))
				PlaySound("intermission/tick");

			stillticking = false;

			for (i=0 ; i<MAXPLAYERS ; i++)
			{
				if (!playeringame[i])
					continue;

				cnt_secret[i] += 2;

				if (cnt_secret[i] > Plrs[i].ssecret)
					cnt_secret[i] = Plrs[i].ssecret;
				else
					stillticking = true;
			}
		
			if (!stillticking)
			{
				PlaySound("intermission/nextstage");
				ng_state ++;
			}
		}
		else if (ng_state == 8)
		{
			if (!(bcnt&3))
				PlaySound("intermission/tick");

			stillticking = false;

			cnt_time += 3;
			cnt_total_time += 3;

			int sec = Thinker.Tics2Seconds(Plrs[me].stime);
			if (cnt_time > sec)
			{
				cnt_time = sec;
				cnt_total_time = Thinker.Tics2Seconds(wbs.totaltime);
			}
			else
				stillticking = true;

			if (!stillticking)
			{
				PlaySound("intermission/nextstage");
				ng_state += 1 + 2*!dofrags;
			}
		}
		else if (ng_state == 10)
		{
			if (!(bcnt&3))
				PlaySound("intermission/tick");

			stillticking = false;

			for (i=0 ; i<MAXPLAYERS ; i++)
			{
				if (!playeringame[i])
					continue;

				cnt_frags[i] += 1;

				if (cnt_frags[i] >= (fsum = fragSum(i)))
					cnt_frags[i] = fsum;
				else
					stillticking = true;
			}
		
			if (!stillticking)
			{
				PlaySound("intermission/cooptotal");
				ng_state++;
			}
		}
		else if (ng_state == 12)
		{
			// All players are ready; proceed.
			if ((acceleratestage) || autoskip)
			{
				PlaySound("intermission/pastcoopstats");
				initShowNextLoc();
			}
		}
		else if (ng_state & 1)
		{
			if (!--cnt_pause)
			{
				ng_state++;
				cnt_pause = GameTicRate;
			}
		}
	}

	//====================================================================
	//
	//
	//
	//====================================================================

	override void drawStats ()
	{
		int i, x, y, ypadding, height, lineheight;
		int maxnamewidth, maxscorewidth, maxiconheight;
		int pwidth = IntermissionFont.GetCharWidth("%");
		int icon_x, name_x, kills_x, bonus_x, secret_x;
		int bonus_len, secret_len;
		int missed_kills, missed_items, missed_secrets;
		float h, s, v, r, g, b;
		int color;
		String text_bonus, text_secret, text_kills;
		TextureID readyico = TexMan.CheckForTexture("READYICO", TexMan.Type_MiscPatch);

		y = drawLF();

		[maxnamewidth, maxscorewidth, maxiconheight] = GetPlayerWidths();
		// Use the readyico height if it's bigger.
		Vector2 readysize = TexMan.GetScaledSize(readyico);
		Vector2 readyoffset = TexMan.GetScaledOffset(readyico);
		height = int(readysize.Y - readyoffset.Y);
		maxiconheight = MAX(height, maxiconheight);
		height = int(displayFont.GetHeight() * FontScale);
		lineheight = MAX(height, maxiconheight * CleanYfac);
		ypadding = (lineheight - height + 1) / 2;
		y += CleanYfac;

		text_bonus = Stringtable.Localize((gameinfo.gametype & GAME_Raven) ? "$SCORE_BONUS" : "$SCORE_ITEMS");
		text_secret = Stringtable.Localize("$SCORE_SECRET");
		text_kills = Stringtable.Localize("$SCORE_KILLS");

		icon_x = int(8 * FontScale);
		name_x = int(icon_x + maxscorewidth * FontScale);
		kills_x = int(name_x + (maxnamewidth + 1 + MAX(displayFont.StringWidth("XXXXXXXXXX"), displayFont.StringWidth(text_kills)) + 16) * FontScale);
		bonus_x = int(kills_x + ((bonus_len = displayFont.StringWidth(text_bonus)) + 16) * FontScale);
		secret_x = int(bonus_x + ((secret_len = displayFont.StringWidth(text_secret)) + 16) * FontScale);

		x = (screen.GetWidth() - secret_x) >> 1;
		icon_x += x;
		name_x += x;
		kills_x += x;
		bonus_x += x;
		secret_x += x;


		drawTextScaled(displayFont, name_x, y, Stringtable.Localize("$SCORE_NAME"), FontScale, textcolor);
		drawTextScaled(displayFont, kills_x - displayFont.StringWidth(text_kills) * FontScale, y, text_kills, FontScale, textcolor);
		drawTextScaled(displayFont, bonus_x - bonus_len * FontScale, y, text_bonus, FontScale, textcolor);
		drawTextScaled(displayFont, secret_x - secret_len * FontScale, y, text_secret, FontScale, textcolor);
		y += int(height + 6 * FontScale);

		missed_kills = wbs.maxkills;
		missed_items = wbs.maxitems;
		missed_secrets = wbs.maxsecret;

		// Draw lines for each player
		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (!playeringame[i])
				continue;

			PlayerInfo player = players[i];

			screen.Dim(player.GetDisplayColor(), 0.8f, x, y - ypadding, (secret_x - x) + (8 * CleanXfac), lineheight);

			//if (playerready[i] || player.Bot != NULL) // Bots are automatically assumed ready, to prevent confusion
				screen.DrawTexture(readyico, true, x - (readysize.Y * CleanXfac), y, DTA_CleanNoMove, true);

			Color thiscolor = GetRowColor(player, i == consoleplayer);
			if (player.mo.ScoreIcon.isValid())
			{
				screen.DrawTexture(player.mo.ScoreIcon, true, icon_x, y, DTA_CleanNoMove, true);
			}
			drawTextScaled(displayFont, name_x, y + ypadding, player.GetUserName(), FontScale, thiscolor);
			drawPercentScaled(displayFont, kills_x, y + ypadding, cnt_kills[i], wbs.maxkills, FontScale, thiscolor);
			missed_kills -= cnt_kills[i];
			if (ng_state >= 4)
			{
				drawPercentScaled(displayFont, bonus_x, y + ypadding, cnt_items[i], wbs.maxitems, FontScale, thiscolor);
				missed_items -= cnt_items[i];
				if (ng_state >= 6)
				{
					drawPercentScaled(displayFont, secret_x, y + ypadding, cnt_secret[i], wbs.maxsecret, FontScale, thiscolor);
					missed_secrets -= cnt_secret[i];
				}
			}
			y += lineheight + CleanYfac;
		}

		// Draw "OTHER" line
		y += 3 * CleanYfac;
		drawTextScaled(displayFont, name_x, y, Stringtable.Localize("$SCORE_OTHER"), FontScale, Font.CR_DARKGRAY);
		drawPercentScaled(displayFont, kills_x, y, cnt_otherkills, wbs.maxkills, FontScale, Font.CR_DARKGRAY);
		missed_kills -= cnt_otherkills;

		// Draw "MISSED" line
		y += height + 3 * CleanYfac;
		drawTextScaled(displayFont, name_x, y, Stringtable.Localize("$SCORE_MISSED"), FontScale, Font.CR_DARKGRAY);
		drawPercentScaled(displayFont, kills_x, y, missed_kills, wbs.maxkills, FontScale, Font.CR_DARKGRAY);
		if (ng_state >= 4)
		{
			drawPercentScaled(displayFont, bonus_x, y, missed_items, wbs.maxitems, FontScale, Font.CR_DARKGRAY);
			if (ng_state >= 6)
			{
				drawPercentScaled(displayFont, secret_x, y, missed_secrets, wbs.maxsecret, FontScale, Font.CR_DARKGRAY);
			}
		}

		// Draw "TOTAL" line
		y += height + 3 * CleanYfac;
		drawTextScaled(displayFont, name_x, y, Stringtable.Localize("$SCORE_TOTAL"), FontScale, textcolor);
		drawNumScaled(displayFont, kills_x, y, FontScale, wbs.maxkills, 0, textcolor);
		if (ng_state >= 4)
		{
			drawNumScaled(displayFont, bonus_x, y, FontScale, wbs.maxitems, 0, textcolor);
			if (ng_state >= 6)
			{
				drawNumScaled(displayFont, secret_x, y, FontScale, wbs.maxsecret, 0, textcolor);
			}
		}

		// Draw "TIME" line
		y += height + 3 * CleanYfac;
		drawTextScaled(displayFont, name_x, y, Stringtable.Localize("$TXT_IMTIME"), FontScale, textcolor);

		if (ng_state >= 8)
		{
			drawTimeScaled(displayFont, kills_x, y, cnt_time, FontScale, textcolor);
			drawTimeScaled(displayFont, secret_x, y, cnt_total_time, FontScale, textcolor);
		}
	}
}
