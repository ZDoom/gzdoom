#ifndef __C_BINDINGS_H__
#define __C_BINDINGS_H__

#include "doomtype.h"
#include <stdio.h>

void C_DoKey (int key, boolean up);
void C_ArchiveBindings (FILE *f);

#endif //__C_BINDINGS_H__