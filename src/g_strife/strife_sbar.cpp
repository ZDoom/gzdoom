#include "doomtype.h"
#include "doomstat.h"
#include "v_font.h"
#include "v_video.h"
#include "sbar.h"
#include "r_defs.h"
#include "w_wad.h"
#include "m_random.h"
#include "d_player.h"
#include "st_stuff.h"
#include "r_utility.h"
#include "m_swap.h"
#include "templates.h"
#include "a_keys.h"
#include "a_strifeglobal.h"
#include "gi.h"
#include "g_level.h"
#include "colormatcher.h"
#include "v_palette.h"

// Number of tics to move the popscreen up and down.
#define POP_TIME (TICRATE/8)

// Popscreen height when fully extended
#define POP_HEIGHT 104

// Number of tics to scroll keys left
#define KEY_TIME (TICRATE/3)

class FHealthBar : public FTexture
{
public:
	FHealthBar ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	bool CheckModified ();
	void Unload ();

	void SetVial (int level);

protected:
	BYTE Pixels[200*2];
	BYTE Colors[8];
	static const Span DummySpan[2];

	int VialLevel;
	bool NeedRefresh;

	void MakeTexture ();
	void FillBar (int min, int max, BYTE light, BYTE dark);
};

const FTexture::Span FHealthBar::DummySpan[2] = { { 0, 2 }, { 0, 0 } };

FHealthBar::FHealthBar ()
: VialLevel(0), NeedRefresh(false)
{
	int i;

	static const BYTE rgbs[8*3] =
	{
		180, 228, 128,	// light green
		128, 180, 80,	// dark green

		196, 204, 252,	// light blue
		148, 152, 200,	// dark blue

		224, 188, 0,	// light gold
		208, 128, 0,	// dark gold

		216, 44,  44,	// light red
		172, 28,  28	// dark red
	};

	Width = 200;
	Height = 2;
	WidthBits = 8;
	HeightBits = 1;
	WidthMask = 255;

	for (i = 0; i < 8; ++i)
	{
		Colors[i] = ColorMatcher.Pick (rgbs[i*3], rgbs[i*3+1], rgbs[i*3+2]);
	}
}

bool FHealthBar::CheckModified ()
{
	return NeedRefresh;
}

void FHealthBar::Unload ()
{
}

const BYTE *FHealthBar::GetColumn (unsigned int column, const Span **spans_out)
{
	if (NeedRefresh)
	{
		MakeTexture ();
	}
	if (column > 199)
	{
		column = 199;
	}
	if (spans_out != NULL)
	{
		*spans_out = &DummySpan[column >= (unsigned int)VialLevel*2];
	}
	return Pixels + column*2;
}

const BYTE *FHealthBar::GetPixels ()
{
	if (NeedRefresh)
	{
		MakeTexture ();
	}
	return Pixels;
}

void FHealthBar::SetVial (int level)
{
	if (level < 0)
	{
		level = 0;
	}
	else if (level > 200 && level != 999)
	{
		level = 200;
	}
	if (VialLevel != level)
	{
		VialLevel = level;
		NeedRefresh = true;
	}
}

void FHealthBar::MakeTexture ()
{
	if (VialLevel == 999)
	{
		FillBar (0, 100, Colors[4], Colors[5]);
	}
	else
	{
		if (VialLevel <= 100)
		{
			if (VialLevel <= 10)
			{
				FillBar (0, VialLevel, Colors[6], Colors[7]);
			}
			else if (VialLevel <= 20)
			{
				FillBar (0, VialLevel, Colors[4], Colors[5]);
			}
			else
			{
				FillBar (0, VialLevel, Colors[0], Colors[1]);
			}
			FillBar (VialLevel, 100, 0, 0);
		}
		else
		{
			int stop = 100 - (VialLevel - 100);
			FillBar (0, stop, Colors[0], Colors[1]);
			FillBar (stop, 100, Colors[2], Colors[3]);
		}
	}
}

