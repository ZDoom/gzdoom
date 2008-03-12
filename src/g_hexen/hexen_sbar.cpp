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
#include "a_hexenglobal.h"
#include "a_keys.h"
#include "r_translate.h"


class FManaBar : public FTexture
{
public:
	FManaBar ();

	const BYTE *GetColumn (unsigned int column, const Span **spans_out);
	const BYTE *GetPixels ();
	void Unload ();
	bool CheckModified ();

	void SetVial (FTexture *pic, AActor *actor, const PClass *manaType);

protected:
	BYTE Pixels[5*24];
	static const Span DummySpan[2];

	FTexture *VialPic;
	int VialLevel;
	bool NeedRefresh;

	void MakeTexture ();
};

const FTexture::Span FManaBar::DummySpan[2] = { { 0, 24 }, { 0, 0 } };

FManaBar::FManaBar ()
: VialPic(0), VialLevel(0), NeedRefresh(false)
{
	Width = 5;
	Height = 24;
	WidthBits = 2;
	HeightBits = 5;
	WidthMask = 3;
}

void FManaBar::Unload ()
{
	if (VialPic != 0)
	{
		VialPic->Unload ();
	}
}

bool FManaBar::CheckModified ()
{
	return NeedRefresh;
}

const BYTE *FManaBar::GetColumn (unsigned int column, const Span **spans_out)
{
	if (NeedRefresh)
	{
		MakeTexture ();
	}
	if (column > 4)
	{
		column = 4;
	}
	if (spans_out != NULL)
	{
		*spans_out = DummySpan;
	}
	return Pixels + column*24;
}

const BYTE *FManaBar::GetPixels ()
{
	if (NeedRefresh)
	{
		MakeTexture ();
	}
	return Pixels;
}

void FManaBar::SetVial (FTexture *pic, AActor *actor, const PClass *manaType)
{
	int level;
	AInventory *ammo;

	ammo = actor->FindInventory (manaType);
	level = ammo != NULL ? ammo->Amount : 0;
	level = MIN (22*level/MAX_MANA, 22);
	if (VialPic != pic || VialLevel != level)
	{
		VialPic = pic;
		VialLevel = level;
		NeedRefresh = true;
	}
}

void FManaBar::MakeTexture ()
{
	int run = 22 - VialLevel;
	BYTE color0 = GPalette.Remap[0];

	NeedRefresh = false;
	VialPic->CopyToBlock (Pixels, 5, 24, 0, 0);
	memset (Pixels + 25, color0, run);
	memset (Pixels + 25+24, color0, run);
	memset (Pixels + 25+24+24, color0, run);
}

