class HarmonyStatusBar : DoomStatusBar
{
	double scaleFactor;
	
	override void Init()
	{
		Super.Init();

		// This is for detecting the DECORATEd version of the mod which sets proper scaling for the HUD related textures.
		let tex = TexMan.CheckForTexture("MEDIA0", TexMan.Type_Sprite);
		if (tex.isValid())
		{
			Vector2 ssize = TexMan.GetScaledSize(tex);
			Vector2 facs = (ssize.X / 31, ssize.Y /  18);
			scaleFactor = min(facs.X, facs.Y, 1);
		}
	}
	
	override void Draw (int state, double TicFrac)
	{
		BaseStatusBar.Draw (state, TicFrac);

		if (state == HUD_StatusBar)
		{
			BeginStatusBar();
			DrawMainBar (TicFrac);
		}
		else if (state == HUD_Fullscreen)
		{
			BeginHUD();
			DrawFullScreenStuff ();
		}
	}

	protected void DrawFullScreenStuff ()
	{
		Vector2 iconbox = (40, 20);
		// Draw health
		DrawImage("MEDIA0", (20, -2), scale:(scaleFactor, scaleFactor));
		DrawString(mHUDFont, FormatNumber(CPlayer.health, 3), (44, -20));
		
		let armor = CPlayer.mo.FindInventory("BasicArmor");
		if (armor != null && armor.Amount > 0)
		{
			DrawInventoryIcon(armor, (20, -22), scale:(scaleFactor, scaleFactor));
			DrawString(mHUDFont, FormatNumber(armor.Amount, 3), (44, -40));
		}
		Inventory ammotype1, ammotype2;
		[ammotype1, ammotype2] = GetCurrentAmmo();
		int invY = -20;
		if (ammotype1 != null)
		{
			DrawInventoryIcon(ammotype1, (-14, -4), scale:(scaleFactor, scaleFactor));
			DrawString(mHUDFont, FormatNumber(ammotype1.Amount, 3), (-30, -20), DI_TEXT_ALIGN_RIGHT);
			invY -= 20;
		}
		if (ammotype2 != null && ammotype2 != ammotype1)
		{
			DrawInventoryIcon(ammotype2, (-14, invY + 17), scale:(scaleFactor, scaleFactor));
			DrawString(mHUDFont, FormatNumber(ammotype2.Amount, 3), (-30, invY), DI_TEXT_ALIGN_RIGHT);
			invY -= 20;
		}
		if (!isInventoryBarVisible() && !Level.NoInventoryBar && CPlayer.mo.InvSel != null)
		{
			DrawInventoryIcon(CPlayer.mo.InvSel, (-14, invY + 17));
			DrawString(mHUDFont, FormatNumber(CPlayer.mo.InvSel.Amount, 3), (-30, invY), DI_TEXT_ALIGN_RIGHT);
		}
		if (deathmatch)
		{
			DrawString(mHUDFont, FormatNumber(CPlayer.FragCount, 3), (-3, 1), DI_TEXT_ALIGN_RIGHT, Font.CR_GOLD);
		}
		
		// Draw the keys. This does not use a special draw function like SBARINFO because the specifics will be different for each mod 
		// so it's easier to copy or reimplement the following piece of code instead of trying to write a complicated all-encompassing solution.
		Vector2 keypos = (-10, 2);
		int rowc = 0;
		double roww = 0;
		for(let i = CPlayer.mo.Inv; i != null; i = i.Inv)
		{
			if (i is "Key" && i.Icon.IsValid())
			{
				DrawTexture(i.Icon, keypos, DI_SCREEN_RIGHT_TOP|DI_ITEM_LEFT_TOP);
				Vector2 size = TexMan.GetScaledSize(i.Icon);
				keypos.Y += size.Y + 2;
				roww = max(roww, size.X);
				if (++rowc == 3)
				{
					keypos.Y = 2;
					keypos.X -= roww + 2;
					roww = 0;
					rowc = 0;
				}
			}
		}
		if (isInventoryBarVisible())
		{
			DrawInventoryBar(diparms, (0, 0), 7, DI_SCREEN_CENTER_BOTTOM, HX_SHADOW);
		}
	}
}
