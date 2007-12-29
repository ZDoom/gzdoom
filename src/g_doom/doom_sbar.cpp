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
#include "a_keys.h"
#include "templates.h"
#include "i_system.h"
#include "r_translate.h"


#define ST_EVILGRINCOUNT		(2*TICRATE)
#define ST_STRAIGHTFACECOUNT	(TICRATE/2)
#define ST_TURNCOUNT			(1*TICRATE)
#define ST_OUCHCOUNT			(1*TICRATE)
#define ST_RAMPAGEDELAY 		(2*TICRATE)

#define ST_MUCHPAIN 			20

EXTERN_CVAR (Bool, vid_fps)


class FDoomStatusBar : public FBaseStatusBar
{
public:
	FDoomStatusBar () : FBaseStatusBar (32)
	{
		static const char *sharedLumpNames[] =
		{
			NULL,		NULL,		NULL,		NULL,		NULL,
			NULL,		NULL,		NULL,		NULL,		NULL,
			NULL,		NULL,		"STTMINUS",	"STTNUM0",	"STTNUM1",
			"STTNUM2",	"STTNUM3",	"STTNUM4",	"STTNUM5",	"STTNUM6",
			"STTNUM7",	"STTNUM8",	"STTNUM9",	"STYSNUM0",	"STYSNUM1",
			"STYSNUM2",	"STYSNUM3",	"STYSNUM4",	"STYSNUM5",	"STYSNUM6",
			"STYSNUM7",	"STYSNUM8",	"STYSNUM9"
		};
		FTexture *tex;

		FBaseStatusBar::Images.Init (sharedLumpNames, NUM_BASESB_IMAGES);
		tex = FBaseStatusBar::Images[imgBNumbers];
		BigWidth = tex->GetWidth();
		BigHeight = tex->GetHeight();

		DoCommonInit ();
		bEvilGrin = false;
	}

	~FDoomStatusBar ()
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

	void SetFace (void *skn)
	{
		const char *nameptrs[ST_NUMFACES];
		char names[ST_NUMFACES][9];
		char prefix[4];
		int i, j;
		int namespc;
		int facenum;
		FPlayerSkin *skin = (FPlayerSkin *)skn;

		for (i = 0; i < ST_NUMFACES; i++)
		{
			nameptrs[i] = names[i];
		}

		if (skin->face[0] != 0)
		{
			prefix[0] = skin->face[0];
			prefix[1] = skin->face[1];
			prefix[2] = skin->face[2];
			prefix[3] = 0;
			namespc = skin->namespc;
		}
		else
		{
			prefix[0] = 'S';
			prefix[1] = 'T';
			prefix[2] = 'F';
			prefix[3] = 0;
			namespc = ns_global;
		}

		facenum = 0;

		for (i = 0; i < ST_NUMPAINFACES; i++)
		{
			for (j = 0; j < ST_NUMSTRAIGHTFACES; j++)
			{
				sprintf (names[facenum++], "%sST%d%d", prefix, i, j);
			}
			sprintf (names[facenum++], "%sTR%d0", prefix, i);  // turn right
			sprintf (names[facenum++], "%sTL%d0", prefix, i);  // turn left
			sprintf (names[facenum++], "%sOUCH%d", prefix, i); // ouch!
			sprintf (names[facenum++], "%sEVL%d", prefix, i);  // evil grin ;)
			sprintf (names[facenum++], "%sKILL%d", prefix, i); // pissed off
		}
		sprintf (names[facenum++], "%sGOD0", prefix);
		sprintf (names[facenum++], "%sDEAD0", prefix);

		Faces.Uninit ();
		Faces.Init (nameptrs, ST_NUMFACES, namespc);

		FaceIndex = 0;
		FaceCount = 0;
		OldFaceIndex = -1;
	}

	void MultiplayerChanged ()
	{
		FBaseStatusBar::MultiplayerChanged ();
		if (multiplayer)
		{
			// set face background color
			StatusBarTex.SetPlayerRemap(translationtables[TRANSLATION_Players][int(CPlayer - players)]);
		}
	}

	void AttachToPlayer (player_t *player)
	{
		player_t *oldplayer = CPlayer;

		FBaseStatusBar::AttachToPlayer (player);
		if (oldplayer != CPlayer)
		{
			SetFace (&skins[CPlayer->userinfo.skin]);
		}
		if (multiplayer)
		{
			// set face background color
			StatusBarTex.SetPlayerRemap(translationtables[TRANSLATION_Players][int(CPlayer - players)]);
		}
		bEvilGrin = false;
	}

