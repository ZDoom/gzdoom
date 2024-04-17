//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/
//
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		Networking stuff.
//
//-----------------------------------------------------------------------------


#ifndef __D_NET__
#define __D_NET__

#include "doomtype.h"
#include "doomdef.h"
#include "d_protocol.h"
#include "i_net.h"

class FDynamicBuffer
{
public:
	FDynamicBuffer ();
	~FDynamicBuffer ();

	void SetData (const uint8_t *data, int len);
	uint8_t *GetData (int *len = NULL);

private:
	uint8_t *m_Data;
	int m_Len, m_BufferLen;
};

extern FDynamicBuffer NetSpecs[MAXPLAYERS][BACKUPTICS];

// Create any new ticcmds and broadcast to other players.
void NetUpdate (void);

// Broadcasts special packets to other players
//	to notify of game exit
void D_QuitNetGame (void);

//? how many ticks to run?
void TryRunTics (void);

//Use for checking to see if the netgame has stalled
void Net_CheckLastReceived(int);

// [RH] Functions for making and using special "ticcmds"
void Net_NewMakeTic ();
void Net_WriteInt8 (uint8_t);
void Net_WriteInt16 (int16_t);
void Net_WriteInt32 (int32_t);
void Net_WriteInt64(int64_t);
void Net_WriteFloat (float);
void Net_WriteDouble(double);
void Net_WriteString (const char *);
void Net_WriteBytes (const uint8_t *, int len);

void Net_DoCommand (int type, uint8_t **stream, int player);
void Net_SkipCommand (int type, uint8_t **stream);

void Net_ClearBuffers ();


// Netgame stuff (buffers and pointers, i.e. indices).

// This is the interface to the packet driver, a separate program
// in DOS, but just an abstraction here.
extern	doomcom_t		doomcom;

extern	struct ticcmd_t	localcmds[LOCALCMDTICS];

extern	int 			maketic;
extern	int 			nettics[MAXNETNODES];
extern	int				netdelay[MAXNETNODES][BACKUPTICS];
extern	int 			nodeforplayer[MAXPLAYERS];

extern	ticcmd_t		netcmds[MAXPLAYERS][BACKUPTICS];
extern	int 			ticdup;

// [RH]
// New generic packet structure:
//
// Header:
//  One byte with following flags.
//  One byte with starttic
//  One byte with master's maketic (master -> slave only!)
//  If NCMD_RETRANSMIT set, one byte with retransmitfrom
//  If NCMD_XTICS set, one byte with number of tics (minus 3, so theoretically up to 258 tics in one packet)
//  If NCMD_QUITTERS, one byte with number of players followed by one byte with each player's consolenum
//  If NCMD_MULTI, one byte with number of players followed by one byte with each player's consolenum
//     - The first player's consolenum is not included in this list, because it always matches the sender
//
// For each tic:
//  Two bytes with consistancy check, followed by tic data
//
// Setup packets are different, and are described just before D_ArbitrateNetStart().

#define NCMD_EXIT				0x80
#define NCMD_RETRANSMIT 		0x40
#define NCMD_SETUP				0x20
#define NCMD_MULTI				0x10		// multiple players in this packet
#define NCMD_QUITTERS			0x08		// one or more players just quit (packet server only)
#define NCMD_COMPRESSED			0x04		// remainder of packet is compressed

#define NCMD_XTICS				0x03		// packet contains >2 tics
#define NCMD_2TICS				0x02		// packet contains 2 tics
#define NCMD_1TICS				0x01		// packet contains 1 tic
#define NCMD_0TICS				0x00		// packet contains 0 tics

#endif
