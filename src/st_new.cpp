#include <stdlib.h>

#include "doomtype.h"
#include "doomdef.h"
#include "doomstat.h"
#include "d_items.h"
#include "v_video.h"
#include "v_text.h"
#include "w_wad.h"
#include "z_zone.h"
#include "i_system.h"
#include "m_swap.h"
#include "st_stuff.h"
#include "c_cvars.h"

static int		widestnum, numheight;
static const patch_t	*medi;
static const patch_t	*armors[2];
static const patch_t	*ammos[4];
static const char		ammopatches[4][8] = { "CLIPA0", "SHELA0", "CELLA0", "ROCKA0" };
static int		NameUp = -1;

extern patch_t	*sttminus;
extern patch_t	*tallnum[10];
extern patch_t	*faces[];
extern int		st_faceindex;
extern player_t *plyr;
extern patch_t	*keys[NUMCARDS+NUMCARDS/2];
extern byte		*Ranges;

CVAR (hud_scale, "0", CVAR_ARCHIVE)

void ST_unloadNew (void)
{
	int i;

	Z_ChangeTag (medi, PU_CACHE);

	for (i = 0; i < 2; i++)
		Z_ChangeTag (armors[i], PU_CACHE);

	for (i = 0; i < 4; i++)
		Z_ChangeTag (ammos[i], PU_CACHE);
}

void ST_initNew (void)
{
	int i;
	int widest = 0;
	char name[8];
	int lump;

	for (i = 0; i < 10; i++) {
		if (SHORT(tallnum[i]->width) > widest)
			widest = SHORT(tallnum[i]->width);
	}

	strcpy (name, "ARM1A0");
	for (i = 0; i < 2; i++) {
		name[3] = i + '1';
		if ((lump = W_CheckNumForName (name, ns_sprites)) != -1)
			armors[i] = (patch_t *)W_CacheLumpNum (lump, PU_STATIC);
	}

	for (i = 0; i < 4; i++) {
		if ((lump = W_CheckNumForName (ammopatches[i], ns_sprites)) != -1)
			ammos[i] = (patch_t *)W_CacheLumpNum (lump, PU_STATIC);
	}

	if ((lump = W_CheckNumForName ("MEDIA0", ns_sprites)) != -1)
		medi = (patch_t *)W_CacheLumpNum (lump, PU_STATIC);

	widestnum = widest;
	numheight = SHORT(tallnum[0]->height);

	if (multiplayer && (!deathmatch.value || demoplayback || !netgame) && level.time)
		NameUp = level.time + 2*TICRATE;
}

void ST_DrawNum (int x, int y, DCanvas *scrn, int num)
{
	char digits[8], *d;

	if (num < 0)
	{
		if (hud_scale.value)
		{
			scrn->DrawPatchCleanNoMove (sttminus, x, y);
			x += CleanXfac * SHORT(sttminus->width);
		}
		else
		{
			scrn->DrawPatch (sttminus, x, y);
			x += SHORT(sttminus->width);
		}
		num = -num;
	}

	sprintf (digits, "%d", num);

	d = digits;
	while (*d)
	{
		if (*d >= '0' && *d <= '9')
		{
			if (hud_scale.value)
			{
				scrn->DrawPatchCleanNoMove (tallnum[*d - '0'], x, y);
				x += CleanXfac * SHORT(tallnum[*d - '0']->width);
			}
			else
			{
				scrn->DrawPatch (tallnum[*d - '0'], x, y);
				x += SHORT(tallnum[*d - '0']->width);
			}
		}
		d++;
	}
}

void ST_DrawNumRight (int x, int y, DCanvas *scrn, int num)
{
	int d = abs(num);
	int xscale = hud_scale.value ? CleanXfac : 1;

	do {
		x -= SHORT(tallnum[d%10]->width) * xscale;
	} while (d /= 10);

	if (num < 0)
		x -= SHORT(sttminus->width) * xscale;

	ST_DrawNum (x, y, scrn, num);
}