void FHealthBar::FillBar (int min, int max, BYTE light, BYTE dark)
{
#ifdef __BIG_ENDIAN__
	SDWORD fill = (light << 24) | (dark << 16) | (light << 8) | dark;
#else
	SDWORD fill = light | (dark << 8) | (light << 16) | (dark << 24);
#endif
	if (max > min)
	{
		clearbuf (&Pixels[min*4], max - min, fill);
	}
}

class DStrifeStatusBar : public DBaseStatusBar
{
	DECLARE_CLASS(DStrifeStatusBar, DBaseStatusBar)
public:
	DStrifeStatusBar () : DBaseStatusBar (32)
	{
		static const char *sharedLumpNames[] =
		{
			NULL,		NULL,		"INVFONY0",	"INVFONY1",	"INVFONY2",
			"INVFONY3",	"INVFONY4",	"INVFONY5",	"INVFONY6",	"INVFONY7",
			"INVFONY8",	"INVFONY9",	NULL,		NULL,		NULL,
			NULL,		NULL,		NULL,		NULL,		NULL,
			NULL,		NULL,		NULL,		"INVFONG0",	"INVFONG1",
			"INVFONG2",	"INVFONG3",	"INVFONG4",	"INVFONG5",	"INVFONG6",
			"INVFONG7",	"INVFONG8",	"INVFONG9"
		};

		DBaseStatusBar::Images.Init (sharedLumpNames, NUM_BASESB_IMAGES);
		DoCommonInit ();
	}

	~DStrifeStatusBar ()
	{
	}

	void NewGame ()
	{
		Images.Uninit ();
		DoCommonInit ();
		if (CPlayer != NULL)
		{
			AttachToPlayer (CPlayer);
		}
	}

	void Draw (EHudState state)
	{
		DBaseStatusBar::Draw (state);

		if (state == HUD_StatusBar)
		{
			if (SB_state != 0)
			{
				SB_state--;
			}
			DrawMainBar ();
		}
		else
		{
			if (state == HUD_Fullscreen)
			{
				ST_SetNeedRefresh();
				DrawFullScreenStuff ();
			}

			// Draw pop screen (log, keys, and status)
			if (CurrentPop != POP_None && PopHeight < 0)
			{
				DrawPopScreen (screen->GetHeight());
			}
		}
	}

