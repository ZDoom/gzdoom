/*
** Enhanced heads up 'overlay' for fullscreen
**
**---------------------------------------------------------------------------
** Copyright 2003-2008 Christoph Oelckers
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

// NOTE: Some stuff in here might seem a little redundant but I wanted this
// to be as true as possible to my original intent which means that it
// only uses that code from ZDoom's status bar that is the same as any
// copy would be.

#include "doomtype.h"
#include "v_video.h"
#include "gi.h"
#include "c_cvars.h"
#include "w_wad.h"
#include "a_keys.h"
#include "sbar.h"
#include "sc_man.h"
#include "templates.h"
#include "p_local.h"
#include "doomstat.h"
#include "g_level.h"


#define HUMETA_AltIcon 0x10f000

EXTERN_CVAR(Bool,am_follow)
EXTERN_CVAR (Int, con_scaletext)
EXTERN_CVAR (Bool, idmypos)

EXTERN_CVAR (Bool, am_showtime)
EXTERN_CVAR (Bool, am_showtotaltime)

CVAR(Int,hud_althudscale, 2, CVAR_ARCHIVE)				// Scale the hud to 640x400?
CVAR(Bool,hud_althud, false, CVAR_ARCHIVE)				// Enable/Disable the alternate HUD

														// These are intentionally not the same as in the automap!
CVAR (Bool,  hud_showsecrets,	true,CVAR_ARCHIVE);		// Show secrets on HUD
CVAR (Bool,  hud_showmonsters,	true,CVAR_ARCHIVE);		// Show monster stats on HUD
CVAR (Bool,  hud_showitems,		false,CVAR_ARCHIVE);	// Show item stats on HUD
CVAR (Bool,  hud_showstats,		false,	CVAR_ARCHIVE);	// for stamina and accuracy. 
CVAR (Bool,  hud_showscore,		false,	CVAR_ARCHIVE);	// for user maintained score

CVAR (Int, hud_ammo_red, 25, CVAR_ARCHIVE)					// ammo percent less than which status is red    
CVAR (Int, hud_ammo_yellow, 50, CVAR_ARCHIVE)				// ammo percent less is yellow more green        
CVAR (Int, hud_health_red, 25, CVAR_ARCHIVE)				// health amount less than which status is red   
CVAR (Int, hud_health_yellow, 50, CVAR_ARCHIVE)				// health amount less than which status is yellow
CVAR (Int, hud_health_green, 100, CVAR_ARCHIVE)				// health amount above is blue, below is green   
CVAR (Int, hud_armor_red, 25, CVAR_ARCHIVE)					// armor amount less than which status is red    
CVAR (Int, hud_armor_yellow, 50, CVAR_ARCHIVE)				// armor amount less than which status is yellow 
CVAR (Int, hud_armor_green, 100, CVAR_ARCHIVE)				// armor amount above is blue, below is green    

CVAR (Int, hudcolor_titl, CR_YELLOW, CVAR_ARCHIVE)			// color of automap title
CVAR (Int, hudcolor_time, CR_RED, CVAR_ARCHIVE)				// color of level/hub time
CVAR (Int, hudcolor_ltim, CR_ORANGE, CVAR_ARCHIVE)			// color of single level time
CVAR (Int, hudcolor_ttim, CR_GOLD, CVAR_ARCHIVE)			// color of total time
CVAR (Int, hudcolor_xyco, CR_GREEN, CVAR_ARCHIVE)			// color of coordinates

CVAR (Int, hudcolor_statnames, CR_RED, CVAR_ARCHIVE)		// For the letters befóre the stats
CVAR (Int, hudcolor_stats, CR_GREEN, CVAR_ARCHIVE)			// For the stats values themselves


CVAR(Bool, map_point_coordinates, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)	// show player or map coordinates?

static FFont * HudFont;						// The font for the health and armor display
static FFont * IndexFont;					// The font for the inventory indices

// Icons
static FTexture * healthpic;				// Health icon
static FTexture * fragpic;					// Frags icon
static FTexture * invgems[4];				// Inventory arrows

static int hudwidth, hudheight;				// current width/height for HUD display
static int statspace;

void AM_GetPosition(fixed_t & x, fixed_t & y);


FTextureID GetHUDIcon(const PClass *cls)
{
	FTextureID tex;
	tex.texnum = cls->Meta.GetMetaInt(HUMETA_AltIcon, 0);
	return tex;
}

void SetHUDIcon(PClass *cls, FTextureID tex)
{
	cls->Meta.SetMetaInt(HUMETA_AltIcon, tex.GetIndex());
}

//---------------------------------------------------------------------------
//
// Draws an image into a box with its bottom center at the bottom
// center of the box. The image is scaled down if it doesn't fit
//
//---------------------------------------------------------------------------
static void DrawImageToBox(FTexture * tex, int x, int y, int w, int h, int trans=0xc000)
{
	float scale1, scale2;

	if (tex)
	{
		int texwidth=tex->GetWidth();
		int texheight=tex->GetHeight();

		if (w<texwidth) scale1=(float)w/texwidth;
		else scale1=1.0f;
		if (h<texheight) scale2=(float)h/texheight;
		else scale2=1.0f;
		if (scale2<scale1) scale1=scale2;

		x+=w>>1;
		y+=h;

		w=(int)(texwidth*scale1);
		h=(int)(texheight*scale1);

		screen->DrawTexture(tex, x, y,
			DTA_KeepRatio, true,
			DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, DTA_Alpha, trans, 
			DTA_DestWidth, w, DTA_DestHeight, h, DTA_CenterBottomOffset, 1, TAG_DONE);

	}
}


//---------------------------------------------------------------------------
//
// Draws a text but uses a fixed width for all characters
//
//---------------------------------------------------------------------------

static void DrawHudText(FFont *font, int color, char * text, int x, int y, int trans=0xc000)
{
	int zerowidth;
	FTexture *tex_zero = font->GetChar('0', &zerowidth);

	x+=zerowidth/2;
	for(int i=0;text[i];i++)
	{
		int width;
		FTexture *texc = font->GetChar(text[i], &width);
		int offset = texc->TopOffset - tex_zero->TopOffset + tex_zero->GetHeight();
		screen->DrawChar(font, color, x, y, text[i],
			DTA_KeepRatio, true,
			DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, DTA_Alpha, trans, 
			DTA_LeftOffset, width/2, DTA_TopOffset, offset,
			/*DTA_CenterBottomOffset, 1,*/ TAG_DONE);
		x+=zerowidth;
	}
}


