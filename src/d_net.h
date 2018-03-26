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

#pragma once

#include "doomtype.h"
#include "doomdef.h"
#include "d_protocol.h"
#include "i_net.h"
#include <memory>

//
// Network play related stuff.
// There is a data struct that stores network
//	communication related stuff, and another
//	one that defines the actual packets to
//	be transmitted.
//

#define MAXNETNODES		8	// max computers in a game
#define BACKUPTICS		36	// number of tics to remember
#define MAXTICDUP		5
#define LOCALCMDTICS	(BACKUPTICS*MAXTICDUP)

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

struct TicSpecial
{
	uint8_t *streams[BACKUPTICS];
	size_t used[BACKUPTICS];
	uint8_t *streamptr;
	size_t streamoffs;
	size_t specialsize;
	int	  lastmaketic;
	bool  okay;

public:
	TicSpecial();
	~TicSpecial();

	void GetMoreSpace(size_t needed);
	void CheckSpace(size_t needed);
	void NewMakeTic();

	TicSpecial &operator << (uint8_t it);
	TicSpecial &operator << (short it);
	TicSpecial &operator << (int it);
	TicSpecial &operator << (float it);
	TicSpecial &operator << (const char *it);
};

struct Network
{
public:
	void Update() { }
	void DispatchEvents(int tic);
	void SetPlayerCommand(int player, ticcmd_t cmd);

	void D_CheckNetGame();
	void Net_ClearBuffers();
	void D_QuitNetGame();

	void ListPingTimes();
	void Network_Controller(int playernum, bool add);

	void Net_NewMakeTic(); // Only public because DoomMain calls it. Really really shouldn't be public.
	void Net_WriteByte(uint8_t it);
	void Net_WriteWord(short it);
	void Net_WriteLong(int it);
	void Net_WriteFloat(float it);
	void Net_WriteString(const char *it);
	void Net_WriteBytes(const uint8_t *block, int len);

	size_t CopySpecData(int player, uint8_t *dest, size_t dest_size);

	void SetBotCommand(int player, const ticcmd_t &cmd);
	ticcmd_t GetPlayerCommand(int player) const;

	bool IsInconsistent(int player, int16_t checkvalue) const;
	void SetConsistency(int player, int16_t checkvalue);
	int16_t GetConsoleConsistency() const;

	void RunNetSpecs(int player);

	ticcmd_t GetLocalCommand(int tic) const;
	//ticcmd_t GetLocalCommand(int tic) const { return localcmds[tic % LOCALCMDTICS]; }

	int GetPing(int player) const;
	int GetServerPing() const;
	int GetHighPingThreshold() const;

	int maketic;
	int ticdup; // 1 = no duplication, 2-5 = dup for slow nets

private:
	void ReadTicCmd(uint8_t **stream, int player, int tic);

	std::unique_ptr<doomcom_t> doomcom;
	NetPacket netbuffer;

	int nodeforplayer[MAXPLAYERS];
	int netdelay[MAXNETNODES][BACKUPTICS];		// Used for storing network delay times.

	short consistency[MAXPLAYERS][BACKUPTICS];
	ticcmd_t netcmds[MAXPLAYERS][BACKUPTICS];
	FDynamicBuffer NetSpecs[MAXPLAYERS][BACKUPTICS];
	ticcmd_t localcmds[LOCALCMDTICS];

	int gametime;

	enum { NET_PeerToPeer, NET_PacketServer };
	uint8_t NetMode = NET_PeerToPeer;

	int nettics[MAXNETNODES];
	bool nodeingame[MAXNETNODES];				// set false as nodes leave game
	bool nodejustleft[MAXNETNODES];				// set when a node just left
	bool remoteresend[MAXNETNODES];				// set when local needs tics
	int resendto[MAXNETNODES];					// set when remote needs tics
	int resendcount[MAXNETNODES];

	uint64_t lastrecvtime[MAXPLAYERS];				// [RH] Used for pings
	uint64_t currrecvtime[MAXPLAYERS];
	uint64_t lastglobalrecvtime;						// Identify the last time a packet was received.
	bool hadlate;
	int lastaverage;

	int playerfornode[MAXNETNODES];

	int skiptics;

	int reboundpacket;
	uint8_t reboundstore[MAX_MSGLEN];

	int frameon;
	int frameskip[4];
	int oldnettics;
	int mastertics;

	int entertic;
	int oldentertics;

	TicSpecial specials;
};

extern Network network;

void Net_DoCommand (int type, uint8_t **stream, int player);
void Net_SkipCommand (int type, uint8_t **stream);

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
//  Two bytes with consistency check, followed by tic data
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

enum
{
	PRE_CONNECT,			// Sent from guest to host for initial connection
	PRE_KEEPALIVE,
	PRE_DISCONNECT,			// Sent from guest that aborts the game
	PRE_ALLHERE,			// Sent from host to guest when everybody has connected
	PRE_CONACK,				// Sent from host to guest to acknowledge PRE_CONNECT receipt
	PRE_ALLFULL,			// Sent from host to an unwanted guest
	PRE_ALLHEREACK,			// Sent from guest to host to acknowledge PRE_ALLHEREACK receipt
	PRE_GO					// Sent from host to guest to continue game startup
};
