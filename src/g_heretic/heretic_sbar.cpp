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

// I've had an instance where two machines got different values for
// this RNG. I don't know how, so I've just removed the name so that
// it won't count towards game consistancy.

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


class FHereticStatusBar : public FBaseStatusBar
{
public:
	FHereticStatusBar () : FBaseStatusBar (42)
	{
		static const char *hereticLumpNames[NUM_HERETICSB_IMAGES] =
		{
			"LTFACE",	"RTFACE",	"BARBACK",	"INVBAR",	"CHAIN",
			NULL,		"LIFEGEM2",	"LTFCTOP",	"RTFCTOP",	"SELECTBOX",
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

		FBaseStatusBar::Images.Init (sharedLumpNames, NUM_BASESB_IMAGES);
		Images.Init (hereticLumpNames, NUM_HERETICSB_IMAGES);

		oldarti = 0;
		oldartiCount = 0;
		oldfrags = -9999;
		oldammo = -1;
		oldarmor = -1;
		oldweapon = -1;
		oldhealth = -1;
		oldlife = -1;
		oldkeys = -1;

		HealthMarker = 0;
		ChainWiggle = 0;
		ArtifactFlash = 0;
	}

	~FHereticStatusBar ()
	{
	}

	void Tick ()
	{
		int curHealth;

		FBaseStatusBar::Tick ();
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
			ArtifactFlash--;
		}
	}

