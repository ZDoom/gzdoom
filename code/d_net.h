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

#include "d_player.h"


//
// Network play related stuff.
// There is a data struct that stores network
//	communication related stuff, and another
//	one that defines the actual packets to
//	be transmitted.
//

#define DOOMCOM_ID		0x12345678l
#define MAXNETNODES		8	// max computers in a game
#define BACKUPTICS		12	// number of tics to remember

#ifdef DJGPP
// The DOS drivers provide a pretty skimpy buffer.
// Should be enough, anyway.
#define MAX_MSGLEN		(BACKUPTICS*10)
#else
#define MAX_MSGLEN		1400
#endif

#define CMD_SEND	1
#define CMD_GET		2

//
// Network packet data.
//
typedef struct
{
	unsigned	checksum;					// high bit is retransmit request
	byte		retransmitfrom;				// only valid if NCMD_RETRANSMIT
	byte		starttic;
	byte		player, numtics;
	byte		cmds[MAX_MSGLEN-8];
} doomdata_t;

typedef struct
{
	long	id;				// should be DOOMCOM_ID
	short	intnum;			// DOOM executes an int to execute commands

// communication between DOOM and the driver
	short	command;		// CMD_SEND or CMD_GET
	short	remotenode;		// dest for send, set by get (-1 = no packet).
	short	datalength;		// bytes in doomdata to be sent

// info common to all nodes
	short	numnodes;		// console is always node 0.
	short	ticdup;			// 1 = no duplication, 2-5 = dup for slow nets
	short	extratics;		// 1 = send a backup tic in every packet
#ifdef DJGPP
	short	pad[5];			// keep things aligned for DOS drivers
#endif

// info specific to this node
	short	consoleplayer;
	short	numplayers;
#ifdef DJGPP
	short	angleoffset;	// does not work, but needed to preserve
	short	drone;			// alignment for DOS drivers
#endif

// packet data to be sent
	doomdata_t	data;
	
} doomcom_t;



// Create any new ticcmds and broadcast to other players.
void NetUpdate (void);

// Broadcasts special packets to other players
//	to notify of game exit
void STACK_ARGS D_QuitNetGame (void);

//? how many ticks to run?
void TryRunTics (void);

// [RH] Functions for making and using special "ticcmds"
void Net_NewMakeTic (void);
void Net_WriteByte (byte);
void Net_WriteWord (short);
void Net_WriteLong (int);
void Net_WriteString (const char *);

void Net_DoCommand (int type, byte **stream, int player);

#endif

//-----------------------------------------------------------------------------
//
// $Log:$
//
//-----------------------------------------------------------------------------

