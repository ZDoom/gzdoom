#ifndef __C_CONSOLE__
#define __C_CONSOLE__

#include <stdio.h>
#include <stdarg.h>

#include "doomtype.h"
#include "doomdef.h"
#include "d_event.h"
#include "cmdlib.h"

#define C_BLINKRATE			(TICRATE/2)

typedef enum cstate_t {
	c_up=0, c_down=1, c_falling=2, c_rising=3
} constate_e;

// Initialize the console
void C_InitConsole (int width, int height, BOOL ingame);

// Adjust the console for a new screen mode
void C_NewModeAdjust (void);

void C_Ticker (void);

int PrintString (int printlevel, const char *string);
int VPrintf (int printlevel, const char *format, va_list parms);
int STACK_ARGS Printf_Bold (const char *format, ...);

void C_DrawConsole (void);
void C_ToggleConsole (void);
void C_FullConsole (void);
void C_HideConsole (void);
void C_AdjustBottom (void);
void C_FlushDisplay (void);

void C_InitTicker (const char *label, unsigned int max);
void C_SetTicker (unsigned int at);

void C_MidPrint (const char *message);

BOOL C_Responder (event_t *ev);

void C_AddTabCommand (const char *name);
void C_RemoveTabCommand (const char *name);

#endif
