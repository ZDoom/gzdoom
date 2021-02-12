#pragma once

enum EMenuState : int
{
	MENU_Off,			// Menu is closed
	MENU_On,			// Menu is opened
	MENU_WaitKey,		// Menu is opened and waiting for a key in the controls menu
	MENU_OnNoPause,		// Menu is opened but does not pause the game
};
extern	EMenuState		menuactive; 	// Menu overlayed?