void ST_newDraw (void)
{
	int y, i;
	ammotype_t ammo = weaponinfo[plyr->readyweapon].ammo;
	int xscale = hud_scale.value ? CleanXfac : 1;
	int yscale = hud_scale.value ? CleanYfac : 1;

	y = screen->height - (numheight + 4) * yscale;

	// Draw health
	if (hud_scale.value)
		screen->DrawPatchCleanNoMove (medi, 20 * CleanXfac,
									  screen->height - 2*CleanYfac);
	else
		screen->DrawPatch (medi, 20, screen->height - 2);
	ST_DrawNum (40 * xscale, y, screen, plyr->health);

	// Draw armor
	if (plyr->armortype && plyr->armorpoints)
	{
		if (armors[plyr->armortype])
		{
			if (hud_scale.value)
				screen->DrawPatchCleanNoMove (plyr->armortype > 2 ?
											  armors[1] :
											  armors[plyr->armortype-1],
											  20 * CleanXfac,
											  y - 4*CleanYfac);
			else
				screen->DrawPatch (plyr->armortype > 2 ? armors[1] :
								   armors[plyr->armortype - 1],
								   20, y - 4);
		}
		ST_DrawNum (40*xscale, y - (SHORT(armors[0]->height)+3)*yscale,
					 screen, plyr->armorpoints);
	}

	// Draw ammo
	if (ammo < NUMAMMO)
	{
		const patch_t *ammopatch = ammos[weaponinfo[plyr->readyweapon].ammo];

		if (hud_scale.value)
			screen->DrawPatchCleanNoMove (ammopatch,
										  screen->width - 14 * CleanXfac,
										  screen->height - 4 * CleanYfac);
		else
			screen->DrawPatch (ammopatch, screen->width - 14,
							   screen->height - 4);
		ST_DrawNumRight (screen->width - 25 * xscale, y, screen,
						 plyr->ammo[ammo]);
	}

	if (deathmatch.value)
	{
		// Draw frags (in DM)
		ST_DrawNumRight (screen->width - 2, 1, screen, plyr->fragcount);
	}
	else
	{
		// Draw keys (not DM)
		y = CleanYfac;
		for (i = 0; i < 6; i++)
		{
			if (plyr->cards[i])
			{
				if (hud_scale.value)
					screen->DrawPatchCleanNoMove (keys[i], screen->width - 10*CleanXfac, y);
				else
					screen->DrawPatch (keys[i], screen->width - 10, y);
				y += (8 + (i < 3 ? 0 : 2)) * yscale;
			}
		}
	}
}

void ST_nameDraw (int y)
{
	int x, color;
	char conbuff[64], *string, pnum[4];
	BOOL inconsistant;

	inconsistant = false;
	for (x = 0; x < MAXPLAYERS; x++)
	{
		if (playeringame[x] && players[x].inconsistant)
		{
			if (!inconsistant)
			{
				strcpy (conbuff, "Consistency failure: ");
				inconsistant = true;
			}
			pnum[0] = '0' + x;
			pnum[1] = 0;
			strcat (conbuff, pnum);
		}
	}
	if (!inconsistant && (!netgame || level.time > NameUp + 5 || (level.time < TICRATE * 3 && !demoplayback)))
		return;

	if (inconsistant)
	{
		color = CR_GREEN;
		string = conbuff;
	}
	else
	{
		string = plyr->userinfo.netname;
		if (plyr - players == consoleplayer)
			color = CR_GOLD;
		else
			color = CR_GREEN;
	}

	x = (screen->width - V_StringWidth (string)*CleanXfac) >> 1;
	if (level.time < NameUp || inconsistant)
		screen->DrawTextClean (color, x, y, string);
	else
		screen->DrawTextCleanLuc (color, x, y, string);
	BorderNeedRefresh = true;
}
