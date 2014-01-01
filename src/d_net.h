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
//		Networking stuff.
//
//-----------------------------------------------------------------------------


#ifndef __D_NET__
#define __D_NET__

#include "doomtype.h"
#include "doomdef.h"
#include "d_ticcmd.h"


//
// Network play related stuff.
// There is a data struct that stores network
//	communication related stuff, and another
//	one that defines the actual packets to
//	be transmitted.
//

#define DOOMCOM_ID		0x12345678l
#define MAXNETNODES		8	// max computers in a game
#define BACKUPTICS		36	// number of tics to remember
#define MAXTICDUP		5
#define LOCALCMDTICS	(BACKUPTICS*MAXTICDUP)


#ifdef DJGPP
// The DOS drivers provide a pretty skimpy buffer.
// Probably not enough.
#define MAX_MSGLEN		(BACKUPTICS*10)
#else
#define MAX_MSGLEN		14000
#endif

#define CMD_SEND	1
#define CMD_GET		2

//
// Network packet data.
//
struct doomcom_t
{
	DWORD	id;				// should be DOOMCOM_ID
	SWORD	intnum;			// DOOM executes an int to execute commands

// communication between DOOM and the driver
	SWORD	command;		// CMD_SEND or CMD_GET
	SWORD	remotenode;		// dest for send, set by get (-1 = no packet).
	SWORD	datalength;		// bytes in doomdata to be sent

// info common to all nodes
	SWORD	numnodes;		// console is always node 0.
	SWORD	ticdup;			// 1 = no duplication, 2-5 = dup for slow nets
	SWORD	extratics;		// 1 = send a backup tic in every packet
#ifdef DJGPP
	SWORD	pad[5];			// keep things aligned for DOS drivers
#endif

// info specific to this node
	SWORD	consoleplayer;
	SWORD	numplayers;
#ifdef DJGPP
	SWORD	angleoffset;	// does not work, but needed to preserve
	SWORD	drone;			// alignment for DOS drivers
#endif

// packet data to be sent
	BYTE	data[MAX_MSGLEN];
	
};


class FDynamicBuffer
{
public:
	FDynamicBuffer ();
	~FDynamicBuffer ();

	void SetData (const BYTE *data, int len);
	BYTE *GetData (int *len = NULL);

private:
	BYTE *m_Data;
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
void Net_CheckLastRecieved(int);

// [RH] Functions for making and using special "ticcmds"
void Net_NewMakeTic ();
void Net_WriteByte (BYTE);
void Net_WriteWord (short);
void Net_WriteLong (int);
void Net_WriteFloat (float);
void Net_WriteString (const char *);
void Net_WriteBytes (const BYTE *, int len);

void Net_DoCommand (int type, BYTE **stream, int player);
void Net_SkipCommand (int type, BYTE **stream);

void Net_ClearBuffers ();


// Netgame stuff (buffers and pointers, i.e. indices).

// This is the interface to the packet driver, a separate program
// in DOS, but just an abstraction here.
extern	doomcom_t		doomcom;

extern	struct ticcmd_t	localcmds[LOCALCMDTICS];

extern	int 			maketic;
extern	int 			nettics[MAXNETNODES];

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
