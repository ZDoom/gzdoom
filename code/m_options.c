/* m_options.c
**
** New options menu code.
*/

#include "m_alloc.h"

#include "doomdef.h"
#include "dstrings.h"

#include "c_console.h"
#include "c_dispatch.h"
#include "c_bindings.h"

#include "d_main.h"

#include "i_system.h"
#include "i_video.h"
#include "z_zone.h"
#include "v_video.h"
#include "w_wad.h"

#include "r_local.h"


#include "hu_stuff.h"

#include "g_game.h"

#include "m_argv.h"
#include "m_swap.h"

#include "s_sound.h"

#include "doomstat.h"

#include "m_misc.h"

// Data.
#include "sounds.h"

#include "m_menu.h"

//
// defaulted values
//
cvar_t 					*mouseSensitivity;		// has default

// Show messages has default, 0 = off, 1 = on
cvar_t 					*showMessages;
		
cvar_t 					*screenblocks;			// has default

extern boolean			message_dontfuckwithme;
extern boolean			OptionsActive;

extern int				screenSize;
extern short			skullAnimCounter;

extern cvar_t			*cl_run;
extern cvar_t			*invertmouse;
extern cvar_t			*lookspring;
extern cvar_t			*lookstrafe;
extern cvar_t			*crosshair;

void M_ChangeMessages(void);
void M_SizeDisplay(float diff);
void M_StartControlPanel(void);

int  M_StringWidth(char *string);
int  M_StringHeight(char *string);
void M_ClearMenus (void);


typedef struct oldmenu_s
{
	short				numitems;		// # of menu items
	struct oldmenu_s*	prevMenu;		// previous menu
	void		* 		menuitems;		// menu items
	void				(*routine)();	// draw routine
	short				x;
	short				y;				// x,y of menu
	short				lastOn; 		// last item user was on in menu
} oldmenu_t;

extern oldmenu_t MainDef;
extern short	 itemOn;
extern oldmenu_t *currentMenu;


typedef enum {
	whitetext,
	redtext,
	more,
	slider,
	floater,
	discrete,
	control
} itemtype;

typedef struct menuitem_s {
	itemtype		  type;
	char			 *label;
	union {
		cvar_t			**cvar;
		double			 *val;
	};
	union {
		float			  min;		/* aka numvalues */
		int				  key1;
	};
	union {
		float			  max;
		int				  key2;
	};
	float			  step;
	union {
		struct value_s	 *values;
		char			 *command;
		void			(*cfunc)(cvar_t *cvar, float newval);
		void			(*dfunc)(double oldval, double newval);
		void			(*mfunc)(void);
	};
} menuitem_t;

typedef struct menu_s {
	char		  title[8];
	int				lastOn;
	int				numitems;
	int				indent;
	menuitem_t	   *items;
	struct menu_s  *prevmenu;
} menu_t;

typedef struct value_s {
	float		value;
	char		*name;
} value_t;

value_t YesNo[] = {
	{ 0.0, "No" },
	{ 1.0, "Yes" }
};

value_t NoYes[] = {
	{ 0.0, "Yes" },
	{ 1.0, "No" }
};

value_t OnOff[] = {
	{ 0.0, "Off" },
	{ 1.0, "On" }
};

static menu_t  *CurrentMenu;
static int		CurrentItem;
static boolean	WaitingForKey;
static char	   *OldMessage;
static itemtype OldType;

/*=======================================
 *
 * Options Menu
 *
 *=======================================*/

void AdjustGamma (double oldval, double newval);
void AdjustScreenSize (cvar_t *cvar, float newval);
void AdjustSfxVol (cvar_t *cvar, float newval);
void AdjustMusicVol (cvar_t *cvar, float newval);
void CustomizeControls (void);
void VideoOptions (void);
void GoToConsole (void);
void Reset2Defaults (void);
void Reset2Saved (void);

