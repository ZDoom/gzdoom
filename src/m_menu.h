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

#include "d_event.h"
#include "c_cvars.h"

//
// MENUS
//
// Called by main loop,
// saves config file and calls I_Quit when user exits.
// Even when the menu is not displayed,
// this can resize the view and change game parameters.
// Does all the real work of the menu interaction.
BOOL M_Responder (event_t *ev);

// Called by main loop,
// only used for menu (skull cursor) animation.
void M_Ticker (void);

// Called by main loop,
// draws the menus directly into the screen buffer.
void M_Drawer (void);

// Called by D_DoomMain,
// loads the config file.
void M_Init (void);

// Called by intro code to force menu up upon a keypress,
// does nothing if menu is already up.
void M_StartControlPanel (void);

// [RH] Setup options menu
BOOL M_StartOptionsMenu (void);

// [RH] Handle keys for options menu
void M_OptResponder (event_t *ev);

// [RH] Draw options menu
void M_OptDrawer (void);

// [RH] Initialize options menu
void M_OptInit (void);

struct menu_s;
void M_SwitchMenu (struct menu_s *menu);

void M_PopMenuStack (void);

// [RH] Called whenever the display mode changes
void M_RefreshModesList ();

//
// MENU TYPEDEFS
//
typedef enum {
	whitetext,
	redtext,
	more,
	slider,
	discrete,
	cdiscrete,
	control,
	screenres,
	bitflag,
	listelement,
	nochoice
} itemtype;

typedef struct menuitem_s {
	itemtype		  type;
	char			 *label;
	union {
		cvar_t			 *cvar;
		int				  selmode;
		int				  flagmask;
	} a;
	union {
		float			  min;		/* aka numvalues aka invflag */
		int				  key1;
		char			 *res1;
	} b;
	union {
		float			  max;
		int				  key2;
		char			 *res2;
	} c;
	union {
		float			  step;
		char			 *res3;
	} d;
	union {
		struct value_s	 *values;
		char			 *command;
		void			(*cfunc)(cvar_t *cvar, float newval);
		void			(*mfunc)(void);
		void			(*lfunc)(int);
		int				  highlight;
		int				 *flagint;
	} e;
} menuitem_t;

typedef struct menu_s {
	char			title[8];
	int				lastOn;
	int				numitems;
	int				indent;
	menuitem_t	   *items;
} menu_t;

typedef struct value_s {
	float		value;
	char		*name;
} value_t;

typedef struct
{
	// -1 = no cursor here, 1 = ok, 2 = arrows ok
	short		status;
	
	char		name[10];
	
	// choice = menu item #.
	// if status = 2,
	//	 choice=0:leftarrow,1:rightarrow
	void		(*routine)(int choice);
	
	// hotkey in menu
	char		alphaKey;						
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
	BOOL isNewStyle;
	BOOL drawSkull;
} menustack_t;

extern value_t YesNo[2];
extern value_t NoYes[2];
extern value_t OnOff[2];

extern menustack_t MenuStack[16];
extern int MenuStackDepth;

extern menu_t  *CurrentMenu;
extern int		CurrentItem;

extern short	 itemOn;
extern oldmenu_t *currentMenu;

#endif