	void Tick ()
	{
		FBaseStatusBar::Tick ();
		RandomNumber = M_Random ();
		UpdateFace ();
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
				DrawImage (&StatusBarTex, 0, 0);
				memset (OldArms, 255, sizeof(OldArms));
				OldKeys = -1;
				memset (OldAmmo, 255, sizeof(OldAmmo));
				memset (OldMaxAmmo, 255, sizeof(OldMaxAmmo));
				OldFaceIndex = -1;
				OldHealth = -1;
				OldArmor = -1;
				OldActiveAmmo = -1;
				OldFrags = -9999;
				FaceHealth = -9999;
			}
			DrawMainBar ();
			if (CPlayer->inventorytics > 0 && !(level.flags & LEVEL_NOINVENTORYBAR))
			{
				DrawInventoryBar ();
				SB_state = screen->GetPageCount ();
			}
		}
	}

private:
	struct FDoomStatusBarTexture : public FTexture
	{
		void DrawToBar (const char *name, int x, int y, const BYTE *colormap_in = NULL);

	public:
		FDoomStatusBarTexture ();
		const BYTE *GetColumn (unsigned int column, const Span **spans_out);
		const BYTE *GetPixels ();
		void Unload ();
		~FDoomStatusBarTexture ();
		void SetPlayerRemap(FRemapTable *remap);
		int CopyTrueColorPixels(BYTE *buffer, int buf_pitch, int buf_height, int x, int y);

		FTextureFormat GetFormat()
		{
			return TEX_RGB;
		}


	protected:
		void MakeTexture ();

		FTexture *BaseTexture;
		BYTE *Pixels;
		FRemapTable *STBFremap;
	}
	StatusBarTex;

	void DoCommonInit ()
	{
		static const char *doomLumpNames[] =
		{
			"STKEYS0",	"STKEYS1",	"STKEYS2",	"STKEYS3",	"STKEYS4",
			"STKEYS5",	"STKEYS6",	"STKEYS7",	"STKEYS8",
			"STGNUM2",	"STGNUM3",	"STGNUM4",	"STGNUM5",	"STGNUM6",
			"STGNUM7",	"MEDIA0",	"ARTIBOX",	"SELECTBO",	"INVGEML1",
			"INVGEML2",	"INVGEMR1",	"INVGEMR2",
		};

		Images.Init (doomLumpNames, NUM_DOOMSB_IMAGES);

		// In case somebody wants to use the Heretic status bar graphics...
		{
			FTexture *artibox = Images[imgARTIBOX];
			FTexture *selectbox = Images[imgSELECTBOX];
			if (artibox != NULL && selectbox != NULL)
			{
				selectbox->LeftOffset = artibox->LeftOffset;
				selectbox->TopOffset = artibox->TopOffset;
			}
		}

		StatusBarTex.Unload ();

		SB_state = screen->GetPageCount ();
		FaceLastAttackDown = -1;
		FacePriority = 0;
	}

	void DrawMainBar ()
	{
		int amount;

		DrawAmmoStats ();
		DrawFace ();
		DrawKeys ();
		if (!deathmatch)
		{
			DrawArms ();
		}
		else
		{
			if (OldFrags != CPlayer->fragcount)
			{
				OldFrags = CPlayer->fragcount;
				FragsRefresh = screen->GetPageCount ();
			}
			if (FragsRefresh)
			{
				FragsRefresh--;
				DrawNumber (OldFrags, 138/*110*/, 3, 2);
			}
		}
		if (CPlayer->health != OldHealth)
		{
			OldHealth = CPlayer->health;
			HealthRefresh = screen->GetPageCount ();
		}
		if (HealthRefresh)
		{
			HealthRefresh--;
			DrawNumber (OldHealth, 90/*48*/, 3);
		}
		AInventory *armor = CPlayer->mo->FindInventory<ABasicArmor>();
		int armorpoints = armor != NULL ? armor->Amount : 0;
		if (armorpoints != OldArmor)
		{
			OldArmor = armorpoints;
			ArmorRefresh = screen->GetPageCount ();
		}
		if (ArmorRefresh)
		{
			ArmorRefresh--;
			DrawNumber (OldArmor, 221/*179*/, 3);
		}
		if (CPlayer->ReadyWeapon != NULL)
		{
			AAmmo *ammo = CPlayer->ReadyWeapon->Ammo1;
			amount = ammo != NULL ? ammo->Amount : -9999;
		}
		else
		{
			amount = -9999;
		}
		if (amount != OldActiveAmmo)
		{
			OldActiveAmmo = amount;
			ActiveAmmoRefresh = screen->GetPageCount ();
		}
		if (ActiveAmmoRefresh)
		{
			ActiveAmmoRefresh--;
			if (OldActiveAmmo != -9999)
			{
				DrawNumber (OldActiveAmmo, 44/*2*/, 3);
			}
			else
			{
				DrawPartialImage (&StatusBarTex, 2, BigWidth*3);
			}
		}
	}

	void DrawArms ()
	{
		BYTE arms[6];
		int i, j;

		// Catalog the weapons the player owns
		memset (arms, 0, sizeof(arms));
		for (i = 0; i < 6; i++)
		{
			for (j = 0; j < MAX_WEAPONS_PER_SLOT; j++)
			{
				const PClass *weap = LocalWeapons.Slots[i+2].GetWeapon (j);
				if (weap != NULL && CPlayer->mo->FindInventory (weap) != NULL)
				{
					arms[i] = 1;
					break;
				}
			}
		}

		// Draw slots that have changed ownership since last time
		for (i = 0; i < 6; i++)
		{
			if (arms[i] != OldArms[i])
			{
				ArmsRefresh[i%3] = screen->GetPageCount ();
			}
		}

		// Remember the arms state for next time
		memcpy (OldArms, arms, sizeof(arms));

		for (i = 0; i < 3; i++)
		{
			if (ArmsRefresh[i])
			{
				ArmsRefresh[i]--;
				int x = 111 + i * 12;

				DrawArm (arms[i], i, x, 4, true);
				DrawArm (arms[i+3], i+3, x, 14, false);
			}
		}
	}

	void DrawArm (int on, int picnum, int x, int y, bool drawBackground)
	{
		int w;
		FTexture *pic = on ? FBaseStatusBar::Images[imgSmNumbers + 2 + picnum] : Images[imgGNUM2 + picnum];

		if (pic != NULL)
		{
			w = pic->GetWidth();
			x -= pic->LeftOffset;
			y -= pic->TopOffset;

			if (drawBackground)
			{
				DrawPartialImage (&StatusBarTex, x, w);
			}
			DrawImage (pic, x, y);
		}
	}

	void DrawAmmoStats ()
	{
		static const ENamedName ammoTypes[4] =
		{
			NAME_Clip,
			NAME_Shell,
			NAME_RocketAmmo,
			NAME_Cell
		};
		int ammo[4], maxammo[4];
		int i;

		// Catalog the player's ammo
		for (i = 0; i < 4; i++)
		{
			const PClass *type = PClass::FindClass (ammoTypes[i]);
			AInventory *item = CPlayer->mo->FindInventory (type);
			if (item != NULL)
			{
				ammo[i] = item->Amount;
				maxammo[i] = item->MaxAmount;
			}
			else
			{
				ammo[i] = 0;
				maxammo[i] = ((AInventory *)GetDefaultByType (type))->MaxAmount;
			}
		}

		// Draw ammo amounts that have changed since last time
		for (i = 0; i < 4; i++)
		{
			if (ammo[i] != OldAmmo[i])
			{
				AmmoRefresh = screen->GetPageCount ();
			}
			if (maxammo[i] != OldMaxAmmo[i])
			{
				MaxAmmoRefresh = screen->GetPageCount ();
			}
		}

		// Remember ammo counts for next time
		memcpy (OldAmmo, ammo, sizeof(ammo));
		memcpy (OldMaxAmmo, maxammo, sizeof(ammo));

		if (AmmoRefresh)
		{
			AmmoRefresh--;
			DrawPartialImage (&StatusBarTex, 276, 4*3);
			for (i = 0; i < 4; i++)
			{
				DrSmallNumber (ammo[i], 276, 5 + 6*i);
			}
		}
		if (MaxAmmoRefresh)
		{
			MaxAmmoRefresh--;
			DrawPartialImage (&StatusBarTex, 302, 4*3);
			for (i = 0; i < 4; i++)
			{
				DrSmallNumber (maxammo[i], 302, 5 + 6*i);
			}
		}
	}

	void DrawFace ()
	{
		// If a player has an inventory item selected, it takes the place of the
		// face, for lack of a better place to put it.
		if (OldFaceIndex != FaceIndex)
		{
			FaceRefresh = screen->GetPageCount ();
			OldFaceIndex = FaceIndex;
		}
		if (FaceRefresh || (CPlayer->mo->InvSel != NULL && !(level.flags & LEVEL_NOINVENTORYBAR)))
		{
			if (FaceRefresh)
			{
				FaceRefresh--;
			}
			DrawPartialImage (&StatusBarTex, 142, 37);
			if (CPlayer->mo->InvSel == NULL || (level.flags & LEVEL_NOINVENTORYBAR))
			{
				DrawImage (Faces[FaceIndex], 143, 0);
			}
			else
			{
				DrawDimImage (TexMan(CPlayer->mo->InvSel->Icon), 144, 0, CPlayer->mo->InvSel->Amount <= 0);
				if (CPlayer->mo->InvSel->Amount != 1)
				{
					DrSmallNumber (CPlayer->mo->InvSel->Amount, 165, 24);
				}
				OldFaceIndex = -1;
			}
		}
	}

	void DrawKeys ()
	{
		AInventory *item;
		int keys;

		// Catalog the player's current keys
		keys = 0;
		for (item = CPlayer->mo->Inventory; item != NULL; item = item->Inventory)
		{
			if (item->IsKindOf (RUNTIME_CLASS(AKey)))
			{
				int keynum = static_cast<AKey*>(item)->KeyNumber;
				if (keynum >= 1 && keynum <= 6)
				{
					keys |= 1 << (keynum-1);
				}
			}
		}

		// Remember keys for next time
		if (OldKeys != keys)
		{
			OldKeys = keys;
			KeysRefresh = screen->GetPageCount ();
		}

		// Draw keys that have changed since last time
		if (KeysRefresh)
		{
			KeysRefresh--;
			DrawPartialImage (&StatusBarTex, 239, 8);

			// Blue Keys
			switch (keys & (2|16))
			{
			case 2:		DrawImage (Images[imgKEYS0], 239, 3);	break;
			case 16:	DrawImage (Images[imgKEYS3], 239, 3);	break;
			case 18:	DrawImage (Images[imgKEYS6], 239, 3);	break;
			}

			// Yellow Keys
			switch (keys & (4|32))
			{
			case 4:		DrawImage (Images[imgKEYS1], 239, 13);	break;
			case 32:	DrawImage (Images[imgKEYS4], 239, 13);	break;
			case 36:	DrawImage (Images[imgKEYS7], 239, 13);	break;
			}

			// Red Keys
			switch (keys & (1|8))
			{
			case 1:		DrawImage (Images[imgKEYS2], 239, 23);	break;
			case 8:		DrawImage (Images[imgKEYS5], 239, 23);	break;
			case 9:		DrawImage (Images[imgKEYS8], 239, 23);	break;
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
		const AInventory *item;
		int i;

		// If the player has no artifacts, don't draw the bar
		CPlayer->mo->InvFirst = ValidateInvFirst (7);
		if (CPlayer->mo->InvFirst != NULL)
		{
			for (item = CPlayer->mo->InvFirst, i = 0; item != NULL && i < 7; item = item->NextInv(), ++i)
			{
				DrawImage (Images[imgARTIBOX], 50+i*31, 2);
				DrawDimImage (TexMan(item->Icon), 50+i*31, 2, item->Amount <= 0);
				if (item->Amount != 1)
				{
					DrSmallNumber (item->Amount, 66+i*31, 24);
				}
				if (item == CPlayer->mo->InvSel)
				{
					DrawImage (Images[imgSELECTBOX], 50+i*31, 2);
				}
			}
			for (; i < 7; ++i)
			{
				DrawImage (Images[imgARTIBOX], 50+i*31, 2);
			}
			// Is there something to the left?
			if (CPlayer->mo->FirstInv() != CPlayer->mo->InvFirst)
			{
				DrawImage (Images[!(gametic & 4) ?
					imgINVLFGEM1 : imgINVLFGEM2], 38, 2);
			}
			// Is there something to the right?
			if (item != NULL)
			{
				DrawImage (Images[!(gametic & 4) ?
					imgINVRTGEM1 : imgINVRTGEM2], 269, 2);
			}
		}
	}

	void DrawNumber (int val, int x, int y, int size=3)
	{
		DrawPartialImage (&StatusBarTex, x-BigWidth*size, size*BigWidth);
		DrBNumber (val, x, y, size);
	}

	void DrawFullScreenStuff ()
	{
		const AInventory *item;
		int i;
		int ammotop;

		// Draw health
		screen->DrawTexture (Images[imgMEDI], 20, -2,
			DTA_HUDRules, HUD_Normal,
			DTA_CenterBottomOffset, true,
			TAG_DONE);
		DrBNumberOuter (CPlayer->health, 40, -BigHeight-4);

		// Draw armor
		ABasicArmor *armor = CPlayer->mo->FindInventory<ABasicArmor>();
		if (armor != NULL && armor->Amount != 0)
		{
			screen->DrawTexture (TexMan(armor->Icon), 20, -24,
				DTA_HUDRules, HUD_Normal,
				DTA_CenterBottomOffset, true,
				TAG_DONE);
			DrBNumberOuter (armor->Amount, 40, -39);
		}

		// Draw ammo
		AAmmo *ammo1, *ammo2;
		int ammocount1, ammocount2;

		GetCurrentAmmo (ammo1, ammo2, ammocount1, ammocount2);
		ammotop = -1;
		if (ammo1 != NULL)
		{
			FTexture *ammoIcon = TexMan(ammo1->Icon);
			// Draw primary ammo in the bottom-right corner
			screen->DrawTexture (ammoIcon, -14, -4,
				DTA_HUDRules, HUD_Normal,
				DTA_CenterBottomOffset, true,
				TAG_DONE);
			DrBNumberOuter (ammo1->Amount, -67, -4 - BigHeight);
			ammotop = -4 - BigHeight;
			if (ammo2 != NULL && ammo2!=ammo1)
			{
				// Draw secondary ammo just above the primary ammo
				int y = MIN (-6 - BigHeight, -6 - (ammoIcon != NULL ? ammoIcon->GetHeight() : 0));
				screen->DrawTexture (TexMan(ammo2->Icon), -14, y,
					DTA_HUDRules, HUD_Normal,
					DTA_CenterBottomOffset, true,
					TAG_DONE);
				DrBNumberOuter (ammo2->Amount, -67, y - BigHeight);
				ammotop = y - BigHeight;
			}
		}

		if (deathmatch)
		{ // Draw frags (in DM)
			DrBNumberOuter (CPlayer->fragcount, -44, 1);
		}
		else
		{ // Draw keys (not DM)
			int maxw = 0;
			int count = 0;
			int x =  -2;
			int y = vid_fps? 12 : 2;

			for (item = CPlayer->mo->Inventory; item != NULL; item = item->Inventory)
			{
				if (item->Icon > 0 && item->IsKindOf (RUNTIME_CLASS(AKey)))
				{
					FTexture *keypic = TexMan(item->Icon);
					if (keypic != NULL)
					{
						int w = keypic->GetWidth ();
						int h = keypic->GetHeight ();
						if (w > maxw)
						{
							maxw = w;
						}
						screen->DrawTexture (keypic, x, y,
							DTA_LeftOffset, w,
							DTA_TopOffset, 0,
							DTA_HUDRules, HUD_Normal,
							TAG_DONE);
						if (++count == 3)
						{
							count = 0;
							y = vid_fps? 12 : 2;
							x -= maxw + 2;
							maxw = 0;
						}
						else
						{
							y += h + 2;
						}
					}
				}
			}
		}

		// Draw inventory
		if (!(level.flags & LEVEL_NOINVENTORYBAR))
		{
			if (CPlayer->inventorytics == 0)
			{
				if (CPlayer->mo->InvSel != NULL)
				{
					screen->DrawTexture (TexMan(CPlayer->mo->InvSel->Icon), -14, ammotop - 1/*-24*/,
						DTA_HUDRules, HUD_Normal,
						DTA_CenterBottomOffset, true,
						DTA_ColorOverlay, CPlayer->mo->InvSel->Amount > 0 ? 0 : DIM_OVERLAY,
						TAG_DONE);
					DrBNumberOuter (CPlayer->mo->InvSel->Amount, -68, ammotop - 18/*-41*/);
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
						screen->DrawTexture (TexMan(item->Icon), -105+i*31, -32,
							DTA_HUDRules, HUD_HorizCenter,
							DTA_ColorOverlay, item->Amount > 0 ? 0 : DIM_OVERLAY,
							TAG_DONE);
						if (item->Amount != 1)
						{
							DrSmallNumberOuter (item->Amount, -90+i*31, -10, true);
						}
						if (item == CPlayer->mo->InvSel)
						{
							screen->DrawTexture (Images[imgSELECTBOX], -91+i*31, -2,
								DTA_HUDRules, HUD_HorizCenter,
								DTA_CenterBottomOffset, true,
								TAG_DONE);
						}
					}
					for (; i < 7; i++)
					{
						screen->DrawTexture (Images[imgARTIBOX], -106+i*31, -32,
							DTA_HUDRules, HUD_HorizCenter,
							DTA_Alpha, HX_SHADOW,
							TAG_DONE);
					}
					// Is there something to the left?
					if (CPlayer->mo->FirstInv() != CPlayer->mo->InvFirst)
					{
						screen->DrawTexture (Images[!(gametic & 4) ?
							imgINVLFGEM1 : imgINVLFGEM2], -118, -33,
							DTA_HUDRules, HUD_HorizCenter,
							TAG_DONE);
					}
					// Is there something to the right?
					if (item != NULL)
					{
						screen->DrawTexture (Images[!(gametic & 4) ?
							imgINVRTGEM1 : imgINVRTGEM2], 113, -33,
							DTA_HUDRules, HUD_HorizCenter,
							TAG_DONE);
					}
				}
			}
		}
	}

	int CalcPainOffset ()
	{
		int 		health;
		static int	lastcalc;
		static int	oldhealth = -1;
		
		health = CPlayer->health > 100 ? 100 : CPlayer->health;

		if (health != oldhealth)
		{
			lastcalc = ST_FACESTRIDE * (((100 - health) * ST_NUMPAINFACES) / 101);
			oldhealth = health;
		}
		return lastcalc;
	}

	void ReceivedWeapon (AWeapon *weapon)
	{
		bEvilGrin = true;
	}

	//
	// This is a not-very-pretty routine which handles the face states
	// and their timing. The precedence of expressions is:
	//	dead > evil grin > turned head > straight ahead
	//
	void UpdateFace ()
	{
		int 		i;
		angle_t 	badguyangle;
		angle_t 	diffang;
		

		if (FacePriority < 10)
		{
			// dead
			if (CPlayer->health <= 0)
			{
				FacePriority = 9;
				FaceIndex = ST_DEADFACE;
				FaceCount = 1;
			}
		}

		if (FacePriority < 9)
		{
			if (CPlayer->bonuscount)
			{
				// picking up bonus
				if (bEvilGrin) 
				{
					// evil grin if just picked up weapon
					bEvilGrin = false;
					FacePriority = 8;
					FaceCount = ST_EVILGRINCOUNT;
					FaceIndex = CalcPainOffset() + ST_EVILGRINOFFSET;
				}
			}
			else 
			{
				// This happens when a weapon is added to the inventory
				// by other means than being picked up.
				bEvilGrin = false;
			}
		}
  
		if (FacePriority < 8)
		{
			if (CPlayer->damagecount
				&& CPlayer->attacker
				&& CPlayer->attacker != CPlayer->mo)
			{
				// being attacked
				FacePriority = 7;
				
				if (FaceHealth != -9999 && FaceHealth - CPlayer->health > ST_MUCHPAIN)
				{
					FaceCount = ST_TURNCOUNT;
					FaceIndex = CalcPainOffset() + ST_OUCHOFFSET;
					FacePriority = 8;
				}
				else if (CPlayer->mo != NULL)
				{
					badguyangle = R_PointToAngle2(CPlayer->mo->x,
												  CPlayer->mo->y,
												  CPlayer->attacker->x,
												  CPlayer->attacker->y);
					
					if (badguyangle > CPlayer->mo->angle)
					{
						// whether right or left
						diffang = badguyangle - CPlayer->mo->angle;
						i = diffang > ANG180; 
					}
					else
					{
						// whether left or right
						diffang = CPlayer->mo->angle - badguyangle;
						i = diffang <= ANG180; 
					} // confusing, aint it?

					
					FaceCount = ST_TURNCOUNT;
					FaceIndex = CalcPainOffset();
					
					if (diffang < ANG45)
					{
						// head-on	  
						FaceIndex += ST_RAMPAGEOFFSET;
					}
					else if (i)
					{
						// turn face right
						FaceIndex += ST_TURNOFFSET;
					}
					else
					{
						// turn face left
						FaceIndex += ST_TURNOFFSET+1;
					}
				}
			}
		}
  
		if (FacePriority < 7)
		{
			// getting hurt because of your own damn stupidity
			if (CPlayer->damagecount)
			{
				if (OldHealth != -1 && CPlayer->health - OldHealth > ST_MUCHPAIN)
				{
					FacePriority = 7;
					FaceCount = ST_TURNCOUNT;
					FaceIndex = CalcPainOffset() + ST_OUCHOFFSET;
				}
				else
				{
					FacePriority = 6;
					FaceCount = ST_TURNCOUNT;
					FaceIndex = CalcPainOffset() + ST_RAMPAGEOFFSET;
				}

			}

		}
  
		if (FacePriority < 6)
		{
			// rapid firing
			if ((CPlayer->cmd.ucmd.buttons & (BT_ATTACK|BT_ALTATTACK)) && !(CPlayer->cheats & (CF_FROZEN | CF_TOTALLYFROZEN)))
			{
				if (FaceLastAttackDown == -1)
					FaceLastAttackDown = ST_RAMPAGEDELAY;
				else if (!--FaceLastAttackDown)
				{
					FacePriority = 5;
					FaceIndex = CalcPainOffset() + ST_RAMPAGEOFFSET;
					FaceCount = 1;
					FaceLastAttackDown = 1;
				}
			}
			else
			{
				FaceLastAttackDown = -1;
			}
		}
  
		if (FacePriority < 5)
		{
			// invulnerability
			if ((CPlayer->cheats & CF_GODMODE)
				|| (CPlayer->mo != NULL && CPlayer->mo->flags2 & MF2_INVULNERABLE))
			{
				FacePriority = 4;
				FaceIndex = ST_GODFACE;
				FaceCount = 1;
			}
		}

		// look left or look right if the facecount has timed out
		if (!FaceCount)
		{
			FaceIndex = CalcPainOffset() + (RandomNumber % 3);
			FaceCount = ST_STRAIGHTFACECOUNT;
			FacePriority = 0;
		}

		FaceCount--;
		FaceHealth = CPlayer->health;
	}

	enum
	{
		imgKEYS0,
		imgKEYS1,
		imgKEYS2,
		imgKEYS3,
		imgKEYS4,
		imgKEYS5,
		imgKEYS6,
		imgKEYS7,
		imgKEYS8,
		imgGNUM2,
		imgGNUM3,
		imgGNUM4,
		imgGNUM5,
		imgGNUM6,
		imgGNUM7,
		imgMEDI,
		imgARTIBOX,
		imgSELECTBOX,
		imgINVLFGEM1,
		imgINVLFGEM2,
		imgINVRTGEM1,
		imgINVRTGEM2,

		NUM_DOOMSB_IMAGES
	};

	enum
	{
		ST_NUMPAINFACES		= 5,
		ST_NUMSTRAIGHTFACES	= 3,
		ST_NUMTURNFACES		= 2,
		ST_NUMSPECIALFACES	= 3,
		ST_NUMEXTRAFACES	= 2,
		ST_FACESTRIDE		= ST_NUMSTRAIGHTFACES+ST_NUMTURNFACES+ST_NUMSPECIALFACES,
		ST_NUMFACES			= ST_FACESTRIDE*ST_NUMPAINFACES+ST_NUMEXTRAFACES,

		ST_TURNOFFSET		= ST_NUMSTRAIGHTFACES,
		ST_OUCHOFFSET		= ST_TURNOFFSET + ST_NUMTURNFACES,
		ST_EVILGRINOFFSET	= ST_OUCHOFFSET + 1,
		ST_RAMPAGEOFFSET	= ST_EVILGRINOFFSET + 1,
		ST_GODFACE			= ST_NUMPAINFACES*ST_FACESTRIDE,
		ST_DEADFACE			= ST_GODFACE + 1
	};

	FImageCollection Images;
	FImageCollection Faces;

	int BigWidth;
	int BigHeight;

	int FaceIndex;
	int FaceCount;
	int RandomNumber;
	int OldFaceIndex;
	BYTE OldArms[6];
	int OldKeys;
	int OldAmmo[4];
	int OldMaxAmmo[4];
	int OldHealth;
	int OldArmor;
	int OldActiveAmmo;
	int OldFrags;
	int FaceHealth;
	int	FaceLastAttackDown;
	int	FacePriority;

	char HealthRefresh;
	char ArmorRefresh;
	char ActiveAmmoRefresh;
	char FragsRefresh;
	char ArmsRefresh[3];
	char AmmoRefresh;
	char MaxAmmoRefresh;
	char FaceRefresh;
	char KeysRefresh;

	bool bEvilGrin;
};

