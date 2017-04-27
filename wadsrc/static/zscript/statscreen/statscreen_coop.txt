
class CoopStatusScreen : StatusScreen
{
	int textcolor;

	//====================================================================
	//
	//
	//
	//====================================================================

	override void initStats ()
	{
		textcolor = (gameinfo.gametype & GAME_Raven) ? Font.CR_GREEN : Font.CR_UNTRANSLATED;

		CurState = StatCount;
		acceleratestage = 0;
		ng_state = 1;

		cnt_pause = Thinker.TICRATE;

		for (int i = 0; i < MAXPLAYERS; i++)
		{
			playerready[i] = false;
			cnt_kills[i] = cnt_items[i] = cnt_secret[i] = cnt_frags[i] = 0;

			if (!playeringame[i])
				continue;

			dofrags += fragSum (i);
		}

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

		if ((acceleratestage || autoskip) && ng_state != 10)
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
			PlaySound("intermission/nextstage");
			ng_state = 10;
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
				ng_state += 1 + 2*!dofrags;
			}
		}
		else if (ng_state == 8)
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
		else if (ng_state == 10)
		{
			int i;
			for (i = 0; i < MAXPLAYERS; i++)
			{
				// If the player is in the game and not ready, stop checking
				if (playeringame[i] && players[i].Bot == NULL && !playerready[i])
					break;
			}

			// All players are ready; proceed.
			if ((i == MAXPLAYERS && acceleratestage) || autoskip)
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
				cnt_pause = Thinker.TICRATE;
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
		height = SmallFont.GetHeight() * CleanYfac;
		lineheight = MAX(height, maxiconheight * CleanYfac);
		ypadding = (lineheight - height + 1) / 2;
		y += CleanYfac;

		text_bonus = Stringtable.Localize((gameinfo.gametype & GAME_Raven) ? "$SCORE_BONUS" : "$SCORE_ITEMS");
		text_secret = Stringtable.Localize("$SCORE_SECRET");
		text_kills = Stringtable.Localize("$SCORE_KILLS");

		icon_x = 8 * CleanXfac;
		name_x = icon_x + maxscorewidth * CleanXfac;
		kills_x = name_x + (maxnamewidth + MAX(SmallFont.StringWidth("XXXXX"), SmallFont.StringWidth(text_kills)) + 8) * CleanXfac;
		bonus_x = kills_x + ((bonus_len = SmallFont.StringWidth(text_bonus)) + 8) * CleanXfac;
		secret_x = bonus_x + ((secret_len = SmallFont.StringWidth(text_secret)) + 8) * CleanXfac;

		x = (screen.GetWidth() - secret_x) >> 1;
		icon_x += x;
		name_x += x;
		kills_x += x;
		bonus_x += x;
		secret_x += x;


		screen.DrawText(SmallFont, textcolor, name_x, y, Stringtable.Localize("$SCORE_NAME"), DTA_CleanNoMove, true);
		screen.DrawText(SmallFont, textcolor, kills_x - SmallFont.StringWidth(text_kills)*CleanXfac, y, text_kills, DTA_CleanNoMove, true);
		screen.DrawText(SmallFont, textcolor, bonus_x - bonus_len*CleanXfac, y, text_bonus, DTA_CleanNoMove, true);
		screen.DrawText(SmallFont, textcolor, secret_x - secret_len*CleanXfac, y, text_secret, DTA_CleanNoMove, true);
		y += height + 6 * CleanYfac;

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

			if (playerready[i] || player.Bot != NULL) // Bots are automatically assumed ready, to prevent confusion
				screen.DrawTexture(readyico, true, x - (readysize.Y * CleanXfac), y, DTA_CleanNoMove, true);

			Color thiscolor = GetRowColor(player, i == consoleplayer);
			if (player.mo.ScoreIcon.isValid())
			{
				screen.DrawTexture(player.mo.ScoreIcon, true, icon_x, y, DTA_CleanNoMove, true);
			}
			screen.DrawText(SmallFont, thiscolor, name_x, y + ypadding, player.GetUserName(), DTA_CleanNoMove, true);
			drawPercent(SmallFont, kills_x, y + ypadding, cnt_kills[i], wbs.maxkills, false, thiscolor);
			missed_kills -= cnt_kills[i];
			if (ng_state >= 4)
			{
				drawPercent(SmallFont, bonus_x, y + ypadding, cnt_items[i], wbs.maxitems, false, thiscolor);
				missed_items -= cnt_items[i];
				if (ng_state >= 6)
				{
					drawPercent(SmallFont, secret_x, y + ypadding, cnt_secret[i], wbs.maxsecret, false, thiscolor);
					missed_secrets -= cnt_secret[i];
				}
			}
			y += lineheight + CleanYfac;
		}

		// Draw "MISSED" line
		y += 3 * CleanYfac;
		screen.DrawText(SmallFont, Font.CR_DARKGRAY, name_x, y, Stringtable.Localize("$SCORE_MISSED"), DTA_CleanNoMove, true);
		drawPercent(SmallFont, kills_x, y, missed_kills, wbs.maxkills, false, Font.CR_DARKGRAY);
		if (ng_state >= 4)
		{
			drawPercent(SmallFont, bonus_x, y, missed_items, wbs.maxitems, false, Font.CR_DARKGRAY);
			if (ng_state >= 6)
			{
				drawPercent(SmallFont, secret_x, y, missed_secrets, wbs.maxsecret, false, Font.CR_DARKGRAY);
			}
		}

		// Draw "TOTAL" line
		y += height + 3 * CleanYfac;
		screen.DrawText(SmallFont, textcolor, name_x, y, Stringtable.Localize("$SCORE_TOTAL"), DTA_CleanNoMove, true);
		drawNum(SmallFont, kills_x, y, wbs.maxkills, 0, false, textcolor);
		if (ng_state >= 4)
		{
			drawNum(SmallFont, bonus_x, y, wbs.maxitems, 0, false, textcolor);
			if (ng_state >= 6)
			{
				drawNum(SmallFont, secret_x, y, wbs.maxsecret, 0, false, textcolor);
			}
		}
	}
}