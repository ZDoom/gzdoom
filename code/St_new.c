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

static int		widestnum, numheight;
static patch_t	*medi;
static patch_t	*armors[2];
static patch_t	*ammos[4];
static char		ammopatches[4][8] = { "CLIPA0", "SHELA0", "CELLA0", "ROCKA0" };
static int		NameUp = -1;

extern patch_t	*sttminus;
extern patch_t	*tallnum[10];
extern patch_t	*faces[];
extern int		st_faceindex;
extern player_t *plyr;
extern patch_t	*keys[NUMCARDS+NUMCARDS/2];
extern byte		*Ranges;

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
		if ((lump = (W_CheckNumForName) (name, ns_sprites)) != -1)
			armors[i] = W_CacheLumpNum (lump, PU_STATIC);
	}

	for (i = 0; i < 4; i++) {
		if ((lump = (W_CheckNumForName) (ammopatches[i], ns_sprites)) != -1)
			ammos[i] = W_CacheLumpNum (lump, PU_STATIC);
	}

	if ((lump = (W_CheckNumForName) ("MEDIA0", ns_sprites)) != -1)
		medi = W_CacheLumpNum (lump, PU_STATIC);

	widestnum = widest;
	numheight = SHORT(tallnum[0]->height);

	if (netgame && (!deathmatch->value || demoplayback) && level.time)
		NameUp = level.time + 2*TICRATE;
}

void ST_DrawNum (int x, int y, screen_t *scrn, int num)
{
	char digits[8], *d;

	if (num < 0) {
		V_DrawPatchCleanNoMove (x, y, scrn, sttminus);
		x += CleanXfac * SHORT(sttminus->width);
		num = -num;
	}

	sprintf (digits, "%d", num);

	d = digits;
	while (*d) {
		if (*d >= '0' && *d <= '9') {
			V_DrawPatchCleanNoMove (x, y, scrn, tallnum[*d - '0']);
			x += CleanXfac * SHORT(tallnum[*d - '0']->width);
		}
		d++;
	}
}

void ST_DrawNumRight (int x, int y, screen_t *scrn, int num)
{
	int d = abs(num);

	do {
		x -= SHORT(tallnum[d%10]->width) * CleanXfac;
	} while (d /= 10);

	if (num < 0)
		x -= SHORT(sttminus->width) * CleanXfac;

	ST_DrawNum (x, y, scrn, num);
}

void ST_newDraw (void)
{
	int y, i;
	ammotype_t ammo = weaponinfo[plyr->readyweapon].ammo;

	y = screen.height - (numheight + 4) * CleanYfac;

	// Draw health
	V_DrawPatchCleanNoMove (20 * CleanXfac, screen.height-2*CleanYfac,
							 &screen, medi);
	ST_DrawNum (40 * CleanXfac, y, &screen, plyr->health);

	// Draw armor
	if (plyr->armortype && plyr->armorpoints) {
		if (armors[plyr->armortype])
			V_DrawPatchCleanNoMove (20 * CleanXfac, y - 4*CleanYfac,
									&screen, armors[plyr->armortype-1]);
		ST_DrawNum (40*CleanXfac, y - (SHORT(armors[0]->height)+3)*CleanYfac,
					 &screen, plyr->armorpoints);
	}

	// Draw ammo
	if (ammo < NUMAMMO) {
		patch_t *ammopatch = ammos[weaponinfo[plyr->readyweapon].ammo];

		V_DrawPatchCleanNoMove (screen.width - 14 * CleanXfac,
								screen.height - 4 * CleanYfac,
								&screen, ammopatch);
		ST_DrawNumRight (screen.width - 25 * CleanXfac, y, &screen, plyr->ammo[ammo]);
	}

	if (deathmatch->value) {
		// Draw frags (in DM)
		ST_DrawNumRight (screen.width - 2, 1, &screen, plyr->fragcount);
	} else {
		// Draw keys (not DM)
		y = CleanYfac;
		for (i = 0; i < 6; i++) {
			if (plyr->cards[i]) {
				V_DrawPatchCleanNoMove (screen.width - 10*CleanXfac, y,
										&screen, keys[i]);
				y += (8 + (i < 3 ? 0 : 2)) * CleanYfac;
			}
		}
	}
}

void ST_nameDraw (int y)
{
	int x, color;

	if (!netgame || level.time > NameUp + 5 || (level.time < TICRATE * 3 && !demoplayback))
		return;

	if (plyr - players == consoleplayer)
		color = CR_GOLD;
	else
		color = CR_GREEN;

	x = (screen.width - V_StringWidth (plyr->userinfo.netname)*CleanXfac) >> 1;
	if (level.time < NameUp)
		V_DrawTextClean (color, x, y, plyr->userinfo.netname);
	else
		V_DrawTextCleanLuc (color, x, y, plyr->userinfo.netname);
	BorderNeedRefresh = true;
}