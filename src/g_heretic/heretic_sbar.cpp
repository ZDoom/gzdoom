#include <assert.h>

#include "doomtype.h"
#include "doomstat.h"
#include "v_font.h"
#include "sbar.h"
#include "r_defs.h"
#include "w_wad.h"
#include "m_random.h"
#include "d_player.h"
#include "st_stuff.h"
#include "v_video.h"
#include "r_draw.h"
#include "templates.h"
#include "a_keys.h"
#include "r_translate.h"
#include "g_level.h"
#include "v_palette.h"


static FRandom pr_chainwiggle;

// This texture is used to shade each end of the health chain
class FHereticShader : public FTexture
{
public:
	FHereticShader ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();

private:
	static const BYTE Pixels[10*16];
	static const Span DummySpan[2];
};

static FHereticShader ChainShade;

const FTexture::Span FHereticShader::DummySpan[2] = { { 0, 10 }, { 0, 0 } };
const BYTE FHereticShader::Pixels[10*16] =
{
	254, 254, 254, 254, 254, 254, 254, 254, 254, 254,
	240, 240, 240, 240, 240, 240, 240, 240, 240, 240,
	224, 224, 224, 224, 224, 224, 224, 224, 224, 224,
	208, 208, 208, 208, 208, 208, 208, 208, 208, 208,
	192, 192, 192, 192, 192, 192, 192, 192, 192, 192,
	176, 176, 176, 176, 176, 176, 176, 176, 176, 176,
	160, 160, 160, 160, 160, 160, 160, 160, 160, 160,
	144, 144, 144, 144, 144, 144, 144, 144, 144, 144,
	128, 128, 128, 128, 128, 128, 128, 128, 128, 128,
	112, 112, 112, 112, 112, 112, 112, 112, 112, 112,
	 96,  96,  96,  96,  96,  96,  96,  96,  96,  96,
	 80,  80,  80,  80,  80,  80,  80,  80,  80,  80,
	 64,  64,  64,  64,  64,  64,  64,  64,  64,  64,
	 48,  48,  48,  48,  48,  48,  48,  48,  48,  48,
	 32,  32,  32,  32,  32,  32,  32,  32,  32,  32,
	 16,  16,  16,  16,  16,  16,  16,  16,  16,  16,
};

FHereticShader::FHereticShader ()
{
	Width = 16;
	Height = 10;
	WidthBits = 4;
	HeightBits = 4;
	WidthMask = 15;
}

void FHereticShader::Unload ()
{
}

const BYTE *FHereticShader::GetColumn (unsigned int column, const Span **spans_out)
{
	if (spans_out != NULL)
	{
		*spans_out = DummySpan;
	}
	return Pixels + 10*(column & 15);
}

const BYTE *FHereticShader::GetPixels ()
{
	return Pixels;
}


class DHereticStatusBar : public DBaseStatusBar
{
	DECLARE_CLASS(DHereticStatusBar, DBaseStatusBar)
	HAS_OBJECT_POINTERS
public:
	DHereticStatusBar () : DBaseStatusBar (42)
	{
		static const char *hereticLumpNames[NUM_HERETICSB_IMAGES] =
		{
			"LTFACE",	"RTFACE",	"BARBACK",	"INVBAR",	"CHAIN",
			NULL,		"LIFEGEM2",	"LTFCTOP",	"RTFCTOP",	"SELECTBO",
			"INVGEML1",	"INVGEML2",	"INVGEMR1",	"INVGEMR2",	"BLACKSQ",
			"ARMCLEAR",	"CHAINBACK","GOD1",		"GOD2",		"USEARTIA",
			"USEARTIB",	"USEARTIC",	"USEARTID",	"YKEYICON",	"GKEYICON",
			"BKEYICON",	"ARTIBOX",	"PTN1A0",	"PTN1B0",	"PTN1C0"
		};
		static const char *sharedLumpNames[] =
		{
			"LAME",		"NEGNUM",	"IN0",		"IN1",		"IN2",
			"IN3",		"IN4",		"IN5",		"IN6",		"IN7",
			"IN8",		"IN9",		"FONTB13",	"FONTB16",	"FONTB17",
			"FONTB18",	"FONTB19",	"FONTB20",	"FONTB21",	"FONTB22",
			"FONTB23",	"FONTB24",	"FONTB25",	"SMALLIN0",	"SMALLIN1",
			"SMALLIN2",	"SMALLIN3",	"SMALLIN4",	"SMALLIN5",	"SMALLIN6",
			"SMALLIN7",	"SMALLIN8",	"SMALLIN9"
		};

		if (deathmatch)
		{
			hereticLumpNames[5] = "STATBAR";
		}
		else
		{
			hereticLumpNames[5] = "LIFEBAR";
		}

		DBaseStatusBar::Images.Init (sharedLumpNames, NUM_BASESB_IMAGES);
		Images.Init (hereticLumpNames, NUM_HERETICSB_IMAGES);

		oldarti = NULL;
		oldammo1 = oldammo2 = NULL;
		oldammocount1 = oldammocount2 = -1;
		oldartiCount = 0;
		oldfrags = -9999;
		oldarmor = -1;
		oldhealth = -1;
		oldlife = -1;
		oldkeys = -1;

		HealthMarker = 0;
		ChainWiggle = 0;
		ArtifactFlash = 0;
	}