FDoomStatusBar::FDoomStatusBarTexture::FDoomStatusBarTexture ()
{
	BaseTexture = TexMan[TexMan.AddPatch("STBAR")];
	if (BaseTexture==NULL)
	{
		I_Error("Fatal error: STBAR not found");
	}
	UseType = FTexture::TEX_MiscPatch;
	Name[0]=0;	// doesn't need a name

	// now copy all the properties from the base texture
	CopySize(BaseTexture);
	Pixels = NULL;
	STBFremap = NULL;
}

const BYTE *FDoomStatusBar::FDoomStatusBarTexture::GetColumn (unsigned int column, const Span **spans_out)
{
	if (Pixels == NULL)
	{
		MakeTexture ();
	}

	BaseTexture->GetColumn(column, spans_out);
	return Pixels + column*Height;
}

const BYTE *FDoomStatusBar::FDoomStatusBarTexture::GetPixels ()
{
	if (Pixels == NULL)
	{
		MakeTexture ();
	}
	return Pixels;
}

void FDoomStatusBar::FDoomStatusBarTexture::Unload ()
{
	if (Pixels != NULL)
	{
		delete[] Pixels;
		Pixels = NULL;
	}
}
												
FDoomStatusBar::FDoomStatusBarTexture::~FDoomStatusBarTexture ()
{
	Unload ();
}


