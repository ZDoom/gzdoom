
class StrifeStatusBar : BaseStatusBar
{
	
	// Number of tics to move the popscreen up and down.
	const POP_TIME = (Thinker.TICRATE/8);

	// Popscreen height when fully extended
	const POP_HEIGHT = 104;

	// Number of tics to scroll keys left
	const KEY_TIME = (Thinker.TICRATE/3);

	enum eImg
	{
		imgINVCURS,
		imgCURSOR01,
		imgINVPOP,
		imgINVPOP2,
		imgINVPBAK,
		imgINVPBAK2,
		imgFONY0,
		imgFONY1,
		imgFONY2,
		imgFONY3,
		imgFONY4,
		imgFONY5,
		imgFONY6,
		imgFONY7,
		imgFONY8,
		imgFONY9,
		imgFONY_PERCENT,
		imgNEGATIVE,
	};
	
	TextureID Images[imgNEGATIVE + 1];
	int CursorImage;
	int CurrentPop, PendingPop, PopHeight, PopHeightChange;
	int KeyPopPos, KeyPopScroll;
	
	HUDFont mYelFont, mGrnFont, mBigFont;

	override void Init()
	{
		static const Name strifeLumpNames[] =
		{
			"INVCURS", "CURSOR01", "INVPOP", "INVPOP2",
			"INVPBAK", "INVPBAK2",
			"INVFONY0", "INVFONY1", "INVFONY2", "INVFONY3", "INVFONY4",
			"INVFONY5", "INVFONY6", "INVFONY7", "INVFONY8", "INVFONY9",
			"INVFONY%", ""

		};
		
		Super.Init();
		SetSize(32, 320, 200);
		Reset();
		
		for(int i = 0; i <= imgNEGATIVE; i++)
		{
			Images[i] = TexMan.CheckForTexture(strifeLumpNames[i], TexMan.TYPE_MiscPatch);
		}

		CursorImage = Images[imgINVCURS].IsValid() ? imgINVCURS : imgCURSOR01;
		
		mYelFont = HUDFont.Create("Indexfont_Strife_Yellow", 7, true, 1, 1);
		mGrnFont = HUDFont.Create("Indexfont_Strife_Green", 7, true, 1, 1);
		mBigFont = HUDFont.Create("BigFont", 0, false, 2, 2);
	}

	override void NewGame ()
	{
		Super.NewGame();
		Reset ();
	}

	override int GetProtrusion(double scaleratio) const
	{
		return 10;
	}

	override void Draw (int state, double TicFrac)
	{
		Super.Draw (state, TicFrac);

		if (state == HUD_StatusBar)
		{
			BeginStatusBar();
			DrawMainBar (TicFrac);
		}
		else
		{
			if (state == HUD_Fullscreen)
			{
				BeginHUD();
				DrawFullScreenStuff ();
			}

			// Draw pop screen (log, keys, and status)
			if (CurrentPop != POP_None && PopHeight < 0)
			{
				// This uses direct low level draw commands and would otherwise require calling
				// BeginStatusBar(true);
				DrawPopScreen (screen.GetHeight(), TicFrac);
			}
		}
	}

	override void ShowPop (int popnum)
	{
		Super.ShowPop(popnum);
		if (popnum == CurrentPop)
		{
			if (popnum == POP_Keys)
			{
				Inventory item;

				KeyPopPos += 10;
				KeyPopScroll = 280;

				int i = 0;
				for (item = CPlayer.mo.Inv; item != NULL; item = item.Inv)
				{
					if (item is "Key")
					{
						if (i == KeyPopPos)
						{
							return;
						}
						i++;
					}
				}
			}
			PendingPop = POP_None;
			// Do not scroll keys horizontally when dropping the popscreen
			KeyPopScroll = 0;
			KeyPopPos -= 10;
		}
		else
		{
			KeyPopPos = 0;
			PendingPop = popnum;
		}
	}

	override bool MustDrawLog(int state)
	{
		// Tell the base class to draw the log if the pop screen won't be displayed.
		return false;
	}