	~DHereticStatusBar ()
	{
	}

	void Tick ()
	{
		int curHealth;

		DBaseStatusBar::Tick ();
		if (level.time & 1)
		{
			ChainWiggle = pr_chainwiggle() & 1;
		}
		curHealth = CPlayer->health;
		if (curHealth < 0)
		{
			curHealth = 0;
		}
		if (curHealth < HealthMarker)
		{
			HealthMarker -= clamp ((HealthMarker - curHealth) >> 2, 1, 8);
		}
		else if (curHealth > HealthMarker)
		{
			HealthMarker += clamp ((curHealth - HealthMarker) >> 2, 1, 8);
		}

		if (ArtifactFlash > 0)
		{
			if (--ArtifactFlash == 0)
			{
				ArtiRefresh = screen->GetPageCount ();
			}
		}
	}

	void Draw (EHudState state)
	{
		DBaseStatusBar::Draw (state);

		if (state == HUD_Fullscreen)
		{
			DrawFullScreenStuff ();
			SB_state = screen->GetPageCount ();
		}
		else if (state == HUD_StatusBar)
		{
			if (SB_state > 0)
			{
				DrawImage (Images[imgBARBACK], 0, 0);
				if (CPlayer->cheats&CF_GODMODE)
				{
					DrawImage (Images[imgGOD1], 16, 9);
					DrawImage (Images[imgGOD2], 287, 9);
				}
				oldhealth = -1;
			}
			DrawCommonBar ();
			if (CPlayer->inventorytics == 0)
			{
				if (SB_state < 0)
				{
					SB_state = screen->GetPageCount ();
				}
				if (SB_state != 0)
				{
					// Main interface
					SB_state--;
					DrawImage (Images[imgSTATBAR], 34, 2);
					oldarti = NULL;
					oldammo1 = oldammo2 = NULL;
					oldammocount1 = oldammocount2 = -1;
					oldarmor = -1;
					oldfrags = -9999; //can't use -1, 'cuz of negative frags
					oldlife = -1;
					oldkeys = -1;
					oldhealth = -1;
					ArtiRefresh = 0;
				}
				DrawMainBar ();
			}
			else
			{
				if (SB_state > -1)
				{
					SB_state = -screen->GetPageCount () - 1;
				}
				if (SB_state < -1)
				{
					SB_state++;
					DrawImage (Images[imgINVBAR], 34, 2);
				}
				DrawInventoryBar ();
			}
		}
	}

private:
//---------------------------------------------------------------------------
//
// PROC DrawCommonBar
//
//---------------------------------------------------------------------------

