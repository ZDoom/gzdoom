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

#define ST_EVILGRINCOUNT		(2*TICRATE)
#define ST_STRAIGHTFACECOUNT	(TICRATE/2)
#define ST_TURNCOUNT			(1*TICRATE)
#define ST_OUCHCOUNT			(1*TICRATE)
#define ST_RAMPAGEDELAY 		(2*TICRATE)

#define ST_MUCHPAIN 			20

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
		tex = FBaseStatusBar::Images[imgBNumbers+3];
		BigWidth = tex->GetWidth();
		BigHeight = tex->GetHeight();

		DoCommonInit ();
		memset (FaceWeaponsOwned, 0, sizeof(FaceWeaponsOwned));
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
			// draw face background
			StatusBarTex.DrawToBar ("STFBANY", 143, 1,
				translationtables[TRANSLATION_Players] + (CPlayer - players)*256);
		}
	}

	void AttachToPlayer (player_t *player)
	{
		player_t *oldplayer = CPlayer;
		int i;

		FBaseStatusBar::AttachToPlayer (player);
		if (oldplayer != CPlayer)
		{
			SetFace (&skins[CPlayer->userinfo.skin]);
		}
		if (multiplayer)
		{
			// draw face background
			StatusBarTex.DrawToBar ("STFBANY", 143, 1,
				translationtables[TRANSLATION_Players] + (CPlayer - players)*256);
		}
		for (i = 0; i < NUMWEAPONS; i++)
		{
			FaceWeaponsOwned[i] = CPlayer->weaponowned[i];
		}
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
				memset (OldKeys, 255, sizeof(OldKeys));
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
	struct FDoomStatusBarTexture : public FPatchTexture
	{
	public:
		FDoomStatusBarTexture ();
		void DrawToBar (const char *name, int x, int y, BYTE *colormap_in = NULL);
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
		if (!deathmatch)
		{
			StatusBarTex.DrawToBar ("STARMS", 104, 0);
		}

		StatusBarTex.DrawToBar ("STTPRCNT", 90, 3);		// Health %
		StatusBarTex.DrawToBar ("STTPRCNT", 221, 3);	// Armor %

		SB_state = screen->GetPageCount ();
	}

	void DrawMainBar ()
	{
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
				DrawNumber (OldFrags, 110, 3, 2);
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
			DrawNumber (OldHealth, 48, 3);
		}
		if (CPlayer->armorpoints[0] != OldArmor)
		{
			OldArmor = CPlayer->armorpoints[0];
			ArmorRefresh = screen->GetPageCount ();
		}
		if (ArmorRefresh)
		{
			ArmorRefresh--;
			DrawNumber (OldArmor, 179, 3);
		}
		ammotype_t ammo = wpnlev1info[CPlayer->readyweapon]->ammo;
		int amount = (ammo == am_noammo) ? -9999 : CPlayer->ammo[ammo];
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
				DrawNumber (OldActiveAmmo, 2, 3);
			}
			else
			{
				DrawPartialImage (&StatusBarTex, 2, BigWidth*3);
			}
		}
	}

	void DrawArms ()
	{
		byte arms[6];
		int i, j;

		// Catalog the weapons the player owns
		memset (arms, 0, sizeof(arms));
		for (i = 0; i < 6; i++)
		{
			for (j = 0; j < MAX_WEAPONS_PER_SLOT; j++)
			{
				weapontype_t weap = CPlayer->WeaponSlots.Slots[i+2].GetWeapon (j);
				if (weap < NUMWEAPONS && CPlayer->weaponowned[weap])
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
				DrawPartialImage (&StatusBarTex, x, 6);


				if (arms[i])
				{
					DrawImage (FBaseStatusBar::Images[imgSmNumbers+2+i], x, 4);
				}
				else
				{
					DrawImage (Images[imgGNUM2+i], x, 4);
				}
				if (arms[i+3])
				{
					DrawImage (FBaseStatusBar::Images[imgSmNumbers+2+i+3], x, 14);
				}
				else
				{
					DrawImage (Images[imgGNUM2+i+3], x, 14);
				}
			}
		}
	}

	void DrawAmmoStats ()
	{
		static const int ammomap[4] = { 0, 1, 3, 2 };
		WORD ammo[4], maxammo[4];
		int i;

		// Catalog the player's ammo
		for (i = 0; i < 4; i++)
		{
			ammo[i] = CPlayer->ammo[am_clip+ammomap[i]];
			maxammo[i] = CPlayer->maxammo[am_clip+ammomap[i]];
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
		if (FaceRefresh || (CPlayer->inventory[CPlayer->readyArtifact] != 0 && !(level.flags & LEVEL_NOINVENTORYBAR)))
		{
			if (FaceRefresh)
			{
				FaceRefresh--;
			}
			DrawPartialImage (&StatusBarTex, 142, 37);
			if (CPlayer->inventory[CPlayer->readyArtifact] == 0 || (level.flags & LEVEL_NOINVENTORYBAR))
			{
				DrawImage (Faces[FaceIndex], 143, 0);
			}
			else
			{
				DrawImage (ArtiImages[CPlayer->readyArtifact], 144, 0);
				if (CPlayer->inventory[CPlayer->readyArtifact] != 1)
				{
					DrSmallNumber (CPlayer->inventory[CPlayer->readyArtifact], 165, 24);
				}
				OldFaceIndex = -1;
			}
		}
	}

	void DrawKeys ()
	{
		byte keys[3];
		int i;

		// Catalog the player's current keys
		for (i = 0; i < 3; i++)
		{
			keys[i] = CPlayer->keys[i] ? i : 255;
			if (CPlayer->keys[i+3])
			{
				keys[i] = (keys[i] == 255) ? i+3 : i+6;
			}
		}

		// Draw keys that have changed since last time
		for (i = 0; i < 3; i++)
		{
			if (keys[i] != OldKeys[i])
			{
				KeysRefresh = screen->GetPageCount ();
			}
		}

		// Remember keys for next time
		memcpy (OldKeys, keys, sizeof(keys));

		if (KeysRefresh)
		{
			KeysRefresh--;
			DrawPartialImage (&StatusBarTex, 239, 8);
			for (i = 0; i < 3; i++)
			{
				if (keys[i] != 255)
				{
					DrawImage (Images[imgKEYS0+keys[i]], 239, 3 + i*10);
				}
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
		int i;
		int x;
		bool left, right;

		// If the player has no artifacts, don't draw the bar
		for (i = 0; i < NUMINVENTORYSLOTS; ++i)
		{
			if (CPlayer->inventory[i] != 0)
			{
				break;
			}
		}
		if (i == NUMINVENTORYSLOTS)
		{
			return;
		}

		FindInventoryPos (x, left, right);
		if (x > 0)
		{
			for (i = 0; i < 7 && x < NUMINVENTORYSLOTS; x++)
			{
				if (CPlayer->inventory[x])
				{
					DrawImage (Images[imgARTIBOX], 50+i*31, 2);
					DrawImage (ArtiImages[x], 50+i*31, 2);
					if (CPlayer->inventory[x] != 1)
					{
						DrSmallNumber (CPlayer->inventory[x], 66+i*31, 24);
					}
					if (x == CPlayer->readyArtifact)
					{
						DrawImage (Images[imgSELECTBOX], 50+i*31, 2);
					}
					i++;
				}
			}
			if (i > 0)
			{
				for (; i < 7; ++i)
				{
					DrawImage (Images[imgARTIBOX], 50+i*31, 2);
				}
				if (left)
				{
					DrawImage (Images[!(level.time & 4) ? imgINVLFGEM1 : imgINVLFGEM2], 38, 2);
				}
				if (right)
				{
					DrawImage (Images[!(level.time & 4) ? imgINVRTGEM1 : imgINVRTGEM2], 269, 2);
				}
			}
		}
	}

	void DrawNumber (int val, int x, int y, int size=3)
	{
		DrawPartialImage (&StatusBarTex, x-1, size*BigWidth+2);
		DrBNumber (val, x, y, size);
	}

	void DrawFullScreenStuff ()
	{
		int i, x;

		// Draw health
		OverrideImageOrigin (true);
		DrawOuterImage (Images[imgMEDI], 20, -2);
		OverrideImageOrigin (false);
		DrBNumberOuter (CPlayer->health, 40, -BigHeight-4);

		// Draw armor
		if (CPlayer->armortype && CPlayer->armorpoints[0])
		{
			OverrideImageOrigin (true);
			DrawOuterImage (ArmorImages[CPlayer->armortype != deh.GreenAC], 20, -24);
			OverrideImageOrigin (false);
			DrBNumberOuter (CPlayer->armorpoints[0], 40, -39);
		}

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
			DrBNumberOuter (amt, -67, -BigHeight-4);
		}

		if (deathmatch)
		{ // Draw frags (in DM)
			DrBNumberOuter (CPlayer->fragcount, -44, 1);
		}
		else
		{ // Draw keys (not DM)
			for (i = 0; i < 3; i++)
			{
				if (CPlayer->keys[i])
				{
					DrawOuterImage (Images[imgKEYS0+i], -10, 2+i*10);
				}
			}
			for (; i < 6; i++)
			{
				if (CPlayer->keys[i])
				{
					int x;

					if (CPlayer->keys[i-3])
					{
						x = -20;
					}
					else
					{
						x = -10;
					}
					DrawOuterImage (Images[imgKEYS0+i], x, -29+i*10);
				}
			}
		}

		// Draw inventory
		if (!(level.flags & LEVEL_NOINVENTORYBAR))
		{
			if (CPlayer->inventorytics == 0)
			{
				if (CPlayer->inventory[CPlayer->readyArtifact] > 0)
				{
					OverrideImageOrigin (true);
					DrawOuterImage (ArtiImages[CPlayer->readyArtifact], -14, -24);
					OverrideImageOrigin (false);
					DrBNumberOuter (CPlayer->inventory[CPlayer->readyArtifact], -67, -41);
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
						DrawOuterFadedImage (Images[imgARTIBOX], -106+i*31, -32, TRANSLUC50);
						DrawOuterImage (ArtiImages[x], -105+i*31, -32);
						if (CPlayer->inventory[x] != 1)
						{
							DrSmallNumberOuter (CPlayer->inventory[x], -90+i*31, -10);
						}
						if (x == CPlayer->readyArtifact)
						{
							OverrideImageOrigin (true);
							DrawOuterImage (Images[imgSELECTBOX], -91+i*31, -3);
							OverrideImageOrigin (false);
						}
						i++;
					}
				}
				if (i > 0)
				{
					for (; i < 7; i++)
					{
						DrawOuterFadedImage (Images[imgARTIBOX], -106+i*31, -32, TRANSLUC50);
					}
					if (left)
					{
						DrawOuterImage (Images[!(level.time & 4) ? imgINVLFGEM1 : imgINVLFGEM2], -118, -33);
					}
					if (right)
					{
						DrawOuterImage (Images[!(level.time & 4) ? imgINVRTGEM1 : imgINVRTGEM2], 113, -33);
					}
				}
				SetHorizCentering (false);
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
		static int	lastattackdown = -1;
		static int	priority = 0;
		bool	 	doevilgrin;

		if (priority < 10)
		{
			// dead
			if (CPlayer->health <= 0)
			{
				priority = 9;
				FaceIndex = ST_DEADFACE;
				FaceCount = 1;
			}
		}

		if (priority < 9)
		{
			if (CPlayer->bonuscount)
			{
				// picking up bonus
				doevilgrin = false;

				for (i = 0; i < NUMWEAPONS; i++)
				{
					if (FaceWeaponsOwned[i] != CPlayer->weaponowned[i])
					{
						doevilgrin = true;
						FaceWeaponsOwned[i] = CPlayer->weaponowned[i];
					}
				}
				if (doevilgrin) 
				{
					// evil grin if just picked up weapon
					priority = 8;
					FaceCount = ST_EVILGRINCOUNT;
					FaceIndex = CalcPainOffset() + ST_EVILGRINOFFSET;
				}
			}
		}
  
		if (priority < 8)
		{
			if (CPlayer->damagecount
				&& CPlayer->attacker
				&& CPlayer->attacker != CPlayer->mo)
			{
				// being attacked
				priority = 7;
				
				if (FaceHealth != -9999 && FaceHealth - CPlayer->health > ST_MUCHPAIN)
				{
					FaceCount = ST_TURNCOUNT;
					FaceIndex = CalcPainOffset() + ST_OUCHOFFSET;
					priority = 8;
				}
				else
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
  
		if (priority < 7)
		{
			// getting hurt because of your own damn stupidity
			if (CPlayer->damagecount)
			{
				if (OldHealth != -1 && CPlayer->health - OldHealth > ST_MUCHPAIN)
				{
					priority = 7;
					FaceCount = ST_TURNCOUNT;
					FaceIndex = CalcPainOffset() + ST_OUCHOFFSET;
				}
				else
				{
					priority = 6;
					FaceCount = ST_TURNCOUNT;
					FaceIndex = CalcPainOffset() + ST_RAMPAGEOFFSET;
				}

			}

		}
  
		if (priority < 6)
		{
			// rapid firing
			if (CPlayer->cmd.ucmd.buttons & BT_ATTACK)
			{
				if (lastattackdown==-1)
					lastattackdown = ST_RAMPAGEDELAY;
				else if (!--lastattackdown)
				{
					priority = 5;
					FaceIndex = CalcPainOffset() + ST_RAMPAGEOFFSET;
					FaceCount = 1;
					lastattackdown = 1;
				}
			}
			else
			{
				lastattackdown = -1;
			}
		}
  
		if (priority < 5)
		{
			// invulnerability
			if ((CPlayer->cheats & CF_GODMODE)
				|| CPlayer->powers[pw_invulnerability])
			{
				priority = 4;
				FaceIndex = ST_GODFACE;
				FaceCount = 1;
			}
		}

		// look left or look right if the facecount has timed out
		if (!FaceCount)
		{
			FaceIndex = CalcPainOffset() + (RandomNumber % 3);
			FaceCount = ST_STRAIGHTFACECOUNT;
			priority = 0;
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
	byte OldArms[6];
	byte OldKeys[3];
	WORD OldAmmo[4];
	WORD OldMaxAmmo[4];
	int OldHealth;
	int OldArmor;
	int OldActiveAmmo;
	int OldFrags;
	int FaceHealth;

	char HealthRefresh;
	char ArmorRefresh;
	char ActiveAmmoRefresh;
	char FragsRefresh;
	char ArmsRefresh[3];
	char AmmoRefresh;
	char MaxAmmoRefresh;
	char FaceRefresh;
	char KeysRefresh;

	bool FaceWeaponsOwned[NUMWEAPONS];
};

FDoomStatusBar::FDoomStatusBarTexture::FDoomStatusBarTexture ()
: FPatchTexture (Wads.GetNumForName ("STBAR"), FTexture::TEX_MiscPatch)
{
}

void FDoomStatusBar::FDoomStatusBarTexture::DrawToBar (const char *name, int x, int y, BYTE *colormap_in)
{
	BYTE colormap[256];

	if (Pixels == NULL)
	{
		MakeTexture ();
	}

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

	TexMan[name]->CopyToBlock (Pixels, Width, Height, x, y, colormap);
}

FBaseStatusBar *CreateDoomStatusBar ()
{
	return new FDoomStatusBar;
}