	void Reset ()
	{
		CurrentPop = POP_None;
		PendingPop = POP_NoChange;
		PopHeight = 0;
		KeyPopPos = 0;
		KeyPopScroll = 0;
	}

	override void Tick ()
	{
		Super.Tick ();

		PopHeightChange = 0;
		if (PendingPop != POP_NoChange)
		{
			if (PopHeight < 0)
			{
				PopHeightChange = POP_HEIGHT / POP_TIME;
				PopHeight += POP_HEIGHT / POP_TIME;
			}
			else
			{
				CurrentPop = PendingPop;
				PendingPop = POP_NoChange;
			}
		}
		else
		{
			if (CurrentPop == POP_None)
			{
				PopHeight = 0;
			}
			else if (PopHeight > -POP_HEIGHT)
			{
				PopHeight -= POP_HEIGHT / POP_TIME;
				if (PopHeight < -POP_HEIGHT)
				{
					PopHeight = -POP_HEIGHT;
				}
				else
				{
					PopHeightChange = -POP_HEIGHT / POP_TIME;
				}
			}
			if (KeyPopScroll > 0)
			{
				KeyPopScroll -= 280 / KEY_TIME;
				if (KeyPopScroll < 0)
				{
					KeyPopScroll = 0;
				}
			}
		}
	}

	private void FillBar(double x, double y, double start, double stopp, Color color1, Color color2)
	{
		Fill(color1, x, y, (stopp-start)*2, 1);
		Fill(color2, x, y+1, (stopp-start)*2, 1);
	}
	
	protected void DrawHealthBar(int health, int x, int y)
	{
		Color green1 = Color(255, 180, 228, 128);	// light green
		Color green2 = Color(255, 128, 180, 80);	// dark green

		Color blue1 = Color(255, 196, 204, 252);	// light blue
		Color blue2 = Color(255, 148, 152, 200);	// dark blue

		Color gold1 = Color(255, 224, 188, 0);	// light gold
		Color gold2 = Color(255, 208, 128, 0);	// dark gold

		Color red1 = Color(255, 216, 44,  44);	// light red
		Color red2 = Color(255, 172, 28,  28);	// dark red
		
		if (health == 999)
		{
			FillBar (x, y, 0, 100, gold1, gold2);
		}
		else
		{
			if (health <= 100)
			{
				if (health <= 10)
				{
					FillBar (x, y, 0, health, red1, red2);
				}
				else if (health <= 20)
				{
					FillBar (x, y, 0, health, gold1, gold2);
				}
				else
				{
					FillBar (x, y, 0, health, green1, green2);
				}
				//FillBar (x, y, health, 100, 0, 0);
			}
			else
			{
				int stopp = 200 - health;
				FillBar (x, y, 0, stopp, green1, green2);
				FillBar (x, y, stopp, 100, blue1, blue2);
			}
		}
	}