menuitem_t OptionItems[] = {
	{ more,		"Customize Controls",	NULL,					0.0, 0.0,	0.0, (value_t *)CustomizeControls },
	{ more,		"Go to console",		NULL,					0.0, 0.0,	0.0, (value_t *)GoToConsole },
	{ more,		"Video Options",		NULL,					0.0, 0.0,	0.0, (value_t *)VideoOptions },
	{ redtext,	" ",					NULL,					0.0, 0.0,	0.0, NULL },
	{ slider,	"Mouse speed",			&mouseSensitivity,		0.5, 2.5,	0.1, NULL },
	{ slider,	"MOD Music volume",		&snd_MusicVolume,		0.0, 64.0,	1.0, (value_t *)AdjustMusicVol },
	{ slider,	"Sound volume",			&snd_SfxVolume,			0.0, 15.0,	1.0, (value_t *)AdjustSfxVol },
	{ discrete,	"Always Run",			&cl_run,				2.0, 0.0,	0.0, OnOff },
	{ discrete, "Invert Mouse",			&invertmouse,			2.0, 0.0,	0.0, OnOff },
	{ discrete, "Lookspring",			&lookspring,			2.0, 0.0,	0.0, OnOff },
	{ discrete, "Lookstrafe",			&lookstrafe,			2.0, 0.0,	0.0, OnOff },
	{ redtext,	" ",					NULL,					0.0, 0.0,	0.0, NULL },
	{ more,		"Reset to defaults",	NULL,					0.0, 0.0,	0.0, (value_t *)Reset2Defaults },
	{ more,		"Reset to last saved",	NULL,					0.0, 0.0,	0.0, (value_t *)Reset2Saved }
};

menu_t OptionMenu = {
	"M_OPTTTL",
	0,
	14,
	177,
	OptionItems,
	NULL
};


/*=======================================
 *
 * Controls Menu
 *
 *=======================================*/

menuitem_t ControlsItems[] = {
	{ whitetext,"ENTER to change, SPACE to clear", NULL, 0.0, 0.0, 0.0, NULL },
	{ whitetext," ", NULL, 0.0, 0.0, 0.0, NULL },
	{ control,	"Attack",				NULL, 0.0, 0.0, 0.0, (value_t *)"+attack" },
	{ control,	"Change Weapon",		NULL, 0.0, 0.0, 0.0, (value_t *)"weapnext" },
	{ control,	"Use / Open",			NULL, 0.0, 0.0, 0.0, (value_t *)"+use" },
	{ control,	"Jump",					NULL, 0.0, 0.0, 0.0, (value_t *)"+jump" },
	{ control,	"Walk forward",			NULL, 0.0, 0.0, 0.0, (value_t *)"+forward" },
	{ control,	"Backpedal",			NULL, 0.0, 0.0, 0.0, (value_t *)"+back" },
	{ control,	"Turn left",			NULL, 0.0, 0.0, 0.0, (value_t *)"+left" },
	{ control,	"Turn right",			NULL, 0.0, 0.0, 0.0, (value_t *)"+right" },
	{ control,	"Run",					NULL, 0.0, 0.0, 0.0, (value_t *)"+speed" },
	{ control,	"Step left",			NULL, 0.0, 0.0, 0.0, (value_t *)"+moveleft" },
	{ control,	"Step right",			NULL, 0.0, 0.0, 0.0, (value_t *)"+moveright" },
	{ control,	"Sidestep",				NULL, 0.0, 0.0, 0.0, (value_t *)"+strafe" },
	{ control,	"Look up",				NULL, 0.0, 0.0, 0.0, (value_t *)"+lookup" },
	{ control,	"Look down",			NULL, 0.0, 0.0, 0.0, (value_t *)"+lookdown" },
	{ control,	"Center view",			NULL, 0.0, 0.0, 0.0, (value_t *)"centerview" },
	{ control,	"Mouse look",			NULL, 0.0, 0.0, 0.0, (value_t *)"+mlook" },
	{ control,	"Keyboard look",		NULL, 0.0, 0.0, 0.0, (value_t *)"+klook" },
	{ control,	"Toggle automap",		NULL, 0.0, 0.0, 0.0, (value_t *)"togglemap" }
};

menu_t ControlsMenu = {
	"M_CONTRO",
	2,
	20,
	0,
	ControlsItems,
	&OptionMenu
};

/*=======================================
 *
 * Video Options Menu
 *
 *=======================================*/
extern cvar_t *am_rotate,
			  *am_overlay,
			  *am_usecustomcolors;

value_t Crosshairs[] = {
	{ 0.0, "None" },
	{ 1.0, "Cross 1" },
	{ 2.0, "Cross 2" },
	{ 3.0, "X" },
	{ 4.0, "Diamond" },
	{ 5.0, "Dot" },
	{ 6.0, "Box" },
	{ 7.0, "Angle" },
	{ 8.0, "Big Thing" }
};

