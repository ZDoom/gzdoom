/*
** playermenu.cpp
** The player setup menu
**
**---------------------------------------------------------------------------
** Copyright 2001-2010 Randy Heit
** Copyright 2010-2017 Christoph Oelckers
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

class PlayerMenu : ListMenu
{
	int mRotation;
	int PlayerClassIndex;
	PlayerClass mPlayerClass;
	Array<int> PlayerColorSets;
	Array<int> mPlayerSkins;
	
	// All write function for the player config are native to prevent abuse.
	protected native void AutoaimChanged(float val);
	protected native void TeamChanged(int val);
	protected native void AlwaysRunChanged(int val);
	protected native void GenderChanged(int val);
	protected native void SwitchOnPickupChanged(int val);
	protected native void ColorChanged(int red, int green, int blue);
	protected native void ColorSetChanged(int red);
	protected native void PlayerNameChanged(String name);
	protected native void SkinChanged (int val);
	protected native void ClassChanged(int sel, PlayerClass cls);
	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	protected void UpdateTranslation()
	{
		Translation.SetPlayerTranslation(TRANSLATION_Players, MAXPLAYERS, consoleplayer, mPlayerClass);
	}
	
	protected void SendNewColor (int red, int green, int blue)
	{
		ColorChanged(red, green, blue);
		UpdateTranslation();
	}
	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void Init(Menu parent, ListMenuDescriptor desc)
	{
		MenuItemBase li;
		PlayerInfo p = players[consoleplayer];

		Super.Init(parent, desc);
		PickPlayerClass();
		mRotation = 0;

		li = GetItem('Playerdisplay');
		if (li != NULL)
		{
			li.SetValue(ListMenuItemPlayerDisplay.PDF_ROTATION, 0);
			li.SetValue(ListMenuItemPlayerDisplay.PDF_MODE, 1);
			li.SetValue(ListMenuItemPlayerDisplay.PDF_TRANSLATE, 1);
			li.SetValue(ListMenuItemPlayerDisplay.PDF_CLASS, p.GetPlayerClassNum());
			if (mPlayerClass != NULL && !(GetDefaultByType (mPlayerClass.Type).bNoSkin) &&
				p.GetPlayerClassNum() != -1)
			{
				li.SetValue(ListMenuItemPlayerDisplay.PDF_SKIN, p.GetSkin());
			}
		}

		li = GetItem('Playerbox');
		if (li != NULL)
		{
			li.SetString(0, p.GetUserName());
		}

		li = GetItem('Team');
		if (li != NULL)
		{
			li.SetString(0, "None");
			for(int i=0;i<Teams.Size(); i++)
			{
				li.SetString(i+1, Teams[i].mName);
			}
			int myteam = players[consoleplayer].GetTeam();
			li.SetValue(0, myteam ==  Team.NoTeam? 0 : myteam + 1);
		}

		int mycolorset = p.GetColorSet();
		Color colr = p.GetColor();

		UpdateColorsets();

		li = GetItem('Red');
		if (li != NULL)
		{
			li.Enable(mycolorset == -1);
			li.SetValue(0, colr.r);
		}

		li = GetItem('Green');
		if (li != NULL)
		{
			li.Enable(mycolorset == -1);
			li.SetValue(0, colr.g);
		}

		li = GetItem('Blue');
		if (li != NULL)
		{
			li.Enable(mycolorset == -1);
			li.SetValue(0, colr.b);
		}

		li = GetItem('Class');
		if (li != NULL)
		{
			if (PlayerClasses.Size() == 1)
			{
				li.SetString(0, PlayerPawn.GetPrintableDisplayName(PlayerClasses[0].Type));
				li.SetValue(0, 0);
			}
			else
			{
				// [XA] Remove the "Random" option if the relevant gameinfo flag is set.
				if(!gameinfo.norandomplayerclass)
					li.SetString(0, "Random");
				for(int i=0; i< PlayerClasses.Size(); i++)
				{
					let cls = PlayerPawn.GetPrintableDisplayName(PlayerClasses[i].Type);
					li.SetString(gameinfo.norandomplayerclass ? i : i+1, cls);
				}
				int pclass = p.GetPlayerClassNum();
				li.SetValue(0, gameinfo.norandomplayerclass && pclass >= 0 ? pclass : pclass + 1);
			}
		}

		UpdateSkins();

		li = GetItem('Gender');
		if (li != NULL)
		{
			li.SetValue(0, p.GetGender());
		}

		li = GetItem('Autoaim');
		if (li != NULL)
		{
			li.SetValue(0, int(p.GetAutoaim()));
		}

		li = GetItem('Switch');
		if (li != NULL)
		{
			li.SetValue(0, p.GetNeverSwitch());
		}

		li = GetItem('AlwaysRun');
		if (li != NULL)
		{
			li.SetValue(0, cl_run);
		}

		if (mDesc.mSelectedItem < 0) mDesc.mSelectedItem = 1;

	}

	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	protected void PickPlayerClass(int pick = -100)
	{
		int pclass = 0;
		// [GRB] Pick a class from player class list
		if (PlayerClasses.Size () > 1)
		{
			pclass = pick == -100? players[consoleplayer].GetPlayerClassNum() : pick;

			if (pclass < 0)
			{
				pclass = (MenuTime() >> 7) % PlayerClasses.Size ();
			}
		}
		PlayerClassIndex = pclass;
		mPlayerClass = PlayerClasses[PlayerClassIndex];
		UpdateTranslation();
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	protected void UpdateColorsets()
	{
		let li = GetItem('Color');
		if (li != NULL)
		{
			int sel = 0;
			mPlayerClass.EnumColorSets(PlayerColorSets);
			li.SetString(0, "Custom");
			for(int i = 0; i < PlayerColorSets.Size(); i++)
			{
				let cname = mPlayerClass.GetColorSetName(PlayerColorSets[i]);
				li.SetString(i+1, cname);
			}
			int mycolorset = players[consoleplayer].GetColorSet();
			if (mycolorset != -1)
			{
				for(int i = 0; i < PlayerColorSets.Size(); i++)
				{
					if (PlayerColorSets[i] == mycolorset)
					{
						sel = i + 1;
					}
				}
			}
			li.SetValue(0, sel);
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	protected void UpdateSkins()
	{
		int sel = 0;
		int skin;
		let li = GetItem('Skin');
		if (li != NULL)
		{
			if (GetDefaultByType (mPlayerClass.Type).bNoSkin ||	players[consoleplayer].GetPlayerClassNum() == -1)
			{
				li.SetString(0, "Base");
				li.SetValue(0, 0);
				skin = 0;
			}
			else
			{
				mPlayerSkins.Clear();
				for (int i = 0; i < PlayerSkins.Size(); i++)
				{
					if (mPlayerClass.CheckSkin(i))
					{
						int j = mPlayerSkins.Push(i);
						li.SetString(j, PlayerSkins[i].SkinName);
						if (players[consoleplayer].GetSkin() == i)
						{
							sel = j;
						}
					}
				}
				li.SetValue(0, sel);
				skin = mPlayerSkins[sel];
			}
			li = GetItem('Playerdisplay');
			if (li != NULL)
			{
				li.SetValue(ListMenuItemPlayerDisplay.PDF_SKIN, skin);
			}
		}
		UpdateTranslation();
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	void ChangeClass (MenuItemBase li)
	{
		if (PlayerClasses.Size () == 1)
		{
			return;
		}

		bool res;
		int	sel;

		[res, sel] = li.GetValue(0);
		if (res)
		{
			PickPlayerClass(gameinfo.norandomplayerclass ? sel : sel-1);
			ClassChanged(sel, mPlayerClass);
			UpdateSkins();
			UpdateColorsets();
			UpdateTranslation();

			li = GetItem('Playerdisplay');
			if (li != NULL)
			{
				li.SetValue(ListMenuItemPlayerDisplay.PDF_CLASS, players[consoleplayer].GetPlayerClassNum());
			}
		}
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	protected void ChangeSkin (MenuItemBase li)
	{
		if (GetDefaultByType (mPlayerClass.Type).bNoSkin || players[consoleplayer].GetPlayerClassNum() == -1)
		{
			return;
		}

		bool res;
		int	sel;

		[res, sel] = li.GetValue(0);
		if (res)
		{
			sel = mPlayerSkins[sel];
			SkinChanged(sel);
			UpdateTranslation();

			li = GetItem('Playerdisplay');
			if (li != NULL)
			{
				li.SetValue(ListMenuItemPlayerDisplay.PDF_SKIN, sel);
			}
		}
	}

	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool OnUIEvent(UIEvent ev)
	{
		if (ev.Type == UIEvent.Type_Char && ev.KeyChar == 32)
		{
			// turn the player sprite around
			mRotation = 8 - mRotation;
			MenuItemBase li = GetItem('Playerdisplay');
			if (li != NULL)
			{
				li.SetValue(ListMenuItemPlayerDisplay.PDF_ROTATION, mRotation);
			}
			return true;
		}
		return Super.OnUIEvent(ev);
	}

	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MenuEvent (int mkey, bool fromcontroller)
	{
		int v;
		bool res;
		String s;
		if (mDesc.mSelectedItem >= 0)
		{
			let li = mDesc.mItems[mDesc.mSelectedItem];
			if (li.MenuEvent(mkey, fromcontroller))
			{
				Name ctrl = li.GetAction();
				switch(ctrl)
				{
					// item specific handling comes here

				case 'Playerbox':
					if (mkey == MKEY_Input)
					{
						[res, s] = li.GetString(0);
						if (res) PlayerNameChanged(s);
					}
					break;

				case 'Team':
					[res, v] = li.GetValue(0);
					if (res) TeamChanged(v);
					break;

				case 'Color':
					[res, v] = li.GetValue(0);
					if (res) 
					{
						int mycolorset = -1;

						if (v > 0) mycolorset = PlayerColorSets[v - 1];

						let red   = GetItem('Red');
						let green = GetItem('Green');
						let blue  = GetItem('Blue');

						// disable the sliders if a valid colorset is selected
						if (red != NULL) red.Enable(mycolorset == -1);
						if (green != NULL) green.Enable(mycolorset == -1);
						if (blue != NULL) blue.Enable(mycolorset == -1);
						
						ColorSetChanged(v - 1);
						UpdateTranslation();
					}
					break;

				case 'Red':
					[res, v] = li.GetValue(0);
					if (res) 
					{
						Color colr = players[consoleplayer].GetColor();
						SendNewColor (v, colr.g, colr.b);
					}
					break;

				case 'Green':
					[res, v] = li.GetValue(0);
					if (res) 
					{
						Color colr = players[consoleplayer].GetColor();
						SendNewColor (colr.r, v, colr.b);
					}
					break;

				case 'Blue':
					[res, v] = li.GetValue(0);
					if (res) 
					{
						Color colr = players[consoleplayer].GetColor();
						SendNewColor (colr.r, colr.g, v);
					}
					break;
					
				case 'Class':
					[res, v] = li.GetValue(0);
					if (res) 
					{
						ChangeClass(li);
					}
					break;

				case 'Skin':
					ChangeSkin(li);
					break;

				case 'Gender':
					[res, v] = li.GetValue(0);
					if (res) 
					{
						GenderChanged(v);
					}
					break;

				case 'Autoaim':
					[res, v] = li.GetValue(0);
					if (res) 
					{
						AutoaimChanged(v);
					}
					break;

				case 'Switch':
					[res, v] = li.GetValue(0);
					if (res) 
					{
						SwitchOnPickupChanged(v);
					}
					break;

				case 'AlwaysRun':
					[res, v] = li.GetValue(0);
					if (res) 
					{
						AlwaysRunChanged(v);
					}
					break;

				default:
					break;
				}
				return true;
			}
		}
		return Super.MenuEvent(mkey, fromcontroller);
	}
	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	override bool MouseEvent(int type, int x, int y)
	{
		let li = mFocusControl;
		bool res = Super.MouseEvent(type, x, y);
		if (li == NULL) li = mFocusControl;
		if (li != NULL)
		{
			// Check if the colors have changed
			Name ctrl = li.GetAction();
			bool resv;
			int v;
			[resv, v]= li.GetValue(0);
			switch(ctrl)
			{
			case 'Red':
				if (resv)
				{
					Color colr = players[consoleplayer].GetColor();
					SendNewColor (v, colr.g, colr.b);
				}
				break;

			case 'Green':
				if (resv)
				{
					Color colr = players[consoleplayer].GetColor();
					SendNewColor (colr.r, v, colr.b);
				}
				break;

			case 'Blue':
				if (resv)
				{
					Color colr = players[consoleplayer].GetColor();
					SendNewColor (colr.r, colr.g, v);
				}
				break;
			case 'Autoaim':
				AutoaimChanged(v);
				break;
			}
		}
		return res;
	}

	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	override void Drawer ()
	{
		Super.Drawer();
		String str = Stringtable.Localize("$PLYRMNU_PRESSSPACE");
		screen.DrawText (SmallFont, Font.CR_GOLD, 320 - 32 - 32 - SmallFont.StringWidth (str)/2, 130, str, DTA_Clean, true);
		str = Stringtable.Localize(mRotation ? "$PLYRMNU_SEEFRONT" : "$PLYRMNU_SEEBACK");
		screen.DrawText (SmallFont, Font.CR_GOLD, 320 - 32 - 32 - SmallFont.StringWidth (str)/2, 130 + SmallFont.GetHeight (), str, DTA_Clean, true);

	}
	
}


class NewPlayerMenu : OptionMenu
{
	TextureID mBackdrop;
	PlayerClass mPlayerClass;
	State mPlayerState;
	int mPlayerTics;
	bool mNoportrait;
	int8 mRotation;
	int8 mMode;	// 0: automatic (used by class selection), 1: manual (used by player setup)
	int8 mTranslate;
	int mSkin;
	int mClassNum;
	Color mBaseColor;
	Color mAddColor;
	
	const PLAYERDISPLAY_X = 220;
	const PLAYERDISPLAY_Y = 60;
	const PLAYERDISPLAY_W = 144;
	const PLAYERDISPLAY_H = 160;
	const PLAYERDISPLAY_SPACE = 180;
	
	override void Init(Menu parent, OptionMenuDescriptor desc)
	{
		Super.Init(parent, desc);
		mBackdrop = TexMan.CheckForTexture("B@CKDROP", TexMan.Type_MiscPatch);	// The weird name is to avoid clashes with mods.
		mBaseColor = gameinfo.gametype == GAME_Hexen? 0x200000 : 0x000700;
		mAddColor = gameinfo.gametype == GAME_Hexen? 0x800040 : 0x405340;
	}

	override int GetIndent()
	{
		return Super.GetIndent() - 45*CleanXfac_1;
	}
	
	override void Drawer()
	{
		Super.Drawer();
		DrawPlayerDisplay();
		
		int x = screen.GetWidth()/(CleanXfac_1*2) + PLAYERDISPLAY_X + PLAYERDISPLAY_W/2;
		int y = PLAYERDISPLAY_Y + PLAYERDISPLAY_H + 5;
		String str = Stringtable.Localize("$PLYRMNU_PRESSSPACE");
		screen.DrawText (NewSmallFont, Font.CR_GOLD, x - NewSmallFont.StringWidth(str)/2, y, str, DTA_VirtualWidth, CleanWidth_1, DTA_VirtualHeight, CleanHeight_1, DTA_KeepRatio, true);
		str = Stringtable.Localize(mRotation ? "$PLYRMNU_SEEFRONT" : "$PLYRMNU_SEEBACK");
		y += NewSmallFont.GetHeight();
		screen.DrawText (NewSmallFont, Font.CR_GOLD,x - NewSmallFont.StringWidth(str)/2, y, str, DTA_VirtualWidth, CleanWidth_1, DTA_VirtualHeight, CleanHeight_1, DTA_KeepRatio, true);

	}
	
	//=============================================================================
	//
	//
	//
	//=============================================================================

	void DrawPlayerDisplay()
	{

		int x = screen.GetWidth()/2 + PLAYERDISPLAY_X * CleanXfac_1;
		int y = PLAYERDISPLAY_Y * CleanYfac_1;

		int r = mBaseColor.r + mAddColor.r;
		int g = mBaseColor.g + mAddColor.g;
		int b = mBaseColor.b + mAddColor.b;
		int m = max(r, g, b);
		r = r * 255 / m;
		g = g * 255 / m;
		b = b * 255 / m;
		Color c = Color(255, r, g, b);
		
		screen.DrawTexture(mBackdrop, false, x, y - 1,
			DTA_DestWidth, PLAYERDISPLAY_W * CleanXfac_1,
			DTA_DestHeight, PLAYERDISPLAY_H * CleanYfac_1,
			DTA_Color, c,
			DTA_KeepRatio, mNoPortrait,
			DTA_Masked, true);

		Screen.DrawFrame (x, y, PLAYERDISPLAY_W*CleanXfac_1, PLAYERDISPLAY_H*CleanYfac_1-1);

		if (mPlayerState != NULL && mPlayerState != NULL)
		{
			Vector2 Scale;
			TextureID sprite;
			bool flip;
			
			let playdef = GetDefaultByType((class<PlayerPawn>)(mPlayerClass.Type));
			[sprite, flip, Scale] = mPlayerState.GetSpriteTexture(mRotation, mSkin, playdef.Scale);
		
			if (sprite.IsValid())
			{
				int trans = mTranslate? Translation.MakeID(TRANSLATION_Players, MAXPLAYERS) : 0;
				let tscale = TexMan.GetScaledSize(sprite);
				Scale.X *= CleanXfac_1 * tscale.X;
				Scale.Y *= CleanYfac_1 * tscale.Y;
				
				screen.DrawTexture (sprite, false,
					x + (PLAYERDISPLAY_X/2) * CleanXfac_1, y + (PLAYERDISPLAY_H-8) * CleanYfac_1,
					DTA_DestWidthF, Scale.X, DTA_DestHeightF, Scale.Y,
					DTA_TranslationIndex, trans,
					DTA_KeepRatio, mNoPortrait,
					DTA_FlipX, flip);
			}
		}
	}
	
}
