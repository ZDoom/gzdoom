#ifndef __C_CONSOLE__
#define __C_CONSOLE__

#include <stdio.h>
#include <stdarg.h>

#include "doomtype.h"
#include "doomdef.h"
#include "d_event.h"
#include "cmdlib.h"

#define C_BLINKRATE			(TICRATE/2)

// Initialize the console
void C_InitConsole (int width, int height, BOOL ingame);

// Adjust the console for a new screen mode
void C_NewModeAdjust (void);

void C_Ticker (void);

int PrintString (const char *string);
int VPrintf (const char *format, va_list parms);
int Printf_Bold (const char *format, ...);

void C_AddNotifyString (const char *s);
void C_DrawConsole (void);
void C_ToggleConsole (void);
void C_HideConsole (void);
void C_FlushDisplay (void);

void C_MidPrint (char *message);
void C_DrawMid (void);

void C_EraseLines (int top, int bottom);

BOOL C_Responder (event_t *ev);

void C_AddTabCommand (char *name);
void C_RemoveTabCommand (char *name);

#endif
