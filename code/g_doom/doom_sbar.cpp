#include "doomtype.h"
#include "doomstat.h"
#include "v_font.h"
#include "v_video.h"
#include "sbar.h"
#include "r_defs.h"
#include "w_wad.h"
#include "z_zone.h"
#include "m_random.h"
#include "d_player.h"
#include "st_stuff.h"
#include "r_local.h"
#include "m_swap.h"

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
		static const char *doomLumpNames[] =
		{
			"STKEYS0",	"STKEYS1",	"STKEYS2",	"STKEYS3",	"STKEYS4",
			"STKEYS5",	"STKEYS6",	"STKEYS7",	"STKEYS8",	"STBAR",
			"STGNUM2",	"STGNUM3",	"STGNUM4",	"STGNUM5",	"STGNUM6",
			"STGNUM7",	"MEDIA0",
		};

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

		int dummy;

		FBaseStatusBar::Images.Init (sharedLumpNames, NUM_BASESB_IMAGES);
		Images.Init (doomLumpNames, NUM_DOOMSB_IMAGES);

		FBaseStatusBar::Images.GetImage (imgBNumbers+3, &BigWidth, &BigHeight,
			&dummy, &dummy);

		if (!*deathmatch)
		{
			DrawToSBar ("STARMS", 104, 0);
		}

		DrawToSBar ("STTPRCNT", 90, 3);		// Health %
		DrawToSBar ("STTPRCNT", 221, 3);	// Armor %

		SB_state = screen->GetPageCount ();
		memset (FaceWeaponsOwned, 0, sizeof(FaceWeaponsOwned));
	}

	~FDoomStatusBar ()
	{
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

	void AttachToPlayer (player_t *player)
	{
		int i;

		FBaseStatusBar::AttachToPlayer (player);
		SetFace (&skins[CPlayer->userinfo.skin]);
		if (multiplayer)
		{
			V_ColorMap = translationtables + (CPlayer - players)*256*2;
			DrawToSBar ("STFBANY", 143, 1);	// face background
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
				DrawImage (Images, imgSBAR, 0, 0);
				memset (OldArms, 255, sizeof(OldArms));
				memset (OldKeys, 255, sizeof(OldKeys));
				memset (OldAmmo, 255, sizeof(OldAmmo));
				memset (OldMaxAmmo, 255, sizeof(OldMaxAmmo));
				OldFaceIndex = -1;
				OldHealth = -1;
				OldArmor = -1;
				OldActiveAmmo = -1;
				OldFrags = -9999;
			}
			DrawMainBar ();
		}
	}