	void Draw (EHudState state)
	{
		int frame;
		static bool hitCenterFrame;
		int picnum;

		FBaseStatusBar::Draw (state);

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
					oldarti = 0;
					oldammo = -1;
					oldarmor = -1;
					oldweapon = -1;
					oldfrags = -9999; //can't use -1, 'cuz of negative frags
					oldlife = -1;
					oldkeys = -1;
					oldhealth = -1;
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

		frame = (level.time/3) & 15;

		// Flight icons
		if (CPlayer->powers[pw_flight])
		{
			if (CPlayer->powers[pw_flight] > BLINKTHRESHOLD
				|| !(CPlayer->powers[pw_flight] & 16))
			{
				picnum = TexMan.GetTexture ("SPFLY0", FTexture::TEX_MiscPatch);

				if (CPlayer->mo->flags2 & MF2_FLY)
				{
					if (hitCenterFrame && (frame != 15 && frame != 0))
					{
						DrawOuterTexture (TexMan[picnum+15], 20, 17);
					}
					else
					{
						DrawOuterTexture (TexMan[picnum+frame], 20, 17);
						hitCenterFrame = false;
					}
				}
				else
				{
					if (!hitCenterFrame && (frame != 15 && frame != 0))
					{
						DrawOuterTexture (TexMan[picnum+frame], 20, 17);
						hitCenterFrame = false;
					}
					else
					{
						DrawOuterTexture (TexMan[picnum+15], 20, 17);
						hitCenterFrame = true;
					}
				}
			}
			BorderTopRefresh = screen->GetPageCount ();
		}

		if (CPlayer->powers[pw_weaponlevel2] && !CPlayer->morphTics)
		{
			if (CPlayer->powers[pw_weaponlevel2] > BLINKTHRESHOLD
				|| !(CPlayer->powers[pw_weaponlevel2] & 16))
			{
				picnum = TexMan.GetTexture ("SPINBK0", FTexture::TEX_MiscPatch);
				DrawOuterTexture (TexMan[picnum+frame], -20, 17);
			}
			BorderTopRefresh = screen->GetPageCount ();
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
			DTA_320x200, Scaled,
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
				translationtables[TRANSLATION_PlayersExtra] + (CPlayer-players)*256 : NULL);
			DrawImage (Images[imgLTFACE], 0, 32);
			DrawImage (Images[imgRTFACE], 276, 32);
			screen->DrawTexture (&ChainShade, ST_X+19, ST_Y+32,
				DTA_320x200, Scaled,
				DTA_AlphaChannel, true,
				DTA_FillColor, 0,
				TAG_DONE);
			screen->DrawTexture (&ChainShade, ST_X+277, ST_Y+32,
				DTA_320x200, Scaled,
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
		int i;
		int temp;
		int playerkeys;
		int ammo;

		// Ready artifact
		if (ArtifactFlash)
		{
			DrawImage (Images[imgBLACKSQ], 180, 3);
			DrawImage (Images[imgUSEARTIA + ArtifactFlash], 182, 3);
			oldarti = -1; // so that the correct artifact fills in after the flash
		}
		else if (oldarti != CPlayer->readyArtifact
			|| oldartiCount != CPlayer->inventory[oldarti])
		{
			oldarti = CPlayer->readyArtifact;
			oldartiCount = CPlayer->inventory[oldarti];
			ArtiRefresh = screen->GetPageCount ();
		}
		if (ArtiRefresh)
		{
			ArtiRefresh--;
			DrawImage (Images[imgBLACKSQ], 180, 3);
			if (CPlayer->inventory[oldarti] > 0)
			{
				DrawImage (ArtiImages[oldarti], 179, 2);
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
			temp = HealthMarker;
			if (temp < 0)
			{
				temp = 0;
			}
			else if (temp > 100)
			{
				temp = 100;
			}
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
		
		for (i = 0; i < NUMKEYS; i++)
		{
			playerkeys <<= 1;
			if (CPlayer->keys[i])
				playerkeys |= 1;
		}

		if (oldkeys != playerkeys)
		{
			oldkeys = playerkeys;
			KeysRefresh = screen->GetPageCount ();
		}
		if (KeysRefresh)
		{
			KeysRefresh--;
			if (CPlayer->keys[key_yellow])
			{
				DrawImage (Images[imgYKEYICON], 153, 6);
			}
			if (CPlayer->keys[key_green])
			{
				DrawImage (Images[imgGKEYICON], 153, 14);
			}
			if (CPlayer->keys[key_blue])
			{
				DrawImage (Images[imgBKEYICON], 153, 22);
			}
		}
		// Ammo
		ammo = wpnlev1info[CPlayer->readyweapon]->ammo;
		if (ammo < NUMAMMO)
		{
			temp = CPlayer->ammo[ammo];
		}
		else if (ammo == MANA_BOTH)
		{
			temp = MIN (CPlayer->ammo[MANA_1], CPlayer->ammo[MANA_2]);
		}
		else
		{
			temp = -2;
		}
		if (oldammo != temp || oldweapon != CPlayer->readyweapon)
		{
			oldammo = temp;
			oldweapon = CPlayer->readyweapon;
			AmmoRefresh = screen->GetPageCount ();
		}
		if (AmmoRefresh)
		{
			AmmoRefresh--;
			DrawImage (Images[imgBLACKSQ], 108, 3);
			if (oldammo >= 0)
			{
				DrINumber (oldammo, 109, 4);
				DrawImage (AmmoImages[ammo], 111, 14);
			}
		}

		// Armor
		if (oldarmor != CPlayer->armorpoints[0])
		{
			oldarmor = CPlayer->armorpoints[0];
			ArmorRefresh = screen->GetPageCount ();
		}
		if (ArmorRefresh)
		{
			ArmorRefresh--;
			DrawImage (Images[imgARMCLEAR], 224, 13);
			DrINumber (CPlayer->armorpoints[0], 228, 12);
		}
	}

//---------------------------------------------------------------------------
//
// PROC DrawInventoryBar
//
//---------------------------------------------------------------------------

	void DrawInventoryBar ()
	{
		int i;
		int x;
		bool left, right;

		DrawImage (Images[imgINVBAR], 34, 2);
		FindInventoryPos (x, left, right);
		if (x > 0)
		{
			for (i = 0; i < 7 && x < NUMINVENTORYSLOTS; x++)
			{
				if (CPlayer->inventory[x])
				{
					DrawImage (ArtiImages[x], 50+i*31, 2);
					if (CPlayer->inventory[x] != 1)
					{
						DrSmallNumber (CPlayer->inventory[x], 65+i*31, 24);
					}
					if (x == CPlayer->readyArtifact)
					{
						DrawImage (Images[imgSELECTBOX], 50+i*31, 31);
					}
					i++;
				}
			}
			if (left)
			{
				DrawImage (Images[!(level.time & 4) ?
					imgINVLFGEM1 : imgINVLFGEM2], 38, 1);
			}
			if (right)
			{
				DrawImage (Images[!(level.time & 4) ?
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
		int i;
		int x;

		// Draw health
		if (CPlayer->mo->health > 0)
		{
			OverrideImageOrigin (true);
			DrawOuterImage (Images[imgPTN1 + gametic/3%3], 48, -3);
			OverrideImageOrigin (false);
			DrBNumberOuter (CPlayer->mo->health, 5, -21);
		}
		else
		{
			DrBNumberOuter (0, 5, -20);
		}

		// Draw armor
		if (CPlayer->armortype && CPlayer->armorpoints[0])
		{
			OverrideImageOrigin (true);
			DrawOuterImage (ArmorImages[CPlayer->armortype != deh.GreenAC], 56, -24);
			OverrideImageOrigin (false);
			DrBNumberOuter (CPlayer->armorpoints[0], 5, -43);
		}

		if (deathmatch)
		{
			// Draw frag count
			DrINumberOuter (CPlayer->fragcount, 45, -16);
		}
		else
		{
			// Draw keys
			i = -8;
			if (CPlayer->keys[key_blue])
			{
				DrawOuterImage (Images[imgBKEYICON], 45, i);
				i -= 8;
			}
			if (CPlayer->keys[key_green])
			{
				DrawOuterImage (Images[imgGKEYICON], 45, i);
				i -= 8;
			}
			if (CPlayer->keys[key_yellow])
			{
				DrawOuterImage (Images[imgYKEYICON], 45, i);
			}
		}

		// Draw ammo
		i = wpnlev1info[CPlayer->readyweapon]->ammo;
		if (i < NUMAMMO || i == MANA_BOTH)
		{
			int amt;

			if (i == MANA_BOTH)
			{
				amt = MIN (CPlayer->ammo[MANA_1], CPlayer->ammo[MANA_2]);
			}
			else
			{
				amt = CPlayer->ammo[i];
			}
			DrINumberOuter (amt, -29, -15);
			DrawOuterImage (AmmoImages[i], -27, -30);
		}

		// Draw inventory
		if (CPlayer->inventorytics == 0)
		{
			if (ArtifactFlash)
			{
				DrawOuterFadedImage (Images[imgARTIBOX], -61, -31, TRANSLUC50);
				DrawOuterImage (Images[imgUSEARTIA + ArtifactFlash], -61, -31);
			}
			else if (CPlayer->inventory[CPlayer->readyArtifact] > 0)
			{
				DrawOuterFadedImage (Images[imgARTIBOX], -61, -31, TRANSLUC50);
				DrawOuterImage (ArtiImages[CPlayer->readyArtifact], -61, -31);
				if (CPlayer->inventory[CPlayer->readyArtifact] != 1)
				{
					DrSmallNumberOuter (CPlayer->inventory[CPlayer->readyArtifact],
						-46, -9);
				}
			}
		}
		else
		{
			SetHorizCentering (true);
			bool left, right;

			FindInventoryPos (x, left, right);
			for (i = 0; i < 7 && x < NUMINVENTORYSLOTS; x++)
			{
				if (CPlayer->inventory[x])
				{
					DrawOuterFadedImage (Images[imgARTIBOX], -100+i*31, -32, TRANSLUC50);
					DrawOuterImage (ArtiImages[x], -100+i*31, -32);
					if (CPlayer->inventory[x] != 1)
					{
						DrSmallNumberOuter (CPlayer->inventory[x], -81+i*31, -10);
					}
					if (x == CPlayer->readyArtifact)
					{
						DrawOuterImage (Images[imgSELECTBOX], -100+i*31, -3);
					}
					i++;
				}
			}
			if (i < 7)
			{
				for (; i < 7; i++)
				{
					DrawOuterFadedImage (Images[imgARTIBOX], -100+i*31, -32, TRANSLUC50);
				}
			}
			if (left)
			{
				DrawOuterImage (Images[!(level.time & 4) ?
					imgINVLFGEM1 : imgINVLFGEM2], -112, -33);
			}
			if (right)
			{
				DrawOuterImage (Images[!(level.time & 4) ?
					imgINVRTGEM1 : imgINVRTGEM2], 119, -33);
			}
			SetHorizCentering (false);
		}
	}

//---------------------------------------------------------------------------
//
// PROC FlashArtifact
//
//---------------------------------------------------------------------------

	void FlashArtifact (int arti)
	{
		ArtifactFlash = 4;
	}

	static const char patcharti[][10];
	static const char ammopic[][10];

	int oldarti;
	int oldartiCount;
	int oldfrags;
	int oldammo;
	int oldarmor;
	int oldweapon;
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

FBaseStatusBar *CreateHereticStatusBar ()
{
	return new FHereticStatusBar;
}
