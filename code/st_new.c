#include "doomtype.h"
#include "doomdef.h"
#include "d_items.h"
#include "v_video.h"
#include "w_wad.h"
#include "z_zone.h"
#include "i_system.h"
#include "m_swap.h"

static int		widestnum, numheight;
//static patch_t	*hudback;
static patch_t	*armors[3];
static patch_t	*ammos[4];
static char		ammopatches[4][8] = { "CLIPA0", "SHELA0", "CELLA0", "ROCKA0" };

extern patch_t	*tallnum[10];
extern patch_t	*faces[];
extern int		st_faceindex;
extern player_t *plyr;

void ST_unloadNew (void)
{
	int i;

	for (i = 0; i < 3; i++)
		Z_ChangeTag (armors[i], PU_CACHE);

	for (i = 0; i < 4; i++)
		Z_ChangeTag (ammos[i], PU_CACHE);
}

void ST_initNew (void)
{
	int i;
	int widest = 0;
	char name[8];

	for (i = 0; i < 10; i++) {
		if (SHORT(tallnum[i]->width) > widest)
			widest = SHORT(tallnum[i]->width);
	}

	strcpy (name, "ARM1A0");
	for (i = 0; i < 3; i++) {
		name[3] = i + '1';
		armors[i] = W_CacheLumpName (name, PU_STATIC);
	}

	for (i = 0; i < 4; i++) {
		ammos[i] = W_CacheLumpName (ammopatches[i], PU_STATIC);
	}

	widestnum = widest;
	numheight = SHORT(tallnum[0]->height);
	//hudback = W_CacheLumpName ("HUDBACK", PU_STATIC);
}

void ST_DrawNum (int x, int y, int scrn, int num)
{
	char digits[8], *d;

	sprintf (digits, "%d", num);

	d = digits;
	while (*d) {
		if (*d >= '0' && *d <= '9') {
			V_DrawPatchCleanNoMove (x, y, scrn, tallnum[*d - '0']);
			x += CleanYfac * SHORT(tallnum[*d - '0']->width);
		}
		d++;
	}
}

void ST_DrawNumRight (int x, int y, int scrn, int num)
{
	int d;

	do {
		d = num % 10;
		x -= SHORT(tallnum[d]->width) * CleanXfac;
		V_DrawPatchCleanNoMove (x, y, scrn, tallnum[d]);
		num /= 10;
	} while (num);
}

void ST_newDraw (void)
{
	int y;
	ammotype_t ammo = weaponinfo[plyr->readyweapon].ammo;

	y = SCREENHEIGHT - (SHORT(tallnum[0]->height) + 4) * CleanYfac;

	ST_DrawNum (4 * CleanXfac, y, 0, plyr->health);

	if (ammo < NUMAMMO) {
		patch_t *ammopatch = ammos[weaponinfo[plyr->readyweapon].ammo];

		V_DrawPatchCleanNoMove (SCREENWIDTH - 14 * CleanXfac, SCREENHEIGHT - 4 * CleanYfac, 0, ammopatch);
		ST_DrawNumRight (SCREENWIDTH - 25 * CleanXfac, y, 0, plyr->ammo[ammo]);
	}
}