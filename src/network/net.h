//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
// Copyright 2018 Magnus Norddahl
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

#define MAXNETNODES		8	// max computers in a game
#define BACKUPTICS		36	// number of tics to remember
#define MAXTICDUP		5
#define LOCALCMDTICS	(BACKUPTICS*MAXTICDUP)

class AActor;

class Network
{
public:
	virtual ~Network() { }

	// Check for incoming packets
	virtual void Update() = 0;

	// Set current tic time
	virtual void SetCurrentTic(int localtic) = 0;

	// Send any pending outgoing data
	virtual void EndCurrentTic() = 0;

	// Retrieve data about the current tic
	virtual int GetSendTick() const = 0;
	virtual ticcmd_t GetPlayerInput(int player) const = 0;
	virtual ticcmd_t GetSentInput(int tic) const = 0;

	// Write outgoing data for the current tic
	virtual void WriteLocalInput(ticcmd_t cmd) = 0;
	virtual void WriteBotInput(int player, const ticcmd_t &cmd) = 0;

	// Statistics
	virtual int GetPing(int player) const = 0;
	virtual int GetServerPing() const = 0;
	int GetHighPingThreshold() const;

	// CCMDs
	virtual void ListPingTimes() = 0;
	virtual void Network_Controller(int playernum, bool add) = 0;

	// Playsim events
	virtual void ActorSpawned(AActor *actor) { }
	virtual void ActorDestroyed(AActor *actor) { }

	// Old init/deinit stuff
	void Startup() { }
	void Net_ClearBuffers() { }
	void D_QuitNetGame() { }

	// Demo recording
	size_t CopySpecData(int player, uint8_t *dest, size_t dest_size) { return 0; }

	// Obsolete; only needed for p2p
	bool IsInconsistent(int player, int16_t checkvalue) const { return false; }
	void SetConsistency(int player, int16_t checkvalue) { }
	int16_t GetConsoleConsistency() const { return 0; }

	// Should probably be removed.
	int ticdup = 1;
};

extern std::unique_ptr<Network> network;
extern std::unique_ptr<Network> netconnect;

// Old packet format. Kept for reference. Should be removed or updated once the c/s migration is complete.

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