	protected void DrawMainBar (double TicFrac)
	{
		Inventory item;
		int i;

		// Pop screen (log, keys, and status)
		if (CurrentPop != POP_None && PopHeight < 0)
		{
			double tmp, h;
			[tmp, tmp, h] = StatusbarToRealCoords(0, 0, 8);
			DrawPopScreen (int(GetTopOfStatusBar() - h), TicFrac);
		}

		DrawImage("INVBACK", (0, 168), DI_ITEM_OFFSETS);
		DrawImage("INVTOP", (0, 160), DI_ITEM_OFFSETS);

		// Health
		DrawString(mGrnFont, FormatNumber(CPlayer.health, 3, 5), (79, 162), DI_TEXT_ALIGN_RIGHT);
		int points;
		if (CPlayer.cheats & CF_GODMODE)
		{
			points = 999;
		}
		else
		{
			points = min(CPlayer.health, 200);
		}
		DrawHealthBar (points, 49, 172);
		DrawHealthBar (points, 49, 175);

		// Armor
		item = CPlayer.mo.FindInventory('BasicArmor');
		if (item != NULL && item.Amount > 0)
		{
			DrawInventoryIcon(item, (2, 177), DI_ITEM_OFFSETS);
			DrawString(mYelFont, FormatNumber(item.Amount, 3, 5), (27, 191), DI_TEXT_ALIGN_RIGHT);
		}

		// Ammo
		Inventory ammo1 = GetCurrentAmmo ();
		if (ammo1 != NULL)
		{
			DrawString(mGrnFont, FormatNumber(ammo1.Amount, 3, 5), (311, 162), DI_TEXT_ALIGN_RIGHT);
			DrawInventoryIcon (ammo1, (290, 181), DI_ITEM_OFFSETS);
		}

		// Sigil
		item = CPlayer.mo.FindInventory('Sigil');
		if (item != NULL)
		{
			DrawInventoryIcon (item, (253, 175), DI_ITEM_OFFSETS);
		}

		// Inventory
		CPlayer.inventorytics = 0;
		CPlayer.mo.InvFirst = ValidateInvFirst (6);
		i = 0;
		for (item = CPlayer.mo.InvFirst; item != NULL && i < 6; item = item.NextInv())
		{
			int flags = item.Amount <= 0? DI_ITEM_OFFSETS|DI_DIM : DI_ITEM_OFFSETS;
			if (item == CPlayer.mo.InvSel)
			{
				DrawTexture (Images[CursorImage], (42 + 35*i, 180), flags, 1. - itemflashFade);
			}
			DrawInventoryIcon (item, (48 + 35*i, 182), flags);
			DrawString(mYelFont, FormatNumber(item.Amount, 3, 5), (75 + 35*i, 191), DI_TEXT_ALIGN_RIGHT);
			i++;
		}
	}

	protected void DrawFullScreenStuff ()
	{
		// Draw health (use red color if health is below the run health threashold.)
		DrawString(mGrnFont, FormatNumber(CPlayer.health, 3), (4, -10), DI_TEXT_ALIGN_LEFT, (CPlayer.health < CPlayer.mo.RunHealth)? Font.CR_BRICK : Font.CR_UNTRANSLATED);
		DrawImage("I_MDKT", (14, -17));

		// Draw armor
		let armor = CPlayer.mo.FindInventory('BasicArmor');
		if (armor != NULL && armor.Amount != 0)
		{
			DrawString(mYelFont, FormatNumber(armor.Amount, 3), (35, -10));
			DrawInventoryIcon(armor, (45, -17));
		}

		// Draw ammo
		Inventory ammo1, ammo2;
		int ammocount1, ammocount2;

		[ammo1, ammo2, ammocount1, ammocount2] = GetCurrentAmmo ();
		if (ammo1 != NULL)
		{
			// Draw primary ammo in the bottom-right corner
			DrawString(mGrnFont, FormatNumber(ammo1.Amount, 3), (-23, -10));
			DrawInventoryIcon(ammo1, (-14, -17));
			if (ammo2 != NULL && ammo1!=ammo2)
			{
				// Draw secondary ammo just above the primary ammo
				DrawString(mGrnFont, FormatNumber(ammo1.Amount, 3), (-23, -48));
				DrawInventoryIcon(ammo1, (-14, -55));
			}
		}

		if (deathmatch)
		{ // Draw frags (in DM)
			DrawString(mBigFont, FormatNumber(CPlayer.FragCount, 3), (4, 1));
		}

		// Draw inventory
		if (CPlayer.inventorytics == 0)
		{
			if (CPlayer.mo.InvSel != null)
			{
				if (itemflashFade > 0)
				{
					DrawTexture(Images[CursorImage], (-42, -15));
				}
				DrawString(mYelFont, FormatNumber(CPlayer.mo.InvSel.Amount, 3, 5), (-30, -10), DI_TEXT_ALIGN_RIGHT);
				DrawInventoryIcon(CPlayer.mo.InvSel, (-42, -17), DI_DIMDEPLETED);
			}
		}
		else
		{
			CPlayer.mo.InvFirst = ValidateInvFirst (6);
			int i = 0;
			Inventory item;
			Vector2 box = TexMan.GetScaledSize(Images[CursorImage]) - (4, 4);	// Fit oversized icons into the box.
			if (CPlayer.mo.InvFirst != NULL)
			{
				for (item = CPlayer.mo.InvFirst; item != NULL && i < 6; item = item.NextInv())
				{
					if (item == CPlayer.mo.InvSel)
					{
						DrawTexture(Images[CursorImage], (-90+i*35, -3), DI_SCREEN_CENTER_BOTTOM, 0.75);
					}
					if (item.Icon.isValid())
					{
						DrawInventoryIcon(item, (-90+i*35, -5), DI_SCREEN_CENTER_BOTTOM|DI_DIMDEPLETED, 0.75);
					}
					DrawString(mYelFont, FormatNumber(item.Amount, 3, 5), (-72 + i*35, -8), DI_TEXT_ALIGN_RIGHT|DI_SCREEN_CENTER_BOTTOM);
					++i;
				}
			}
		}
	}

