#ifndef __I_INPUT_H__
#define __I_INPUT_H__

#include "doomtype.h"

extern int KeyRepeatRate;
extern int KeyRepeatDelay;

BOOL I_InitInput(void);
void I_ShutdownInput(void);

void I_GetEvent(void);

#endif