//---------------------------------------------------------------------------
//
// Draws a numberses a fixed widh for all characters
//
//---------------------------------------------------------------------------

static void DrawHudNumber(FFont *font, int color, int num, int x, int y, int trans=0xc000)
{
	char text[15];

	mysnprintf(text, countof(text), "%d", num);
	DrawHudText(font, color, text, x, y, trans);
}


//===========================================================================
//
// draw the status (number of kills etc)
//
//===========================================================================

static void DrawStatLine(int x, int &y, const char *prefix, const char *string)
{
	y -= SmallFont->GetHeight()-1;
	screen->DrawText(SmallFont, hudcolor_statnames, x, y, prefix, 
		DTA_KeepRatio, true,
		DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, DTA_Alpha, 0xc000, TAG_DONE);

	screen->DrawText(SmallFont, hudcolor_stats, x+statspace, y, string,
		DTA_KeepRatio, true,
		DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, DTA_Alpha, 0xc000, TAG_DONE);
}

static void DrawStatus(player_t * CPlayer, int x, int y)
{
	char tempstr[50];
	
	if (hud_showscore)
	{
		mysnprintf(tempstr, countof(tempstr), "%i ", CPlayer->mo->Score);
		DrawStatLine(x, y, "Sc:", tempstr);
	}
	
	if (hud_showstats)
	{
		mysnprintf(tempstr, countof(tempstr), "%i ", CPlayer->accuracy);
		DrawStatLine(x, y, "Ac:", tempstr);
		mysnprintf(tempstr, countof(tempstr), "%i ", CPlayer->stamina);
		DrawStatLine(x, y, "St:", tempstr);
	}
	
	if (!deathmatch)
	{
		// FIXME: ZDoom doesn't preserve the player's stat counters across hubs so this doesn't
		// work in cooperative hub games
		if (hud_showsecrets)
		{
			mysnprintf(tempstr, countof(tempstr), "%i/%i ", multiplayer? CPlayer->secretcount : level.found_secrets, level.total_secrets);
			DrawStatLine(x, y, "S:", tempstr);
		}
		
		if (hud_showitems)
		{
			mysnprintf(tempstr, countof(tempstr), "%i/%i ", multiplayer? CPlayer->itemcount : level.found_items, level.total_items);
			DrawStatLine(x, y, "I:", tempstr);
		}
		
		if (hud_showmonsters)
		{
			mysnprintf(tempstr, countof(tempstr), "%i/%i ", multiplayer? CPlayer->killcount : level.killed_monsters, level.total_monsters);
			DrawStatLine(x, y, "K:", tempstr);
		}
	}
}


