#ifndef __C_CONSOLE__
#define __C_CONSOLE__

#include "doomtype.h"
#include "doomdef.h"
#include "d_event.h"
#include "cmdlib.h"

#define C_BLINKRATE			(TICRATE/2)

// Initialize the console
void C_InitConsole (int width, int height, boolean ingame);

void C_Ticker (void);

int VPrintf (const char *string);
int Printf_Bold (const char *format, ...);

void C_AddNotifyString (char *s);
void C_DrawConsole (void);
void C_ToggleConsole (void);
void C_HideConsole (void);
void C_FlushDisplay (void);

void C_MidPrint (char *message);
void C_DrawMid (void);

void C_EraseLines (int top, int bottom);

boolean C_Responder (event_t *ev);

#endif