menuitem_t VideoItems[] = {
	{ slider,	"Screen size",			&screenblocks,			3.0, 12.0,	1.0, (value_t *)AdjustScreenSize },
	{ floater,	"Brightness",			(cvar_t **)&gammalevel,	1.0, 3.0,	0.1, (value_t *)AdjustGamma },
	{ discrete,	"Crosshair",			(cvar_t **)&crosshair,	9.0, 0.0,	0.0, Crosshairs },
	{ discrete, "Rotate automap",		(cvar_t **)&am_rotate,	2.0, 0.0,	0.0, OnOff },
	{ discrete, "Overlay automap",		(cvar_t **)&am_overlay,	2.0, 0.0,	0.0, OnOff },
	{ discrete, "Use custom automap colors?", (cvar_t **)&am_usecustomcolors, 2.0, 0.0, 0.0, YesNo }
};

menu_t VideoMenu = {
	"M_VIDEO",
	0,
	6,
	0,
	VideoItems,
	&OptionMenu
};

//
//		Toggle messages on/off
//
void M_ChangeMessages (void)
{
	if (showMessages->value) {
		SetCVarFloat (showMessages, 0.0);
		players[consoleplayer].message = MSGOFF;
	} else {
		SetCVarFloat (showMessages, 1.0);
		players[consoleplayer].message = MSGON ;
	}

	message_dontfuckwithme = true;
}

void Cmd_ToggleMessages (void)
{
	M_ChangeMessages;
}

void M_SizeDisplay (float diff)
{
	float newval;

	newval = screenblocks->value + diff;

	if (newval > 12.0)
		newval = 12.0;
	else if (newval < 3.0)
		newval = 3.0;

	SetCVarFloat (screenblocks, newval);
	R_SetViewSize ((int)newval);
}

void Cmd_Sizedown (void *plyr, int argc, char **argv)
{
	M_SizeDisplay (-1.0);
	S_StartSound(ORIGIN_AMBIENT,sfx_stnmov);
}

void Cmd_Sizeup (void *plyr, int argc, char **argv)
{
	M_SizeDisplay(1.0);
	S_StartSound(ORIGIN_AMBIENT,sfx_stnmov);
}

void M_BuildKeyList (menuitem_t *item, int numitems)
{
	int i;

	for (i = 0; i < numitems; i++, item++) {
		if (item->type == control)
			C_GetKeysForCommand (item->command, &item->key1, &item->key2);
	}
}

void M_SwitchMenu (menu_t *menu)
{
	int i, widest = 0, thiswidth;
	menuitem_t *item;

	CurrentMenu = menu;
	CurrentItem = menu->lastOn;

	if (!menu->indent) {
		for (i = 0; i < menu->numitems; i++) {
			item = menu->items + i;
			if (item->type != whitetext && item->type != redtext) {
				thiswidth = M_StringWidth (item->label);
				if (thiswidth > widest)
					widest = thiswidth;
			}
		}
		menu->indent = widest + 6;
	}
}

boolean M_StartOptionsMenu (void)
{
	M_SwitchMenu (&OptionMenu);
	return true;
}

void M_DrawSlider (int x, int y, float min, float max, float cur)
{
	float range;
	int i;

	range = max - min;

	if (cur > max)
		cur = max;
	else if (cur < min)
		cur = min;

	cur -= min;

	V_DrawPatchClean (x, y, 0, W_CacheLumpName ("LSLIDE", PU_CACHE));
	for (i = 1; i < 11; i++)
		V_DrawPatchClean (x + i*8, y, 0, W_CacheLumpName ("MSLIDE", PU_CACHE));
	V_DrawPatchClean (x + 88, y, 0, W_CacheLumpName ("RSLIDE", PU_CACHE));

	V_DrawPatchClean (x + 5 + (int)((cur * 78.0) / range), y, 0, W_CacheLumpName ("CSLIDE", PU_CACHE));
}

int M_FindCurVal (float cur, value_t *values, int numvals)
{
	int v;

	for (v = 0; v < numvals; v++)
		if (values[v].value == cur)
			break;

	return v;
}