//===========================================================================
//
// draw health
//
//===========================================================================

static void DrawHealth(int health, int x, int y)
{
	// decide on the color first
	int fontcolor =
		health < hud_health_red ? CR_RED :
		health < hud_health_yellow ? CR_GOLD :
		health <= hud_health_green ? CR_GREEN :
		CR_BLUE;
	
	DrawImageToBox(healthpic, x, y, 31, 17);
	DrawHudNumber(HudFont, fontcolor, health, x + 33, y + 17);
}

//===========================================================================
//
// Draw Armor.
// very similar to drawhealth.
//
//===========================================================================

static void DrawArmor(AInventory * armor, int x, int y)
{
	if (armor)
	{
		int ap=armor->Amount;

		// decide on color
		int fontcolor =
			ap < hud_armor_red ? CR_RED :
			ap < hud_armor_yellow ? CR_GOLD :
			ap <= hud_armor_green ? CR_GREEN :
			CR_BLUE;


		if (ap)
		{
			DrawImageToBox(TexMan[armor->Icon], x, y, 31, 17);
			DrawHudNumber(HudFont, fontcolor, ap, x + 33, y + 17);
		}
	}
}

//===========================================================================
//
// KEYS
//
//===========================================================================

//---------------------------------------------------------------------------
//
// create a sorted list of the defined keys so 
// this doesn't have to be done each frame
//
//---------------------------------------------------------------------------
static TArray<const PClass*> KeyTypes, UnassignedKeyTypes;

static int STACK_ARGS ktcmp(const void * a, const void * b)
{
	AKey * key1 = (AKey*)GetDefaultByType ( *(const PClass**)a );
	AKey * key2 = (AKey*)GetDefaultByType ( *(const PClass**)b );
	return key1->KeyNumber - key2->KeyNumber;
}

static void SetKeyTypes()
{
	for(unsigned int i=0;i<PClass::m_Types.Size();i++)
	{
		const PClass * ti = PClass::m_Types[i];

		if (ti->IsDescendantOf(RUNTIME_CLASS(AKey)))
		{
			AKey * key = (AKey*)GetDefaultByType(ti);

			if (key->Icon.isValid() && key->KeyNumber>0)
			{
				KeyTypes.Push(ti);
			}
			else 
			{
				UnassignedKeyTypes.Push(ti);
			}
		}
	}
	if (KeyTypes.Size())
	{
		qsort(&KeyTypes[0], KeyTypes.Size(), sizeof(KeyTypes[0]), ktcmp);
	}
	else
	{
		// Don't leave the list empty
		const PClass * ti = RUNTIME_CLASS(AKey);
		KeyTypes.Push(ti);
	}
}

//---------------------------------------------------------------------------
//
// Draw one key
//
// Regarding key icons, Doom's are too small, Heretic doesn't have any,
// for Hexen the in-game sprites look better and for Strife it doesn't matter
// so always use the spawn state's sprite instead of the icon here unless an
// override is specified in ALTHUDCF.
//
//---------------------------------------------------------------------------