private:
	void DrawMainBar ()
	{
		DrawAmmoStats ();
		DrawFace ();
		DrawKeys ();
		if (!*deathmatch)
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
				DrawPartialImage (Images, imgSBAR, 2, 3, 2, 3,
					BigWidth*3, BigHeight);
				if (Scaled)
				{
					ScaleCopy->Lock ();
					CopyToScreen (2, 3, BigWidth*3, BigHeight);
					ScaleCopy->Unlock ();
				}
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
				weapontype_t weap = WeaponSlots[i+2].GetWeapon (j);
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
				ArmsRefresh[i] = screen->GetPageCount ();
			}
		}

		// Remember the arms state for next time
		memcpy (OldArms, arms, sizeof(arms));

		for (i = 0; i < 6; i++)
		{
			if (ArmsRefresh[i])
			{
				ArmsRefresh[i]--;

				int x = 111 + (i % 3) * 12;
				int y = 4 + (i / 3) * 10;

				DrawPartialImage (Images, imgSBAR, x, y, x, y, 6, 6);
				if (arms[i])
				{
					DrawImage (FBaseStatusBar::Images, imgSmNumbers+2+i, x, y);
				}
				else
				{
					DrawImage (Images, imgGNUM2+i, x, y);
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
				AmmoRefresh[i] = screen->GetPageCount ();
			}
			if (maxammo[i] != OldMaxAmmo[i])
			{
				MaxAmmoRefresh[i] = screen->GetPageCount ();
			}
		}

		// Remember ammo counts for next time
		memcpy (OldAmmo, ammo, sizeof(ammo));
		memcpy (OldMaxAmmo, maxammo, sizeof(ammo));

		for (i = 0; i < 4; i++)
		{
			int y = 5 + 6*i;
			
			if (AmmoRefresh[i])
			{
				AmmoRefresh[i]--;
				DrawPartialImage (Images, imgSBAR, 276, y, 276, y, 4*3, 6);
				DrSmallNumber (ammo[i], 276, y);
			}
			if (MaxAmmoRefresh[i])
			{
				MaxAmmoRefresh[i]--;
				DrawPartialImage (Images, imgSBAR, 302, y, 302, y, 4*3, 6);
				DrSmallNumber (maxammo[i], 302, y);
			}
		}
	}

	void DrawFace ()
	{
		if (OldFaceIndex != FaceIndex)
		{
			FaceRefresh = screen->GetPageCount ();
			OldFaceIndex = FaceIndex;
		}
		if (FaceRefresh)
		{
			FaceRefresh--;
			DrawPartialImage (Images, imgSBAR, 142, 0, 142, 0, 37, 32);
			DrawImage (Faces, FaceIndex, 143, 0);
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
				KeysRefresh[i] = screen->GetPageCount ();
			}
		}

		// Remember keys for next time
		memcpy (OldKeys, keys, sizeof(keys));

		for (i = 0; i < 3; i++)
		{
			if (KeysRefresh[i])
			{
				KeysRefresh[i]--;

				int y = 3 + i*10;

				DrawPartialImage (Images, imgSBAR, 239, y, 239, y, 8, 5);
				DrawImage (Images, imgKEYS0+keys[i], 239, y);
			}
		}
	}

	void DrawNumber (int val, int x, int y, int size=3) const
	{
		DrawPartialImage (Images, imgSBAR, x-1, y, x-1, y,
			size*BigWidth+2, BigHeight);
		DrBNumber (val, x, y, size);
	}

	void DrawFullScreenStuff ()
	{
		int i;

		// Draw health
		OverrideImageOrigin (true);
		DrawOuterImage (Images, imgMEDI, 20, -2);
		OverrideImageOrigin (false);
		DrBNumberOuter (CPlayer->health, 40, -20);

		// Draw armor
		if (CPlayer->armortype && CPlayer->armorpoints[0])
		{
			OverrideImageOrigin (true);
			DrawOuterImage (ArmorImages, (CPlayer->armortype != deh.GreenAC),
				20, -24);
			OverrideImageOrigin (false);
			DrBNumberOuter (CPlayer->armorpoints[0], 40, -39);
		}

		// Draw ammo
		i = wpnlev1info[CPlayer->readyweapon]->ammo;
		if (i < NUMAMMO)
		{
			OverrideImageOrigin (true);
			DrawOuterImage (AmmoImages, i, -14, -4);
			OverrideImageOrigin (false);
			DrBNumberOuter (CPlayer->ammo[i], -67, -20);
		}

		if (*deathmatch)
		{ // Draw frags (in DM)
			DrBNumberOuter (CPlayer->fragcount, -44, 1);
		}
		else
		{ // Draw keys (not DM)
			for (i = 0; i < 3; i++)
			{
				if (CPlayer->keys[i])
				{
					DrawOuterImage (Images, imgKEYS0+i, -10, 2+i*10);
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
					DrawOuterImage (Images, imgKEYS0+i, x, -29+i*10);
				}
			}
		}
	}

	void DrawToSBar (const char *name, int x, int y) const
	{
		int dummy;
		byte *desttop = Images.GetImage (imgSBAR,
			&dummy, &dummy, &dummy, &dummy);

		if (desttop != NULL)
		{
			patch_t *arms = (patch_t *)W_CacheLumpName (name, PU_CACHE);
			int w = SHORT(arms->width);
			int *ofs = &arms->columnofs[0];
			desttop += x + 320*y;

			do
			{
				column_t *column = (column_t *)((byte *)arms + LONG(*ofs));

				while (column->topdelta != 0xff)
				{
					byte *source = (byte *)column + 3;
					byte *dest = desttop + column->topdelta * 320;
					int count = column->length;

					do
					{
						*dest = *source++;
						dest += 320;
					} while (--count);

					column = (column_t *)(source + 1);
				}
				ofs++;
				desttop++;
			} while (--w);
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
	// This is a not-very-pretty routine which handles
	//	the face states and their timing.
	// the precedence of expressions is:
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
				
				if (OldHealth != -1 && CPlayer->health - OldHealth > ST_MUCHPAIN)
				{
					FaceCount = ST_TURNCOUNT;
					FaceIndex = CalcPainOffset() + ST_OUCHOFFSET;
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
		imgSBAR,
		imgGNUM2,
		imgGNUM3,
		imgGNUM4,
		imgGNUM5,
		imgGNUM6,
		imgGNUM7,
		imgMEDI,

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

	char HealthRefresh;
	char ArmorRefresh;
	char ActiveAmmoRefresh;
	char FragsRefresh;
	char ArmsRefresh[6];
	char AmmoRefresh[4];
	char MaxAmmoRefresh[4];
	char FaceRefresh;
	char KeysRefresh[3];

	bool FaceWeaponsOwned[NUMWEAPONS];
};

FBaseStatusBar *CreateDoomStatusBar ()
{
	return new FDoomStatusBar;
}
