#ifndef __I_INPUT_H__
#define __I_INPUT_H__

#include "doomtype.h"

BOOL I_InitInput (void *hwnd);
void STACK_ARGS I_ShutdownInput ();
void I_PutInClipboard (const char *str);
char *I_GetFromClipboard ();

void I_GetEvent ();

#endif