static void DrawOneKey(int xo, int & x, int & y, int & c, AInventory * inv)
{
	FTextureID icon = FNullTextureID();
	FTextureID AltIcon = GetHUDIcon(inv->GetClass());

	if (!AltIcon.Exists()) return;

	if (AltIcon.isValid()) 
	{
		icon = AltIcon;
	}
	else if (inv->SpawnState && inv->SpawnState->sprite!=0)
	{
		FState * state = inv->SpawnState;
		if (state &&  (unsigned)state->sprite < (unsigned)sprites.Size ())
		{
			spritedef_t * sprdef = &sprites[state->sprite];
			spriteframe_t * sprframe = &SpriteFrames[sprdef->spriteframes + state->GetFrame()];
			icon = sprframe->Texture[0];
		}
	}
	if (icon.isNull()) icon = inv->Icon;

	if (icon.isValid())	
	{
		x -= 9;
		DrawImageToBox(TexMan[icon], x, y, 8, 10);
		c++;
		if (c>=10)
		{
			x=xo;
			y-=11;
			c=0;
		}
	}
}

//---------------------------------------------------------------------------
//
// Draw all keys
//
//---------------------------------------------------------------------------

static int DrawKeys(player_t * CPlayer, int x, int y)
{
	int yo=y;
	int xo=x;
	int i;
	int c=0;
	AInventory * inv;

	if (!deathmatch)
	{
		if (KeyTypes.Size()==0) SetKeyTypes();

		// First all keys that are assigned to locks (in reverse order of definition)
		for(i=KeyTypes.Size()-1;i>=0;i--)
		{
			if ((inv=CPlayer->mo->FindInventory(KeyTypes[i])))
			{
				DrawOneKey(xo, x, y, c, inv);
			}
		}
		// And now the rest
		for(i=UnassignedKeyTypes.Size()-1;i>=0;i--)
		{
			if ((inv=CPlayer->mo->FindInventory(UnassignedKeyTypes[i])))
			{
				DrawOneKey(xo, x, y, c, inv);
			}
		}
	}
	if (x==xo && y!=yo) y+=11;
	return y-11;
}


//---------------------------------------------------------------------------
//
// Drawing Ammo
//
//---------------------------------------------------------------------------
static TArray<const PClass *> orderedammos;

static void AddAmmoToList(AWeapon * weapdef)
{

	for(int i=0; i<2;i++)
	{
		const PClass * ti = i==0? weapdef->AmmoType1 : weapdef->AmmoType2;
		if (ti)
		{
			AAmmo * ammodef=(AAmmo*)GetDefaultByType(ti);

			if (ammodef && !(ammodef->ItemFlags&IF_INVBAR))
			{
				unsigned int j;

				for(j=0;j<orderedammos.Size();j++)
				{
					if (ti == orderedammos[j]) break;
				}
				if (j==orderedammos.Size()) orderedammos.Push(ti);
			}
		}
	}
}