void FDoomStatusBar::FDoomStatusBarTexture::MakeTexture ()
{
	Pixels = new BYTE[Width*Height];
	const BYTE *pix = BaseTexture->GetPixels();
	memcpy(Pixels, pix, Width*Height);
	if (!deathmatch) DrawToBar("STARMS", 104, 0, NULL);
	DrawToBar("STTPRCNT", 90, 3, NULL);
	DrawToBar("STTPRCNT", 221, 3, NULL);
	if (multiplayer) DrawToBar("STFBANY", 143, 1, STBFremap? STBFremap->Remap : NULL);
}

int FDoomStatusBar::FDoomStatusBarTexture::CopyTrueColorPixels(BYTE *buffer, int buf_pitch, int buf_height, int x, int y)
{
	FTexture *tex;

	BaseTexture->CopyTrueColorPixels(buffer, buf_pitch, buf_height, x, y);
	if (!deathmatch)
	{
		tex = TexMan["STARMS"];
		if (tex != NULL)
		{
			tex->CopyTrueColorPixels(buffer, buf_pitch, buf_height, x+104, y);
		}
	}

	tex = TexMan["STTPRCNT"];
	if (tex != NULL)
	{
		tex->CopyTrueColorPixels(buffer, buf_pitch, buf_height, x+90, y+3);
		tex->CopyTrueColorPixels(buffer, buf_pitch, buf_height, x+221, y+3);
	}
	if (multiplayer)
	{
		tex = TexMan["STFBANY"];
		if (tex != NULL)
		{
			tex->CopyTrueColorTranslated(buffer, buf_pitch, buf_height, x+143, y+1, STBFremap);
		}
	}
	return -1;
}



void FDoomStatusBar::FDoomStatusBarTexture::DrawToBar (const char *name, int x, int y, const BYTE *colormap_in)
{
	FTexture *pic;
	BYTE colormap[256];

	if (colormap_in != NULL)
	{
		for (int i = 0; i < 256; ++i)
		{
			colormap[i] = colormap_in[i] == 255 ? Near255 : colormap_in[i];
		}
	}
	else
	{
		for (int i = 0; i < 255; ++i)
		{
			colormap[i] = i;
		}
		colormap[255] = Near255;
	}

	pic = TexMan[name];
	if (pic != NULL)
	{
		x -= pic->LeftOffset;
		pic->CopyToBlock (Pixels, Width, Height, x, y, colormap);
	}
}

void FDoomStatusBar::FDoomStatusBarTexture::SetPlayerRemap(FRemapTable *remap)
{
	Unload();
	KillNative();
	STBFremap = remap;
}

FBaseStatusBar *CreateDoomStatusBar ()
{
	return new FDoomStatusBar;
}