	void ShowPop (int popnum)
	{
		DBaseStatusBar::ShowPop(popnum);
		if (popnum == CurrentPop)
		{
			if (popnum == POP_Keys)
			{
				AInventory *item;
				int i;

				KeyPopPos += 10;
				KeyPopScroll = 280;

				for (item = CPlayer->mo->Inventory, i = 0;
					item != NULL;
					item = item->Inventory)
				{
					if (item->IsKindOf (RUNTIME_CLASS(AKey)))
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

	bool MustDrawLog(EHudState state)
	{
		// Tell the base class to draw the log if the pop screen won't be displayed.
		return false;
	}

private:
	void DoCommonInit ()
	{
		static const char *strifeLumpNames[] =
		{
			"INVCURS", "CURSOR01", "INVBACK", "INVTOP", "INVPOP", "INVPOP2",
			"INVPBAK", "INVPBAK2",
			"INVFONG0", "INVFONG1", "INVFONG2", "INVFONG3", "INVFONG4",
			"INVFONG5", "INVFONG6", "INVFONG7", "INVFONG8", "INVFONG9",
			"INVFONG%",
			"INVFONY0", "INVFONY1", "INVFONY2", "INVFONY3", "INVFONY4",
			"INVFONY5", "INVFONY6", "INVFONY7", "INVFONY8", "INVFONY9",
			"INVFONY%",
			"I_COMM", "I_MDKT", "I_ARM1", "I_ARM2"
		};

		Images.Init (strifeLumpNames, NUM_STRIFESB_IMAGES);

		CursorImage = Images[imgINVCURS] != NULL ? imgINVCURS : imgCURSOR01;

		ST_SetNeedRefresh();

		CurrentPop = POP_None;
		PendingPop = POP_NoChange;
		PopHeight = 0;
		KeyPopPos = 0;
		KeyPopScroll = 0;
		ItemFlash = 0;
	}

	void Tick ()
	{
		DBaseStatusBar::Tick ();

		if (ItemFlash > 0)
		{
			ItemFlash -= 1/14.;
			if (ItemFlash < 0)
			{
				ItemFlash = 0;
			}
		}

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

	void FlashItem (const PClass *itemtype)
	{
		ItemFlash = 0.75;
	}

	void DrawMainBar ()
	{
		AInventory *item;
		int i;

		// Pop screen (log, keys, and status)
		if (CurrentPop != POP_None && PopHeight < 0)
		{
			DrawPopScreen (Scaled ? (ST_Y - 8) * screen->GetHeight() / 200 : ST_Y - 8);
		}

		DrawImage (Images[imgINVBACK], 0, 0);
		DrawImage (Images[imgINVTOP], 0, -8);

		// Health
		DrINumber (CPlayer->health, 79, -6, imgFONG0);
		if (CPlayer->cheats & CF_GODMODE)
		{
			HealthBar.SetVial (999);
		}
		else
		{
			HealthBar.SetVial (CPlayer->health);
		}
		DrawImage (&HealthBar, 49, 4);
		DrawImage (&HealthBar, 49, 7);

		// Armor
		item = CPlayer->mo->FindInventory<ABasicArmor>();
		if (item != NULL && item->Amount > 0)
		{
			DrawImage (TexMan(item->Icon), 2, 9);
			DrINumber (item->Amount, 27, 23, imgFONY0);
		}

		// Ammo
		AAmmo *ammo1, *ammo2;
		int ammocount1, ammocount2;

		GetCurrentAmmo (ammo1, ammo2, ammocount1, ammocount2);
		if (ammo1 != NULL)
		{
			DrINumber (ammo1->Amount, 311, -6, imgFONG0);
			DrawImage (TexMan(ammo1->Icon), 290, 13);
			if (ammo2 != NULL)
			{
/*				int y = MIN (-5 - BigHeight, -5 - TexMan(ammo1->Icon)->GetHeight());
				screen->DrawTexture (TexMan(ammo2->Icon), -14, y,
					DTA_HUDRules, HUD_Normal,
					DTA_CenterBottomOffset, true,
					TAG_DONE);
				DrBNumberOuterFont (ammo2->Amount, -67, y - BigHeight);
*/			}
		}

		// Sigil
		item = CPlayer->mo->FindInventory<ASigil>();
		if (item != NULL)
		{
			DrawImage (TexMan(item->Icon), 253, 7);
		}

		// Inventory
		CPlayer->inventorytics = 0;
		CPlayer->mo->InvFirst = ValidateInvFirst (6);
		for (item = CPlayer->mo->InvFirst, i = 0; item != NULL && i < 6; item = item->NextInv(), ++i)
		{
			if (item == CPlayer->mo->InvSel)
			{
				screen->DrawTexture (Images[CursorImage],
					42 + 35*i + ST_X, 12 + ST_Y,
					DTA_Bottom320x200, Scaled,
					DTA_AlphaF, 1. - ItemFlash,
					TAG_DONE);
			}
			if (item->Icon.isValid())
			{
				DrawDimImage (TexMan(item->Icon), 48 + 35*i, 14, item->Amount <= 0);
			}
			DrINumber (item->Amount, 74 + 35*i, 23, imgFONY0);
		}
	}

	void DrawFullScreenStuff ()
	{
		// Draw health
		DrINumberOuter (CPlayer->health, 4, -10, false, 7);
		screen->DrawTexture (Images[imgMEDI], 14, -17,
			DTA_HUDRules, HUD_Normal,
			DTA_CenterBottomOffset, true,
			TAG_DONE);

		// Draw armor
		ABasicArmor *armor = CPlayer->mo->FindInventory<ABasicArmor>();
		if (armor != NULL && armor->Amount != 0)
		{
			DrINumberOuter (armor->Amount, 35, -10, false, 7);
			screen->DrawTexture (TexMan(armor->Icon), 45, -17,
				DTA_HUDRules, HUD_Normal,
				DTA_CenterBottomOffset, true,
				TAG_DONE);
		}

		// Draw ammo
		AAmmo *ammo1, *ammo2;
		int ammocount1, ammocount2;

		GetCurrentAmmo (ammo1, ammo2, ammocount1, ammocount2);
		if (ammo1 != NULL)
		{
			// Draw primary ammo in the bottom-right corner
			DrINumberOuter (ammo1->Amount, -23, -10, false, 7);
			screen->DrawTexture (TexMan(ammo1->Icon), -14, -17,
				DTA_HUDRules, HUD_Normal,
				DTA_CenterBottomOffset, true,
				TAG_DONE);
			if (ammo2 != NULL && ammo1!=ammo2)
			{
				// Draw secondary ammo just above the primary ammo
				DrINumberOuter (ammo2->Amount, -23, -48, false, 7);
				screen->DrawTexture (TexMan(ammo2->Icon), -14, -55,
					DTA_HUDRules, HUD_Normal,
					DTA_CenterBottomOffset, true,
					TAG_DONE);
			}
		}

		if (deathmatch)
		{ // Draw frags (in DM)
			DrBNumberOuterFont (CPlayer->fragcount, -44, 1);
		}

		// Draw inventory
		if (CPlayer->inventorytics == 0)
		{
			if (CPlayer->mo->InvSel != 0)
			{
				if (ItemFlash > 0)
				{
					FTexture *cursor = Images[CursorImage];
					screen->DrawTexture (cursor, -28, -15,
						DTA_HUDRules, HUD_Normal,
						DTA_LeftOffset, cursor->GetWidth(),
						DTA_TopOffset, cursor->GetHeight(),
						DTA_AlphaF, ItemFlash,
						TAG_DONE);
				}
				DrINumberOuter (CPlayer->mo->InvSel->Amount, -51, -10, false, 7);
				screen->DrawTexture (TexMan(CPlayer->mo->InvSel->Icon), -42, -17,
					DTA_HUDRules, HUD_Normal,
					DTA_CenterBottomOffset, true,
					DTA_ColorOverlay, CPlayer->mo->InvSel->Amount > 0 ? 0 : DIM_OVERLAY,
					TAG_DONE);
			}
		}
		else
		{
			CPlayer->mo->InvFirst = ValidateInvFirst (6);
			int i = 0;
			AInventory *item;
			if (CPlayer->mo->InvFirst != NULL)
			{
				for (item = CPlayer->mo->InvFirst; item != NULL && i < 6; item = item->NextInv(), ++i)
				{
					if (item == CPlayer->mo->InvSel)
					{
						screen->DrawTexture (Images[CursorImage], -100+i*35, -21,
							DTA_HUDRules, HUD_HorizCenter,
							DTA_Alpha, TRANSLUC75,
							TAG_DONE);
					}
					if (item->Icon.isValid())
					{
						screen->DrawTexture (TexMan(item->Icon), -94 + i*35, -19,
							DTA_HUDRules, HUD_HorizCenter,
							DTA_ColorOverlay, CPlayer->mo->InvSel->Amount > 0 ? 0 : DIM_OVERLAY,
							TAG_DONE);
					}
					DrINumberOuter (item->Amount, -89 + i*35, -10, true, 7);
				}
			}
		}
	}

	void DrawPopScreen (int bottom)
	{
		char buff[64];
		const char *label;
		int i;
		AInventory *item;
		int xscale, yscale, left, top;
		int bars = (CurrentPop == POP_Status) ? imgINVPOP : imgINVPOP2;
		int back = (CurrentPop == POP_Status) ? imgINVPBAK : imgINVPBAK2;
		// Extrapolate the height of the popscreen for smoother movement
		int height = clamp<int> (PopHeight + int(r_TicFracF * PopHeightChange), -POP_HEIGHT, 0);

		xscale = CleanXfac;
		yscale = CleanYfac;
		left = screen->GetWidth()/2 - 160*CleanXfac;
		top = bottom + height * yscale;

		screen->DrawTexture (Images[back], left, top, DTA_CleanNoMove, true, DTA_AlphaF, 0.75, TAG_DONE);
		screen->DrawTexture (Images[bars], left, top, DTA_CleanNoMove, true, TAG_DONE);


		switch (CurrentPop)
		{
		case POP_Log:
		{
			int seconds = Tics2Seconds(level.time);
			// Draw the latest log message.
			mysnprintf(buff, countof(buff), "%02d:%02d:%02d",
				seconds / 3600,
				(seconds % 3600) / 60,
				(seconds) % 60);

			screen->DrawText(SmallFont2, CR_UNTRANSLATED, left + 210 * xscale, top + 8 * yscale, buff,
				DTA_CleanNoMove, true, TAG_DONE);

			if (CPlayer->LogText.IsNotEmpty())
			{
				FBrokenLines *lines = V_BreakLines(SmallFont2, 272, CPlayer->LogText);
				for (i = 0; lines[i].Width >= 0; ++i)
				{
					screen->DrawText(SmallFont2, CR_UNTRANSLATED, left + 24 * xscale, top + (18 + i * 12)*yscale,
						lines[i].Text, DTA_CleanNoMove, true, TAG_DONE);
				}
				V_FreeBrokenLines(lines);
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
				int scroll = MAX<int> (0,KeyPopScroll - int(r_TicFracF * (280./KEY_TIME)));
				pos -= 10;
				leftcol = leftcol - 280 + scroll;
			}
			for (i = 0, item = CPlayer->mo->Inventory;
				i < endpos && item != NULL;
				item = item->Inventory)
			{
				if (!item->IsKindOf (RUNTIME_CLASS(AKey)))
					continue;
				
				if (i < pos)
				{
					i++;
					continue;
				}

				label = item->GetTag();

				int colnum = ((i-pos) / 5) & (KeyPopScroll > 0 ? 3 : 1);
				int rownum = (i % 5) * 18;

				screen->DrawTexture (TexMan(item->Icon),
					left + (colnum * 140 + leftcol)*xscale,
					top + (6 + rownum)*yscale,
					DTA_CleanNoMove, true,
					DTA_ClipLeft, clipleft,
					DTA_ClipRight, clipright,
					TAG_DONE);
				screen->DrawText (SmallFont2, CR_UNTRANSLATED,
					left + (colnum * 140 + leftcol + 17)*xscale,
					top + (11 + rownum)*yscale,
					label,
					DTA_CleanNoMove, true,
					DTA_ClipLeft, clipleft,
					DTA_ClipRight, clipright,
					TAG_DONE);
				i++;
			}
			break;

		case POP_Status:
			// Show miscellaneous status items.
			
			// Print stats
			DrINumber2 (CPlayer->mo->accuracy, left+268*xscale, top+28*yscale, 7*xscale, imgFONY0);
			DrINumber2 (CPlayer->mo->stamina, left+268*xscale, top+52*yscale, 7*xscale, imgFONY0);

			// How many keys does the player have?
			for (i = 0, item = CPlayer->mo->Inventory;
				item != NULL;
				item = item->Inventory)
			{
				if (item->IsKindOf (RUNTIME_CLASS(AKey)))
				{
					i++;
				}
			}
			DrINumber2 (i, left+268*xscale, top+76*yscale, 7*xscale, imgFONY0);

			// Does the player have a communicator?
			item = CPlayer->mo->FindInventory (NAME_Communicator);
			if (item != NULL)
			{
				screen->DrawTexture (TexMan(item->Icon),
					left + 280*xscale,
					top + 74*yscale,
					DTA_CleanNoMove, true, TAG_DONE);
			}

			// How much ammo does the player have?
			static const struct
			{
				ENamedName AmmoType;
				int Y;
			} AmmoList[7] =
			{
				{ NAME_ClipOfBullets,			19 },
				{ NAME_PoisonBolts,				35 },
				{ NAME_ElectricBolts,			43 },
				{ NAME_HEGrenadeRounds,			59 },
				{ NAME_PhosphorusGrenadeRounds,	67 },
				{ NAME_MiniMissiles,			75 },
				{ NAME_EnergyPod,				83 }
			};
			for (i = 0; i < 7; ++i)
			{
				PClassActor *ammotype = PClass::FindActor(AmmoList[i].AmmoType);
				item = CPlayer->mo->FindInventory (ammotype);

				if (item == NULL)
				{
					DrINumber2 (0, left+206*xscale, top+AmmoList[i].Y*yscale, 7*xscale, imgFONY0);
					DrINumber2 (((AInventory *)GetDefaultByType (ammotype))->MaxAmount,
						left+239*xscale, top+AmmoList[i].Y*yscale, 7*xscale, imgFONY0);
				}
				else
				{
					DrINumber2 (item->Amount, left+206*xscale, top+AmmoList[i].Y*yscale, 7*xscale, imgFONY0);
					DrINumber2 (item->MaxAmount, left+239*xscale, top+AmmoList[i].Y*yscale, 7*xscale, imgFONY0);
				}
			}

			// What weapons does the player have?
			static const struct
			{
				ENamedName TypeName;
				int X, Y;
			} WeaponList[6] =
			{
				{ NAME_StrifeCrossbow,			23, 19 },
				{ NAME_AssaultGun,				21, 41 },
				{ NAME_FlameThrower,			57, 50 },
				{ NAME_MiniMissileLauncher,		20, 64 },
				{ NAME_StrifeGrenadeLauncher,	55, 20 },
				{ NAME_Mauler,					52, 75 },
			};
			for (i = 0; i < 6; ++i)
			{
				item = CPlayer->mo->FindInventory (WeaponList[i].TypeName);

				if (item != NULL)
				{
					screen->DrawTexture (TexMan(item->Icon),
						left + WeaponList[i].X*xscale,
						top + WeaponList[i].Y*yscale,
						DTA_CleanNoMove, true,
						DTA_LeftOffset, 0,
						DTA_TopOffset, 0,
						TAG_DONE);
				}
			}
			break;
		}
	}

	void DrINumber (signed int val, int x, int y, int imgBase) const
	{
		x -= 7;

		if (val == 0)
		{
			DrawImage (Images[imgBase], x, y);
		}
		else
		{
			while (val != 0)
			{
				DrawImage (Images[imgBase+val%10], x, y);
				val /= 10;
				x -= 7;
			}
		}
	}

	void DrINumber2 (signed int val, int x, int y, int width, int imgBase) const
	{
		x -= width;

		if (val == 0)
		{
			screen->DrawTexture (Images[imgBase], x, y, DTA_CleanNoMove, true, TAG_DONE);
		}
		else
		{
			while (val != 0)
			{
				screen->DrawTexture (Images[imgBase+val%10], x, y, DTA_CleanNoMove, true, TAG_DONE);
				val /= 10;
				x -= width;
			}
		}
	}

	enum
	{
		imgINVCURS,
		imgCURSOR01,
		imgINVBACK,
		imgINVTOP,
		imgINVPOP,
		imgINVPOP2,
		imgINVPBAK,
		imgINVPBAK2,
		imgFONG0,
		imgFONG1,
		imgFONG2,
		imgFONG3,
		imgFONG4,
		imgFONG5,
		imgFONG6,
		imgFONG7,
		imgFONG8,
		imgFONG9,
		imgFONG_PERCENT,
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
		imgCOMM,
		imgMEDI,
		imgARM1,
		imgARM2,

		NUM_STRIFESB_IMAGES
	};

	FImageCollection Images;
	FHealthBar HealthBar;

	int CursorImage;
	int CurrentPop, PendingPop, PopHeight, PopHeightChange;
	int KeyPopPos, KeyPopScroll;
	double ItemFlash;
};

IMPLEMENT_CLASS(DStrifeStatusBar);

DBaseStatusBar *CreateStrifeStatusBar ()
{
	return new DStrifeStatusBar;
}