static int DrawAmmo(player_t *CPlayer, int x, int y)
{

	int i,j,k;
	char buf[256];
	AInventory *inv;

	AWeapon *wi=CPlayer->ReadyWeapon;

	orderedammos.Clear();

	// Order ammo by use of weapons in the weapon slots
	// Do not check for actual presence in the inventory!
	// We want to show all ammo types that can be used by
	// the weapons in the weapon slots.
	for (k = 0; k < NUM_WEAPON_SLOTS; k++) for(j = 0; j < CPlayer->weapons.Slots[k].Size(); j++)
	{
		const PClass *weap = CPlayer->weapons.Slots[k].GetWeapon(j);

		if (weap) AddAmmoToList((AWeapon*)GetDefaultByType(weap));
	}

	// Now check for the remaining weapons that are in the inventory but not in the weapon slots
	for(inv=CPlayer->mo->Inventory;inv;inv=inv->Inventory)
	{
		if (inv->IsKindOf(RUNTIME_CLASS(AWeapon)))
		{
			AddAmmoToList((AWeapon*)inv);
		}
	}

	// ok, we got all ammo types. Now draw the list back to front (bottom to top)

	int def_width = ConFont->StringWidth("000/000");
	x-=def_width;
	int yadd = ConFont->GetHeight();

	for(i=orderedammos.Size()-1;i>=0;i--)
	{

		const PClass * type = orderedammos[i];
		AAmmo * ammoitem = (AAmmo*)CPlayer->mo->FindInventory(type);

		AAmmo * inv = ammoitem? ammoitem : (AAmmo*)GetDefaultByType(orderedammos[i]);
		FTextureID AltIcon = GetHUDIcon(type);
		FTextureID icon = !AltIcon.isNull()? AltIcon : inv->Icon;
		if (!icon.isValid()) continue;

		int trans= (wi && (type==wi->AmmoType1 || type==wi->AmmoType2)) ? 0xc000:0x6000;

		int maxammo = inv->MaxAmount;
		int ammo = ammoitem? ammoitem->Amount : 0;

		mysnprintf(buf, countof(buf), "%3d/%3d", ammo, maxammo);

		int tex_width= clamp<int>(ConFont->StringWidth(buf)-def_width, 0, 1000);

		int fontcolor=( !maxammo ? CR_GRAY :    
						 ammo < ( (maxammo * hud_ammo_red) / 100) ? CR_RED :   
						 ammo < ( (maxammo * hud_ammo_yellow) / 100) ? CR_GOLD : CR_GREEN );

		DrawHudText(ConFont, fontcolor, buf, x-tex_width, y+yadd, trans);
		DrawImageToBox(TexMan[icon], x-20, y, 16, 8, trans);
		y-=10;
	}
	return y;
}


//---------------------------------------------------------------------------
//
// Weapons List
//
//---------------------------------------------------------------------------

static void DrawOneWeapon(player_t * CPlayer, int x, int & y, AWeapon * weapon)
{
	int trans;
	FTextureID picnum;

	// Powered up weapons and inherited sister weapons are not displayed.
	if (weapon->WeaponFlags & WIF_POWERED_UP) return;
	if (weapon->SisterWeapon && weapon->IsKindOf(RUNTIME_TYPE(weapon->SisterWeapon))) return;

	trans=0x6666;
	if (CPlayer->ReadyWeapon)
	{
		if (weapon==CPlayer->ReadyWeapon || weapon==CPlayer->ReadyWeapon->SisterWeapon) trans=0xd999;
	}

	FState * state=NULL, *ReadyState;
	
	FTextureID AltIcon = GetHUDIcon(weapon->GetClass());
	picnum = !AltIcon.isNull()? AltIcon : weapon->Icon;

	if (picnum.isNull())
	{
		if (weapon->SpawnState && weapon->SpawnState->sprite!=0)
		{
			state = weapon->SpawnState;
		}
		// no spawn state - now try the ready state
		else if ((ReadyState = weapon->FindState(NAME_Ready)) && ReadyState->sprite!=0)
		{
			state = ReadyState;
		}
		if (state &&  (unsigned)state->sprite < (unsigned)sprites.Size ())
		{
			spritedef_t * sprdef = &sprites[state->sprite];
			spriteframe_t * sprframe = &SpriteFrames[sprdef->spriteframes + state->GetFrame()];

			picnum = sprframe->Texture[0];
		}
	}

	if (picnum.isValid())
	{
		FTexture * tex = TexMan[picnum];
		int w = tex->GetWidth();
		int h = tex->GetHeight();
		int rh;
		if (w>h) rh=8;
		else rh=16,y-=8;		// don't draw tall sprites too small!
		DrawImageToBox(tex, x-24, y, 20, rh, trans);
		y-=10;
	}
}