	void DrawCommonBar ()
	{
		int chainY;
		int healthPos;

		DrawImage (Images[imgLTFCTOP], 0, -10);
		//DrawImage (Images[imgRTFCTOP], 290, -10);
		screen->DrawTexture (Images[imgRTFCTOP], ST_X+290, ST_Y,
			DTA_Bottom320x200, Scaled,
			DTA_TopOffset, Images[imgRTFCTOP]->GetHeight(),
			TAG_DONE);

		if (oldhealth != HealthMarker)
		{
			oldhealth = HealthMarker;
			HealthRefresh = screen->GetPageCount ();
		}
		if (HealthRefresh)
		{
			HealthRefresh--;
			healthPos = HealthMarker;
			if (healthPos < 0)
			{
				healthPos = 0;
			}
			if (healthPos > 100)
			{
				healthPos = 100;
			}
			healthPos = (healthPos * 256) / 100;
			chainY = (HealthMarker == (CPlayer->health > 0 ? CPlayer->health : 0)) ? 33 : 33 + ChainWiggle;
			DrawImage (Images[imgCHAINBACK], 0, 32);
			DrawImage (Images[imgCHAIN], 2+(healthPos%17), chainY);
			DrawImage (Images[imgLIFEGEM], 17+healthPos, chainY, multiplayer ?
				translationtables[TRANSLATION_PlayersExtra][int(CPlayer - players)] : NULL);
			DrawImage (Images[imgLTFACE], 0, 32);
			DrawImage (Images[imgRTFACE], 276, 32);
			screen->DrawTexture (&ChainShade, ST_X+19, ST_Y+32,
				DTA_Bottom320x200, Scaled,
				DTA_AlphaChannel, true,
				DTA_FillColor, 0,
				TAG_DONE);
			screen->DrawTexture (&ChainShade, ST_X+277, ST_Y+32,
				DTA_Bottom320x200, Scaled,
				DTA_AlphaChannel, true,
				DTA_FillColor, 0,
				DTA_FlipX, true,
				TAG_DONE);
		}
	}

//---------------------------------------------------------------------------
//
// PROC DrawMainBar
//
//---------------------------------------------------------------------------

	void DrawMainBar ()
	{
		AInventory *item;
		AAmmo *ammo1, *ammo2;
		int ammocount1, ammocount2;
		int temp;
		int playerkeys;

		// Ready artifact
		if (ArtifactFlash)
		{
			DrawImage (Images[imgBLACKSQ], 180, 3);
			DrawImage (Images[imgUSEARTIA + ArtifactFlash], 182, 3);
			oldarti = NULL; // so that the correct artifact fills in after the flash
		}
		else if (oldarti != CPlayer->mo->InvSel
			|| (oldarti != NULL && oldartiCount != oldarti->Amount))
		{
			oldarti = CPlayer->mo->InvSel;
			GC::WriteBarrier(this, oldarti);
			oldartiCount = oldarti != NULL ? oldarti->Amount : 0;
			ArtiRefresh = screen->GetPageCount ();
		}
		if (ArtiRefresh)
		{
			ArtiRefresh--;
			DrawImage (Images[imgBLACKSQ], 180, 3);
			if (oldarti != NULL)
			{
				DrawDimImage (TexMan(oldarti->Icon), 179, 2, oldarti->Amount <= 0);
				if (oldartiCount != 1)
				{
					DrSmallNumber (oldartiCount, 197, 24);
				}
			}
		}

		// Frags
		if (deathmatch)
		{
			temp = CPlayer->fragcount;
			if (temp != oldfrags)
			{
				oldfrags = temp;
				FragHealthRefresh = screen->GetPageCount ();
			}
			if (FragHealthRefresh)
			{
				FragHealthRefresh--;
				DrawImage (Images[imgARMCLEAR], 57, 13);
				DrINumber (temp, 61, 12);
			}
		}
		else
		{
			temp = MAX(0, HealthMarker);
			if (oldlife != temp)
			{
				oldlife = temp;
				FragHealthRefresh = screen->GetPageCount ();
			}
			if (FragHealthRefresh)
			{
				FragHealthRefresh--;
				DrawImage (Images[imgARMCLEAR], 57, 13);
				DrINumber (temp, 61, 12);
			}
		}

		// Keys
		playerkeys = 0;

		for (item = CPlayer->mo->Inventory; item != NULL; item = item->Inventory)
		{
			if (item->IsKindOf (RUNTIME_CLASS(AKey)))
			{
				int keynum = static_cast<AKey*>(item)->KeyNumber;
				if (keynum >= 1 && keynum <= 3)
				{
					playerkeys |= 1 << (keynum-1);
				}
			}
		}
		if (oldkeys != playerkeys)
		{
			oldkeys = playerkeys;
			KeysRefresh = screen->GetPageCount ();
		}
		if (KeysRefresh)
		{
			KeysRefresh--;
			// [RH] Erase the key images so the player can drop keys
			// and see the status update.
			screen->DrawTexture (Images[imgSTATBAR], ST_X+34, ST_Y+2,
				DTA_WindowLeft, 119,
				DTA_WindowRight, 129,
				DTA_Bottom320x200, Scaled,
				TAG_DONE);
			if (playerkeys & 4)
			{
				DrawImage (Images[imgYKEYICON], 153, 6);
			}
			if (playerkeys & 1)
			{
				DrawImage (Images[imgGKEYICON], 153, 14);
			}
			if (playerkeys & 2)
			{
				DrawImage (Images[imgBKEYICON], 153, 22);
			}
		}

		// Ammo
		GetCurrentAmmo (ammo1, ammo2, ammocount1, ammocount2);
		if (ammo1 == ammo2)
		{
			// Don't show the same ammo twice.
			ammo2 = NULL;
		}
		if (oldammo1 != ammo1 || oldammo2 != ammo2 ||
			oldammocount1 != ammocount1 || oldammocount2 != ammocount2)
		{
			oldammo1 = ammo1;
			oldammo2 = ammo2;
			oldammocount1 = ammocount1;
			oldammocount2 = ammocount2;
			GC::WriteBarrier(this, ammo1);
			GC::WriteBarrier(this, ammo2);
			AmmoRefresh = screen->GetPageCount ();
		}
		if (AmmoRefresh)
		{
			AmmoRefresh--;
			DrawImage (Images[imgBLACKSQ], 108, 3);
			if (ammo2 != NULL)
			{ // Draw both ammos
				screen->DrawTexture (TexMan[ammo1->Icon], 115+ST_X, 11+ST_Y,
					DTA_CenterOffset, true,
					DTA_Bottom320x200, Scaled,
					TAG_DONE);
				DrSmallNumber (ammo1->Amount, 124, 7);
				screen->DrawTexture (TexMan[ammo2->Icon], 115+ST_X, 22+ST_Y,
					DTA_CenterOffset, true,
					DTA_Bottom320x200, Scaled,
					TAG_DONE);
				DrSmallNumber (ammo2->Amount, 124, 19);
			}
			else if (ammo1 != NULL)
			{ // Draw just one ammo
				DrINumber (ammo1->Amount, 109, 4);
				screen->DrawTexture (TexMan[ammo1->Icon], 123+ST_X, 22+ST_Y,
					DTA_CenterOffset, true,
					DTA_Bottom320x200, Scaled,
					TAG_DONE);
			}
		}

		// Armor
		AInventory *armor = CPlayer->mo->FindInventory<ABasicArmor>();
		int armorpoints = armor != NULL ? armor->Amount : 0;
		if (oldarmor != armorpoints)
		{
			oldarmor = armorpoints;
			ArmorRefresh = screen->GetPageCount ();
		}
		if (ArmorRefresh)
		{
			ArmorRefresh--;
			DrawImage (Images[imgARMCLEAR], 224, 13);
			DrINumber (armorpoints, 228, 12);
		}
	}

//---------------------------------------------------------------------------
//
// PROC DrawInventoryBar
//
//---------------------------------------------------------------------------

