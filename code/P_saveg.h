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


#ifndef __P_SAVEG__
#define __P_SAVEG__



// Persistent storage/archiving.
// These are the load / save game routines.
void P_ArchivePlayers (void);
void P_UnArchivePlayers (void);
void P_ArchiveWorld (void);
void P_UnArchiveWorld (void);
void P_ArchiveThinkers (void);
void P_UnArchiveThinkers (BOOL keepPlayers);	// [RH] added parameter
void P_ArchiveSpecials (void);
void P_UnArchiveSpecials (void);
void P_ArchiveRNGState (void);		// [RH]
void P_UnArchiveRNGState (void);	// [RH]
void P_ArchiveScripts (void);		// [RH]
void P_UnArchiveScripts (void);		// [RH]
void P_ArchiveACSDefereds (void);	// [RH]
void P_UnArchiveACSDefereds (void);	// [RH]

extern byte *save_p, *savebuffer;
extern size_t savegamesize;
void CheckSaveGame (size_t);		// killough


// Pads save_p to a 4-byte boundary
//	so that the load/save works on SGI&Gecko.
#define PADSAVEP()		save_p += (4 - ((int) save_p & 3)) & 3


#endif
//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------