static void DrawWeapons(player_t * CPlayer, int x, int y)
{
	int k,j;
	AInventory * inv;

	// First draw all weapons in the inventory that are not assigned to a weapon slot
	for(inv=CPlayer->mo->Inventory;inv;inv=inv->Inventory)
	{
		if (inv->IsKindOf(RUNTIME_CLASS(AWeapon)) && 
			!CPlayer->weapons.LocateWeapon(RUNTIME_TYPE(inv), NULL, NULL))
		{
			DrawOneWeapon(CPlayer, x, y, static_cast<AWeapon*>(inv));
		}
	}

	// And now everything in the weapon slots back to front
	for (k = NUM_WEAPON_SLOTS - 1; k >= 0; k--) for(j = CPlayer->weapons.Slots[k].Size() - 1; j >= 0; j--)
	{
		const PClass *weap = CPlayer->weapons.Slots[k].GetWeapon(j);
		if (weap) 
		{
			inv=CPlayer->mo->FindInventory(weap);
			if (inv) 
			{
				DrawOneWeapon(CPlayer, x, y, static_cast<AWeapon*>(inv));
			}
		}
	}
}


//---------------------------------------------------------------------------
//
// Draw the Inventory
//
//---------------------------------------------------------------------------

static void DrawInventory(player_t * CPlayer, int x,int y)
{
	AInventory * rover;
	int numitems = (hudwidth - 2*x) / 32;
	int i;

	CPlayer->mo->InvFirst = rover = StatusBar->ValidateInvFirst(numitems);
	if (rover!=NULL)
	{
		if(rover->PrevInv())
		{
			screen->DrawTexture(invgems[!!(level.time&4)], x-10, y,
				DTA_KeepRatio, true,
				DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, DTA_Alpha, 0x6666, TAG_DONE);
		}

		for(i=0;i<numitems && rover;rover=rover->NextInv())
		{
			if (rover->Amount>0)
			{
				FTextureID AltIcon = GetHUDIcon(rover->GetClass());

				if (AltIcon.Exists() && (rover->Icon.isValid() || AltIcon.isValid()) )
				{
					int trans = rover==CPlayer->mo->InvSel ? FRACUNIT : 0x6666;

					DrawImageToBox(TexMan[AltIcon.isValid()? AltIcon : rover->Icon], x, y, 19, 25, trans);
					if (rover->Amount>1)
					{
						char buffer[10];
						int xx;
						mysnprintf(buffer, countof(buffer), "%d", rover->Amount);
						if (rover->Amount>=1000) xx = 32 - IndexFont->StringWidth(buffer);
						else xx = 22;

						screen->DrawText(IndexFont, CR_GOLD, x+xx, y+20, buffer, 
							DTA_KeepRatio, true,
							DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, DTA_Alpha, trans, TAG_DONE);
					}
					
					x+=32;
					i++;
				}
			}
		}
		if(rover)
		{
			screen->DrawTexture(invgems[2 + !!(level.time&4)], x-10, y,
				DTA_KeepRatio, true,
				DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, DTA_Alpha, 0x6666, TAG_DONE);
		}
	}
}

//---------------------------------------------------------------------------
//
// Draw the Frags
//
//---------------------------------------------------------------------------

static void DrawFrags(player_t * CPlayer, int x, int y)
{
	DrawImageToBox(fragpic, x, y, 31, 17);
	DrawHudNumber(HudFont, CR_GRAY, CPlayer->fragcount, x + 33, y + 17);
}



//---------------------------------------------------------------------------
//
// PROC DrawCoordinates
//
//---------------------------------------------------------------------------

