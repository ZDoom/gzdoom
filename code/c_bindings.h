#ifndef __C_BINDINGS_H__
#define __C_BINDINGS_H__

#include "doomtype.h"
#include <stdio.h>

boolean C_DoKey (int key, boolean up);
void C_ArchiveBindings (FILE *f);

// Stuff used by the customize controls menu
int  C_GetKeysForCommand (char *cmd, int *first, int *second);
void C_NameKeys (char *str, int first, int second);
void C_UnbindACommand (char *str);
void C_ChangeBinding (char *str, int newone);

#endif //__C_BINDINGS_H__