	void DrawInventoryBar ()
	{
		AInventory *item;
		int i;

		DrawImage (Images[imgINVBAR], 34, 2);
		CPlayer->mo->InvFirst = ValidateInvFirst (7);
		if (CPlayer->mo->InvFirst != NULL)
		{
			for (item = CPlayer->mo->InvFirst, i = 0; item != NULL && i < 7; item = item->NextInv(), ++i)
			{
				DrawDimImage (TexMan(item->Icon), 50+i*31, 2, item->Amount <= 0);
				if (item->Amount != 1)
				{
					DrSmallNumber (item->Amount, 65+i*31, 24);
				}
				if (item == CPlayer->mo->InvSel)
				{
					DrawImage (Images[imgSELECTBOX], 50+i*31, 31);
				}
			}
			// Is there something to the left?
			if (CPlayer->mo->FirstInv() != CPlayer->mo->InvFirst)
			{
				DrawImage (Images[!(gametic & 4) ?
					imgINVLFGEM1 : imgINVLFGEM2], 38, 1);
			}
			// Is there something to the right?
			if (item != NULL)
			{
				DrawImage (Images[!(gametic & 4) ?
					imgINVRTGEM1 : imgINVRTGEM2], 269, 1);
			}
		}
	}

//---------------------------------------------------------------------------
//
// PROC DrawFullScreenStuff
//
//---------------------------------------------------------------------------

