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
//		Savegame I/O, archiving, persistence.
//
//-----------------------------------------------------------------------------


#ifndef __P_SAVEG_H__
#define __P_SAVEG_H__

#include "farchive.h"

// Persistent storage/archiving.
// These are the load / save game routines.
// Also see farchive.(h|cpp)
void P_SerializePlayers (FArchive &arc);
void P_SerializeWorld (FArchive &arc);
void P_SerializeThinkers (FArchive &arc, bool);
void P_SerializeRNGState (FArchive &arc);
void P_SerializeACSDefereds (FArchive &arc);
void P_SerializePolyobjs (FArchive &arc);
void P_SerializeSounds (FArchive &arc);

void SV_UpdateRebornSlot ();
void SV_ClearRebornSlot ();
bool SV_RebornSlotAvailable ();

#endif // __P_SAVEG_H__
