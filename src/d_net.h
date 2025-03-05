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
#include <queue>

uint64_t I_msTime();

enum EChatType
{
	CHAT_DISABLED,
	CHAT_TEAM_ONLY,
	CHAT_GLOBAL,
};

enum EClientFlags
{
	CF_NONE = 0,
	CF_QUIT = 1,		// If in packet server mode, this client sent an exit command and needs to be disconnected.
	CF_MISSING_SEQ = 1 << 1,	// If a sequence was missed/out of order, ask this client to send back over their info.
	CF_RETRANSMIT_SEQ = 1 << 2,	// If set, this client needs command data resent to them.
	CF_MISSING_CON = 1 << 3,	// If a consistency was missed/out of order, ask this client to send back over their info.
	CF_RETRANSMIT_CON = 1 << 4,	// If set, this client needs consistency data resent to them.
	CF_UPDATED = 1 << 5,	// Got an updated packet from this client.

	CF_RETRANSMIT = CF_RETRANSMIT_CON | CF_RETRANSMIT_SEQ,
	CF_MISSING = CF_MISSING_CON | CF_MISSING_SEQ,
};

class FDynamicBuffer
{
public:
	FDynamicBuffer();
	~FDynamicBuffer();

	void SetData(const uint8_t* data, int len);
	uint8_t* GetData(int* len = nullptr);

private:
	uint8_t* m_Data;
	int m_Len, m_BufferLen;
};

// New packet structure:
//
//  One byte for the net command flags.
//  Four bytes for the last sequence we got from that client.
//  Four bytes for the last consistency we got from that client.
//  If NCMD_QUITTERS set, one byte for the number of players followed by one byte for each player's consolenum. Packet server mode only.
//  One byte for the number of players.
//  One byte for the number of tics.
//   If > 0, four bytes for the base sequence being worked from.
//  One byte for the number of world tics ran.
//   If > 0, four bytes for the base consistency being worked from.
//  If in packet server mode and from the host, one byte for how far ahead of the host we are.
//  For each player:
//   One byte for the player number.
//	 If in packet server mode and from the host, two bytes for the latency to the host.
//   For each consistency:
//    One byte for the delta from the base consistency.
//    Two bytes for each consistency.
//   For each tic:
//    One byte for the delta from the base sequence.
//    The remaining command and event data for that player.
struct FClientNetState
{
	// Networked client data.
	struct FNetTic {
		FDynamicBuffer	Data;
		usercmd_t		Command;
	} Tics[BACKUPTICS] = {};

	// Local information about client.
	uint8_t		CurrentLatency = 0u;		// Current latency id the client is on. If the one the client sends back is > this, update RecvTime and mark a new SentTime.
	bool		bNewLatency = true;			// If the sequence was bumped, the next latency packet sent out should record the send time.
	uint16_t	AverageLatency = 0u;		// Calculate the average latency every second or so, that way it doesn't give huge variance in the scoreboard.
	uint64_t	SentTime[MAXSENDTICS] = {};	// Timestamp for when we sent out the packet to this client.
	uint64_t	RecvTime[MAXSENDTICS] = {};	// Timestamp for when the client acknowledged our last packet. If in packet server mode, this is the server's delta.

	int				Flags = 0;				// State of this client.

	uint8_t			ResendID = 0u;			// Make sure that if the retransmit happened on a wait barrier, it can be properly resent back over.
	int				ResendSequenceFrom = -1; // If >= 0, send from this sequence up to the most recent one, capped to MAXSENDTICS.
	int				SequenceAck = -1;		// The last sequence the client reported from us.
	int 			CurrentSequence = -1;	// The last sequence we've gotten from this client.

	// Every packet includes consistencies for tics that client ran. When
	// a world tic is ran, the local client will store all the consistencies
	// of the clients in their LocalConsistency. Then the consistencies will
	// be checked against retroactively as they come in.
	int ResendConsistencyFrom = -1;				// If >= 0, send from this consistency up to the most recent one, capped to MAXSENDTICS.
	int ConsistencyAck = -1;					// Last consistency the client reported from us.
	int LastVerifiedConsistency = -1;			// Last consistency we checked from this client. If < CurrentNetConsistency, run through them.
	int CurrentNetConsistency = -1;				// Last consistency we got from this client.
	int16_t NetConsistency[BACKUPTICS] = {};	// Consistencies we got from this client.
	int16_t LocalConsistency[BACKUPTICS] = {};	// Local consistency of the client to check against.
};

// Create any new ticcmds and broadcast to other players.
void NetUpdate(int tics);

// Broadcasts special packets to other players
//	to notify of game exit
void D_QuitNetGame (void);

//? how many ticks to run?
void TryRunTics (void);

// [RH] Functions for making and using special "ticcmds"
void Net_NewClientTic();
void Net_Initialize();
void Net_WriteInt8(uint8_t);
void Net_WriteInt16(int16_t);
void Net_WriteInt32(int32_t);
void Net_WriteInt64(int64_t);
void Net_WriteFloat(float);
void Net_WriteDouble(double);
void Net_WriteString(const char *);
void Net_WriteBytes(const uint8_t *, int len);

void Net_DoCommand(int cmd, uint8_t **stream, int player);
void Net_SkipCommand(int cmd, uint8_t **stream);

void Net_ResetCommands(bool midTic);
void Net_SetWaiting();
void Net_ClearBuffers();

// Netgame stuff (buffers and pointers, i.e. indices).

extern usercmd_t			LocalCmds[LOCALCMDTICS];
extern int					ClientTic;
extern FClientNetState		ClientStates[MAXPLAYERS];

class player_t;
class DObject;

#endif
