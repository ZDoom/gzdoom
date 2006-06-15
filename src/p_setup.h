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
//	 Setup a game, startup stuff.
//
//-----------------------------------------------------------------------------


#ifndef __P_SETUP__
#define __P_SETUP__

#include "w_wad.h"
#include "files.h"
#include "doomdata.h"


struct MapData
{
	wadlump_t MapLumps[ML_MAX];
	bool HasBehavior;
	bool CloseOnDestruct;
	bool Encrypted;
	int lumpnum;
	FileReader * file;
	
	MapData()
	{
		memset(MapLumps, 0, sizeof(MapLumps));
		file = NULL;
		lumpnum = -1;
		HasBehavior = false;
		CloseOnDestruct = true;
		Encrypted = false;
	}
	
	~MapData()
	{
		if (CloseOnDestruct && file != NULL) delete file;
		file = NULL;
	}

	void Seek(unsigned int lumpindex)
	{
		if (lumpindex<countof(MapLumps))
		{
			file->Seek(MapLumps[lumpindex].FilePos, SEEK_SET);
		}
	}

	void Read(unsigned int lumpindex, void * buffer)
	{
		if (lumpindex<countof(MapLumps))
		{
			file->Seek(MapLumps[lumpindex].FilePos, SEEK_SET);
			file->Read(buffer, MapLumps[lumpindex].Size);
		}
	}

	DWORD Size(unsigned int lumpindex)
	{
		if (lumpindex<countof(MapLumps))
		{
			return MapLumps[lumpindex].Size;
		}
		return 0;
	}
};

MapData * P_OpenMapData(const char * mapname);

// NOT called by W_Ticker. Fixme. [RH] Is that bad?
//
// [RH] The only parameter used is mapname, so I removed playermask and skill.
//		On September 1, 1998, I added the position to indicate which set
//		of single-player start spots should be spawned in the level.
void P_SetupLevel (char *mapname, int position);

void P_FreeLevelData();
void P_FreeExtraLevelData();

// Called by startup code.
void P_Init (void);

#endif
