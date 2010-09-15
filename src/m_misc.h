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
//
//	  
//-----------------------------------------------------------------------------


#ifndef __M_MISC__
#define __M_MISC__

#include "basictypes.h"

class FConfigFile;
class FGameConfigFile;

extern FGameConfigFile *GameConfig;

bool M_WriteFile (char const *name, void *source, int length);
int M_ReadFile (char const *name, BYTE **buffer);
void M_FindResponseFile (void);

// [RH] M_ScreenShot now accepts a filename parameter.
//		Pass a NULL to get the original behavior.
void M_ScreenShot (const char *filename);

void M_LoadDefaults ();

bool M_SaveDefaults (const char *filename);
void M_SaveCustomKeys (FConfigFile *config, char *section, char *subsection, size_t sublen);


// Prepends ~/.zdoom to path
FString GetUserFile (const char *path);

#endif
