/*
** Enhanced heads up 'overlay' for fullscreen
**
**---------------------------------------------------------------------------
** Copyright 2003-2008 Christoph Oelckers
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

class AltHud ui
{
	TextureID tnt1a0;
	TextureID invgem_left, invgem_right;
	TextureID fragpic;
	int hudwidth, hudheight;
	int statspace;
	Font HudFont;					// The font for the health and armor display
	Font IndexFont;					// The font for the inventory indices
	Array< Class<Ammo> > orderedammos;
	const POWERUPICONSIZE = 32;

	
	virtual void Init()
	{
		switch (gameinfo.gametype)
		{
		case GAME_Heretic:
		case GAME_Hexen:
			HudFont = Font.FindFont("HUDFONT_RAVEN");
			break;

		case GAME_Strife:
			HudFont = BigFont;	// Strife doesn't have anything nice so use the standard font
			break;

		default:
			HudFont = Font.FindFont("HUDFONT_DOOM");
			break;
		}

		IndexFont = Font.GetFont("INDEXFONT");

		if (HudFont == NULL) HudFont = BigFont;
		if (IndexFont == NULL) IndexFont = ConFont;	// Emergency fallback

		invgem_left = TexMan.CheckForTexture("INVGEML1", TexMan.Type_MiscPatch);
		invgem_right = TexMan.CheckForTexture("INVGEMR1", TexMan.Type_MiscPatch);
		tnt1a0 = TexMan.CheckForTexture("TNT1A0", TexMan.Type_Sprite);
		fragpic = TexMan.CheckForTexture("HU_FRAGS", TexMan.Type_MiscPatch);
		statspace = SmallFont.StringWidth("Ac:");
	}
	
	//---------------------------------------------------------------------------
	//
	// Draws an image into a box with its bottom center at the bottom
	// center of the box. The image is scaled down if it doesn't fit
	//
	//---------------------------------------------------------------------------

	void DrawImageToBox(TextureID tex, int x, int y, int w, int h, double trans = 0.75, bool animate = false)
	{
		double scale1, scale2;

		if (tex)
		{
			let texsize = TexMan.GetScaledSize(tex);

			if (w < texsize.X) scale1 = w / texsize.X;
			else scale1 = 1.0;
			if (h < texsize.Y) scale2 = h / texsize.Y;
			else scale2 = 1.0;
			scale1 = min(scale1, scale2);
			if (scale2 < scale1) scale1=scale2;

			x += w >> 1;
			y += h;

			w = (int)(texsize.X * scale1);
			h = (int)(texsize.Y * scale1);

			screen.DrawTexture(tex, animate, x, y,
				DTA_KeepRatio, true,
				DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, DTA_Alpha, trans, 
				DTA_DestWidth, w, DTA_DestHeight, h, DTA_CenterBottomOffset, 1);

		}
	}

	
	//---------------------------------------------------------------------------
	//
	// Draws a text but uses a fixed width for all characters
	//
	//---------------------------------------------------------------------------

	void DrawHudText(Font fnt, int color, String text, int x, int y, double trans = 0.75)
	{
		int zerowidth = fnt.GetCharWidth("0");
		screen.DrawText(fnt, color, x, y-fnt.GetHeight(), text, DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight,
			 DTA_KeepRatio, true, DTA_Alpha, trans, DTA_Monospace, MONO_CellCenter, DTA_Spacing, zerowidth);
	}


	//---------------------------------------------------------------------------
	//
	// Draws a number with a fixed width for all digits
	//
	//---------------------------------------------------------------------------

	void DrawHudNumber(Font fnt, int color, int num, int x, int y, double trans = 0.75)
	{
		DrawHudText(fnt, color, String.Format("%d", num), x, y, trans);
	}

	//---------------------------------------------------------------------------
	//
	// Draws a time string as hh:mm:ss
	//
	//---------------------------------------------------------------------------

	virtual void DrawTimeString(Font fnt, int color, int timer, int x, int y, double trans = 0.75)
	{
		let seconds = Thinker.Tics2Seconds(timer);
		String s = String.Format("%02i:%02i:%02i", seconds / 3600, (seconds % 3600) / 60, seconds % 60);
		int length = 8 * fnt.GetCharWidth("0");
		DrawHudText(fnt, color, s, x-length, y, trans);
	}
	
	//===========================================================================
	//
	// draw the status (number of kills etc)
	//
	//===========================================================================

	virtual void DrawStatLine(int x, in out int y, String prefix, String text)
	{
		y -= SmallFont.GetHeight()-1;
		screen.DrawText(SmallFont, hudcolor_statnames, x, y, prefix, 
			DTA_KeepRatio, true,
			DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, DTA_Alpha, 0.75);

		screen.DrawText(SmallFont, hudcolor_stats, x+statspace, y, text,
			DTA_KeepRatio, true,
			DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, DTA_Alpha, 0.75);
	}

	virtual void DrawStatus(PlayerInfo CPlayer, int x, int y)
	{
		let mo = CPlayer.mo;
		if (hud_showscore)
		{
			DrawStatLine(x, y, "Sc:", String.Format("%d ", mo.Score));
		}
		
		if (hud_showstats)
		{
			DrawStatLine(x, y, "Ac:", String.Format("%i ", mo.accuracy));
			DrawStatLine(x, y, "St:", String.Format("%i ", mo.stamina));
		}
		
		if (!deathmatch)
		{
			if (hud_showsecrets)
			{
				DrawStatLine(x, y, "S:", multiplayer
					? String.Format("%i/%i/%i ", CPlayer.secretcount, Level.found_secrets, Level.total_secrets)
					: String.Format("%i/%i ", Level.found_secrets, Level.total_secrets));
			}
			
			if (hud_showitems)
			{
				DrawStatLine(x, y, "I:", multiplayer
					? String.Format("%i/%i/%i ", CPlayer.itemcount, Level.found_items, Level.total_items)
					: String.Format("%i/%i ", Level.found_items, Level.total_items));
			}
			
			if (hud_showmonsters)
			{
				DrawStatLine(x, y, "K:", multiplayer
					? String.Format("%i/%i/%i ", CPlayer.killcount, Level.killed_monsters, Level.total_monsters)
					: String.Format("%i/%i ", Level.killed_monsters, Level.total_monsters));
			}

			if (hud_showtimestat)
			{
				String s;
				let seconds = Thinker.Tics2Seconds(level.time);
				if (seconds >= 3600)
					s = String.Format("%02i:%02i:%02i", seconds / 3600, (seconds % 3600) / 60, seconds % 60);
				else
					s = String.Format("%02i:%02i", seconds / 60, seconds % 60);
				DrawStatLine(x, y, "T:", s);
			}

		}
	}

	//===========================================================================
	//
	// draw health
	//
	//===========================================================================

	virtual void DrawHealth(PlayerInfo CPlayer, int x, int y)
	{
		int health = CPlayer.health;

		// decide on the color first
		int fontcolor =
			health < hud_health_red ? Font.CR_RED :
			health < hud_health_yellow ? Font.CR_GOLD :
			health <= hud_health_green ? Font.CR_GREEN :
			Font.CR_BLUE;

		bool haveBerserk = hud_berserk_health
			&& !gameinfo.berserkpic.IsNull()
			&& CPlayer.mo.FindInventory('PowerStrength');

		DrawImageToBox(haveBerserk ? gameinfo.berserkpic : gameinfo.healthpic, x, y, 31, 17);
		DrawHudNumber(HudFont, fontcolor, health, x + 33, y + 17);
	}

	//===========================================================================
	//
	// Draw Armor.
	// very similar to drawhealth, but adapted to handle Hexen armor too
	//
	//===========================================================================

	virtual void DrawArmor(BasicArmor barmor, HexenArmor harmor, int x, int y)
	{
		int ap = 0;
		int bestslot = 4;

		if (harmor)
		{
			let ac = (harmor.Slots[0] + harmor.Slots[1] + harmor.Slots[2] + harmor.Slots[3] + harmor.Slots[4]);
			ap += int(ac);
			
			if (ac)
			{
				// Find the part of armor that protects the most
				bestslot = 0;
				for (int i = 1; i < 4; ++i)
				{
					if (harmor.Slots[i] > harmor.Slots[bestslot])
					{
						bestslot = i;
					}
				}
			}
		}

		if (barmor)
		{
			ap += barmor.Amount;
		}

		if (ap)
		{
			// decide on color
			int fontcolor =
				ap < hud_armor_red ? Font.CR_RED :
				ap < hud_armor_yellow ? Font.CR_GOLD :
				ap <= hud_armor_green ? Font.CR_GREEN :
				Font.CR_BLUE;


			// Use the sprite of one of the predefined Hexen armor bonuses.
			// This is not a very generic approach, but it is not possible
			// to truly create new types of Hexen armor bonus items anyway.
			if (harmor && bestslot < 4)
			{
				static const String harmorIcons[] = { "AR_1A0", "AR_2A0", "AR_3A0", "AR_4A0" };
				DrawImageToBox(TexMan.CheckForTexture(harmorIcons[bestslot], TexMan.Type_Sprite), x, y, 31, 17);
			}
			else if (barmor) DrawImageToBox(barmor.Icon, x, y, 31, 17);
			DrawHudNumber(HudFont, fontcolor, ap, x + 33, y + 17);
		}
	}

	//===========================================================================
	//
	// KEYS
	//
	//===========================================================================

	//---------------------------------------------------------------------------
	//
	// Draw one key
	//
	// Regarding key icons, Doom's are too small, Heretic doesn't have any,
	// for Hexen the in-game sprites look better and for Strife it doesn't matter
	// so always use the spawn state's sprite instead of the icon here unless an
	// override is specified in ALTHUDCF.
	//
	//---------------------------------------------------------------------------

	virtual bool DrawOneKey(int xo, int x, int y, in out int c, Key inv)
	{
		TextureID icon;
		
		if (!inv) return false;
		
		TextureID AltIcon = inv.AltHUDIcon;
		if (!AltIcon.Exists()) return false;	// Setting a non-existent AltIcon hides this key.

		if (AltIcon.isValid()) 
		{
			icon = AltIcon;
		}
		else if (inv.SpawnState && inv.SpawnState.sprite!=0)
		{
			let state = inv.SpawnState;
			if (state != null) icon = state.GetSpriteTexture(0);
			else icon.SetNull();
		}
		// missing sprites map to TNT1A0. So if that gets encountered, use the default icon instead.
		if (icon.isNull() || icon == tnt1a0) icon = inv.Icon; 

		if (icon.isValid())
		{
			DrawImageToBox(icon, x, y, 8, 10);
			return true;
		}
		return false;
	}

	//---------------------------------------------------------------------------
	//
	// Draw all keys
	//
	//---------------------------------------------------------------------------

	virtual int DrawKeys(PlayerInfo CPlayer, int x, int y)
	{
		int yo = y;
		int xo = x;
		int i;
		int c = 0;
		Key inv;

		if (!deathmatch)
		{
			int count = Key.GetKeyTypeCount();
			
			// Go through the key in reverse order of definition, because we start at the right.
			for(int i = count-1; i >= 0; i--)
			{
				if ((inv = Key(CPlayer.mo.FindInventory(Key.GetKeyType(i)))))
				{
					if (DrawOneKey(xo, x - 9, y, c, inv))
					{
						x -= 9;
						if (++c >= 10)
						{
							x = xo;
							y -= 11;
							c = 0;
						}
					}
				}
			}
		}
		if (x == xo && y != yo) y += 11;	// undo the last wrap if the current line is empty.
		return y - 11;
	}

	//---------------------------------------------------------------------------
	//
	// Drawing Ammo helpers
	//
	//---------------------------------------------------------------------------

	void AddAmmoToList(readonly<Weapon> weapdef)
	{
		for (int i = 0; i < 2; i++)
		{
			let ti = i == 0? weapdef.AmmoType1 : weapdef.AmmoType2;
			if (ti)
			{
				let ammodef = GetDefaultByType(ti);

				if (ammodef && !ammodef.bInvBar)
				{
					if (orderedAmmos.Find(ti) == orderedAmmos.Size())
					{	
						orderedammos.Push(ti);					
					}
				}
			}
		}
	}

	static int GetDigitCount(int value)
	{
		int digits = 0;

		do
		{
			value /= 10;
			++digits;
		}
		while (0 != value);

		return digits;
	}

	int, int GetAmmoTextLengths(PlayerInfo CPlayer)
	{
		int tammomax = 0, tammocur = 0;
		for(int i = 0; i < orderedammos.Size(); i++)
		{
			let type = orderedammos[i];
			let ammoitem = CPlayer.mo.FindInventory(type);
			int ammomax, ammocur;
			if (ammoitem == null) 
			{
				ammomax = GetDefaultByType(type).MaxAmount;
				ammocur = 0;
			}
			else
			{
				ammomax = ammoitem.MaxAmount;
				ammocur = ammoItem.Amount;
			}

			tammocur = MAX(ammocur, tammocur);
			tammomax = MAX(ammomax, tammomax);
		}
		return GetDigitCount(tammocur), GetDigitCount(tammomax);
	}

	//---------------------------------------------------------------------------
	//
	// Drawing Ammo 
	//
	//---------------------------------------------------------------------------

	virtual int DrawAmmo(PlayerInfo CPlayer, int x, int y)
	{

		int i,j,k;
		String buf;
		Inventory inv;

		let wi = CPlayer.ReadyWeapon;

		orderedammos.Clear();

		if (0 == hud_showammo)
		{
			// Show ammo for current weapon if any
			if (wi) AddAmmoToList(wi.default);
		}
		else
		{
			// Order ammo by use of weapons in the weapon slots
			for (k = 0; k < PlayerPawn.NUM_WEAPON_SLOTS; k++) 
			{
				int slotsize = CPlayer.weapons.SlotSize(k);
				for(j = 0; j < slotsize; j++)
				{
					let weap = CPlayer.weapons.GetWeapon(k, j);

					if (weap)
					{
						// Show ammo for available weapons if hud_showammo CVAR is 1
						// or show ammo for all weapons if hud_showammo is greater than 1
						
						if (hud_showammo > 1 || CPlayer.mo.FindInventory(weap))
						{
							AddAmmoToList(GetDefaultByType(weap));
						}
					}
				}
			}

			// Now check for the remaining weapons that are in the inventory but not in the weapon slots
			for(inv = CPlayer.mo.Inv; inv; inv = inv.Inv)
			{
				let weap = Weapon(inv);
				if (weap)
				{
					AddAmmoToList(weap.default);
				}
			}
		}

		// ok, we got all ammo types. Now draw the list back to front (bottom to top)

		int ammocurlen = 0;
		int ammomaxlen = 0;
		[ammocurlen, ammomaxlen] = GetAmmoTextLengths(CPlayer);

		//buf = String.Format("%0d/%0d", 0, 0);
		buf = String.Format("%0*d/%0*d", ammocurlen, 0, ammomaxlen, 0);

		int def_width = ConFont.StringWidth(buf);
		int yadd = ConFont.GetHeight();

		int xtext = x - def_width;
		int ximage = x;

		if (hud_ammo_order > 0)
		{
			xtext -= 24;
			ximage -= 20;
		}
		else
		{
			ximage -= def_width + 20;
		}

		for(i = orderedammos.Size() - 1; i >= 0; i--)
		{
			let type = orderedammos[i];
			let ammoitem = CPlayer.mo.FindInventory(type);
			let inv = GetDefaultByType(type);

			let AltIcon = inv.AltHUDIcon;
			int maxammo = ammoitem? ammoitem.MaxAmount : inv.MaxAmount;

			let icon = !AltIcon.isNull()? AltIcon : inv.Icon;
			if (!icon.isValid()) continue;

			double trans= (wi && (type == wi.AmmoType1 || type == wi.AmmoType2)) ? 0.75 : 0.375;

			int ammo = ammoitem? ammoitem.Amount : 0;

			// buf = String.Format("%d/%d", ammo, maxammo);
			buf = String.Format("%*d/%*d", ammocurlen, ammo, ammomaxlen, maxammo);

			int tex_width= clamp(ConFont.StringWidth(buf) - def_width, 0, 1000);

			int fontcolor=( !maxammo ? Font.CR_GRAY :    
							 ammo < ( (maxammo * hud_ammo_red) / 100) ? Font.CR_RED :   
							 ammo < ( (maxammo * hud_ammo_yellow) / 100) ? Font.CR_GOLD : Font.CR_GREEN );

			DrawHudText(ConFont, fontcolor, buf, xtext-tex_width, y+yadd, trans);
			DrawImageToBox(icon, ximage, y, 16, 8, trans);
			y-=10;
		}
		return y;
	}

	//---------------------------------------------------------------------------
	//
	// Drawing weapons
	//
	//---------------------------------------------------------------------------

	virtual void DrawOneWeapon(PlayerInfo CPlayer, int x, in out int y, Weapon weapon)
	{
		double trans;

		// Powered up weapons and inherited sister weapons are not displayed.
		if (weapon.bPOWERED_UP) return;
		let SisterWeapon = weapon.SisterWeapon;
		if (SisterWeapon && (weapon is SisterWeapon.GetClass())) return;

		trans=0.4;
		let ReadyWeapon = CPlayer.ReadyWeapon;
		if (ReadyWeapon)
		{
			if (weapon == CPlayer.ReadyWeapon || SisterWeapon == CPlayer.ReadyWeapon) trans = 0.85;
		}

		TextureID picnum = StatusBar.GetInventoryIcon(weapon, StatusBar.DI_ALTICONFIRST);

		if (picnum.isValid())
		{
			// don't draw tall sprites too small.
			int w, h;
			[w, h] = TexMan.GetSize(picnum);
			int rh;
			if (w > h) rh = 8;
			else 
			{
				rh = 16;
				y -= 8;	
			}
			DrawImageToBox(picnum, x-24, y, 20, rh, trans);
			y-=10;
		}
	}


	virtual void DrawWeapons(PlayerInfo CPlayer, int x, int y)
	{
		int k,j;
		Inventory inv;

		// First draw all weapons in the inventory that are not assigned to a weapon slot
		for(inv = CPlayer.mo.Inv; inv; inv = inv.Inv)
		{
			let weap = Weapon(inv);
			if (weap && 
				!CPlayer.weapons.LocateWeapon(weap.GetClass()))
			{
				DrawOneWeapon(CPlayer, x, y, weap);
			}
		}

		// And now everything in the weapon slots back to front
		for (k = PlayerPawn.NUM_WEAPON_SLOTS - 1; k >= 0; k--) for(j = CPlayer.weapons.SlotSize(k) - 1; j >= 0; j--)
		{
			let weap = CPlayer.weapons.GetWeapon(k, j);
			if (weap) 
			{
				let weapitem = Weapon(CPlayer.mo.FindInventory(weap));
				if (weapitem) 
				{
					DrawOneWeapon(CPlayer, x, y, weapitem);
				}
			}
		}
	}

	//---------------------------------------------------------------------------
	//
	// Draw the Inventory
	//
	//---------------------------------------------------------------------------

	virtual void DrawInventory(PlayerInfo CPlayer, int x,int y)
	{
		Inventory rover;
		int numitems = (hudwidth - 2*x) / 32;
		int i;

		CPlayer.mo.InvFirst = rover = StatusBar.ValidateInvFirst(numitems);
		if (rover!=NULL)
		{
			if(rover.PrevInv())
			{
				screen.DrawTexture(invgem_left, true, x-10, y,
					DTA_KeepRatio, true,
					DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, DTA_Alpha, 0.4);
			}

			for(i = 0; i < numitems && rover; rover = rover.NextInv())
			{
				if (rover.Amount > 0)
				{
					let AltIcon = rover.AltHUDIcon;

					if (AltIcon.Exists() && (rover.Icon.isValid() || AltIcon.isValid()) )
					{
						double trans = rover == CPlayer.mo.InvSel ? 1.0 : 0.4;

						DrawImageToBox(AltIcon.isValid()? AltIcon : rover.Icon, x, y, 19, 25, trans, true);
						if (rover.Amount > 1)
						{
							int xx;
							String buffer = String.Format("%d", rover.Amount);
							if (rover.Amount >= 1000) xx = 32 - IndexFont.StringWidth(buffer);
							else xx = 22;

							screen.DrawText(IndexFont, Font.CR_GOLD, x+xx, y+20, buffer, 
								DTA_KeepRatio, true,
								DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, DTA_Alpha, trans);
						}
						
						x+=32;
						i++;
					}
				}
			}
			if(rover)
			{
				screen.DrawTexture(invgem_right, true, x-10, y,
					DTA_KeepRatio, true,
					DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, DTA_Alpha, 0.4);
			}
		}
	}

	//---------------------------------------------------------------------------
	//
	// Draw the Frags
	//
	//---------------------------------------------------------------------------

	virtual void DrawFrags(PlayerInfo CPlayer, int x, int y)
	{
		DrawImageToBox(fragpic, x, y, 31, 17);
		DrawHudNumber(HudFont, Font.CR_GRAY, CPlayer.fragcount, x + 33, y + 17);
	}
	
	//---------------------------------------------------------------------------
	//
	// PROC DrawCoordinates
	//
	//---------------------------------------------------------------------------
	
	void DrawCoordinateEntry(int xpos, int ypos, String coordstr)
	{
		screen.DrawText(SmallFont, hudcolor_xyco, xpos, ypos, coordstr,
						 DTA_KeepRatio, true,
						 DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight);
	}

	virtual void DrawCoordinates(PlayerInfo CPlayer, bool withmapname)
	{
		Vector3 pos;
		String coordstr;
		int h = SmallFont.GetHeight();
		let mo = CPlayer.mo;
		
		if (!map_point_coordinates || !automapactive) 
		{
			pos = mo.Pos;
		}
		else 
		{
			pos.xy = Level.GetAutomapPosition();
			pos.z = Level.PointInSector(pos.xy).floorplane.ZatPoint(pos.xy);
		}

		int xpos = hudwidth - SmallFont.StringWidth("X: -00000")-6;
		int ypos = 18;

		if (withmapname)
		{
			let font = generic_ui? NewSmallFont : SmallFont.CanPrint(Level.LevelName)? SmallFont : OriginalSmallFont;
			int hh = font.GetHeight();

			screen.DrawText(font, hudcolor_titl, hudwidth - 6 - font.StringWidth(Level.MapName), ypos, Level.MapName,
				DTA_KeepRatio, true,
				DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight);

			screen.DrawText(font, hudcolor_titl, hudwidth - 6 - font.StringWidth(Level.LevelName), ypos + hh, Level.LevelName,
				DTA_KeepRatio, true,
				DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight);
	
			ypos += 2 * hh + h;
		}
		
		DrawCoordinateEntry(xpos, ypos, String.Format("X: %.0f", pos.X));
		ypos += h;
		DrawCoordinateEntry(xpos, ypos, String.Format("Y: %.0f", pos.Y));
		ypos += h;
		DrawCoordinateEntry(xpos, ypos, String.Format("Z: %.0f", pos.Z));
		ypos += h;

		if (hud_showangles)
		{
			DrawCoordinateEntry(xpos, ypos, String.Format("Y: %.0f", Actor.Normalize180(mo.Angle)));
			ypos += h;
			DrawCoordinateEntry(xpos, ypos, String.Format("P: %.0f", Actor.Normalize180(mo.Pitch)));
			ypos += h;
			DrawCoordinateEntry(xpos, ypos, String.Format("R: %.0f", Actor.Normalize180(mo.Roll)));
		}
	}
	
	//---------------------------------------------------------------------------
	//
	// Draw in-game time
	//
	// Check AltHUDTime option value in wadsrc/static/menudef.txt
	// for meaning of all display modes
	//
	//---------------------------------------------------------------------------
	virtual bool DrawTime(int y)
	{
		if (hud_showtime > 0 && hud_showtime <= 9)
		{
			int timeSeconds;
			String timeString;

			if (hud_showtime < 8)
			{
				int timeTicks =
					hud_showtime < 4
						? Level.maptime
						: (hud_showtime < 6
							? Level.time
							: Level.totaltime);
				timeSeconds = Thinker.Tics2Seconds(timeTicks);

				int hours   =  timeSeconds / 3600;
				int minutes = (timeSeconds % 3600) / 60;
				int seconds =  timeSeconds % 60;

				bool showMillis  = 1 == hud_showtime;
				bool showSeconds = showMillis || (0 == hud_showtime % 2);

				if (showMillis)
				{
					int millis  = (Level.time % GameTicRate) * 1000 / GameTicRate;
					timeString = String.Format("%02i:%02i:%02i.%03i", hours, minutes, seconds, millis);
				}
				else if (showSeconds)
				{
					timeString = String.Format("%02i:%02i:%02i", hours, minutes, seconds);
				}
				else
				{
					timeString = String.Format("%02i:%02i", hours, minutes);
				}
			}
			else if (hud_showtime == 8)
			{
				timeString = SystemTime.Format("%H:%M:%S",SystemTime.Now());
			}
			else //if (hud_showtime == 9)
			{
				timeString = SystemTime.Format("%H:%M",SystemTime.Now());
			}

			int characterCount = timeString.length();
			int width  = SmallFont.GetCharWidth("0") * characterCount + 2; // small offset from screen's border
			DrawHudText(SmallFont, hud_timecolor, timeString, hudwidth - width, y, 1);
			return true;
		}
		return false;
	}

	//---------------------------------------------------------------------------
	//
	// Draw in-game latency
	//
	// 
	//
	//---------------------------------------------------------------------------
	native static int, int, int GetLatency();

	virtual bool DrawLatency(int y)
	{
		if ((hud_showlag == 1 && netgame) || hud_showlag == 2)
		{
			int severity, localdelay, arbitratordelay;
			[severity, localdelay, arbitratordelay] = GetLatency();
			int color = severity == 0? Font.CR_GREEN : severity == 1? Font.CR_YELLOW : severity == 2? Font.CR_ORANGE : Font.CR_RED;

			String tempstr = String.Format("a:%dms - l:%dms", arbitratordelay, localdelay);

			int characterCount = tempstr.Length();
			int width = SmallFont.GetCharWidth("0") * characterCount + 2; // small offset from screen's border

			DrawHudText(SmallFont, color, tempstr, hudwidth - width, y, 1);
			return true;
		}
		return false;
	}

	//---------------------------------------------------------------------------
	//
	// draw the overlay
	//
	//---------------------------------------------------------------------------

	virtual void DrawPowerups(PlayerInfo CPlayer, int y)
	{
		// Each icon gets a 32x32 block to draw itself in.
		int x, y;
		Inventory item;

		x = hudwidth - POWERUPICONSIZE - 4;

		for (item = CPlayer.mo.Inv; item != NULL; item = item.Inv)
		{
			let power = Powerup(item);
			if (power)
			{
				let icon = power.GetPowerupIcon();
				if (icon.isValid())
				{
					if (!power.isBlinking())
						DrawImageToBox(icon, x, y, POWERUPICONSIZE, POWERUPICONSIZE, 1, true);
					x -= POWERUPICONSIZE;
					if (x < -hudwidth / 2)
					{
						x = hudwidth - 20;
						y += POWERUPICONSIZE * 3 / 2;
					}
				}
			}
		}
	}
	
	//---------------------------------------------------------------------------
	//
	// main drawer
	//
	//---------------------------------------------------------------------------

	virtual void DrawInGame(PlayerInfo CPlayer)
	{
		// No HUD in the title level!
		if (gamestate == GS_TITLELEVEL || !CPlayer) return;

		if (!deathmatch) 
		{
			DrawStatus(CPlayer, 5, hudheight-50);
		}
		else
		{
			DrawStatus(CPlayer, 5, hudheight-75);
			DrawFrags(CPlayer, 5, hudheight-70);
		}
		DrawHealth(CPlayer, 5, hudheight-45);
		DrawArmor(BasicArmor(CPlayer.mo.FindInventory('BasicArmor')), HexenArmor(CPlayer.mo.FindInventory('HexenArmor')), 5, hudheight-20);
		int y = DrawKeys(CPlayer, hudwidth-4, hudheight-10);
		y = DrawAmmo(CPlayer, hudwidth-5, y);
		if (hud_showweapons) DrawWeapons(CPlayer, hudwidth - 5, y);
		DrawInventory(CPlayer, 144, hudheight - 28);
		if (idmypos) DrawCoordinates(CPlayer, true);

		int h = SmallFont.GetHeight();
		y = h;
		if (DrawTime(y)) y += h;
		if (DrawLatency(y)) y += h;
		DrawPowerups(CPlayer, y - h + POWERUPICONSIZE * 5 / 4);
	}

	//---------------------------------------------------------------------------
	//
	// automap drawer
	//
	//---------------------------------------------------------------------------

	virtual void DrawAutomap(PlayerInfo CPlayer)
	{
		let font = generic_ui? NewSmallFont : SmallFont;

		int fonth = font.GetHeight() + 1;
		int bottom = hudheight - 1;

		if (am_showtotaltime)
		{
			DrawTimeString(font, hudcolor_ttim, Level.totaltime, hudwidth-2, bottom, 1);
			bottom -= fonth;
		}

		if (am_showtime)
		{
			if (Level.clusterflags & Level.CLUSTER_HUB)
			{
				DrawTimeString(font, hudcolor_time, Level.time, hudwidth-2, bottom, 1);
				bottom -= fonth;
			}

			// Single level time for hubs
			DrawTimeString(font, hudcolor_ltim, Level.maptime, hudwidth-2, bottom, 1);
		}

		let amstr = Level.FormatMapName(hudcolor_titl);
		font = generic_ui? NewSmallFont : SmallFont.CanPrint(amstr)? SmallFont : OriginalSmallFont;

		bottom = hudheight - fonth - 1;

		screen.DrawText(font, Font.CR_BRICK, 2, bottom, amstr,
			DTA_KeepRatio, true, DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight);

		if (am_showcluster && (Level.clusterflags & Level.CLUSTER_HUB))
		{
			let text = Level.GetClusterName();
			if (text != "")
			{
				bottom -= fonth;
				screen.DrawText(font, Font.CR_ORANGE, 2, bottom, text,
					DTA_KeepRatio, true, DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight);
			}
		}
		if (am_showepisode)
		{
			let text = Level.GetEpisodeName();
			if (text != "")
			{
				bottom -= fonth;
				screen.DrawText(font, Font.CR_RED, 2, bottom, text,
					DTA_KeepRatio, true, DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight);
			}
		}

		DrawCoordinates(CPlayer, false);
	}

	//---------------------------------------------------------------------------
	//
	// main drawer
	//
	//---------------------------------------------------------------------------

	virtual void Draw(PlayerInfo CPlayer, int w, int h)
	{
		hudwidth = w;
		hudheight = h;
		if (!automapactive)
		{
			DrawInGame(CPlayer);
		}
		else
		{
			DrawAutomap(CPlayer);
		}
	}
	
}