void M_OptDrawer (void)
{
	void (*drawer)(int, int, char *);
	int y, width, i, x;
	menuitem_t *item;
	patch_t *title;

	title = W_CacheLumpName (CurrentMenu->title, PU_CACHE);
	
	V_DrawPatchClean (160-title->width/2,10,0,title);

	for (i = 0, y = 20 + title->height; i < CurrentMenu->numitems; i++, y += 8) {
		item = CurrentMenu->items + i;

		width = M_StringWidth (item->label);
		switch (item->type) {
			case more:
				x = CurrentMenu->indent - width;
				drawer = V_DrawWhiteTextClean;
				break;

			case redtext:
				x = 160 - width / 2;
				drawer = V_DrawRedTextClean;
				break;

			case whitetext:
				x = 160 - width / 2;
				drawer = V_DrawWhiteTextClean;
				break;

			default:
				x = CurrentMenu->indent - width;
				drawer = V_DrawRedTextClean;
				break;
		}
		drawer (x, y, item->label);

		switch (item->type) {
			case discrete:
				{
					int v, vals;

					vals = (int)item->min;
					v = M_FindCurVal ((*item->cvar)->value, item->values, vals);

					if (v == vals) {
						V_DrawWhiteTextClean (CurrentMenu->indent + 14, y, "Unknown");
					} else {
						V_DrawWhiteTextClean (CurrentMenu->indent + 14, y, item->values[v].name);
					}
				}
				break;

			case slider:
				M_DrawSlider (CurrentMenu->indent + 14, y, item->min, item->max, (*item->cvar)->value);
				break;

			case floater:
				M_DrawSlider (CurrentMenu->indent + 14, y, item->min, item->max, (float)(*item->val));
				break;

			case control:
				{
					char description[64];

					C_NameKeys (description, item->key1, item->key2);
					V_DrawWhiteTextClean (CurrentMenu->indent + 14, y, description);
				}
				break;
		}

		if (i == CurrentItem && (skullAnimCounter < 6 || WaitingForKey)) {
			V_DrawPatchClean (CurrentMenu->indent + 3, y, 0, W_CacheLumpName ("LITLCURS", PU_CACHE));
		}
	}
}

boolean M_OptResponder (int ch)
{
	menuitem_t *item;
	
	item = CurrentMenu->items + CurrentItem;

	if (WaitingForKey) {
		if (ch != KEY_ESCAPE) {
			C_ChangeBinding (item->command, ch);
			M_BuildKeyList (CurrentMenu->items, CurrentMenu->numitems);
		}
		WaitingForKey = false;
		CurrentMenu->items[0].label = OldMessage;
		CurrentMenu->items[0].type = OldType;
		return true;
	}

	switch (ch)	{
		case KEY_DOWNARROW:
			do {
				if (++CurrentItem == CurrentMenu->numitems)
					CurrentItem = 0;
			} while (CurrentMenu->items[CurrentItem].type == redtext ||
					 CurrentMenu->items[CurrentItem].type == whitetext);
			S_StartSound (ORIGIN_AMBIENT, sfx_pstop);
			break;

		case KEY_UPARROW:
			do {
				if (--CurrentItem < 0)
					CurrentItem = CurrentMenu->numitems - 1;
			} while (CurrentMenu->items[CurrentItem].type == redtext ||
					 CurrentMenu->items[CurrentItem].type == whitetext);
			S_StartSound (ORIGIN_AMBIENT, sfx_pstop);
			break;

		case KEY_LEFTARROW:
			switch (item->type) {
				case floater:
					{
						double newval = *item->val - item->step;

						if (newval < item->min)
							newval = item->min;

						if (item->dfunc)
							item->dfunc (*item->val, newval);
						else
							*item->val = newval;
					}
					S_StartSound(ORIGIN_AMBIENT,sfx_stnmov);
					break;

				case slider:
					{
						float newval = (*item->cvar)->value - item->step;

						if (newval < item->min)
							newval = item->min;

						if (item->cfunc)
							item->cfunc (*item->cvar, newval);
						else
							SetCVarFloat (*item->cvar, newval);
					}
					S_StartSound(ORIGIN_AMBIENT,sfx_stnmov);
					break;

				case discrete:
					{
						int cur;
						int numvals;

						numvals = (int)item->min;
						cur = M_FindCurVal ((*item->cvar)->value, item->values, numvals);
						if (--cur < 0)
							cur = numvals - 1;

						SetCVarFloat (*item->cvar, item->values[cur].value);
					}
					S_StartSound(ORIGIN_AMBIENT,sfx_stnmov);
					break;
			}
			break;

		case KEY_RIGHTARROW:
			switch (item->type) {
				case floater:
					{
						double newval = *item->val + item->step;

						if (newval > item->max)
							newval = item->max;

						if (item->dfunc)
							item->dfunc (*item->val, newval);
						else
							*item->val = newval;
					}
					S_StartSound(ORIGIN_AMBIENT,sfx_stnmov);
					break;

				case slider:
					{
						float newval = (*item->cvar)->value + item->step;

						if (newval > item->max)
							newval = item->max;

						if (item->cfunc)
							item->cfunc (*item->cvar, newval);
						else
							SetCVarFloat (*item->cvar, newval);
					}
					S_StartSound(ORIGIN_AMBIENT,sfx_stnmov);
					break;

				case discrete:
					{
						int cur;
						int numvals;

						numvals = (int)item->min;
						cur = M_FindCurVal ((*item->cvar)->value, item->values, numvals);
						if (++cur >= numvals)
							cur = 0;

						SetCVarFloat (*item->cvar, item->values[cur].value);
					}
					S_StartSound(ORIGIN_AMBIENT,sfx_stnmov);
					break;
			}
			break;

		case KEY_SPACE:
			if (item->type == control) {
				C_UnbindACommand (item->command);
				item->key1 = item->key2 = 0;
			}
			break;

		case KEY_ENTER:
			if (item->type == more && item->mfunc) {
				CurrentMenu->lastOn = CurrentItem;
				S_StartSound(ORIGIN_AMBIENT,sfx_pistol);
				item->mfunc();
			} else if (item->type == discrete) {
				int cur;
				int numvals;

				numvals = (int)item->min;
				cur = M_FindCurVal ((*item->cvar)->value, item->values, numvals);
				if (++cur >= numvals)
					cur = 0;

				SetCVarFloat (*item->cvar, item->values[cur].value);
				S_StartSound(ORIGIN_AMBIENT,sfx_stnmov);
			} else if (item->type == control) {
				WaitingForKey = true;
				OldMessage = CurrentMenu->items[0].label;
				OldType = CurrentMenu->items[0].type;
				CurrentMenu->items[0].label = "Press new key for control or ESC to cancel";
				CurrentMenu->items[0].type = redtext;
			}
			break;

		case KEY_ESCAPE:
			CurrentMenu->lastOn = CurrentItem;
			M_ClearMenus ();
			S_StartSound(ORIGIN_AMBIENT,sfx_swtchx);
			return false;
				
		case KEY_BACKSPACE:
			CurrentMenu->lastOn = CurrentItem;
			if (CurrentMenu->prevmenu) {
				CurrentMenu = CurrentMenu->prevmenu;
				CurrentItem = CurrentMenu->lastOn;
				S_StartSound (ORIGIN_AMBIENT, sfx_swtchn);
				return true;
			} else {
				currentMenu = &MainDef;
				itemOn = currentMenu->lastOn;
				S_StartSound (ORIGIN_AMBIENT, sfx_swtchn);
				return false;
			}
	}
	return true;
}

