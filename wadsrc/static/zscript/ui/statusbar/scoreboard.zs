
extend class BaseStatusBar
{
    Font scoreboardFont;

    virtual void InitScoreboard()
    {
        scoreboardFont = NewSmallFont;
    }

    static clearscope int Scoreboard_CompareByTeams(int playerA, int playerB)
    {
	    // Compare first by teams, then by frags, then by name.
	    PlayerInfo p1 = players[playerA];
	    PlayerInfo p2 = players[playerB];

	    int diff = p1.GetTeam() - p2.GetTeam();

	    if(diff == 0)
	    {
		    diff = p2.fragcount - p1.fragcount;
		    if(diff == 0)
		    {
			    diff = p1.GetUserName().CompareNoCase(p2.GetUserName());
		    }
	    }
	    return diff;
    }
    
    static clearscope int Scoreboard_CompareByPoints(int playerA, int playerB)
    {
	    // Compare first by frags/kills, then by name.
	    PlayerInfo p1 = players[playerA];
	    PlayerInfo p2 = players[playerB];

	    int diff = deathmatch ? (p2.fragcount - p1.fragcount) : (p2.killcount - p1.killcount);

		if(diff == 0)
		{
			diff = p1.GetUserName().CompareNoCase(p2.GetUserName());
		}
	    return diff;
    }

    static void Scoreboard_SortPlayers(out Array<int> players, Function<clearscope int(int, int)> compareFunc)
    {
        Array<int> unsorted;
        unsorted.Move(players);

		players.Push(unsorted[0]);

		for(int i = 1; i < unsorted.Size(); i++)
		{
			bool inserted = false;

			for(int j = 0; j < players.Size(); j++)
			{
				if(compareFunc.Call(players[j], unsorted[i]) > 0)
				{
					players.Insert(j, unsorted[i]);
					inserted = true;
					break;
				}
			}

			if(!inserted)
			{
				players.Push(unsorted[i]);
			}
		}

    }

    virtual void Scoreboard_DrawScores(int playerNum, double ticFrac)
    {
	    if(deathmatch)
	    {
		    if(teamplay)
		    {
			    if(!sb_teamdeathmatch_enable)
				    return;
		    }
		    else if(!sb_deathmatch_enable)
		    {
			    return;
		    }
	    }
	    else if(!multiplayer || !sb_cooperative_enable)
	    {
		    return;
	    }

        if(!scoreboardFont) InitScoreboard();
        
        PlayerInfo player = players[playernum];

        if(player.camera && player.camera.player)
        {
            player = player.camera.player;
			playerNum = Level.PlayerNum(player);
        }
        
        Array<int> sortedPlayers;

        for(int i = 0; i < MAXPLAYERS; i++)
        {
            if(playeringame[i])
            {
                sortedPlayers.Push(i);
            }
        }

		if(teamplay && deathmatch)
		{
			Scoreboard_SortPlayers(sortedPlayers, Scoreboard_CompareByTeams);
        }
		else
		{
			Scoreboard_SortPlayers(sortedPlayers, Scoreboard_CompareByPoints);
        }

        int numPlayers = sortedPlayers.size();
        
	    int FontScale = max(screen.GetHeight() / 400, 1);

	    if(numPlayers > 8)
		    FontScale = int(ceil(FontScale * 0.75));
		
		Scoreboard_DoDrawScores(player, sortedPlayers, FontScale, ticFrac);
    }
	
	//==========================================================================
	//
	// HU_DrawFontScaled
	//
	//==========================================================================

	void Scoreboard_DrawFontScaled(double x, double y, Color color, String text, int FontScale)
	{
		screen.DrawText(scoreboardFont, color, x / FontScale, y / FontScale, text, DTA_VirtualWidth, screen.GetWidth() / FontScale, DTA_VirtualHeight, screen.GetHeight() / FontScale);
	}
	
	//==========================================================================
	//
	// HU_DoDrawScores
	//
	//==========================================================================
	
	virtual void Scoreboard_DoDrawScores(PlayerInfo player, Array<int> sortedPlayers, int FontScale, double ticFrac)
	{
		Color _color = sb_cooperative_headingcolor;
		if(deathmatch)
		{
			if (teamplay)
				_color = sb_teamdeathmatch_headingcolor;
			else
				_color = sb_deathmatch_headingcolor;
		}

		int maxNameWidth, maxScoreWidth, maxIconHeight;
		Scoreboard_GetPlayerWidths(maxNameWidth, maxScoreWidth, maxIconHeight);
		int height = scoreboardFont.GetHeight() * FontScale;
		int lineHeight = max(height, maxIconHeight * CleanYfac);
		int yPadding = (lineHeight - height + 1) / 2;

		int bottom = GetTopOfStatusbar();
		int y = max(48 * CleanYfac, (bottom - MAXPLAYERS * (height + CleanYfac + 1)) / 2);

		Scoreboard_DrawTimeRemaining(bottom - height, FontScale);

		Array<int> teamPlayerCounts;
		Array<int> teamScores;

		teamPlayerCounts.Resize(Teams.Size());
		teamScores.Resize(Teams.Size());

		if(teamplay && deathmatch)
		{
			y -= (BigFont.GetHeight() + 8) * CleanYfac;

			int numTeams = 0;
			for(int i = 0; i < MAXPLAYERS; ++i)
			{
				PlayerInfo p = players[sortedPlayers[i]];
				if (playeringame[sortedPlayers[i]] && Team.IsValid(p.GetTeam()))
				{
					if (teamPlayerCounts[p.GetTeam()]++ == 0)
						++numTeams;

					teamScores[p.GetTeam()] += p.fragcount;
				}
			}

			int scoreXWidth = screen.GetWidth() / max(8, numTeams);
			int numScores = 0;
			for(int i = 0; i < Teams.Size(); ++i)
			{
				if (teamPlayerCounts[i])
					++numScores;
			}

			int scoreX = (screen.GetWidth() - scoreXWidth * (numScores - 1)) / 2;
			for(int i = 0; i < Teams.Size(); ++i)
			{
				if (!teamPlayerCounts[i])
					continue;

				String score = String.Format("%d", teamScores[i]);

				screen.DrawText(BigFont, Teams[i].GetTextColor(),
					scoreX - BigFont.StringWidth(score)*CleanXfac/2, y, score,
					DTA_CleanNoMove, true);

				scoreX += scoreXWidth;
			}

			y += (BigFont.GetHeight() + 8) * CleanYfac;
		}

		String  text_color = StringTable.Localize("$SCORE_COLOR"),
				text_frags = StringTable.Localize(deathmatch ? "$SCORE_FRAGS" : "$SCORE_KILLS"),
				text_name = StringTable.Localize("$SCORE_NAME"),
				text_delay = StringTable.Localize("$SCORE_DELAY");

		int col2 = (scoreboardFont.StringWidth(text_color) + 16) * FontScale;
		int col3 = col2 + (scoreboardFont.StringWidth(text_frags) + 16) * FontScale;
		int col4 = col3 + maxScoreWidth * FontScale;
		int col5 = col4 + (maxNameWidth + 16) * FontScale;
		int x = (screen.GetWidth() >> 1) - (((scoreboardFont.StringWidth(text_delay) * FontScale) + col5) >> 1);

		//Scoreboard_DrawFontScaled(x, y, _color, text_color);
		Scoreboard_DrawFontScaled(x + col2, y, _color, text_frags, FontScale);
		Scoreboard_DrawFontScaled(x + col4, y, _color, text_name, FontScale);
		Scoreboard_DrawFontScaled(x + col5, y, _color, text_delay, FontScale);

		y += height + 6 * CleanYfac;
		bottom -= height;

		for(int i = 0; i < sortedPlayers.Size() && y <= bottom; ++i)
		{
			Scoreboard_DrawPlayer(players[sortedPlayers[i]], Level.PlayerNum(player) == sortedPlayers[i], x, col2, col3, col4, col5, maxNameWidth, y, yPadding, lineHeight, FontScale);
			y += lineHeight + CleanYfac;
		}
	}

	//==========================================================================
	//
	// HU_DrawTimeRemaining
	//
	//==========================================================================

	void Scoreboard_DrawTimeRemaining(int y, int FontScale)
	{
		if(deathmatch && timelimit && gamestate == GS_LEVEL)
		{
			int timeleft = int(timelimit * GameTicRate * 60) - Level.maptime;

			int hours, minutes, seconds;

			if(timeleft < 0) timeleft = 0;

			hours = timeleft / (GameTicRate * 3600);
			timeleft -= hours * GameTicRate * 3600;
			minutes = timeleft / (GameTicRate * 60);
			timeleft -= minutes * GameTicRate * 60;
			seconds = timeleft / GameTicRate;
			
			String str;
			if(hours)
			{
				str = String.Format("Level ends in %d:%02d:%02d", hours, minutes, seconds);
			}
			else
			{
				str = String.Format("Level ends in %d:%02d", minutes, seconds);
			}

			Scoreboard_DrawFontScaled(screen.GetWidth() / 2 - scoreboardFont.StringWidth(str) / 2 * FontScale, y, Font.CR_GRAY, str, FontScale);
		}
	}

	
	//==========================================================================
	//
	// HU_DrawPlayer
	//
	//==========================================================================
	
	void Scoreboard_DrawPlayer(PlayerInfo player, bool highlight, int col1, int col2, int col3, int col4, int col5, int maxnamewidth, int y, int ypadding, int height,  int FontScale)
	{
		String str;

		if(highlight)
		{
			// The teamplay mode uses colors to show teams, so we need some
			// other way to do highlighting. And it may as well be used for
			// all modes for the sake of consistancy.
			screen.Dim(Color(200,245,255), 0.125f, col1 - 12 * FontScale, y - 1, col5 + (maxnamewidth + 24) * FontScale, height + 2);
		}

		col2 += col1;
		col3 += col1;
		col4 += col1;
		col5 += col1;

		Color _color = Scoreboard_GetRowColor(player, highlight);
		Scoreboard_DrawColorBar(col1, y, height, player, FontScale);

		str = String.Format("%d", deathmatch ? player.fragcount : player.killcount);

		Scoreboard_DrawFontScaled(col2, y + ypadding, _color, player.playerstate == PST_DEAD && !deathmatch ? "DEAD" : str, FontScale);

		TextureID icon = player.mo.ScoreIcon;

		if(icon.isValid())
		{
			screen.DrawTexture(icon, false, col3, y, DTA_CleanNoMove, true);
		}

		Scoreboard_DrawFontScaled(col4, y + ypadding, _color, player.GetUserName(), FontScale);

		str = String.Format("%d", player.GetAverageLatency());

		Scoreboard_DrawFontScaled(col5, y + ypadding, _color, str, FontScale);

		int team = player.GetTeam();
		
		if(team != TEAM_NONE && teamplay && Teams[team].GetLogoName().IsNotEmpty())
		{
			TextureID pic = Teams[team].GetLogo();
			screen.DrawTexture(pic, col1 - (screen.GetTextureWidth(pic) + 2) * CleanXfac, y, DTA_CleanNoMove, true);
		}
	}
	
	//==========================================================================
	//
	// HU_DrawColorBar
	//
	//==========================================================================

	static void Scoreboard_DrawColorBar(int x, int y, int height, PlayerInfo player, int FontScale)
	{
		Screen.Clear(x, y, x + 24 * FontScale, y + height, player.GetDisplayColor());
	}
	
	//==========================================================================
	//
	// HU_GetRowColor
	//
	//==========================================================================

	static Color Scoreboard_GetRowColor(PlayerInfo player, bool highlight)
	{
		if(teamplay && deathmatch)
		{
			if(Team.IsValid(player.GetTeam()))
			{
				Color teamColor = Teams[player.GetTeam()].GetTextColor();
				return teamColor;
			}
			else
			{
				return Font.CR_GREY;
			}
		}
		else
		{
			if(!highlight)
			{
				if(demoplayback && Level.PlayerNum(player) == consoleplayer)
				{
					return Font.CR_GOLD;
				}
				else
				{
					return deathmatch ? sb_deathmatch_otherplayercolor : sb_cooperative_otherplayercolor;
				}
			}
			else
			{
				return deathmatch ? sb_deathmatch_yourplayercolor : sb_cooperative_yourplayercolor;
			}
		}
	}
	
	//==========================================================================
	//
	// HU_GetPlayerWidths
	//
	// Returns the widest player name and class icon.
	//
	//==========================================================================

	void Scoreboard_GetPlayerWidths(int& maxNameWidth, int& maxScoreWidth, int& maxIconHeight)
	{
        if(!scoreboardFont) InitScoreboard();

		maxNameWidth = scoreboardFont.StringWidth("Name");
		maxScoreWidth = 0;
		maxIconHeight = 0;

		for (int i = 0; i < MAXPLAYERS; ++i)
		{
			if (!playeringame[i])
				continue;

			int width = scoreboardFont.StringWidth(players[i].GetUserName(16));
			if (width > maxNameWidth)
				maxNameWidth = width;

			TextureID icon = players[i].mo.ScoreIcon;
			if (icon.isValid())
			{
				width = int(screen.GetTextureWidth(icon) - screen.GetTextureLeftOffset(icon) + 2.5);
				if (width > maxScoreWidth)
					maxScoreWidth = width;

				// The icon's top offset does not count toward its height, because
				// zdoom.pk3's standard Hexen class icons are designed that way.
				int height = int(screen.GetTextureHeight(icon) - screen.GetTextureTopOffset(icon) + 0.5);
				if (height > maxIconHeight)
					maxIconHeight = height;
			}
		}
	}

}