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
#include "r_local.h"
#include "m_swap.h"
#include "templates.h"

class FStrifeStatusBar : public FBaseStatusBar
{
public:
	FStrifeStatusBar () : FBaseStatusBar (32)
	{
		static const char *sharedLumpNames[] =
		{
			NULL,		NULL,		NULL,		NULL,		NULL,
			NULL,		NULL,		NULL,		NULL,		NULL,
			NULL,		NULL,		NULL,		"INVFONG0",	"INVFONG1",
			"INVFONG2",	"INVFONG3",	"INVFONG4",	"INVFONG5",	"INVFONG6",
			"INVFONG7",	"INVFONG8",	"INVFONG9",	"INVFONG0",	"INVFONG1",
			"INVFONG2",	"INVFONG3",	"INVFONG4",	"INVFONG5",	"INVFONG6",
			"INVFONG7",	"INVFONG8",	"INVFONG9"
		};

		FBaseStatusBar::Images.Init (sharedLumpNames, NUM_BASESB_IMAGES);
		DoCommonInit ();
	}

	~FStrifeStatusBar ()
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
		FBaseStatusBar::Draw (state);

		if (state == HUD_Fullscreen)
		{
			SB_state = screen->GetPageCount ();
			DrawFullScreenStuff ();
		}
		else if (state == HUD_StatusBar)
		{
			if (SB_state != 0)
			{
				SB_state--;
			}
			DrawMainBar ();
		}
	}

private:
	void DoCommonInit ()
	{
		static const char *strifeLumpNames[] =
		{
			"INVBACK", "INVTOP", "I_MDKT"
		};

		Images.Init (strifeLumpNames, NUM_STRIFESB_IMAGES);

		SB_state = screen->GetPageCount ();
	}

	void DrawMainBar ()
	{
		int i;

		DrawImage (Images[imgINVBACK], 0, 0);
		DrawImage (Images[imgINVTOP], 0, -8);
		DrINumber (CPlayer->health, 58, -6, imgSmNumbers);

		i = wpnlev1info[CPlayer->readyweapon]->ammo;
		if (i < NUMAMMO || i == MANA_BOTH)
		{
			int amt;
			if (i < NUMAMMO)
			{
				amt = CPlayer->ammo[i];
			}
			else
			{
				amt = MIN (CPlayer->ammo[MANA_1], CPlayer->ammo[MANA_2]);
			}
			DrINumber (amt, 290, -6, imgSmNumbers);
		}
	}

	void DrawFullScreenStuff ()
	{
		int i;

		// Draw health
		OverrideImageOrigin (true);
		DrawOuterImage (Images[imgMEDI], 20, -2);
		OverrideImageOrigin (false);
		DrBNumberOuter (CPlayer->health, 40, -8);

		// Draw ammo
		i = wpnlev1info[CPlayer->readyweapon]->ammo;
		if (i < NUMAMMO || i == MANA_BOTH)
		{
			int amt;

			OverrideImageOrigin (true);
			DrawOuterImage (AmmoImages[i], -14, -4);
			OverrideImageOrigin (false);
			if (i < NUMAMMO)
			{
				amt = CPlayer->ammo[i];
			}
			else
			{
				amt = MIN (CPlayer->ammo[MANA_1], CPlayer->ammo[MANA_2]);
			}
			DrBNumberOuter (amt, -67, -8);
		}

		if (deathmatch)
		{ // Draw frags (in DM)
			DrBNumberOuter (CPlayer->fragcount, -44, 1);
		}
	}

	void DrINumber (signed int val, int x, int y, int imgBase) const
	{
		int oldval;

		if (val > 999)
			val = 999;
		oldval = val;
		if (val < 0)
		{
			return;
		}
		if (val > 99)
		{
			DrawImage (FBaseStatusBar::Images[imgBase+val/100], x, y);
		}
		val = val % 100;
		if (val > 9 || oldval > 99)
		{
			DrawImage (FBaseStatusBar::Images[imgBase+val/10], x+7, y);
		}
		val = val % 10;
		DrawImage (FBaseStatusBar::Images[imgBase+val], x+14, y);
	}

	enum
	{
		imgINVBACK,
		imgINVTOP,
		imgMEDI,

		NUM_STRIFESB_IMAGES
	};

	FImageCollection Images;
};

FBaseStatusBar *CreateStrifeStatusBar ()
{
	return new FStrifeStatusBar;
}
