#ifndef __I_NET_H__
#define __I_NET_H__

#include <stdint.h>

// Called by D_DoomMain.
int I_InitNetwork (void);
void I_NetCmd (void);

enum ENetConstants
{
	MAXNETNODES = 8,	// max computers in a game 
	DOOMCOM_ID = 0x12345678,
	BACKUPTICS = 36,	// number of tics to remember
	MAXTICDUP = 5,
	LOCALCMDTICS =(BACKUPTICS*MAXTICDUP),
	MAX_MSGLEN = 14000,

	CMD_SEND = 1,
	CMD_GET = 2,
};

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

enum ENCMD
{
	NCMD_EXIT				= 0x80,
	NCMD_RETRANSMIT 		= 0x40,
	NCMD_SETUP				= 0x20,
	NCMD_MULTI				= 0x10,		// multiple players in this packet
	NCMD_QUITTERS			= 0x08,		// one or more players just quit (packet server only)
	NCMD_COMPRESSED			= 0x04,		// remainder of packet is compressed

	NCMD_XTICS				= 0x03,		// packet contains >2 tics
	NCMD_2TICS				= 0x02,		// packet contains 2 tics
	NCMD_1TICS				= 0x01,		// packet contains 1 tic
	NCMD_0TICS				= 0x00,		// packet contains 0 tics
};

//
// Network packet data.
//
struct doomcom_t
{
	uint32_t	id;				// should be DOOMCOM_ID
	int16_t	intnum;			// DOOM executes an int to execute commands

// communication between DOOM and the driver
	int16_t	command;		// CMD_SEND or CMD_GET
	int16_t	remotenode;		// dest for send, set by get (-1 = no packet).
	int16_t	datalength;		// bytes in data to be sent

// info common to all nodes
	int16_t	numnodes;		// console is always node 0.
	int16_t	ticdup;			// 1 = no duplication, 2-5 = dup for slow nets

// info specific to this node
	int16_t	consoleplayer;
	int16_t	numplayers;

// packet data to be sent
	uint8_t	data[MAX_MSGLEN];
	
}; 

extern doomcom_t doomcom;
extern bool netgame, multiplayer;
extern int consoleplayer;

#endif