	void DrawFullScreenStuff ()
	{
		AInventory *item;
		FTexture *pic;
		int i;

		// Draw health
		if (CPlayer->mo->health > 0)
		{
			pic = Images[imgPTN1 + gametic/3%3];
			screen->DrawTexture (pic, 48, -3,
				DTA_HUDRules, HUD_Normal,
				DTA_LeftOffset, pic->GetWidth()/2,
				DTA_TopOffset, pic->GetHeight(),
				TAG_DONE);
			DrBNumberOuter (CPlayer->mo->health, 5, -21);
		}
		else
		{
			DrBNumberOuter (0, 5, -20);
		}

		// Draw armor
		ABasicArmor *armor = CPlayer->mo->FindInventory<ABasicArmor>();
		if (armor != NULL && armor->Amount != 0)
		{
			pic = TexMan(armor->Icon);
			if (pic != NULL)
			{
				screen->DrawTexture (pic, 56, -24,
					DTA_HUDRules, HUD_Normal,
					DTA_LeftOffset, pic->GetWidth()/2,
					DTA_TopOffset, pic->GetHeight(),
					TAG_DONE);
			}
			DrBNumberOuter (armor->Amount, 5, -43);
		}

		if (deathmatch)
		{
			// Draw frag count
			DrINumberOuter (CPlayer->fragcount, 45, -16);
		}
		else
		{
			// Draw keys
			int playerkeys = 0;

			for (item = CPlayer->mo->Inventory; item != NULL; item = item->Inventory)
			{
				if (item->IsKindOf (RUNTIME_CLASS(AKey)))
				{
					int keynum = static_cast<const AKey*>(item)->KeyNumber;
					if (keynum >= 1 && keynum <= 3)
					{
						playerkeys |= 1 << (keynum-1);
					}
				}
			}
			i = -7;
			if (playerkeys & 2)
			{
				screen->DrawTexture (Images[imgBKEYICON], 54, i,
					DTA_HUDRules, HUD_Normal,
					TAG_DONE);
				i -= 8;
			}
			if (playerkeys & 1)
			{
				screen->DrawTexture (Images[imgGKEYICON], 54, i,
					DTA_HUDRules, HUD_Normal,
					TAG_DONE);
				i -= 8;
			}
			if (playerkeys & 4)
			{
				screen->DrawTexture (Images[imgYKEYICON], 54, i,
					DTA_HUDRules, HUD_Normal,
					TAG_DONE);
			}
		}

		// Draw ammo
		AAmmo *ammo1, *ammo2;
		int ammocount1, ammocount2;

		GetCurrentAmmo (ammo1, ammo2, ammocount1, ammocount2);
		if (ammo1 != NULL)
		{
			// Draw primary ammo in the bottom-right corner
			DrINumberOuter (ammo1->Amount, -29, -15);
			screen->DrawTexture (TexMan(ammo1->Icon), -14, -22,
				DTA_HUDRules, HUD_Normal,
				DTA_CenterBottomOffset, true,
				TAG_DONE);
			if (ammo2 != NULL && ammo2!=ammo1)
			{
				// Draw secondary ammo just above the primary ammo
				DrINumberOuter (ammo2->Amount, -29, -56);
				screen->DrawTexture (TexMan(ammo2->Icon), -14, -63,
					DTA_HUDRules, HUD_Normal,
					DTA_CenterBottomOffset, true,
					TAG_DONE);
			}
		}

		// Draw inventory
		if (CPlayer->inventorytics == 0)
		{
			if (ArtifactFlash)
			{
				screen->DrawTexture (Images[imgARTIBOX], -61, -31,
					DTA_HUDRules, HUD_Normal,
					DTA_Alpha, TRANSLUC50,
					TAG_DONE);
				screen->DrawTexture (Images[imgUSEARTIA + ArtifactFlash], -61, -31,
					DTA_HUDRules, HUD_Normal,
					TAG_DONE);
			}
			else if (CPlayer->mo->InvSel != NULL)
			{
				screen->DrawTexture (Images[imgARTIBOX], -61, -31,
					DTA_HUDRules, HUD_Normal,
					DTA_Alpha, TRANSLUC50,
					TAG_DONE);
				screen->DrawTexture (TexMan(CPlayer->mo->InvSel->Icon), -61, -31,
					DTA_HUDRules, HUD_Normal,
					DTA_ColorOverlay, CPlayer->mo->InvSel->Amount > 0 ? 0 : DIM_OVERLAY,
					TAG_DONE);
				if (CPlayer->mo->InvSel->Amount != 1)
				{
					DrSmallNumberOuter (CPlayer->mo->InvSel->Amount, -46, -9, false);
				}
			}
		}
		else
		{
			CPlayer->mo->InvFirst = ValidateInvFirst (7);
			i = 0;
			if (CPlayer->mo->InvFirst != NULL)
			{
				for (item = CPlayer->mo->InvFirst; item != NULL && i < 7; item = item->NextInv(), ++i)
				{
					screen->DrawTexture (Images[imgARTIBOX], -100+i*31, -32,
						DTA_HUDRules, HUD_HorizCenter,
						DTA_Alpha, HX_SHADOW,
						TAG_DONE);
					screen->DrawTexture (TexMan(item->Icon), -100+i*31, -32,
						DTA_HUDRules, HUD_HorizCenter,
						DTA_ColorOverlay, item->Amount > 0 ? 0 : DIM_OVERLAY,
						TAG_DONE);
					if (item->Amount != 1)
					{
						DrSmallNumberOuter (item->Amount, -84+i*31, -10, true);
					}
					if (item == CPlayer->mo->InvSel)
					{
						screen->DrawTexture (Images[imgSELECTBOX], -100+i*31, -3,
							DTA_HUDRules, HUD_HorizCenter,
							TAG_DONE);
					}
				}
				// Is there something to the left?
				if (CPlayer->mo->FirstInv() != CPlayer->mo->InvFirst)
				{
					screen->DrawTexture (Images[!(gametic & 4) ?
						imgINVLFGEM1 : imgINVLFGEM2], -112, -33,
						DTA_HUDRules, HUD_HorizCenter, TAG_DONE);
				}
				// Is there something to the right?
				if (item != NULL)
				{
					screen->DrawTexture (Images[!(gametic & 4) ?
						imgINVRTGEM1 : imgINVRTGEM2], 119, -33,
						DTA_HUDRules, HUD_HorizCenter, TAG_DONE);
				}
			}
			for (; i < 7; i++)
			{
				screen->DrawTexture (Images[imgARTIBOX], -100+i*31, -32,
					DTA_HUDRules, HUD_HorizCenter,
					DTA_Alpha, HX_SHADOW,
					TAG_DONE);
			}
		}
	}

//---------------------------------------------------------------------------
//
// PROC FlashItem
//
//---------------------------------------------------------------------------

