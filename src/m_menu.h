// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This source is available for distribution and/or modification
// only under the terms of the DOOM Source Code License as
// published by id Software. All rights reserved.
//
// The source is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// FITNESS FOR A PARTICULAR PURPOSE. See the DOOM Source Code License
// for more details.
//
// DESCRIPTION:
//	 Menu widget stuff, episode selection and such.
//	  
//-----------------------------------------------------------------------------


#ifndef __M_MENU_H__
#define __M_MENU_H__

#include "c_cvars.h"

struct event_t;
struct menu_t;
//
// MENUS
//
// Called by main loop,
// saves config file and calls I_Quit when user exits.
// Even when the menu is not displayed,
// this can resize the view and change game parameters.
// Does all the real work of the menu interaction.
bool M_Responder (event_t *ev);

// Called by main loop,
// only used for menu (skull cursor) animation.
void M_Ticker (void);

// Called by main loop,
// draws the menus directly into the screen buffer.
void M_Drawer (void);

// Called by D_DoomMain, loads the config file.
void M_Init (void);

void M_Deinit ();

// Called by intro code to force menu up upon a keypress,
// does nothing if menu is already up.
void M_StartControlPanel (bool makeSound);

// Turns off the menu
void M_ClearMenus ();

// [RH] Setup options menu
bool M_StartOptionsMenu (void);

// [RH] Handle keys for options menu
void M_OptResponder (event_t *ev);

// [RH] Draw options menu
void M_OptDrawer (void);

// [RH] Initialize options menu
void M_OptInit (void);

// [RH] Initialize the video modes menu
void M_InitVideoModesMenu (void);

void M_SwitchMenu (struct menu_t *menu);

void M_PopMenuStack (void);

// [RH] Called whenever the display mode changes
void M_RefreshModesList ();

void M_ActivateMenuInput ();
void M_DeactivateMenuInput ();

void M_NotifyNewSave (const char *file, const char *title, bool okForQuicksave);

//
// MENU TYPEDEFS
//
typedef enum {
	whitetext,
	redtext,
	more,
	rightmore,
	safemore,
	rsafemore,
	slider,
	absslider,
	inverter,
	discrete,
	discretes,
	cdiscrete,
	ediscrete,
	discrete_guid,
	control,
	screenres,
	bitflag,
	bitmask,
	listelement,
	nochoice,
	numberedmore,
	colorpicker,
	intslider,
	palettegrid,
} itemtype;

struct GUIDName;

typedef struct menuitem_s {
	itemtype		  type;
	const char		 *label;
	union {
		FBaseCVar		 *cvar;
		FIntCVar		 *intcvar;
		FGUIDCVar		 *guidcvar;
		FColorCVar		 *colorcvar;
		int				  selmode;
		float			  fval;
	} a;
	union {
		float			  min;		/* aka numvalues aka invflag */
		float			  numvalues;
		float			  invflag;
		int				  key1;
		char			 *res1;
		int				  position;
	} b;
	union {
		float			  max;
		int				  key2;
		char			 *res2;
		void			 *extra;
		float			  discretecenter;
	} c;
	union {
		float			  step;
		char			 *res3;
		FBoolCVar		 *graycheck;	// for drawing discrete items
	} d;
	union {
		struct value_t	 *values;
		struct valuestring_t *valuestrings;
		struct valueenum_t	 *enumvalues;
		GUIDName		 *guidvalues;
		char			 *command;
		void			(*cfunc)(FBaseCVar *cvar, float newval);
		void			(*mfunc)(void);
		void			(*lfunc)(int);
		int				  highlight;
		int				  flagmask;
	} e;
} menuitem_t;

struct menu_t {
	const char	   *texttitle;
	int				lastOn;
	int				numitems;
	int				indent;
	menuitem_t	   *items;
	int				scrolltop;
	int				scrollpos;
	int				y;
	void		  (*PreDraw)(void);
	bool			DontDim;
	void		  (*EscapeHandler)(void);
};

struct value_t {
	float		value;
	const char	*name;
};

struct valuestring_t {
	float		value;
	FString		name;
};

struct valueenum_t {
	const char *value;	// Value of cvar
	const char *name;	// Name on menu
};

typedef struct
{
	// -1 = no cursor here, 1 = ok, 2 = arrows ok
	SBYTE		status;
	BYTE		fulltext;	// [RH] Menu name is text, not a graphic
	
	// hotkey in menu
	char		alphaKey;						
	
	const char *name;
	
	// choice = menu item #.
	// if status = 2,
	//	 choice=0:leftarrow,1:rightarrow
	void		(*routine)(int choice);
	int			textcolor;
} oldmenuitem_t;

typedef struct oldmenu_s
{
	short				numitems;		// # of menu items
	oldmenuitem_t		*menuitems;		// menu items
	void				(*routine)(void);	// draw routine
	short				x;
	short				y;				// x,y of menu
	short				lastOn; 		// last item user was on in menu
} oldmenu_t;

typedef struct
{
	union {
		menu_t *newmenu;
		oldmenu_t *old;
	} menu;
	bool isNewStyle;
	bool drawSkull;
} menustack_t;

extern value_t YesNo[2];
extern value_t NoYes[2];
extern value_t OnOff[2];

extern menustack_t MenuStack[16];
extern int MenuStackDepth;

extern bool	OptionsActive;

extern menu_t  *CurrentMenu;
extern int		CurrentItem;

#define MAX_EPISODES	8

extern oldmenuitem_t EpisodeMenu[MAX_EPISODES];
extern bool EpisodeNoSkill[MAX_EPISODES];
extern char EpisodeMaps[MAX_EPISODES][8];
extern oldmenu_t EpiDef;

#endif