static void DrawCoordinates(player_t * CPlayer)
{
	fixed_t x;
	fixed_t y;
	fixed_t z;
	char coordstr[18];
	int h = SmallFont->GetHeight()+1;

	
	if (!map_point_coordinates || !automapactive) 
	{
		x=CPlayer->mo->x;
		y=CPlayer->mo->y;                     
		z=CPlayer->mo->z;
	}
	else 
	{
		AM_GetPosition(x,y);
		z = P_PointInSector(x, y)->floorplane.ZatPoint(x, y);
	}

	int vwidth = con_scaletext==0? SCREENWIDTH : SCREENWIDTH/2;
	int vheight = con_scaletext==0? SCREENHEIGHT : SCREENHEIGHT/2;
	int xpos = vwidth - SmallFont->StringWidth("X: -00000")-6;
	int ypos = 18;

	mysnprintf(coordstr, countof(coordstr), "X: %d", x>>FRACBITS);
	screen->DrawText(SmallFont, hudcolor_xyco, xpos, ypos, coordstr,
		DTA_KeepRatio, true,
		DTA_VirtualWidth, vwidth, DTA_VirtualHeight, vheight, TAG_DONE);

	mysnprintf(coordstr, countof(coordstr), "Y: %d", y>>FRACBITS);
	screen->DrawText(SmallFont, hudcolor_xyco, xpos, ypos+h, coordstr,
		DTA_KeepRatio, true,
		DTA_VirtualWidth, vwidth, DTA_VirtualHeight, vheight, TAG_DONE);

	mysnprintf(coordstr, countof(coordstr), "Z: %d", z>>FRACBITS);
	screen->DrawText(SmallFont, hudcolor_xyco, xpos, ypos+2*h, coordstr,
		DTA_KeepRatio, true,
		DTA_VirtualWidth, vwidth, DTA_VirtualHeight, vheight, TAG_DONE);
}


//---------------------------------------------------------------------------
//
// draw the overlay
//
//---------------------------------------------------------------------------
void HUD_InitHud();

void DrawHUD()
{
	player_t * CPlayer = StatusBar->CPlayer;

	if (HudFont==NULL) HUD_InitHud();

	players[consoleplayer].inventorytics = 0;
	if (hud_althudscale && SCREENWIDTH>640) 
	{
		hudwidth=SCREENWIDTH/2;
		if (hud_althudscale == 2) 
		{
			// Optionally just double the pixels to reduce scaling artifacts.
			hudheight=SCREENHEIGHT/2;
		}
		else 
		{
			if (WidescreenRatio == 4)
			{
				hudheight = hudwidth * 30 / BaseRatioSizes[WidescreenRatio][3];	// BaseRatioSizes is inverted for this mode
			}
			else
			{
				hudheight = hudwidth * 30 / (48*48/BaseRatioSizes[WidescreenRatio][3]);
			}
		}
	}
	else
	{
		hudwidth=SCREENWIDTH;
		hudheight=SCREENHEIGHT;
	}

	if (!automapactive)
	{
		int i;

		// No HUD in the title level!
		if (gamestate == GS_TITLELEVEL || !CPlayer) return;

		if (!deathmatch) DrawStatus(CPlayer, 5, hudheight-50);
		else
		{
			DrawStatus(CPlayer, 5, hudheight-75);
			DrawFrags(CPlayer, 5, hudheight-70);
		}
		DrawHealth(CPlayer->health, 5, hudheight-45);
		// Yes, that doesn't work properly for Hexen but frankly, I have no
		// idea how to make a meaningful value out of Hexen's armor system!
		DrawArmor(CPlayer->mo->FindInventory(RUNTIME_CLASS(ABasicArmor)), 5, hudheight-20);
		i=DrawKeys(CPlayer, hudwidth-4, hudheight-10);
		i=DrawAmmo(CPlayer, hudwidth-5, i);
		DrawWeapons(CPlayer, hudwidth-5, i);
		DrawInventory(CPlayer, 144, hudheight-28);
		if (CPlayer->camera && CPlayer->camera->player)
		{
			StatusBar->DrawCrosshair();
		}
		if (idmypos) DrawCoordinates(CPlayer);
	}
	else
	{
		char printstr[256];
		int seconds;
		cluster_info_t *thiscluster = FindClusterInfo (level.cluster);
		bool hub = !!(thiscluster->flags&CLUSTER_HUB);
		int length=8*SmallFont->GetCharWidth('0');
		int fonth=SmallFont->GetHeight()+1;
		int bottom=hudheight-1;

		if (am_showtotaltime)
		{
			seconds = level.totaltime / TICRATE;
			mysnprintf(printstr, countof(printstr), "%02i:%02i:%02i", seconds/3600, (seconds%3600)/60, seconds%60);
			DrawHudText(SmallFont, hudcolor_ttim, printstr, hudwidth-length, bottom, FRACUNIT);
			bottom -= fonth;
		}

		if (am_showtime)
		{
			seconds = level.time /TICRATE;
			mysnprintf(printstr, countof(printstr), "%02i:%02i:%02i", seconds/3600, (seconds%3600)/60, seconds%60);
			DrawHudText(SmallFont, hudcolor_time, printstr, hudwidth-length, bottom, FRACUNIT);
			bottom -= fonth;

			// Single level time for hubs
			if (level.clusterflags&CLUSTER_HUB)
			{
				seconds= level.maptime /TICRATE;
				mysnprintf(printstr, countof(printstr), "%02i:%02i:%02i", seconds/3600, (seconds%3600)/60, seconds%60);
				DrawHudText(SmallFont, hudcolor_ltim, printstr, hudwidth-length, bottom, FRACUNIT);
			}
		}

		mysnprintf(printstr, countof(printstr), "%s: %s", level.mapname, level.LevelName.GetChars());
		screen->DrawText(SmallFont, hudcolor_titl, 1, hudheight-fonth-1, printstr,
			DTA_KeepRatio, true,
			DTA_VirtualWidth, hudwidth, DTA_VirtualHeight, hudheight, TAG_DONE);

		DrawCoordinates(CPlayer);
	}
}