	void FlashItem (const PClass *itemtype)
	{
		ArtifactFlash = 4;
	}

	static const char patcharti[][10];
	static const char ammopic[][10];

	TObjPtr<AInventory> oldarti;
	TObjPtr<AAmmo> oldammo1, oldammo2;
	int oldammocount1, oldammocount2;
	int oldartiCount;
	int oldfrags;
	int oldarmor;
	int oldhealth;
	int oldlife;
	int oldkeys;

	enum
	{
		imgLTFACE,
		imgRTFACE,
		imgBARBACK,
		imgINVBAR,
		imgCHAIN,
		imgSTATBAR,
		imgLIFEGEM,
		imgLTFCTOP,
		imgRTFCTOP,
		imgSELECTBOX,
		imgINVLFGEM1,
		imgINVLFGEM2,
		imgINVRTGEM1,
		imgINVRTGEM2,
		imgBLACKSQ,
		imgARMCLEAR,
		imgCHAINBACK,
		imgGOD1,
		imgGOD2,
		imgUSEARTIA,
		imgUSEARTIB,
		imgUSEARTIC,
		imgUSEARTID,
		imgYKEYICON,
		imgGKEYICON,
		imgBKEYICON,
		imgARTIBOX,
		imgPTN1,
		imgPTN2,
		imgPTN3,

		NUM_HERETICSB_IMAGES
	};

	FImageCollection Images;

	int HealthMarker;
	int ChainWiggle;
	int ArtifactFlash;

	char HealthRefresh;
	char ArtiRefresh;
	char FragHealthRefresh;
	char KeysRefresh;
	char AmmoRefresh;
	char ArmorRefresh;
};

IMPLEMENT_POINTY_CLASS(DHereticStatusBar)
 DECLARE_POINTER(oldarti)
 DECLARE_POINTER(oldammo1)
 DECLARE_POINTER(oldammo2)
END_POINTERS

DBaseStatusBar *CreateHereticStatusBar ()
{
	return new DHereticStatusBar;
}