class DHexenStatusBar : public DBaseStatusBar
{
	DECLARE_CLASS(DHexenStatusBar, DBaseStatusBar)
	HAS_OBJECT_POINTERS
public:
	DHexenStatusBar () : DBaseStatusBar (38)
	{
		static const char *hexenLumpNames[NUM_HEXENSB_IMAGES] =
		{
			"H2BAR",	"H2TOP",	"INVBAR",	"LFEDGE",	"RTEDGE",
			"STATBAR",	"KEYBAR",	"SELECTBO",	"ARTICLS",	"ARMCLS",
			"MANACLS",	"MANAVL1",	"MANAVL2",	"MANAVL1D",	"MANAVL2D",
			"MANADIM1",	"MANADIM2",	"MANABRT1",	"MANABRT2",	"INVGEML1",
			"INVGEML2",	"INVGEMR1",	"INVGEMR2",	"KILLS",	"USEARTIA",
			"USEARTIB",	"USEARTIC",	"USEARTID",	"USEARTIE",	"KEYSLOT1",
			"KEYSLOT2",	"KEYSLOT3",	"KEYSLOT4",	"KEYSLOT5",	"KEYSLOT6",
			"KEYSLOT7",	"KEYSLOT8",	"KEYSLOT9",	"KEYSLOTA",	"KEYSLOTB",
			"ARMSLOT1",	"ARMSLOT2",	"ARMSLOT3",	"ARMSLOT4",	"ARTIBOX",
			"HAMOBACK"
		};
		static const char *classLumpNames[3][NUM_HEXENCLASSSB_IMAGES] =
		{
			{
				"WPSLOT0",	"WPFULL0",	"WPIECEF1",	"WPIECEF2",
				"WPIECEF3",	"CHAIN",	"LIFEGMF2"
			},
			{
				"WPSLOT1",	"WPFULL1",	"WPIECEC1",	"WPIECEC2",
				"WPIECEC3",	"CHAIN2",	"LIFEGMC2"
			},
			{
				"WPSLOT2",	"WPFULL2",	"WPIECEM1",	"WPIECEM2",
				"WPIECEM3",	"CHAIN3",	"LIFEGMM2"
			}
		};
		static const char *sharedLumpNames[] =
		{
			"LAME",		"NEGNUM",	"IN0",		"IN1",		"IN2",
			"IN3",		"IN4",		"IN5",		"IN6",		"IN7",
			"IN8",		"IN9",		"FONTB13",	"FONTB16",	"FONTB17",
			"FONTB18",	"FONTB19",	"FONTB20",	"FONTB21",	"FONTB22",
			"FONTB23",	"FONTB24",	"FONTB25",	"SMALLIN0",	"SMALLIN1",
			"SMALLIN2",	"SMALLIN3",	"SMALLIN4",	"SMALLIN5",	"SMALLIN6",
			"SMALLIN7",	"SMALLIN8",	"SMALLIN9",

			"INRED0",	"INRED1",	"INRED2",	"INRED3",	"INRED4",
			"INRED5",	"INRED6",	"INRED7",	"INRED8",	"INRED9"
		};

		DBaseStatusBar::Images.Init (sharedLumpNames, NUM_BASESB_IMAGES + 10);
		Images.Init (hexenLumpNames, NUM_HEXENSB_IMAGES);
		ClassImages[0].Init (classLumpNames[0], NUM_HEXENCLASSSB_IMAGES);
		ClassImages[1].Init (classLumpNames[1], NUM_HEXENCLASSSB_IMAGES);
		ClassImages[2].Init (classLumpNames[2], NUM_HEXENCLASSSB_IMAGES);

		oldarti = NULL;
		oldartiCount = 0;
		oldammo1 = oldammo2 = NULL;
		oldammocount1 = oldammocount2 = -1;
		oldfrags = -9999;
		oldmana1 = -1;
		oldmana2 = -1;
		oldusemana1 = -1;
		oldusemana2 = -1;
		olddrawbars = -1;
		oldarmor = -1;
		oldhealth = -1;
		oldlife = -1;
		oldpieces = -1;
		oldkeys[0] = oldkeys[1] = oldkeys[2] = oldkeys[3] = oldkeys[4] = NULL;

		HealthMarker = 0;
		ArtifactFlash = 0;

		ArtiRefresh = 0;
		FragHealthRefresh = 0;
		KeysRefresh = 0;
		ArmorRefresh = 0;
		HealthRefresh = 0;
		Mana1Refresh = 0;
		Mana2Refresh = 0;
		AmmoRefresh = 0;
	}

	~DHexenStatusBar ()
	{
	}