static void AdjustGamma (double oldval, double newval)
{
	char cmd[32];

	sprintf (cmd, "gamma %g", newval);
	AddCommandString (cmd);
	C_FlushDisplay ();
}

static void AdjustScreenSize (cvar_t *cvar, float newval)
{
	M_SizeDisplay (newval - cvar->value);
}

static void AdjustSfxVol (cvar_t *cvar, float newval)
{
	SetCVarFloat (cvar, newval);
	S_SetSfxVolume ((int)newval);
}

static void AdjustMusicVol (cvar_t *cvar, float newval)
{
	SetCVarFloat (cvar, newval);
	S_SetMusicVolume ((int)newval);
}

static void GoToConsole (void)
{
	M_ClearMenus ();
	C_ToggleConsole ();
}

static void UpdateStuff (void)
{
	M_SizeDisplay (0.0);
	S_SetSfxVolume ((int)snd_SfxVolume->value);
	S_SetMusicVolume ((int)snd_MusicVolume->value);
}

void Reset2Defaults (void)
{
	AddCommandString ("unbindall; binddefaults; gamma 1");
	C_SetCVarsToDefaults ();
	UpdateStuff();
	C_FlushDisplay ();
}

void Reset2Saved (void)
{
	char *execcommand;
	char *configfile = GetConfigPath ();

	execcommand = Malloc (strlen (configfile) + 6);
	sprintf (execcommand, "exec %s", configfile);
	free (configfile);
	AddCommandString (execcommand);
	free (execcommand);
	UpdateStuff();
	C_FlushDisplay ();
}

static void CustomizeControls (void)
{
	M_BuildKeyList (ControlsMenu.items, ControlsMenu.numitems);
	M_SwitchMenu (&ControlsMenu);
}

void Cmd_Menu_Keys (void *plyr, int argc, char **argv)
{
	M_StartControlPanel ();
	S_StartSound(ORIGIN_AMBIENT,sfx_swtchn);
	OptionsActive = true;
	CustomizeControls();
}
static void VideoOptions (void)
{
	M_SwitchMenu (&VideoMenu);
}

void Cmd_Menu_Video (void *plyr, int argc, char **argv)
{
	M_StartControlPanel ();
	S_StartSound(ORIGIN_AMBIENT,sfx_swtchn);
	OptionsActive = true;
	M_SwitchMenu (&VideoMenu);
}