/////////////////////////////////////////////////////////////////////////
//
// Initialize the fonts and other data
//
/////////////////////////////////////////////////////////////////////////

void HUD_InitHud()
{
	switch (gameinfo.gametype)
	{
	case GAME_Heretic:
	case GAME_Hexen:
		healthpic = TexMan.FindTexture("ARTIPTN2");
		HudFont=FFont::FindFont("HUDFONT_RAVEN");
		break;

	case GAME_Strife:
		healthpic = TexMan.FindTexture("I_MDKT");
		HudFont=BigFont;	// Strife doesn't have anything nice so use the standard font
		break;

	default:
		healthpic = TexMan.FindTexture("MEDIA0");
		HudFont=FFont::FindFont("HUDFONT_DOOM");
		break;
	}

	IndexFont = V_GetFont("INDEXFONT");

	if (HudFont == NULL) HudFont = BigFont;
	if (IndexFont == NULL) IndexFont = ConFont;	// Emergency fallback

	invgems[0] = TexMan.FindTexture("INVGEML1");
	invgems[1] = TexMan.FindTexture("INVGEML2");
	invgems[2] = TexMan.FindTexture("INVGEMR1");
	invgems[3] = TexMan.FindTexture("INVGEMR2");

	fragpic = TexMan.FindTexture("HU_FRAGS");	// Sadly, I don't have anything usable for this. :(

	KeyTypes.Clear();
	UnassignedKeyTypes.Clear();

	statspace = SmallFont->StringWidth("Ac:");



	// Now read custom icon overrides
	int lump, lastlump = 0;

	while ((lump = Wads.FindLump ("ALTHUDCF", &lastlump)) != -1)
	{
		FScanner sc(lump);
		while (sc.GetString())
		{
			if (sc.Compare("Health"))
			{
				sc.MustGetString();
				FTextureID tex = TexMan.CheckForTexture(sc.String, FTexture::TEX_MiscPatch);
				if (tex.isValid()) healthpic = TexMan[tex];
			}
			else
			{
				const PClass * ti = PClass::FindClass(sc.String);
				if (!ti)
				{
					Printf("Unknown item class '%s' in ALTHUDCF\n", sc.String);
				}
				else if (!ti->IsDescendantOf(RUNTIME_CLASS(AInventory)))
				{
					Printf("Invalid item class '%s' in ALTHUDCF\n", sc.String);
					ti=NULL;
				}
				sc.MustGetString();
				FTextureID tex;

				if (!sc.Compare("0") && !sc.Compare("NULL") && !sc.Compare(""))
				{
					tex = TexMan.CheckForTexture(sc.String, FTexture::TEX_MiscPatch);
				}
				else tex.SetInvalid();

				if (ti) SetHUDIcon(const_cast<PClass*>(ti), tex);
			}
		}
	}
}