	protected void DrawPopScreen (int bottom, double TicFrac)
	{
		String buff;
		String label;
		int i;
		Inventory item;
		int xscale, yscale, left, top;
		int bars = (CurrentPop == POP_Status) ? imgINVPOP : imgINVPOP2;
		int back = (CurrentPop == POP_Status) ? imgINVPBAK : imgINVPBAK2;
		// Extrapolate the height of the popscreen for smoother movement
		int height = clamp (PopHeight + int(TicFrac * PopHeightChange), -POP_HEIGHT, 0);

		xscale = CleanXfac;
		yscale = CleanYfac;
		left = screen.GetWidth()/2 - 160*CleanXfac;
		top = bottom + height * yscale;

		screen.DrawTexture (Images[back], true, left, top, DTA_CleanNoMove, true, DTA_Alpha, 0.75);
		screen.DrawTexture (Images[bars], true, left, top, DTA_CleanNoMove, true);


		switch (CurrentPop)
		{
		case POP_Log:
		{
			// Draw the latest log message.
			screen.DrawText(SmallFont2, Font.CR_UNTRANSLATED, left + 210 * xscale, top + 8 * yscale, Level.TimeFormatted(),
				DTA_CleanNoMove, true);

			if (CPlayer.LogText.Length() > 0)
			{
				let text = Stringtable.Localize(CPlayer.LogText);
				BrokenLines lines = SmallFont2.BreakLines(text, 272);
				for (i = 0; i < lines.Count(); ++i)
				{
					screen.DrawText(SmallFont2, Font.CR_UNTRANSLATED, left + 24 * xscale, top + (18 + i * 12)*yscale,
						lines.StringAt(i), DTA_CleanNoMove, true);
				}
			}
			break;
		}

		case POP_Keys:
			// List the keys the player has.
			int pos, endpos, leftcol;
			int clipleft, clipright;
			
			pos = KeyPopPos;
			endpos = pos + 10;
			leftcol = 20;
			clipleft = left + 17*xscale;
			clipright = left + (320-17)*xscale;
			if (KeyPopScroll > 0)
			{
				// Extrapolate the scroll position for smoother scrolling
				int scroll = MAX (0, KeyPopScroll - int(TicFrac * (280./KEY_TIME)));
				pos -= 10;
				leftcol = leftcol - 280 + scroll;
			}
			i = 0;
			for (item = CPlayer.mo.Inv; i < endpos && item != NULL; item = item.Inv)
			{
				if (!(item is "Key"))
					continue;
				
				if (i < pos)
				{
					i++;
					continue;
				}

				label = item.GetTag();

				int colnum = ((i-pos) / 5) & (KeyPopScroll > 0 ? 3 : 1);
				int rownum = (i % 5) * 18;

				screen.DrawTexture (item.Icon, true,
					left + (colnum * 140 + leftcol)*xscale,
					top + (6 + rownum)*yscale,
					DTA_CleanNoMove, true,
					DTA_ClipLeft, clipleft,
					DTA_ClipRight, clipright);
				screen.DrawText (SmallFont2, Font.CR_UNTRANSLATED,
					left + (colnum * 140 + leftcol + 17)*xscale,
					top + (11 + rownum)*yscale,
					label,
					DTA_CleanNoMove, true,
					DTA_ClipLeft, clipleft,
					DTA_ClipRight, clipright);
				i++;
			}
			break;

		case POP_Status:
			// Show miscellaneous status items.
			
			// Print stats
			DrINumber2 (CPlayer.mo.accuracy, left+268*xscale, top+28*yscale, 7*xscale, imgFONY0);
			DrINumber2 (CPlayer.mo.stamina, left+268*xscale, top+52*yscale, 7*xscale, imgFONY0);

			// How many keys does the player have?
			i = 0;
			for (item = CPlayer.mo.Inv; item != NULL; item = item.Inv)
			{
				if (item is "Key")
				{
					i++;
				}
			}
			DrINumber2 (i, left+268*xscale, top+76*yscale, 7*xscale, imgFONY0);

			// Does the player have a communicator?
			item = CPlayer.mo.FindInventory ("Communicator");
			if (item != NULL)
			{
				screen.DrawTexture (item.Icon, true,
					left + 280*xscale,
					top + 74*yscale,
					DTA_CleanNoMove, true);
			}

			// How much ammo does the player have?
			static const class<Ammo> AmmoList[] = {
				"ClipOfBullets",			
				"PoisonBolts",				
				"ElectricBolts",			
				"HEGrenadeRounds",			
				"PhosphorusGrenadeRounds",	
				"MiniMissiles",			
				"EnergyPod"};				
				
			static const int AmmoY[] = {19, 35, 43, 59, 67, 75, 83};
			
			for (i = 0; i < 7; ++i)
			{
				item = CPlayer.mo.FindInventory (AmmoList[i]);

				if (item == NULL)
				{
					DrINumber2 (0, left+206*xscale, top+AmmoY[i] * yscale, 7*xscale, imgFONY0);
					DrINumber2 (GetDefaultByType(AmmoList[i]).MaxAmount, left+239*xscale, top+AmmoY[i] * yscale, 7*xscale, imgFONY0);
				}
				else
				{
					DrINumber2 (item.Amount, left+206*xscale, top+AmmoY[i] * yscale, 7*xscale, imgFONY0);
					DrINumber2 (item.MaxAmount, left+239*xscale, top+AmmoY[i] * yscale, 7*xscale, imgFONY0);
				}
			}
					

			// What weapons does the player have?
			static const class<Weapon> WeaponList[] = 
			{
				"StrifeCrossbow",
				"AssaultGun",
				"FlameThrower",
				"MiniMissileLauncher",
				"StrifeGrenadeLauncher",
				"Mauler"
			};
			static const int WeaponX[] = {23, 21, 57, 20, 55, 52};
			static const int WeaponY[] = {19, 41, 50, 64, 20, 75};

			for (i = 0; i < 6; ++i)
			{
				item = CPlayer.mo.FindInventory (WeaponList[i]);
				if (item != NULL)
				{
					screen.DrawTexture (item.Icon, true,
						left + WeaponX[i] * xscale,
						top + WeaponY[i] * yscale,
						DTA_CleanNoMove, true,
						DTA_LeftOffset, 0,
						DTA_TopOffset, 0);
				}
			}
			break;
		}
	}

	void DrINumber2 (int val, int x, int y, int width, int imgBase) const
	{
		x -= width;

		if (val == 0)
		{
			screen.DrawTexture (Images[imgBase], true, x, y, DTA_CleanNoMove, true);
		}
		else
		{
			while (val != 0)
			{
				screen.DrawTexture (Images[imgBase+val%10], true, x, y, DTA_CleanNoMove, true);
				val /= 10;
				x -= width;
			}
		}
	}
}