	void Tick ()
	{
		int curHealth;

		DBaseStatusBar::Tick ();
		if (CPlayer->mo == NULL)
		{
			curHealth = 0;
		}
		else
		{
			curHealth = CPlayer->mo->health;
		}
		if (curHealth < 0)
		{
			curHealth = 0;
		}
		if (curHealth < HealthMarker)
		{
			HealthMarker -= clamp ((HealthMarker - curHealth) >> 2, 1, 6);
		}
		else if (curHealth > HealthMarker)
		{
			HealthMarker += clamp ((curHealth - HealthMarker) >> 2, 1, 6);
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
				DrawImage (Images[imgH2BAR], 0, -27);
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
					DrawImage (Images[!automapactive ? imgSTATBAR : imgKEYBAR], 38, 0);
					oldarti = NULL;
					oldammo1 = oldammo2 = NULL;
					oldmana1 = -1;
					oldmana2 = -1;
					oldusemana1 = -1;
					oldusemana2 = -1;
					olddrawbars = -1;
					oldarmor = -1;
					oldpieces = -1;
					oldfrags = -9999; //can't use -1, 'cuz of negative frags
					oldlife = -1;
					oldkeys[0] = oldkeys[1] = oldkeys[2] = oldkeys[3] = oldkeys[4] = NULL;
					ArtiRefresh = 0;
					//oldhealth = -1;
				}
				if (!automapactive)
				{
					DrawMainBar ();
				}
				else
				{
					DrawKeyBar ();
				}
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
				}
				DrawInventoryBar ();
			}
		}
	}

	void AttachToPlayer (player_s *player)
	{
		DBaseStatusBar::AttachToPlayer (player);
		if (player->mo != NULL)
		{
			if (player->mo->IsKindOf (PClass::FindClass(NAME_MagePlayer)))
			{
				FourthWeaponShift = 6;
				FourthWeaponClass = 2;
				LifeBarClass = 2;
			}
			else if (player->mo->IsKindOf (PClass::FindClass(NAME_ClericPlayer)))
			{
				FourthWeaponShift = 3;
				FourthWeaponClass = 1;
				LifeBarClass = 1;
			}
			else
			{
				FourthWeaponShift = 0;
				FourthWeaponClass = 0;
				LifeBarClass = 0;
			}
		}
	}

	void SetInteger (int pname, int param)
	{
		if (pname == 0)
		{
			FourthWeaponShift = param;
			FourthWeaponClass = param / 3;
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
		int healthPos;

		DrawImage (Images[imgH2TOP], 0, -27);

		if (oldhealth != HealthMarker)
		{
			oldhealth = HealthMarker;
			HealthRefresh = screen->GetPageCount ();
		}
		if (HealthRefresh)
		{
			int lifeClass = LifeBarClass;
			
			HealthRefresh--;
			healthPos = clamp (HealthMarker, 0, 100);
			DrawImage (ClassImages[lifeClass][imgCHAIN], 35+((healthPos*196/100)%9), 31);
			DrawImage (ClassImages[lifeClass][imgLIFEGEM], 7+(healthPos*11/5), 31, multiplayer ?
				translationtables[TRANSLATION_PlayersExtra][int(CPlayer - players)] : NULL);
			DrawImage (Images[imgLFEDGE], 0, 31);
			DrawImage (Images[imgRTEDGE], 277, 31);
		}
	}

//---------------------------------------------------------------------------
//
// PROC DrawMainBar
//
//---------------------------------------------------------------------------

	void DrawMainBar ()
	{
		int temp;

		// Ready artifact
		if (ArtifactFlash)
		{
			DrawImage (Images[imgARTICLEAR], 144, -1);
			DrawImage (Images[imgUSEARTIA + ArtifactFlash], 148, 2);
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
			DrawImage (Images[imgARTICLEAR], 144, -1);
			if (oldarti != NULL)
			{
				DrawDimImage (TexMan(oldarti->Icon), 143, 2, oldarti->Amount <= 0);
				if (oldartiCount != 1)
				{
					DrSmallNumber (oldartiCount, 162, 23);
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
				DrawImage (Images[imgKILLS], 38, 1);
				DrINumber (temp, 40, 15);
			}
		}
		else
		{
			temp = MAX (0, HealthMarker);
			if (oldlife != temp)
			{
				oldlife = temp;
				FragHealthRefresh = screen->GetPageCount ();
			}
			if (FragHealthRefresh)
			{
				FragHealthRefresh--;
				DrawImage (Images[imgARMCLEAR], 41, 16);
				DrINumber (temp, 40, 14, temp >= 25 ? imgINumbers : NUM_BASESB_IMAGES);
			}
		}

		// Mana
		AAmmo *ammo1, *ammo2;
		int ammocount1, ammocount2;
		int drawbar;

		GetCurrentAmmo (ammo1, ammo2, ammocount1, ammocount2);
		if (ammo1==ammo2)
		{
			// Don't show the same ammo twice.
			ammo2=NULL;
		}

		// If the weapon uses some ammo that is not mana, do not draw
		// the mana bars; draw the specific used ammo instead.
		const PClass *mana1 = PClass::FindClass(NAME_Mana1);
		const PClass *mana2 = PClass::FindClass(NAME_Mana2);

		drawbar = !((ammo1 != NULL && ammo1->GetClass() != mana1 && ammo1->GetClass() != mana2) ||
				    (ammo2 != NULL && ammo2->GetClass() != mana1 && ammo2->GetClass() != mana2));

		if (drawbar != olddrawbars)
		{
			AmmoRefresh = screen->GetPageCount ();
			olddrawbars = drawbar;
			oldmana1 = -1;
			oldmana2 = -1;
		}
		if (drawbar && oldammo2 != ammo2)
		{
			AmmoRefresh = screen->GetPageCount ();
		}
		if (drawbar)
		{
			DrawManaBars (ammo1, ammo2, mana1, mana2);
		}
		else
		{
			DrawMainAltAmmo (ammo1, ammo2, ammocount1, ammocount2);
		}

		// Armor
		temp = GetArmorPercent (NULL);
		if (oldarmor != temp)
		{
			oldarmor = temp;
			ArmorRefresh = screen->GetPageCount ();
		}
		if (ArmorRefresh)
		{
			ArmorRefresh--;
			DrawImage (Images[imgARMCLEAR], 255, 16);
			DrINumber (temp / (5*FRACUNIT), 250, 14);
		}

		// Weapon Pieces
		if (oldpieces != CPlayer->pieces)
		{
			DrawWeaponPieces();
			oldpieces = CPlayer->pieces;
		}
	}

//---------------------------------------------------------------------------
//
// PROC DrawMainAltAmmo
//
// Draw a generic ammo readout on the main bar, instead of the mana bars.
//
//---------------------------------------------------------------------------

	void DrawMainAltAmmo (AAmmo *ammo1, AAmmo *ammo2, int ammocount1, int ammocount2)
	{
		if (ammo1 != oldammo1 || ammocount1 != oldammocount1)
		{
			AmmoRefresh = screen->GetPageCount ();
			oldammo1 = ammo1;
			oldammocount1 = ammocount1;
			GC::WriteBarrier(this, ammo1);
		}
		if (ammo2 != oldammo2 || ammocount2 != oldammocount2)
		{
			AmmoRefresh = screen->GetPageCount ();
			oldammo2 = ammo2;
			oldammocount2 = ammocount2;
			GC::WriteBarrier(this, ammo2);
		}

		if (AmmoRefresh)
		{
			AmmoRefresh--;
			DrawImage (Images[imgAMMOBACK], 77, 2);
			if (ammo2 != NULL)
			{ // Draw both ammos
				AmmoRefresh--;
				screen->DrawTexture (TexMan[ammo1->Icon], 89+ST_X, 10+ST_Y,
					DTA_CenterOffset, true,
					DTA_Bottom320x200, true,
					TAG_DONE);
				DrSmallNumber (ammo1->Amount, 86, 20);

				screen->DrawTexture (TexMan[ammo2->Icon], 113+ST_X, 10+ST_Y,
					DTA_CenterOffset, true,
					DTA_Bottom320x200, true,
					TAG_DONE);
				DrSmallNumber (ammo2->Amount, 110, 20);
			}
			else
			{ // Draw one ammo
				screen->DrawTexture (TexMan[ammo1->Icon], 100+ST_X, 10+ST_Y,
					DTA_CenterOffset, true,
					DTA_Bottom320x200, true,
					TAG_DONE);
				DrSmallNumber (ammo1->Amount, 97, 20);
			}
		}
	}

//---------------------------------------------------------------------------
//
// PROC DrawManaBars
//
// Draws the mana bars on the main status bar
//
//---------------------------------------------------------------------------

	void DrawManaBars (AAmmo *ammo1, AAmmo *ammo2, const PClass *manatype1, const PClass *manatype2)
	{
		AAmmo *mana1 = NULL, *mana2 = NULL;
		int usemana1 = false, usemana2 = false;
		int manacount1, manacount2;
		int manaPatch1, manaPatch2;
		int manaVialPatch1, manaVialPatch2;

		manaPatch1 = 0;
		manaPatch2 = 0;
		manaVialPatch1 = 0;
		manaVialPatch2 = 0;

		if (AmmoRefresh)
		{
			Mana1Refresh = MAX(AmmoRefresh, Mana1Refresh);
			Mana2Refresh = MAX(AmmoRefresh, Mana2Refresh);
			AmmoRefresh--;
			screen->DrawTexture (Images[imgSTATBAR], ST_X+38, ST_Y,
				DTA_WindowLeft, 39,
				DTA_WindowRight, 87,
				DTA_Bottom320x200, Scaled,
				TAG_DONE);
		}

		// Locate Mana1 and Mana2 in the inventory, and decide which ones are used.
		if (ammo1 == NULL)
		{
		}
		else if (ammo1->GetClass() == manatype1)
		{
			mana1 = ammo1;
			usemana1 = true;
		}
		else if (ammo1->GetClass() == manatype2)
		{
			mana2 = ammo1;
			usemana2 = true;
		}
		if (ammo2 == NULL)
		{
		}
		else if (ammo2->GetClass() == manatype1)
		{
			mana1 = ammo2;
			usemana1 = true;
		}
		else if (ammo2->GetClass() == manatype2)
		{
			mana2 = ammo2;
			usemana2 = true;
		}
		if (mana1 == NULL)
		{
			mana1 = static_cast<AAmmo*>(CPlayer->mo->FindInventory(manatype1));
		}
		if (mana2 == NULL)
		{
			mana2 = static_cast<AAmmo*>(CPlayer->mo->FindInventory(manatype2));
		}
		manacount1 = mana1 != NULL ? mana1->Amount : 0;
		manacount2 = mana2 != NULL ? mana2->Amount : 0;

		// Has Mana1 changed since last time?
		if (oldmana1 != manacount1 || oldusemana1 != usemana1)
		{
			oldmana1 = manacount1;
			oldusemana1 = usemana1;
			Mana1Refresh = screen->GetPageCount ();
		}

		// Has Mana2 changed since last time?
		if (oldmana2 != manacount2 || oldusemana2 != usemana2)
		{
			oldmana2 = manacount2;
			oldusemana2 = usemana2;
			Mana2Refresh = screen->GetPageCount ();
		}
		// Decide what to draw for vial 1
		if (Mana1Refresh)
		{
			Mana1Refresh--;
			DrawImage (Images[imgMANACLEAR], 77, 16);
			DrSmallNumber (manacount1, 79, 19);
			if (!usemana1)
			{ // Draw Dim Mana icon
				manaPatch1 = imgMANADIM1;
				manaVialPatch1 = imgMANAVIALDIM1;
			}
			else
			{
				manaPatch1 = imgMANABRIGHT1;
				manaVialPatch1 = manacount1 ? imgMANAVIAL1 : imgMANAVIALDIM1;
			}		
		}
		// Decide what to draw for vial 2
		if (Mana2Refresh)
		{
			Mana2Refresh--;
			DrawImage (Images[imgMANACLEAR], 109, 16);
			DrSmallNumber (manacount2, 111, 19);
			if (!usemana2)
			{ // Draw Dim Mana icon
				manaPatch2 = imgMANADIM2;
				manaVialPatch2 = imgMANAVIALDIM2;
			}		
			else
			{
				manaPatch2 = imgMANABRIGHT2;
				manaVialPatch2 = manacount2 ? imgMANAVIAL2 : imgMANAVIALDIM2;
			}		
		}
		// Update mana graphics
		if (manaPatch1 || manaPatch2 || manaVialPatch1)
		{
			if (manaVialPatch1)
			{
				DrawImage (Images[manaPatch1], 77, 2);
				ManaVial1Pic.SetVial (Images[manaVialPatch1], CPlayer->mo, manatype1);
				DrawImage (&ManaVial1Pic, 94, 2);
			}
			if (manaVialPatch2)
			{
				DrawImage (Images[manaPatch2], 110, 2);
				ManaVial2Pic.SetVial (Images[manaVialPatch2], CPlayer->mo, manatype2);
				DrawImage (&ManaVial2Pic, 102, 2);
			}
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

		DrawImage (Images[imgINVBAR], 38, 0);
		CPlayer->mo->InvFirst = ValidateInvFirst (7);
		if (CPlayer->mo->InvFirst != NULL)
		{
			for (item = CPlayer->mo->InvFirst, i = 0; item != NULL && i < 7; item = item->NextInv(), ++i)
			{
				DrawDimImage (TexMan(item->Icon), 50+i*31, 1, item->Amount <= 0);
				if (item->Amount != 1)
				{
					DrSmallNumber (item->Amount, 68+i*31, 23);
				}
				if (item == CPlayer->mo->InvSel)
				{
					DrawImage (Images[imgSELECTBOX], 51+i*31, 1);
				}
			}
			// Is there something to the left?
			if (CPlayer->mo->FirstInv() != CPlayer->mo->InvFirst)
			{
				DrawImage (Images[!(gametic & 4) ?
					imgINVLFGEM1 : imgINVLFGEM2], 42, 1);
			}
			// Is there something to the right?
			if (item != NULL)
			{
				DrawImage (Images[!(gametic & 4) ?
					imgINVRTGEM1 : imgINVRTGEM2], 269, 1);
			}
		}
	}

//==========================================================================
//
// DrawKeyBar
//
//==========================================================================

	void DrawKeyBar ()
	{
		AInventory *item;
		AHexenArmor *armor;
		AKey *keys[5];
		int i;
		int temp;
		bool different;

		keys[0] = keys[1] = keys[2] = keys[3] = keys[4] = NULL;
		for (item = CPlayer->mo->Inventory, i = 0;
			item != NULL && i < 5;
			item = item->Inventory)
		{
			if (item->Icon > 0 &&
				item->IsKindOf (RUNTIME_CLASS(AKey)) &&
				item->GetClass() != RUNTIME_CLASS(AKey))
			{
				keys[i++] = static_cast<AKey*>(item);
			}
		}
		different = false;
		for (i = 0; i < 5; ++i)
		{
			if (keys[i] != oldkeys[i])
			{
				oldkeys[i] = keys[i];
				GC::WriteBarrier(this, keys[i]);
				different = true;
			}
		}
		if (different)
		{
			KeysRefresh = screen->GetPageCount ();
		}
		if (KeysRefresh)
		{
			KeysRefresh--;
			for (i = 0; i < 5 && keys[i] != NULL; i++)
			{
				DrawImage (TexMan[keys[i]->Icon], 46 + i*20, 2);
			}
		}

		temp = GetArmorPercent (&armor);
		if (oldarmor != temp && armor != NULL)
		{
			for (i = 0; i < 4; i++)
			{
				if (armor->Slots[i] > 0 && armor->SlotsIncrement[i] > 0)
				{
					DrawFadedImage (Images[imgARMSLOT1+i], 150+31*i, 2,
						MIN<fixed_t> (OPAQUE, Scale (armor->Slots[i], OPAQUE,
											armor->SlotsIncrement[i])));
				}
			}
			oldarmor = temp;
		}
	}

//==========================================================================
//
// GetArmorPercent
//
//==========================================================================

	fixed_t GetArmorPercent (AHexenArmor **armorp)
	{
		AHexenArmor *harmor = CPlayer->mo->FindInventory<AHexenArmor>();
		fixed_t amount = 0;
		if (harmor != NULL)
		{
			amount  = harmor->Slots[0]
					+ harmor->Slots[1]
					+ harmor->Slots[2]
					+ harmor->Slots[3]
					+ harmor->Slots[4];
		}
		// [RH] Count basic armor too.
		ABasicArmor *barmor = CPlayer->mo->FindInventory<ABasicArmor>();
		if (barmor != NULL)
		{
			amount += barmor->SavePercent;
		}
		if (armorp != NULL)
		{
			*armorp = harmor;
		}
		return amount;
	}

//==========================================================================
//
// DrawWeaponPieces
//
//==========================================================================

	void DrawWeaponPieces ()
	{
		static int PieceX[3][3] = 
		{
			{ 190, 225, 234 },
			{ 190, 212, 225 },
			{ 190, 205, 224 },
		};
		int pieces = (CPlayer->pieces >> FourthWeaponShift) & 7;
		int weapClass = FourthWeaponClass;

		if (pieces == 7)
		{
			DrawImage (ClassImages[weapClass][imgWEAPONFULL], 190, 0);
			return;
		}
		DrawImage (ClassImages[weapClass][imgWEAPONSLOT], 190, 0);
		if (pieces & WPIECE1)
		{
			DrawImage (ClassImages[weapClass][imgPIECE1], PieceX[weapClass][0], 0);
		}
		if (pieces & WPIECE2)
		{
			DrawImage (ClassImages[weapClass][imgPIECE2], PieceX[weapClass][1], 0);
		}
		if (pieces & WPIECE3)
		{
			DrawImage (ClassImages[weapClass][imgPIECE3], PieceX[weapClass][2], 0);
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
		int i;

		// Health
		DrBNumberOuter (MAX (0, CPlayer->mo->health), 5, -20);

		// Frags
		if (deathmatch)
		{
			DrINumberOuter (CPlayer->fragcount, 45, -15);
		}

		// Inventory
		if (CPlayer->inventorytics == 0)
		{
			if (ArtifactFlash)
			{
				screen->DrawTexture (Images[imgARTIBOX], -80, -30,
					DTA_HUDRules, HUD_Normal,
					DTA_Alpha, HX_SHADOW,
					TAG_DONE);
				screen->DrawTexture (Images[imgUSEARTIA + ArtifactFlash], -76, -26,
					DTA_HUDRules, HUD_Normal,
					TAG_DONE);
			}
			else if (CPlayer->mo->InvSel != NULL)
			{
				screen->DrawTexture (Images[imgARTIBOX], -80, -30,
					DTA_HUDRules, HUD_Normal,
					DTA_Alpha, HX_SHADOW,
					TAG_DONE);
				screen->DrawTexture (TexMan(CPlayer->mo->InvSel->Icon), -82, -31,
					DTA_HUDRules, HUD_Normal,
					DTA_ColorOverlay, CPlayer->mo->InvSel->Amount > 0 ? 0 : DIM_OVERLAY,
					TAG_DONE);
				if (CPlayer->mo->InvSel->Amount != 1)
				{
					DrSmallNumberOuter (CPlayer->mo->InvSel->Amount, -64, -8, false);
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
					screen->DrawTexture (Images[imgARTIBOX], -106+i*31, -32,
						DTA_HUDRules, HUD_HorizCenter,
						DTA_Alpha, HX_SHADOW,
						TAG_DONE);
					screen->DrawTexture (TexMan(item->Icon), -108+i*31, -33,
						DTA_HUDRules, HUD_HorizCenter,
						DTA_ColorOverlay, item->Amount > 0 ? 0 : DIM_OVERLAY,
						TAG_DONE);
					if (item->Amount != 1)
					{
						DrSmallNumberOuter (item->Amount, -90+i*31, -11, true);
					}
					if (item == CPlayer->mo->InvSel)
					{
						screen->DrawTexture (Images[imgSELECTBOX], -107+i*31, -33,
							DTA_HUDRules, HUD_HorizCenter,
							TAG_DONE);
					}
				}
				// Is there something to the left?
				if (CPlayer->mo->FirstInv() != CPlayer->mo->InvFirst)
				{
					screen->DrawTexture (Images[!(gametic & 4) ?
						imgINVLFGEM1 : imgINVLFGEM2], -118, -33,
						DTA_HUDRules, HUD_HorizCenter, TAG_DONE);
				}
				// Is there something to the right?
				if (item != NULL)
				{
					screen->DrawTexture (Images[!(gametic & 4) ?
						imgINVRTGEM1 : imgINVRTGEM2], 113, -33,
						DTA_HUDRules, HUD_HorizCenter, TAG_DONE);
				}
			}
			for (; i < 7; i++)
			{
				screen->DrawTexture (Images[imgARTIBOX], -106+i*31, -32,
					DTA_HUDRules, HUD_HorizCenter,
					DTA_Alpha, HX_SHADOW,
					TAG_DONE);
			}
		}

		// Mana
		AAmmo *ammo1, *ammo2;
		int ammocount1, ammocount2;
		bool drawmana;

		GetCurrentAmmo (ammo1, ammo2, ammocount1, ammocount2);

		// If the weapon uses some ammo that is not mana, do not draw
		// the mana blocks; draw the specific used ammo instead.
		const PClass *mana1 = PClass::FindClass(NAME_Mana1);
		const PClass *mana2 = PClass::FindClass(NAME_Mana2);

		drawmana = !((ammo1 != NULL && ammo1->GetClass() != mana1 && ammo1->GetClass() != mana2) ||
				     (ammo2 != NULL && ammo2->GetClass() != mana1 && ammo2->GetClass() != mana2));

		if (drawmana)
		{
			int manaImage;
			int ammo = 0;

			if (CPlayer->ReadyWeapon != NULL)
			{
				if (CPlayer->ReadyWeapon->Ammo1 != NULL)
				{
					if (CPlayer->ReadyWeapon->Ammo1->GetClass() == mana1)
					{
						ammo |= 1;
					}
					else if (CPlayer->ReadyWeapon->Ammo1->GetClass() == mana2)
					{
						ammo |= 2;
					}
				}
				if (CPlayer->ReadyWeapon->Ammo2 != NULL)
				{
					if (CPlayer->ReadyWeapon->Ammo2->GetClass() == mana1)
					{
						ammo |= 1;
					}
					else if (CPlayer->ReadyWeapon->Ammo2->GetClass() == mana2)
					{
						ammo |= 2;
					}
				}
			}

			item = CPlayer->mo->FindInventory (mana1);
			i = item != NULL ? item->Amount : 0;
			manaImage = ((ammo & 1) && i > 0) ? imgMANABRIGHT1 : imgMANADIM1;
			screen->DrawTexture (Images[manaImage], -17, -30,
				DTA_HUDRules, HUD_Normal, TAG_DONE);
			DrINumberOuter (i, -47, -30);

			item = CPlayer->mo->FindInventory (mana2);
			i = item != NULL ? item->Amount : 0;
			manaImage = ((ammo & 2) && i > 0) ? imgMANABRIGHT2 : imgMANADIM2;
			screen->DrawTexture (Images[manaImage], -17, -15,
				DTA_HUDRules, HUD_Normal, TAG_DONE);
			DrINumberOuter (i, -47, -15);
		}
		else
		{
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
	TObjPtr<AKey> oldkeys[5];
	int oldammocount1, oldammocount2;
	int oldartiCount;
	int oldfrags;
	int oldmana1;
	int oldmana2;
	int oldusemana1;
	int oldusemana2;
	int olddrawbars;
	int oldarmor;
	int oldhealth;
	int oldlife;
	int oldpieces;

	enum
	{
		imgH2BAR,
		imgH2TOP,
		imgINVBAR,
		imgLFEDGE,
		imgRTEDGE,
		imgSTATBAR,
		imgKEYBAR,
		imgSELECTBOX,
		imgARTICLEAR,
		imgARMCLEAR,
		imgMANACLEAR,
		imgMANAVIAL1,
		imgMANAVIAL2,
		imgMANAVIALDIM1,
		imgMANAVIALDIM2,
		imgMANADIM1,
		imgMANADIM2,
		imgMANABRIGHT1,
		imgMANABRIGHT2,
		imgINVLFGEM1,
		imgINVLFGEM2,
		imgINVRTGEM1,
		imgINVRTGEM2,
		imgKILLS,
		imgUSEARTIA,
		imgUSEARTIB,
		imgUSEARTIC,
		imgUSEARTID,
		imgUSEARTIE,
		imgKEYSLOT1,
		imgKEYSLOT2,
		imgKEYSLOT3,
		imgKEYSLOT4,
		imgKEYSLOT5,
		imgKEYSLOT6,
		imgKEYSLOT7,
		imgKEYSLOT8,
		imgKEYSLOT9,
		imgKEYSLOTA,
		imgKEYSLOTB,
		imgARMSLOT1,
		imgARMSLOT2,
		imgARMSLOT3,
		imgARMSLOT4,
		imgARTIBOX,
		imgAMMOBACK,

		NUM_HEXENSB_IMAGES
	};

	enum
	{
		imgWEAPONSLOT,
		imgWEAPONFULL,
		imgPIECE1,
		imgPIECE2,
		imgPIECE3,
		imgCHAIN,
		imgLIFEGEM,

		NUM_HEXENCLASSSB_IMAGES
	};

	FImageCollection Images;
	FImageCollection ClassImages[3];

	int HealthMarker;
	char ArtifactFlash;

	char FourthWeaponShift;
	char FourthWeaponClass;
	char LifeBarClass;

	char ArtiRefresh;
	char FragHealthRefresh;
	char KeysRefresh;
	char ArmorRefresh;
	char HealthRefresh;
	char Mana1Refresh;
	char Mana2Refresh;
	char AmmoRefresh;

	FManaBar ManaVial1Pic;
	FManaBar ManaVial2Pic;
};

IMPLEMENT_POINTY_CLASS(DHexenStatusBar)
 DECLARE_POINTER(oldarti)
 DECLARE_POINTER(oldammo1)
 DECLARE_POINTER(oldammo2)
 DECLARE_POINTER(oldkeys[0])
 DECLARE_POINTER(oldkeys[1])
 DECLARE_POINTER(oldkeys[2])
 DECLARE_POINTER(oldkeys[3])
 DECLARE_POINTER(oldkeys[4])
END_POINTERS

DBaseStatusBar *CreateHexenStatusBar ()
{
	return new DHexenStatusBar;
}
