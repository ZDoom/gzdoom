
class DeathmatchStatusScreen : StatusScreen
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

	override void initStats (void)
	{
		int i, j;

		textcolor = Font.CR_GRAY;

		CurState = StatCount;
		acceleratestage = 0;
		displayFont = NewSmallFont;
		FontScale = max(screen.GetHeight() / 400, 1);
		RowHeight = int(max((displayFont.GetHeight() + 1) * FontScale, 1));

		for(i = 0; i < MAXPLAYERS; i++)
		{
			playerready[i] = false;
			cnt_frags[i] = cnt_deaths[i] = player_deaths[i] = 0;
		}
		total_frags = 0;
		total_deaths = 0;

		ng_state = 1;
		cnt_pause = GameTicRate;

		for (i=0 ; i<MAXPLAYERS ; i++)
		{
			if (playeringame[i])
			{
				for (j = 0; j < MAXPLAYERS; j++)
				{
					if (playeringame[j])
						player_deaths[i] += Plrs[j].frags[i];
				}
				total_deaths += player_deaths[i];
				total_frags += Plrs[i].fragcount;
			}
		}
	}

	override void updateStats ()
	{

		int i;
		bool stillticking;
		bool doautoskip = autoSkip();

		if ((acceleratestage || doautoskip) && ng_state != 6)
		{
			acceleratestage = 0;

			for (i = 0; i<MAXPLAYERS; i++)
			{
				if (!playeringame[i]) continue;

				cnt_frags[i] = Plrs[i].fragcount;
				cnt_deaths[i] = player_deaths[i];
			}
			PlaySound("intermission/nextstage");
			ng_state = 6;
		}

		if (ng_state == 2)
		{
			if (!(bcnt & 3))
				PlaySound("intermission/tick");

			stillticking = false;

			for (i = 0; i<MAXPLAYERS; i++)
			{
				if (!playeringame[i])
					continue;

				cnt_frags[i] += 2;

				if (cnt_frags[i] > Plrs[i].fragcount)
					cnt_frags[i] = Plrs[i].fragcount;
				else
					stillticking = true;
			}

			if (!stillticking)
			{
				PlaySound("intermission/nextstage");
				ng_state++;
			}
		}
		else if (ng_state == 4)
		{
			if (!(bcnt & 3))
				PlaySound("intermission/tick");

			stillticking = false;

			for (i = 0; i<MAXPLAYERS; i++)
			{
				if (!playeringame[i])
					continue;

				cnt_deaths[i] += 2;
				if (cnt_deaths[i] > player_deaths[i])
					cnt_deaths[i] = player_deaths[i];
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
			// All players are ready; proceed.
			if ((acceleratestage) || doautoskip)
			{
				PlaySound("intermission/pastdmstats");
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

	override void drawStats ()
	{
		int i, pnum, x, y, ypadding, height, lineheight;
		int maxnamewidth, maxscorewidth, maxiconheight;
		int pwidth = IntermissionFont.GetCharWidth("%");
		int icon_x, name_x, frags_x, deaths_x;
		int deaths_len;
		String text_deaths, text_frags;
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

		text_deaths = Stringtable.Localize("$SCORE_DEATHS");
		//text_color = Stringtable.Localize("$SCORE_COLOR");
		text_frags = Stringtable.Localize("$SCORE_FRAGS");

		icon_x = int(8 * FontScale);
		name_x = int(icon_x + maxscorewidth * FontScale);
		frags_x = name_x + int((maxnamewidth + 1 + MAX(displayFont.StringWidth("XXXXXXXXXX"), displayFont.StringWidth(text_frags)) + 16) * FontScale);
		deaths_x = frags_x + int(((deaths_len = displayFont.StringWidth(text_deaths)) + 16) * FontScale);
		
		x = (Screen.GetWidth() - deaths_x) >> 1;
		icon_x += x;
		name_x += x;
		frags_x += x;
		deaths_x += x;

		drawTextScaled(displayFont, name_x, y, Stringtable.Localize("$SCORE_NAME"), FontScale, textcolor);
		drawTextScaled(displayFont, frags_x - displayFont.StringWidth(text_frags) * FontScale, y, text_frags, FontScale, textcolor);
		drawTextScaled(displayFont, deaths_x - deaths_len * FontScale, y, text_deaths, FontScale, textcolor);
		y += height + int(6 * FontScale);
		
		// Sort all players
		Array<int> sortedplayers;
		GetSortedPlayers(sortedplayers, teamplay);

		// Draw lines for each player
		for (i = 0; i < sortedplayers.Size(); i++)
		{
			pnum = sortedplayers[i];
			PlayerInfo player = players[pnum];

			if (!playeringame[pnum])
				continue;

			screen.Dim(player.GetDisplayColor(), 0.8, x, y - ypadding, (deaths_x - x) + (8 * CleanXfac), lineheight);

			//if (playerready[pnum] || player.Bot != NULL) // Bots are automatically assumed ready, to prevent confusion
				screen.DrawTexture(readyico, true, x - (readysize.X * CleanXfac), y, DTA_CleanNoMove, true);

			let thiscolor = GetRowColor(player, pnum == consoleplayer);
			if (player.mo.ScoreIcon.isValid())
			{
				screen.DrawTexture(player.mo.ScoreIcon, true, icon_x, y, DTA_CleanNoMove, true);
			}
			
			drawTextScaled(displayFont, name_x, y + ypadding, player.GetUserName(), FontScale, thiscolor);
			drawNumScaled(displayFont, frags_x, y + ypadding, FontScale, cnt_frags[pnum], 0, textcolor);

			if (ng_state >= 2)
			{
				drawNumScaled(displayFont, deaths_x, y + ypadding, FontScale, cnt_deaths[pnum], 0, textcolor);
			}
			y += lineheight + CleanYfac;
		}

		// Draw "TOTAL" line
		y += height + 3 * CleanYfac;
		drawTextScaled(displayFont, name_x, y, Stringtable.Localize("$SCORE_TOTAL"), FontScale, textcolor);
		drawNumScaled(displayFont, frags_x, y, FontScale, total_frags, 0, textcolor);
		
		if (ng_state >= 4)
		{
			drawNumScaled(displayFont, deaths_x, y, FontScale, total_deaths, 0, textcolor);
		}

		// Draw game time
		y += height + CleanYfac;

		int seconds = Thinker.Tics2Seconds(Plrs[me].stime);
		int hours = seconds / 3600;
		int minutes = (seconds % 3600) / 60;
		seconds = seconds % 60;

		String leveltime = Stringtable.Localize("$SCORE_LVLTIME") .. ": " .. String.Format("%02i:%02i:%02i", hours, minutes, seconds);
		drawTextScaled(displayFont, x, y, leveltime, FontScale, textcolor);
	}
}
