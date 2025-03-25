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
//		DOOM Network game communication and protocol,
//		all OS independent parts.
//
//-----------------------------------------------------------------------------

#include <stddef.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "version.h"
#include "menu.h"
#include "i_video.h"
#include "i_net.h"
#include "g_game.h"
#include "c_console.h"
#include "d_netinf.h"
#include "d_net.h"
#include "cmdlib.h"
#include "m_cheat.h"
#include "p_local.h"
#include "c_dispatch.h"
#include "sbar.h"
#include "gi.h"
#include "m_misc.h"
#include "gameconfigfile.h"
#include "p_acs.h"
#include "p_trace.h"
#include "a_sharedglobal.h"
#include "st_start.h"
#include "teaminfo.h"
#include "p_conversation.h"
#include "d_eventbase.h"
#include "p_enemy.h"
#include "m_argv.h"
#include "p_lnspec.h"
#include "p_spec.h"
#include "hardware.h"
#include "r_utility.h"
#include "a_keys.h"
#include "intermission/intermission.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "events.h"
#include "i_time.h"
#include "i_system.h"
#include "vm.h"
#include "gstrings.h"
#include "s_music.h"
#include "screenjob.h"
#include "d_main.h"
#include "i_interface.h"
#include "savegamemanager.h"

void P_RunClientsideLogic();

EXTERN_CVAR (Int, disableautosave)
EXTERN_CVAR (Int, autosavecount)
EXTERN_CVAR (Bool, cl_capfps)
EXTERN_CVAR (Bool, vid_vsync)
EXTERN_CVAR (Int, vid_maxfps)

extern uint8_t		*demo_p;		// [RH] Special "ticcmds" get recorded in demos
extern FString	savedescription;
extern FString	savegamefile;

extern bool AppActive;

void P_ClearLevelInterpolation();

enum ELevelStartStatus
{
	LST_READY,
	LST_HOST,
	LST_WAITING,
};

// NETWORKING
//
// gametic is the tic about to (or currently being) run.
// ClientTic is the tick the client is currently on and building a command for.
//
// A world tick cannot be ran until CurrentSequence >= gametic for all clients.

int 				ClientTic = 0;
usercmd_t			LocalCmds[LOCALCMDTICS] = {};
int					LastSentConsistency = 0;		// Last consistency we sent out. If < CurrentConsistency, send them out.
int					CurrentConsistency = 0;			// Last consistency we generated.
FClientNetState		ClientStates[MAXPLAYERS] = {};

// If we're sending a packet to ourselves, store it here instead. This is the simplest way to execute
// playback as it means in the world running code itself all player commands are built the exact same way
// instead of having to rely on pulling from the correct local buffers. It also ensures all commands are
// executed over the net at the exact same tick.
static size_t	LocalNetBufferSize = 0;
static uint8_t	LocalNetBuffer[MAX_MSGLEN] = {};

static uint8_t	CurrentLobbyID = 0u;	// Ignore commands not from this lobby (useful when transitioning levels).
static int		LastGameUpdate = 0;		// Track the last time the game actually ran the world.
static uint64_t	MutedClients = 0u;		// Ignore messages from these clients.

static int  LevelStartDebug = 0;
static int	LevelStartDelay = 0; // While this is > 0, don't start generating packets yet.
static ELevelStartStatus LevelStartStatus = LST_READY; // Listen for when to actually start making tics.
static uint64_t	LevelStartAck = 0u; // Used by the host to determine if everyone has loaded in.

static int FullLatencyCycle = MAXSENDTICS * 3;	// Give ~3 seconds to gather latency info about clients on boot up.
static int LastLatencyUpdate = 0;				// Update average latency every ~1 second.

static int 	EnterTic = 0;
static int	LastEnterTic = 0;
static bool bCommandsReset = false;	// If true, commands were recently cleared. Don't generate any more tics.

static int	CommandsAhead = 0;		// In packet server mode, the host will let us know if we're outpacing them.
static int	SkipCommandTimer = 0;	// Tracker for when to check for skipping commands. ~0.5 seconds in a row of being ahead will start skipping.
static int	SkipCommandAmount = 0;	// Amount of commands to skip. Try and batch skip them all at once since we won't be able to get an update until the full RTT.

void D_ProcessEvents(void); 
void G_BuildTiccmd(usercmd_t *cmd);
void D_DoAdvanceDemo(void);

static void RunScript(uint8_t **stream, AActor *pawn, int snum, int argn, int always);

extern	bool	 advancedemo;

CVAR(Bool, vid_dontdowait, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR(Bool, vid_lowerinbackground, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

CVAR(Bool, net_ticbalance, false, CVAR_SERVERINFO | CVAR_NOSAVE) // Currently deprecated, but may be brought back later.
CVAR(Bool, net_extratic, false, CVAR_SERVERINFO | CVAR_NOSAVE)
CVAR(Bool, net_disablepause, false, CVAR_SERVERINFO | CVAR_NOSAVE)
CVAR(Bool, net_repeatableactioncooldown, true, CVAR_SERVERINFO | CVAR_NOSAVE)

CVAR(Bool, cl_noboldchat, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CVAR(Bool, cl_nochatsound, false, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
CUSTOM_CVAR(Int, cl_showchat, CHAT_GLOBAL, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
{
	if (self < CHAT_DISABLED)
		self = CHAT_DISABLED;
	else if (self > CHAT_GLOBAL)
		self = CHAT_GLOBAL;
}

CUSTOM_CVAR(Int, cl_debugprediction, 0, CVAR_CHEAT)
{
	if (self < 0)
		self = 0;
	else if (self > BACKUPTICS - 1)
		self = BACKUPTICS - 1;
}

// Used to write out all network events that occured leading up to the next tick.
static struct NetEventData
{
	struct FStream {
		uint8_t* Stream;
		size_t Used = 0;

		FStream()
		{
			Grow(256);
		}

		~FStream()
		{
			if (Stream != nullptr)
				M_Free(Stream);
		}

		void Grow(size_t size)
		{
			Stream = (uint8_t*)M_Realloc(Stream, size);
		}
	} Streams[BACKUPTICS];

private:
	size_t CurrentSize = 0;
	size_t MaxSize = 256;
	int CurrentClientTic = 0;

	// Make more room for special Command.
	void GetMoreBytes(size_t newSize)
	{
		MaxSize = max<size_t>(MaxSize * 2, newSize + 30);

		DPrintf(DMSG_NOTIFY, "Expanding special size to %zu\n", MaxSize);

		for (auto& stream : Streams)
			Streams->Grow(MaxSize);

		CurrentStream = Streams[CurrentClientTic % BACKUPTICS].Stream + CurrentSize;
	}

	void AddBytes(size_t bytes)
	{
		if (CurrentSize + bytes >= MaxSize)
			GetMoreBytes(CurrentSize + bytes);

		CurrentSize += bytes;
	}

public:
	uint8_t* CurrentStream = nullptr;

	// Boot up does some faux network events so we need to wait until after
	// everything is initialized to actually set up the network stream.
	void InitializeEventData()
	{
		CurrentStream = Streams[0].Stream;
		CurrentSize = 0;
	}

	void ResetStream()
	{
		CurrentClientTic = ClientTic / TicDup;
		CurrentStream = Streams[CurrentClientTic % BACKUPTICS].Stream;
		CurrentSize = 0;
	}

	void NewClientTic()
	{
		const int tic = ClientTic / TicDup;
		if (CurrentClientTic == tic)
			return;

		Streams[CurrentClientTic % BACKUPTICS].Used = CurrentSize;
		
		CurrentClientTic = tic;
		CurrentStream = Streams[tic % BACKUPTICS].Stream;
		CurrentSize = 0;
	}

	NetEventData& operator<<(uint8_t it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(1);
			WriteInt8(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(int16_t it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(2);
			WriteInt16(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(int32_t it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(4);
			WriteInt32(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(int64_t it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(8);
			WriteInt64(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(float it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(4);
			WriteFloat(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(double it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(8);
			WriteDouble(it, &CurrentStream);
		}
		return *this;
	}

	NetEventData& operator<<(const char *it)
	{
		if (CurrentStream != nullptr)
		{
			AddBytes(strlen(it) + 1);
			WriteString(it, &CurrentStream);
		}
		return *this;
	}
} NetEvents;

void Net_ClearBuffers()
{
	for (int i = 0; i < MAXPLAYERS; ++i)
	{
		playeringame[i] = false;
		players[i].waiting = players[i].inconsistant = false;

		auto& state = ClientStates[i];
		state.AverageLatency = state.CurrentLatency = 0u;
		memset(state.SentTime, 0, sizeof(state.SentTime));
		memset(state.RecvTime, 0, sizeof(state.RecvTime));
		state.bNewLatency = true;

		state.ResendID = 0u;
		state.CurrentNetConsistency = state.LastVerifiedConsistency = state.ConsistencyAck = state.ResendConsistencyFrom = -1;
		state.CurrentSequence = state.SequenceAck = state.ResendSequenceFrom = -1;
		state.Flags = 0;

		for (int j = 0; j < BACKUPTICS; ++j)
			state.Tics[j].Data.SetData(nullptr, 0);
	}

	NetBufferLength = 0u;
	RemoteClient = -1;
	MaxClients = TicDup = 1u;
	consoleplayer = 0;
	LocalNetBufferSize = 0u;
	Net_Arbitrator = 0;

	MutedClients = 0u;
	CurrentLobbyID = 0u;
	NetworkClients.Clear();
	NetMode = NET_PeerToPeer;
	netgame = multiplayer = false;
	LastSentConsistency = CurrentConsistency = 0;
	LastEnterTic = LastGameUpdate = EnterTic;
	gametic = ClientTic = 0;
	SkipCommandTimer = SkipCommandAmount = CommandsAhead = 0;
	NetEvents.ResetStream();

	LevelStartAck = 0u;
	LevelStartDelay = LevelStartDebug = 0;
	LevelStartStatus = LST_READY;

	FullLatencyCycle = MAXSENDTICS * 3;
	LastLatencyUpdate = 0;

	playeringame[0] = true;
	NetworkClients += 0;
}

void Net_ResetCommands(bool midTic)
{
	bCommandsReset = midTic;
	++CurrentLobbyID;
	SkipCommandTimer = SkipCommandAmount = CommandsAhead = 0;

	int tic = gametic / TicDup;
	if (midTic)
	{
		// If we're mid ticdup cycle, make sure we immediately enter the next one after
		// the current tic we're in finishes.
		ClientTic = (tic + 1) * TicDup;
		gametic = (tic * TicDup) + (TicDup - 1);
	}
	else
	{
		ClientTic = gametic = tic * TicDup;
		--tic;
	}
	
	for (auto client : NetworkClients)
	{
		auto& state = ClientStates[client];
		state.Flags &= CF_QUIT;
		state.CurrentSequence = min<int>(state.CurrentSequence, tic);
		state.SequenceAck = min<int>(state.SequenceAck, tic);
		if (state.ResendSequenceFrom >= tic)
			state.ResendSequenceFrom = -1;
		
		// Make sure not to run its current command either.
		auto& curTic = state.Tics[tic % BACKUPTICS];
		memset(&curTic.Command, 0, sizeof(curTic.Command));
	}

	NetEvents.ResetStream();
}

void Net_SetWaiting()
{
	if (netgame && !demoplayback && NetworkClients.Size() > 1)
		LevelStartStatus = LST_WAITING;
}

// [RH] Rewritten to properly calculate the packet size
//		with our variable length Command.
static int GetNetBufferSize()
{
	if (NetBuffer[0] & NCMD_EXIT)
		return 1 + (NetMode == NET_PacketServer && RemoteClient == Net_Arbitrator);
	// TODO: Need a skipper for this.
	if (NetBuffer[0] & NCMD_SETUP)
		return NetBufferLength;
	if (NetBuffer[0] & (NCMD_LATENCY | NCMD_LATENCYACK))
		return 2;

	if (NetBuffer[0] & NCMD_LEVELREADY)
	{
		int bytes = 2;
		if (NetMode == NET_PacketServer && RemoteClient == Net_Arbitrator)
			bytes += 2;

		return bytes;
	}

	// Header info
	int totalBytes = 10;
	if (NetBuffer[0] & NCMD_QUITTERS)
		totalBytes += NetBuffer[totalBytes] + 1;

	const int playerCount = NetBuffer[totalBytes++];
	const int numTics = NetBuffer[totalBytes++];
	if (numTics > 0)
		totalBytes += 4;
	const int ranTics = NetBuffer[totalBytes++];
	if (ranTics > 0)
		totalBytes += 4;
	if (NetMode == NET_PacketServer && RemoteClient == Net_Arbitrator)
		++totalBytes;

	// Minimum additional packet size per player:
	// 1 byte for player number
	// If in packet server mode and from the host, 2 bytes for the latency to the host
	int padding = 1;
	if (NetMode == NET_PacketServer && RemoteClient == Net_Arbitrator)
		padding += 2;
	if (NetBufferLength < totalBytes + playerCount * padding)
		return totalBytes + playerCount * padding;

	uint8_t* skipper = &NetBuffer[totalBytes];
	for (int p = 0; p < playerCount; ++p)
	{
		++skipper;
		if (NetMode == NET_PacketServer && RemoteClient == Net_Arbitrator)
			skipper += 2;

		for (int i = 0; i < ranTics; ++i)
			skipper += 3;

		for (int i = 0; i < numTics; ++i)
		{
			++skipper;
			SkipUserCmdMessage(skipper);
		}
	}

	return int(skipper - NetBuffer);
}

//
// HSendPacket
//
static void HSendPacket(int client, size_t size)
{
	// This data already exists locally in the demo file, so don't write it out.
	if (demoplayback)
		return;

	RemoteClient = client;
	NetBufferLength = size;
	if (client == consoleplayer)
	{
		memcpy(LocalNetBuffer, NetBuffer, size);
		LocalNetBufferSize = size;
		return;
	}

	if (!netgame)
		I_Error("Tried to send a packet to a client while offline");

	I_NetCmd(CMD_SEND);
}

// HGetPacket
// Returns false if no packet is waiting
static bool HGetPacket()
{
	if (demoplayback)
		return false;

	if (LocalNetBufferSize)
	{
		memcpy(NetBuffer, LocalNetBuffer, LocalNetBufferSize);
		NetBufferLength = LocalNetBufferSize;
		RemoteClient = consoleplayer;
		LocalNetBufferSize = 0u;
		return true;
	}

	if (!netgame)
		return false;

	I_NetCmd(CMD_GET);
	if (RemoteClient == -1)
		return false;

	int sizeCheck = GetNetBufferSize();
	if (NetBufferLength != sizeCheck)
	{
		Printf("Incorrect packet size %d (expected %d)\n", NetBufferLength, sizeCheck);
		return false;
	}

	return true;
}

static void ClientConnecting(int client)
{
	if (consoleplayer != Net_Arbitrator)
		return;

	// TODO: Eventually...
}

static void DisconnectClient(int clientNum)
{
	NetworkClients -= clientNum;
	MutedClients &= ~((uint64_t)1u << clientNum);
	I_ClearClient(clientNum);
	// Capture the pawn leaving in the next world tick.
	players[clientNum].playerstate = PST_GONE;
}

static void SetArbitrator(int clientNum)
{
	Net_Arbitrator = clientNum;
	players[Net_Arbitrator].settings_controller = true;
	Printf("%s is the new host\n", players[Net_Arbitrator].userinfo.GetName());
	if (NetMode == NET_PacketServer)
	{
		for (auto client : NetworkClients)
			ClientStates[client].AverageLatency = 0u;

		Net_ResetCommands(false);
		Net_SetWaiting();
	}
}

static void ClientQuit(int clientNum, int newHost)
{
	if (!NetworkClients.InGame(clientNum))
		return;

	// This will get caught in the main loop and send it out to everyone as one big packet. The only
	// exception is the host who will leave instantly and send out any needed data.
	if (NetMode == NET_PacketServer && clientNum != Net_Arbitrator)
	{
		if (consoleplayer != Net_Arbitrator)
			DPrintf(DMSG_WARNING, "Received disconnect packet from client %d erroneously\n", clientNum);
		else
			ClientStates[clientNum].Flags |= CF_QUIT;

		return;
	}

	DisconnectClient(clientNum);
	if (clientNum == Net_Arbitrator)
		SetArbitrator(newHost >= 0 ? newHost : NetworkClients[0]);

	if (demorecording)
		G_CheckDemoStatus();
}

static bool IsMapLoaded()
{
	return gamestate == GS_LEVEL;
}

static void CheckLevelStart(int client, int delayTics)
{
	if (LevelStartStatus != LST_WAITING)
	{
		if (consoleplayer == Net_Arbitrator && client != consoleplayer)
		{
			// Someone might've missed the previous packet, so resend it just in case.
			NetBuffer[0] = NCMD_LEVELREADY;
			NetBuffer[1] = CurrentLobbyID;
			if (NetMode == NET_PacketServer)
			{
				NetBuffer[2] = 0;
				NetBuffer[3] = 0;
			}

			HSendPacket(client, NetMode == NET_PacketServer ? 4 : 2);
		}

		return;
	}

	if (client == Net_Arbitrator)
	{
		LevelStartAck = 0u;
		LevelStartStatus = NetMode == NET_PacketServer && consoleplayer == Net_Arbitrator ? LST_HOST : LST_READY;
		LevelStartDelay = LevelStartDebug = delayTics;
		LastGameUpdate = EnterTic;
		return;
	}

	uint64_t mask = 0u;
	for (auto pNum : NetworkClients)
	{
		if (pNum != Net_Arbitrator)
			mask |= (uint64_t)1u << pNum;
	}

	LevelStartAck |= (uint64_t)1u << client;
	if ((LevelStartAck & mask) == mask && IsMapLoaded())
	{
		// Beyond this point a player is likely lagging out anyway.
		constexpr uint16_t LatencyCap = 350u;

		NetBuffer[0] = NCMD_LEVELREADY;
		NetBuffer[1] = CurrentLobbyID;
		uint16_t highestAvg = 0u;
		if (NetMode == NET_PacketServer)
		{
			// Wait for enough latency info to be accepted so a better average
			// can be calculated for everyone.
			if (FullLatencyCycle > 0)
				return;

			for (auto client : NetworkClients)
			{
				if (client == Net_Arbitrator)
					continue;

				const uint16_t latency = min<uint16_t>(ClientStates[client].AverageLatency, LatencyCap);
				if (latency > highestAvg)
					highestAvg = latency;
			}
		}

		constexpr double MS2Sec = 1.0 / 1000.0;
		for (auto client : NetworkClients)
		{
			if (NetMode == NET_PacketServer)
			{
				int delay = 0;
				if (client != Net_Arbitrator)
					delay = int(floor((highestAvg - min<uint16_t>(ClientStates[client].AverageLatency, LatencyCap)) * MS2Sec * TICRATE));

				NetBuffer[2] = (delay << 8);
				NetBuffer[3] = delay;
			}

			HSendPacket(client, NetMode == NET_PacketServer ? 4 : 2);
		}
	}
}

struct FLatencyAck
{
	int Client;
	uint8_t Seq;

	FLatencyAck(int client, uint8_t seq) : Client(client), Seq(seq) {}
};

//
// GetPackets
//
static void GetPackets()
{
	TArray<FLatencyAck> latencyAcks = {};
	while (HGetPacket())
	{
		const int clientNum =  RemoteClient;
		auto& clientState = ClientStates[clientNum];

		if (NetBuffer[0] & NCMD_EXIT)
		{
			ClientQuit(clientNum, NetMode == NET_PacketServer && clientNum == Net_Arbitrator ? NetBuffer[1] : -1);
			continue;
		}

		if (NetBuffer[0] & NCMD_SETUP)
		{
			HandleIncomingConnection();
			continue;
		}

		if (NetBuffer[0] & NCMD_LATENCY)
		{
			size_t i = 0u;
			for (; i < latencyAcks.Size(); ++i)
			{
				if (latencyAcks[i].Client == clientNum)
					break;
			}

			if (i >= latencyAcks.Size())
				latencyAcks.Push({ clientNum, NetBuffer[1] });

			continue;
		}

		if (NetBuffer[0] & NCMD_LATENCYACK)
		{
			if (NetBuffer[1] == clientState.CurrentLatency)
			{
				clientState.RecvTime[clientState.CurrentLatency++ % MAXSENDTICS] = I_msTime();
				clientState.bNewLatency = true;
			}

			continue;
		}

		if (NetBuffer[0] & NCMD_LEVELREADY)
		{
			if (NetBuffer[1] == CurrentLobbyID)
			{
				int delay = 0;
				if (NetMode == NET_PacketServer && clientNum == Net_Arbitrator)
					delay = (NetBuffer[2] << 8) | NetBuffer[3];

				CheckLevelStart(clientNum, delay);
			}

			continue;
		}

		if (NetBuffer[0] & NCMD_RETRANSMIT)
		{
			clientState.ResendID = NetBuffer[1];
			clientState.Flags |= CF_RETRANSMIT;
		}

		const bool validID = NetBuffer[1] == CurrentLobbyID;
		if (validID)
		{
			clientState.Flags |= CF_UPDATED;
			clientState.SequenceAck = (NetBuffer[2] << 24) | (NetBuffer[3] << 16) | (NetBuffer[4] << 8) | NetBuffer[5];
		}

		const int consistencyAck = (NetBuffer[6] << 24) | (NetBuffer[7] << 16) | (NetBuffer[8] << 8) | NetBuffer[9];

		int curByte = 10;
		if (NetBuffer[0] & NCMD_QUITTERS)
		{
			int numPlayers = NetBuffer[curByte++];
			for (int i = 0; i < numPlayers; ++i)
				DisconnectClient(NetBuffer[curByte++]);
		}

		const int playerCount = NetBuffer[curByte++];

		int baseSequence = -1;
		const int totalTics = NetBuffer[curByte++];
		if (totalTics > 0)
			baseSequence = (NetBuffer[curByte++] << 24) | (NetBuffer[curByte++] << 16) | (NetBuffer[curByte++] << 8) | NetBuffer[curByte++];

		int baseConsistency = -1;
		const int ranTics = NetBuffer[curByte++];
		if (ranTics > 0)
			baseConsistency = (NetBuffer[curByte++] << 24) | (NetBuffer[curByte++] << 16) | (NetBuffer[curByte++] << 8) | NetBuffer[curByte++];

		if (NetMode == NET_PacketServer && clientNum == Net_Arbitrator)
		{
			if (validID)
				CommandsAhead = NetBuffer[curByte++];
			else
				++curByte;
		}
		
		for (int p = 0; p < playerCount; ++p)
		{
			const int pNum = NetBuffer[curByte++];
			auto& pState = ClientStates[pNum];

			// This gets sent over per-player so latencies are correct in packet server mode.
			if (NetMode == NET_PacketServer && clientNum == Net_Arbitrator)
			{
				if (consoleplayer != Net_Arbitrator)
					pState.AverageLatency = (NetBuffer[curByte++] << 8) | NetBuffer[curByte++];
				else
					curByte += 2;
			}

			// Make sure the host doesn't update a player's last consistency ack with their own data.
			if (NetMode != NET_PacketServer || consoleplayer != Net_Arbitrator
				|| pNum == Net_Arbitrator || clientNum != Net_Arbitrator)
			{
				pState.ConsistencyAck = consistencyAck;
			}

			TArray<int16_t> consistencies = {};
			for (int r = 0; r < ranTics; ++r)
			{
				int ofs = NetBuffer[curByte++];
				consistencies.Insert(ofs, (NetBuffer[curByte++] << 8) | NetBuffer[curByte++]);
			}

			for (size_t i = 0u; i < consistencies.Size(); ++i)
			{
				const int cTic = baseConsistency + i;
				if (cTic <= pState.CurrentNetConsistency)
					continue;

				if (cTic > pState.CurrentNetConsistency + 1 || !consistencies[i])
				{
					clientState.Flags |= CF_MISSING_CON;
					break;
				}

				pState.NetConsistency[cTic % BACKUPTICS] = consistencies[i];
				pState.CurrentNetConsistency = cTic;
			}

			// Each tic within a given packet is given a sequence number to ensure that things were put
			// back together correctly. Normally this wouldn't matter as much but since we need to keep
			// clients in lock step a misordered packet will instantly cause a desync.
			TArray<uint8_t*> data = {};
			for (int t = 0; t < totalTics; ++t)
			{
				// Try and reorder the tics if they're all there but end up out of order.
				const int ofs = NetBuffer[curByte++];
				data.Insert(ofs, &NetBuffer[curByte]);
				uint8_t* skipper = &NetBuffer[curByte];
				curByte += SkipUserCmdMessage(skipper);
			}

			// If it's from a previous waiting period, the commands are no longer relevant.
			if (!validID)
				continue;

			for (size_t i = 0u; i < data.Size(); ++i)
			{
				const int seq = baseSequence + i;
				// Duplicate command, ignore it.
				if (seq <= pState.CurrentSequence)
					continue;

				// Skipped a command. Packet likely got corrupted while being put back together, so have
				// the client send over the properly ordered commands. 
				if (seq > pState.CurrentSequence + 1 || data[i] == nullptr)
				{
					clientState.Flags |= CF_MISSING_SEQ;
					break;
				}

				ReadUserCmdMessage(data[i], pNum, seq);
				// The host and clients are a bit desynched here. We don't want to update the host's latest ack with their own
				// info since they get those from the actual clients, but clients have to get them from the host since they
				// don't commincate with each other except in P2P mode.
				if (NetMode != NET_PacketServer || consoleplayer != Net_Arbitrator
					|| pNum == Net_Arbitrator || clientNum != Net_Arbitrator)
				{
					pState.CurrentSequence = seq;
				}
				// Update this so host switching doesn't have any hiccups in packet-server mode.
				if (NetMode == NET_PacketServer && consoleplayer != Net_Arbitrator && pNum != Net_Arbitrator)
					pState.SequenceAck = seq;
			}
		}
	}

	for (const auto& ack : latencyAcks)
	{
		NetBuffer[0] = NCMD_LATENCYACK;
		NetBuffer[1] = ack.Seq;
		HSendPacket(ack.Client, 2);
	}
}

static void SendHeartbeat()
{
	// TODO: This could probably also be used to determine if there's packets
	// missing and a retransmission is needed.
	const uint64_t time = I_msTime();
	for (auto client : NetworkClients)
	{
		if (client == consoleplayer)
			continue;

		auto& state = ClientStates[client];
		if (LastLatencyUpdate >= MAXSENDTICS)
		{
			int delta = 0;
			const uint8_t startTic = state.CurrentLatency - MAXSENDTICS;
			for (int i = 0; i < MAXSENDTICS; ++i)
			{
				const int tic = (startTic + i) % MAXSENDTICS;
				const uint64_t high = state.RecvTime[tic] < state.SentTime[tic] ? time : state.RecvTime[tic];
				delta += high - state.SentTime[tic];
			}

			state.AverageLatency = delta / MAXSENDTICS;
		}

		if (state.bNewLatency)
		{
			// Use the most up-to-date time here for better accuracy.
			state.SentTime[state.CurrentLatency % MAXSENDTICS] = I_msTime();
			state.bNewLatency = false;
		}

		NetBuffer[0] = NCMD_LATENCY;
		NetBuffer[1] = state.CurrentLatency;
		HSendPacket(client, 2);
	}
}

static void CheckConsistencies()
{
	// Check consistencies retroactively to see if there was a desync at some point. We still
	// check the local client here because in packet server mode these could realistically desync
	// if the client's current position doesn't agree with the host.
	for (auto client : NetworkClients)
	{
		auto& clientState = ClientStates[client];
		// If previously inconsistent, always mark it as such going forward. We don't want this to
		// accidentally go away at some point since the game state is already completely broken.
		if (players[client].inconsistant)
		{
			clientState.LastVerifiedConsistency = clientState.CurrentNetConsistency;
		}
		else
		{
			// Make sure we don't check past tics we haven't even ran yet.
			const int limit = min<int>(CurrentConsistency - 1, clientState.CurrentNetConsistency);
			while (clientState.LastVerifiedConsistency < limit)
			{
				++clientState.LastVerifiedConsistency;
				const int tic = clientState.LastVerifiedConsistency % BACKUPTICS;
				if (clientState.LocalConsistency[tic] != clientState.NetConsistency[tic])
				{
					players[client].inconsistant = true;
					clientState.LastVerifiedConsistency = clientState.CurrentNetConsistency;
					break;
				}
			}
		}
	}
}

//==========================================================================
//
// FRandom :: StaticSumSeeds
//
// This function produces a uint32_t that can be used to check the consistancy
// of network games between different machines. Only a select few RNGs are
// used for the sum, because not all RNGs are important to network sync.
//
//==========================================================================

extern FRandom pr_spawnmobj;
extern FRandom pr_acs;
extern FRandom pr_chase;
extern FRandom pr_damagemobj;

static uint32_t StaticSumSeeds()
{
	return
		pr_spawnmobj.Seed() +
		pr_acs.Seed() +
		pr_chase.Seed() +
		pr_damagemobj.Seed();
}

static int16_t CalculateConsistency(int client, uint32_t seed)
{
	if (players[client].mo != nullptr)
	{
		seed += int((players[client].mo->X() + players[client].mo->Y() + players[client].mo->Z()) * 257) + players[client].mo->Angles.Yaw.BAMs() + players[client].mo->Angles.Pitch.BAMs();
		seed ^= players[client].health;
	}

	// Zero value consistencies are seen as invalid, so always have a valid value.
	return (seed & 0xFFFF) ? seed : 1;
}

// Ran a tick, so prep the next consistencies to send out.
// [RH] Include some random seeds and player stuff in the consistancy
// check, not just the player's x position like BOOM.
static void MakeConsistencies()
{
	if (!netgame || demoplayback || (gametic % TicDup) || !IsMapLoaded())
		return;

	const uint32_t rngSum = StaticSumSeeds();
	for (auto client : NetworkClients)
	{
		auto& clientState = ClientStates[client];
		clientState.LocalConsistency[CurrentConsistency % BACKUPTICS] = CalculateConsistency(client, rngSum);
	}

	++CurrentConsistency;
}

static bool Net_UpdateStatus()
{
	if (!netgame || demoplayback || NetworkClients.Size() <= 1)
		return true;

	if (LevelStartStatus == LST_WAITING || LevelStartDelay > 0)
		return false;

	// Check against the previous tick in case we're recovering from a huge
	// system hiccup. If the game has taken too long to update, it's likely
	// another client is hanging up the game.
	if (LastEnterTic - LastGameUpdate >= MAXSENDTICS * TicDup)
	{
		// Try again in the next MaxDelay tics.
		LastGameUpdate = EnterTic;

		if (NetMode != NET_PacketServer || consoleplayer == Net_Arbitrator)
		{
			// Use a missing packet here to tell the other players to retransmit instead of simply retransmitting our
			// own data over instantly. This avoids flooding the network at a time where it's not opportune to do so.
			const int curTic = gametic / TicDup;
			for (auto client : NetworkClients)
			{
				if (client == consoleplayer)
					continue;

				if (ClientStates[client].CurrentSequence < curTic)
				{
					ClientStates[client].Flags |= CF_MISSING;
					players[client].waiting = true;
				}
				else
				{
					players[client].waiting = false;
				}
			}
		}
		else
		{
			// In packet server mode, the client is waiting for data from the host and hasn't recieved it yet. Send
			// our data back over in case the host is waiting for us.
			ClientStates[Net_Arbitrator].Flags |= CF_MISSING;
			players[Net_Arbitrator].waiting = true;
		}
	}

	if (LevelStartStatus == LST_HOST)
		return false;

	for (auto client : NetworkClients)
	{
		if (players[client].waiting)
			return false;
	}

	// Wait for the game to stabilize a bit after launch before skipping commands.
	bool updated = false;
	int lowestDiff = INT_MAX;
	if (gametic > TICRATE * 2)
	{
		if (NetMode != NET_PacketServer)
		{
			// Check if everyone has a buffer for us. If they do, we're too far ahead.
			bool allUpdated = true;
			int highestLatency = 0;
			for (auto client : NetworkClients)
			{
				if (client != consoleplayer)
				{
					if (ClientStates[client].Flags & CF_UPDATED)
					{
						updated = true;
						int diff = ClientStates[client].SequenceAck - ClientStates[client].CurrentSequence;
						if (diff < lowestDiff)
							lowestDiff = diff;
						if (ClientStates[client].AverageLatency > highestLatency)
							highestLatency = ClientStates[client].AverageLatency;
					}
					else
					{
						allUpdated = false;
					}
				}

				ClientStates[client].Flags &= ~CF_UPDATED;
			}

			if (allUpdated)
			{
				// If we're consistently ahead of the highest latency player we're connected to, slow down
				// as well since we should generally be in that ballpark.
				const int diff = (ClientTic - gametic) / TicDup;
				const int goal = static_cast<int>(ceil((double)highestLatency / TICRATE)) / TicDup + 1;
				if (diff > goal)
					lowestDiff = diff - goal;
			}
		}
		else if (consoleplayer == Net_Arbitrator)
		{
			// If we're consistenty ahead of the highest sequence player, slow down.
			bool allUpdated = true;
			const int curTic = ClientTic / TicDup;
			for (auto client : NetworkClients)
			{
				if (client != Net_Arbitrator)
				{
					if (ClientStates[client].Flags & CF_UPDATED)
					{
						updated = true;
						int diff = curTic - ClientStates[client].CurrentSequence;
						if (diff < lowestDiff)
							lowestDiff = diff;
					}
					else
					{
						allUpdated = false;
					}
				}

				ClientStates[client].Flags &= ~CF_UPDATED;
			}

			if (allUpdated)
			{
				// If we're consistently ahead of the world, force a stop here as well. Likely some client
				// has fallen super far behind and needs to be reset.
				const int diff = curTic - gametic / TicDup;
				if (diff > 1)
					lowestDiff = diff;
			}
		}
		else if (ClientStates[Net_Arbitrator].Flags & CF_UPDATED)
		{
			// Check if the host is reporting that we're too far ahead of them.
			updated = true;
			lowestDiff = CommandsAhead;
			ClientStates[Net_Arbitrator].Flags &= ~CF_UPDATED;
		}
	}

	if (updated)
	{
		if (lowestDiff > 0)
		{
			if (SkipCommandTimer++ > TICRATE / 2)
			{
				SkipCommandTimer = 0;
				if (SkipCommandAmount <= 0)
					SkipCommandAmount = lowestDiff * TicDup;
			}
		}
		else
		{
			SkipCommandTimer = 0;
		}
	}

	return true;
}

void NetUpdate(int tics)
{
	GetPackets();
	if (tics <= 0)
		return;

	if (netgame && !demoplayback)
	{
		// If a tic has passed, always send out a heartbeat packet (also doubles as
		// a latency measurement tool).
		if (NetMode != NET_PacketServer || consoleplayer == Net_Arbitrator)
		{
			LastLatencyUpdate += tics;
			if (FullLatencyCycle > 0)
				FullLatencyCycle = max<int>(FullLatencyCycle - tics, 0);

			SendHeartbeat();

			if (LastLatencyUpdate >= MAXSENDTICS)
				LastLatencyUpdate = 0;
		}

		CheckConsistencies();
	}

	// Sit idle after the level has loaded until everyone is ready to go. This keeps players better
	// in sync with each other than relying on tic balancing to speed up/slow down the game and mirrors
	// how players would wait for a true server to load.
	if (LevelStartStatus != LST_READY)
	{
		if (LevelStartStatus == LST_WAITING)
		{
			if (NetworkClients.Size() == 1)
			{
				// If we got stuck in limbo waiting, force start the map.
				CheckLevelStart(Net_Arbitrator, 0);
			}
			else
			{
				if (consoleplayer != Net_Arbitrator && IsMapLoaded())
				{
					NetBuffer[0] = NCMD_LEVELREADY;
					NetBuffer[1] = CurrentLobbyID;
					HSendPacket(Net_Arbitrator, 2);
				}
			}
		}
		else if (LevelStartStatus == LST_HOST)
		{
			// If we're the host, idly wait until all packets have arrived. There's no point in predicting since we
			// know for a fact the game won't be started until everyone is accounted for. (Packet server only)
			const int curTic = gametic / TicDup;
			int lowestSeq = curTic;
			for (auto client : NetworkClients)
			{
				if (client != Net_Arbitrator && ClientStates[client].CurrentSequence < lowestSeq)
					lowestSeq = ClientStates[client].CurrentSequence;
			}

			if (lowestSeq >= curTic)
				LevelStartStatus = LST_READY;
		}
	}
	else if (LevelStartDelay > 0)
	{
		if (LevelStartDelay < tics)
			tics -= LevelStartDelay;

		LevelStartDelay = max<int>(LevelStartDelay - tics, 0);
	}
		
	const bool netGood = Net_UpdateStatus();
	const int startTic = ClientTic;
	tics = min<int>(tics, MAXSENDTICS * TicDup);
	for (int i = 0; i < tics; ++i)
	{
		I_StartTic();
		D_ProcessEvents();
		if (pauseext || !netGood)
			break;

		if (SkipCommandAmount > 0)
		{
			--SkipCommandAmount;
			continue;
		}
		
		G_BuildTiccmd(&LocalCmds[ClientTic++ % LOCALCMDTICS]);
		if (TicDup == 1)
		{
			Net_NewClientTic();
		}
		else
		{
			const int ticDiff = ClientTic % TicDup;
			if (ticDiff)
			{
				const int startTic = ClientTic - ticDiff;

				// Even if we're not sending out inputs, update the local commands so that the TicDup
				// is correctly played back while predicting as best as possible. This will help prevent
				// minor hitches when playing online.
				for (int j = ClientTic - 1; j > startTic; --j)
					LocalCmds[(j - 1) % LOCALCMDTICS].buttons |= LocalCmds[j % LOCALCMDTICS].buttons;
			}
			else
			{
				// Gather up the Command across the last TicDup number of tics
				// and average them out. These are propagated back to the local
				// command so that they'll be predicted correctly.
				const int lastTic = ClientTic - TicDup;
				for (int j = ClientTic - 1; j > lastTic; --j)
					LocalCmds[(j - 1) % LOCALCMDTICS].buttons |= LocalCmds[j % LOCALCMDTICS].buttons;

				int pitch = 0;
				int yaw = 0;
				int roll = 0;
				int forwardmove = 0;
				int sidemove = 0;
				int upmove = 0;

				for (int j = 0; j < TicDup; ++j)
				{
					const int mod = (lastTic + j) % LOCALCMDTICS;
					pitch += LocalCmds[mod].pitch;
					yaw += LocalCmds[mod].yaw;
					roll += LocalCmds[mod].roll;
					forwardmove += LocalCmds[mod].forwardmove;
					sidemove += LocalCmds[mod].sidemove;
					upmove += LocalCmds[mod].upmove;
				}

				pitch /= TicDup;
				yaw /= TicDup;
				roll /= TicDup;
				forwardmove /= TicDup;
				sidemove /= TicDup;
				upmove /= TicDup;

				for (int j = 0; j < TicDup; ++j)
				{
					const int mod = (lastTic + j) % LOCALCMDTICS;
					LocalCmds[mod].pitch = pitch;
					LocalCmds[mod].yaw = yaw;
					LocalCmds[mod].roll = roll;
					LocalCmds[mod].forwardmove = forwardmove;
					LocalCmds[mod].sidemove = sidemove;
					LocalCmds[mod].upmove = upmove;
				}

				Net_NewClientTic();
			}
		}
	}

	const int newestTic = ClientTic / TicDup;
	if (demoplayback)
	{
		// Don't touch net command data while playing a demo, as it'll already exist.
		for (auto client : NetworkClients)
			ClientStates[client].CurrentSequence = newestTic;

		return;
	}

	constexpr size_t MaxPlayersPerPacket = 16u;

	int startSequence = startTic / TicDup;
	int endSequence = newestTic;
	int quitters = 0;
	int quitNums[MAXPLAYERS];
	size_t players = 1u;
	int maxCommands = MAXSENDTICS;
	if (NetMode == NET_PacketServer && consoleplayer == Net_Arbitrator)
	{
		// In packet server mode special handling is used to ensure the host only
		// sends out available tics when ready instead of constantly shotgunning
		// them out as they're made locally.
		startSequence = gametic / TicDup;
		int lowestSeq = endSequence - 1;
		for (auto client : NetworkClients)
		{
			if (client == Net_Arbitrator)
				continue;

			// The host has special handling when disconnecting in a packet server game.
			if (ClientStates[client].Flags & CF_QUIT)
			{
				quitNums[quitters++] = client;
			}
			else
			{
				++players;
				if (ClientStates[client].CurrentSequence < lowestSeq)
					lowestSeq = ClientStates[client].CurrentSequence;
			}
		}

		endSequence = lowestSeq + 1;

		// To avoid fragmenting, split up commands into groups of 16p with only 2 commands per packet.
		// If the average packet size with 16p is ~500b, this gives up to ~1000b per packet of data
		// with some leeway for network events and UDP header data. Most routers have an MTU of 1500b.
		// If player count is < 16, scale the number of commands by 1 per every 4 less players.
		// If player count is < 8, scale the number of commands by 1 per every 1 less player.
		// If player count is < 4, scale the number of commands by 4 per every 1 less player.
		constexpr size_t MaxTicsPerPacket = 2u;
		if (players > 1u)
		{
			maxCommands = MaxTicsPerPacket;
			if (players >= MaxPlayersPerPacket / 2 && players < MaxPlayersPerPacket)
				maxCommands = MaxTicsPerPacket + (MaxPlayersPerPacket - players) / 4;
			else if (players >= MaxPlayersPerPacket / 4 && players < MaxPlayersPerPacket / 2)
				maxCommands = MaxPlayersPerPacket / 4 + MaxPlayersPerPacket / 2 - players;
			else if (players < MaxPlayersPerPacket / 4)
				maxCommands = MaxPlayersPerPacket / 2 + (MaxPlayersPerPacket / 4 - players) * 4;
		}
	}

	const bool resendOnly = startSequence == endSequence && (ClientTic % TicDup);
	const int playerLoops = static_cast<int>(ceil((double)players / MaxPlayersPerPacket));
	for (auto client : NetworkClients)
	{
		// If in packet server mode, we don't want to send information to anyone but the host. On the other
		// hand, if we're the host we send out everyone's info to everyone else.
		if (NetMode == NET_PacketServer && consoleplayer != Net_Arbitrator && client != Net_Arbitrator)
			continue;

		auto& curState = ClientStates[client];
		// If we can only resend, don't send clients any information that they already have. If
		// we couldn't generate any commands because we're at the cap, instead send out a heartbeat.
		if ((curState.Flags & CF_QUIT) || (resendOnly && !(curState.Flags & (CF_RETRANSMIT | CF_MISSING))))
			continue;

		const bool isSelf = client == consoleplayer;
		NetBuffer[0] = (curState.Flags & CF_MISSING) ? NCMD_RETRANSMIT : 0;
		curState.Flags &= ~CF_MISSING;

		NetBuffer[1] = (curState.Flags & CF_RETRANSMIT_SEQ) ? curState.ResendID : CurrentLobbyID;
		int lastSeq = curState.CurrentSequence;
		int lastCon = curState.CurrentNetConsistency;
		if (NetMode == NET_PacketServer && consoleplayer != Net_Arbitrator)
		{
			// If in packet-server mode, make sure to get the lowest sequence of all players
			// since the host themselves might have gotten updated but someone else in the packet
			// did not. That way the host knows to send over the correct tic.
			for (auto cl : NetworkClients)
			{
				if (ClientStates[cl].CurrentSequence < lastSeq)
					lastSeq = ClientStates[cl].CurrentSequence;
				if (ClientStates[cl].CurrentNetConsistency < lastCon)
					lastCon = ClientStates[cl].CurrentNetConsistency;
			}
		}
		// Last sequence we got from this client.
		NetBuffer[2] = (lastSeq >> 24);
		NetBuffer[3] = (lastSeq >> 16);
		NetBuffer[4] = (lastSeq >> 8);
		NetBuffer[5] = lastSeq;
		// Last consistency we got from this client.
		NetBuffer[6] = (lastCon >> 24);
		NetBuffer[7] = (lastCon >> 16);
		NetBuffer[8] = (lastCon >> 8);
		NetBuffer[9] = lastCon;

		if (curState.Flags & CF_RETRANSMIT_SEQ)
		{
			curState.Flags &= ~CF_RETRANSMIT_SEQ;
			if (curState.ResendSequenceFrom < 0)
				curState.ResendSequenceFrom = curState.SequenceAck + 1;
		}

		const int sequenceNum = curState.ResendSequenceFrom >= 0 ? curState.ResendSequenceFrom : startSequence;
		const int numTics = clamp<int>(endSequence - sequenceNum, 0, MAXSENDTICS);

		if (curState.Flags & CF_RETRANSMIT_CON)
		{
			curState.Flags &= ~CF_RETRANSMIT_CON;
			if (curState.ResendConsistencyFrom < 0)
				curState.ResendConsistencyFrom = curState.ConsistencyAck + 1;
		}

		const int baseConsistency = curState.ResendConsistencyFrom >= 0 ? curState.ResendConsistencyFrom : LastSentConsistency;
		// Don't bother sending over consistencies in packet server unless you're the host.
		int ran = 0;
		if (NetMode != NET_PacketServer || consoleplayer == Net_Arbitrator)
			ran = clamp<int>(CurrentConsistency - baseConsistency, 0, MAXSENDTICS);

		int ticLoops = static_cast<int>(ceil(max<double>(numTics, ran) / maxCommands));
		if (isSelf || !ticLoops)
			ticLoops = 1;

		const int maxPlayerLoops = isSelf ? 1 : playerLoops;
		int totalQuits = quitters;
		for (int tLoops = 0, curTicOfs = 0; tLoops < ticLoops; ++tLoops, curTicOfs += maxCommands)
		{
			for (int pLoops = 0, curPlayerOfs = 0; pLoops < maxPlayerLoops; ++pLoops, curPlayerOfs += MaxPlayersPerPacket)
			{
				size_t size = 10;
				if (totalQuits > 0)
				{
					NetBuffer[0] |= NCMD_QUITTERS;
					NetBuffer[size++] = totalQuits;
					for (int i = 0; i < totalQuits; ++i)
						NetBuffer[size++] = quitNums[i];

					totalQuits = 0;
				}
				else
				{
					NetBuffer[0] &= ~NCMD_QUITTERS;
				}

				int playerNums[MAXPLAYERS];
				int playerCount = isSelf ? players : min<int>(players - curPlayerOfs, MaxPlayersPerPacket);
				NetBuffer[size++] = playerCount;
				if (players > 1)
				{
					int i = 0;
					for (auto cl : NetworkClients)
					{
						if (ClientStates[cl].Flags & CF_QUIT)
							continue;

						if (i >= curPlayerOfs)
							playerNums[i - curPlayerOfs] = cl;

						++i;
						if (!isSelf && i >= curPlayerOfs + MaxPlayersPerPacket)
							break;
					}
				}
				else
				{
					playerNums[0] = consoleplayer;
				}

				int sendTics = isSelf ? numTics : clamp<int>(numTics - curTicOfs, 0, maxCommands);
				if (curState.ResendSequenceFrom >= 0)
				{
					curState.ResendSequenceFrom += sendTics;
					if (curState.ResendSequenceFrom >= endSequence)
						curState.ResendSequenceFrom = -1;
				}

				NetBuffer[size++] = sendTics;
				if (sendTics > 0)
				{
					NetBuffer[size++] = (sequenceNum + curTicOfs) >> 24;
					NetBuffer[size++] = (sequenceNum + curTicOfs) >> 16;
					NetBuffer[size++] = (sequenceNum + curTicOfs) >> 8;
					NetBuffer[size++] = sequenceNum + curTicOfs;
				}

				int sendCon = isSelf ? ran : clamp<int>(ran - curTicOfs, 0, maxCommands);
				if (curState.ResendConsistencyFrom >= 0)
				{
					curState.ResendConsistencyFrom += sendCon;
					if (curState.ResendConsistencyFrom >= CurrentConsistency)
						curState.ResendConsistencyFrom = -1;
				}

				NetBuffer[size++] = sendCon;
				if (sendCon > 0)
				{
					NetBuffer[size++] = (baseConsistency + curTicOfs) >> 24;
					NetBuffer[size++] = (baseConsistency + curTicOfs) >> 16;
					NetBuffer[size++] = (baseConsistency + curTicOfs) >> 8;
					NetBuffer[size++] = baseConsistency + curTicOfs;
				}

				if (NetMode == NET_PacketServer && consoleplayer == Net_Arbitrator)
					NetBuffer[size++] = client == Net_Arbitrator ? 0 : max<int>(curState.CurrentSequence - newestTic, 0);

				// Client commands.

				uint8_t* cmd = &NetBuffer[size];
				for (int i = 0; i < playerCount; ++i)
				{
					cmd[0] = playerNums[i];
					++cmd;

					auto& clientState = ClientStates[playerNums[i]];
					// Time used to track latency since in packet server mode we want each
					// client's latency to the server itself.
					if (NetMode == NET_PacketServer && consoleplayer == Net_Arbitrator)
					{
						cmd[0] = (clientState.AverageLatency >> 8);
						++cmd;
						cmd[0] = clientState.AverageLatency;
						++cmd;
					}

					for (int r = 0; r < sendCon; ++r)
					{
						cmd[0] = r;
						++cmd;
						const int tic = (baseConsistency + curTicOfs + r) % BACKUPTICS;
						cmd[0] = (clientState.LocalConsistency[tic] >> 8);
						++cmd;
						cmd[0] = clientState.LocalConsistency[tic];
						++cmd;
					}

					for (int t = 0; t < sendTics; ++t)
					{
						cmd[0] = t;
						++cmd;

						int curTic = sequenceNum + curTicOfs + t, lastTic = curTic - 1;
						if (playerNums[i] == consoleplayer)
						{
							int realTic = (curTic * TicDup) % LOCALCMDTICS;
							int realLastTic = (lastTic * TicDup) % LOCALCMDTICS;
							// Write out the net events before the user commands so inputs can
							// be used as a marker for when the given command ends.
							auto& stream = NetEvents.Streams[curTic % BACKUPTICS];
							if (stream.Used)
							{
								memcpy(cmd, stream.Stream, stream.Used);
								cmd += stream.Used;
							}

							WriteUserCmdMessage(LocalCmds[realTic],
								realLastTic >= 0 ? &LocalCmds[realLastTic] : nullptr, cmd);
						}
						else
						{
							auto& netTic = clientState.Tics[curTic % BACKUPTICS];

							int len;
							uint8_t* data = netTic.Data.GetData(&len);
							if (data != nullptr)
							{
								memcpy(cmd, data, len);
								cmd += len;
							}

							WriteUserCmdMessage(netTic.Command,
								lastTic >= 0 ? &clientState.Tics[lastTic % BACKUPTICS].Command : nullptr, cmd);
						}
					}
				}

				HSendPacket(client, int(cmd - NetBuffer));
				if (net_extratic && !isSelf)
					HSendPacket(client, int(cmd - NetBuffer));
			}
		}
	}

	// Update this now that all the packets have been sent out.
	if (!resendOnly)
		LastSentConsistency = CurrentConsistency;

	// Listen for other packets. This has to also come after sending so the player that sent
	// data to themselves gets it immediately (important for singleplayer, otherwise there
	// would always be a one-tic delay).
	GetPackets();
}

// These have to be here since they have game-specific data. Only the data
// from the frontend should be put in these, all backend handling should be
// done in the core files.

void Net_SetupUserInfo()
{
	D_SetupUserInfo();
}

const char* Net_GetClientName(int client, unsigned int charLimit = 0u)
{
	return players[client].userinfo.GetName(charLimit);
}

int Net_SetUserInfo(int client, uint8_t*& stream)
{
	auto str = D_GetUserInfoStrings(client, true);
	const size_t userSize = str.Len() + 1;
	memcpy(stream, str.GetChars(), userSize);
	return userSize;
}

int Net_ReadUserInfo(int client, uint8_t*& stream)
{
	const uint8_t* start = stream;
	D_ReadUserInfoStrings(client, &stream, false);
	return int(stream - start);
}

int Net_SetGameInfo(uint8_t*& stream)
{
	const uint8_t* start = stream;
	WriteString(startmap.GetChars(), &stream);
	WriteInt32(rngseed, &stream);
	C_WriteCVars(&stream, CVAR_SERVERINFO, true);
	return int(stream - start);
}

int Net_ReadGameInfo(uint8_t*& stream)
{
	const uint8_t* start = stream;
	startmap = ReadStringConst(&stream);
	rngseed = ReadInt32(&stream);
	C_ReadCVars(&stream);
	return int(stream - start);
}

// Connects players to each other if needed.
bool D_CheckNetGame()
{
	if (!I_InitNetwork())
		return false;

	if (GameID != DEFAULT_GAME_ID)
		I_FatalError("Invalid id set for network buffer");

	if (Args->CheckParm("-extratic"))
		net_extratic = true;

	players[Net_Arbitrator].settings_controller = true;
	for (auto client : NetworkClients)
		playeringame[client] = true;

	if (MaxClients > 1u)
	{
		if (consoleplayer == Net_Arbitrator)
			Printf("Selected " TEXTCOLOR_BLUE "%s" TEXTCOLOR_NORMAL " networking mode\n", NetMode == NET_PeerToPeer ? "peer to peer" : "packet server");
		else
			Printf("Host selected " TEXTCOLOR_BLUE "%s" TEXTCOLOR_NORMAL " networking mode\n", NetMode == NET_PeerToPeer ? "peer to peer" : "packet server");

		Printf("Player %d of %d\n", consoleplayer + 1, MaxClients);
	}
	
	return true;
}

//
// D_QuitNetGame
// Called before quitting to leave a net game
// without hanging the other players
//
void D_QuitNetGame()
{
	if (!netgame || !usergame || consoleplayer == -1 || demoplayback || NetworkClients.Size() == 1)
		return;

	// Send a bunch of packets for stability.
	NetBuffer[0] = NCMD_EXIT;
	if (NetMode == NET_PacketServer && consoleplayer == Net_Arbitrator)
	{
		// This currently isn't much different from the regular P2P code, but it's being split off into its
		// own branch should proper host migration be added in the future (i.e. sending over stored event
		// data rather than just dropping it entirely).
		int nextHost = 0;
		for (auto client : NetworkClients)
		{
			if (client != Net_Arbitrator)
			{
				nextHost = client;
				break;
			}
		}

		NetBuffer[1] = nextHost;
		for (int i = 0; i < 4; ++i)
		{
			for (auto client : NetworkClients)
			{
				if (client != Net_Arbitrator)
					HSendPacket(client, 2);
			}

			I_WaitVBL(1);
		}
	}
	else
	{
		for (int i = 0; i < 4; ++i)
		{
			// If in packet server mode, only the host should know about this
			// information.
			if (NetMode == NET_PacketServer)
			{
				HSendPacket(Net_Arbitrator, 1);
			}
			else
			{
				for (auto client : NetworkClients)
				{
					if (client != consoleplayer)
						HSendPacket(client, 1);
				}
			}

			I_WaitVBL(1);
		}
	}
}

ADD_STAT(network)
{
	FString out = {};
	if (!netgame || demoplayback)
	{
		out.AppendFormat("No network stats available.");
		return out;
	}

	out.AppendFormat("Max players: %d\tNet mode: %s\tTic dup: %d",
		MaxClients,
		NetMode == NET_PacketServer ? "Packet server" : "Peer to peer",
		TicDup);

	if (net_extratic)
		out.AppendFormat("\tExtra tic enabled");

	out.AppendFormat("\nWorld tic: %06d (sequence %06d)", gametic, gametic / TicDup);
	if (NetMode == NET_PacketServer && consoleplayer != Net_Arbitrator)
		out.AppendFormat("\tStart tics delay: %d", LevelStartDebug);

	const int delay = max<int>((ClientTic - gametic) / TicDup, 0);
	const int msDelay = min<int>(delay * TicDup * 1000.0 / TICRATE, 999);
	out.AppendFormat("\nLocal\n\tIs arbitrator: %d\tDelay: %02d (%03dms)",
		consoleplayer == Net_Arbitrator,
		delay, msDelay);

	if (NetMode == NET_PacketServer && consoleplayer != Net_Arbitrator)
		out.AppendFormat("\tAvg latency: %03ums", min<unsigned int>(ClientStates[consoleplayer].AverageLatency, 999u));

	if (LevelStartStatus != LST_READY)
	{
		if (LevelStartStatus == LST_HOST)
			out.AppendFormat("\tWaiting for packets");
		else if (consoleplayer == Net_Arbitrator)
			out.AppendFormat("\tWaiting for acks");
		else
			out.AppendFormat("\tWaiting for arbitrator");
	}

	int lowestSeq = ClientTic / TicDup;
	for (auto client : NetworkClients)
	{
		if (client == consoleplayer)
			continue;

		auto& state = ClientStates[client];
		if (state.CurrentSequence < lowestSeq)
			lowestSeq = state.CurrentSequence;

		out.AppendFormat("\n%s", players[client].userinfo.GetName(12));
		if (client == Net_Arbitrator)
			out.AppendFormat("\t(Host)");

		if ((state.Flags & CF_RETRANSMIT) == CF_RETRANSMIT)
			out.AppendFormat("\t(RT)");
		else if (state.Flags & CF_RETRANSMIT_SEQ)
			out.AppendFormat("\t(RT SEQ)");
		else if (state.Flags & CF_RETRANSMIT_CON)
			out.AppendFormat("\t(RT CON)");

		if ((state.Flags & CF_MISSING) == CF_MISSING)
			out.AppendFormat("\t(MISS)");
		else if (state.Flags & CF_MISSING_SEQ)
			out.AppendFormat("\t(MISS SEQ)");
		else if (state.Flags & CF_MISSING_CON)
			out.AppendFormat("\t(MISS CON)");

		out.AppendFormat("\n");

		if (NetMode != NET_PacketServer)
		{
			const int cDelay = max<int>(state.CurrentSequence - (gametic / TicDup), 0);
			const int mscDelay = min<int>(cDelay * TicDup * 1000.0 / TICRATE, 999);
			out.AppendFormat("\tDelay: %02d (%03dms)", cDelay, mscDelay);
		}
		
		out.AppendFormat("\tAck: %06d\tConsistency: %06d", state.SequenceAck, state.ConsistencyAck);
		if (NetMode != NET_PacketServer || client != Net_Arbitrator)
			out.AppendFormat("\tAvg latency: %03ums", min<unsigned int>(state.AverageLatency, 999u));
	}

	if (NetMode != NET_PacketServer || consoleplayer == Net_Arbitrator)
		out.AppendFormat("\nAvailable tics: %03d", max<int>(lowestSeq - (gametic / TicDup), 0));
	return out;
}

// Forces playsim processing time to be consistent across frames.
// This improves interpolation for frames in between tics.
//
// With this cvar off the mods with a high playsim processing time will appear
// less smooth as the measured time used for interpolation will vary.

CVAR(Bool, r_ticstability, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

static uint64_t stabilityticduration = 0;
static uint64_t stabilitystarttime = 0;

static void TicStabilityWait()
{
	using namespace std::chrono;
	using namespace std::this_thread;

	if (!r_ticstability)
		return;

	uint64_t start = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
	while (true)
	{
		uint64_t cur = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
		if (cur - start > stabilityticduration)
			break;
	}
}

static void TicStabilityBegin()
{
	using namespace std::chrono;
	stabilitystarttime = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

static void TicStabilityEnd()
{
	using namespace std::chrono;
	uint64_t stabilityendtime = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
	stabilityticduration = min(stabilityendtime - stabilitystarttime, (uint64_t)1'000'000);
}

// Don't stabilize tics that are going to have incredibly long pauses in them.
static bool ShouldStabilizeTick()
{
	return gameaction != ga_recordgame && gameaction != ga_newgame && gameaction != ga_newgame2
			&& gameaction != ga_loadgame && gameaction != ga_loadgamehidecon && gameaction != ga_autoloadgame && gameaction != ga_loadgameplaydemo
			&& gameaction != ga_savegame && gameaction != ga_autosave
			&& gameaction != ga_worlddone && gameaction != ga_completed && gameaction != ga_screenshot && gameaction != ga_fullconsole;
}

//
// TryRunTics
//
void TryRunTics()
{
	GC::CheckGC();

	if (ToggleFullscreen)
	{
		ToggleFullscreen = false;
		AddCommandString("toggle vid_fullscreen");
	}
	
	bool doWait = (cl_capfps || pauseext || (!netgame && r_NoInterpolate && !M_IsAnimated()));
	if (vid_dontdowait && (vid_maxfps > 0 || vid_vsync))
		doWait = false;
	if (!netgame && !AppActive && vid_lowerinbackground)
		doWait = true;

	// Get the full number of tics the client can run.
	if (doWait)
		EnterTic = I_WaitForTic(LastEnterTic);
	else
		EnterTic = I_GetTime();

	const int startCommand = ClientTic;
	int totalTics = EnterTic - LastEnterTic;
	if (totalTics > 1 && singletics)
		totalTics = 1;

	// Listen for other clients and send out data as needed. This is also
	// needed for singleplayer! But is instead handled entirely through local
	// buffers. This has a limit of 17 tics that can be generated.
	NetUpdate(totalTics);

	LastEnterTic = EnterTic;

	// If the game is paused, everything we need to update has already done so.
	if (pauseext)
		return;

	// Get the amount of tics the client can actually run. This accounts for waiting for other
	// players over the network.
	int lowestSequence = INT_MAX;
	for (auto client : NetworkClients)
	{
		if (ClientStates[client].CurrentSequence < lowestSequence)
			lowestSequence = ClientStates[client].CurrentSequence;
	}

	// If the lowest confirmed tic matches the server gametic or greater, allow the client
	// to run some of them.
	const int availableTics = (lowestSequence - gametic / TicDup) + 1;

	// If the amount of tics to run is falling behind the amount of available tics,
	// speed the playsim up a bit to help catch up.
	int runTics = min<int>(totalTics, availableTics);
	if (totalTics > 0 && totalTics < availableTics - 1 && !singletics)
		++runTics;

	// Test player prediction code in singleplayer
	// by running the gametic behind the ClientTic
	if (!netgame && !demoplayback && cl_debugprediction > 0)
	{
		int debugTarget = ClientTic - cl_debugprediction;
		int debugOffset = gametic - debugTarget;
		if (debugOffset > 0)
		{
			runTics = max<int>(runTics - debugOffset, 0);
		}
	}

	// If there are no tics to run, check for possible stall conditions and new
	// commands to predict.
	if (runTics <= 0)
	{
		// If we're in between a tic, try and balance things out.
		if (totalTics <= 0)
			TicStabilityWait();
		else
			P_ClearLevelInterpolation();

		// If we actually advanced a command, update the player's position (even if a
		// tic passes this isn't guaranteed to happen since it's capped to 35 in advance).
		if (ClientTic > startCommand)
		{
			P_UnPredictPlayer();
			P_PredictPlayer(&players[consoleplayer]);
			S_UpdateSounds(players[consoleplayer].camera);	// Update sounds only after predicting the client's newest position.
		}

		// If we actually did have some tics available, make sure the UI
		// still has a chance to run.
		for (int i = 0; i < totalTics; ++i)
			P_RunClientsideLogic();

		return;
	}

	for (auto client : NetworkClients)
		players[client].waiting = false;

	// Update the last time the game tic'd.
	LastGameUpdate = EnterTic;

	// Run the available tics.
	P_UnPredictPlayer();
	while (runTics--)
	{
		const bool stabilize = ShouldStabilizeTick();
		if (stabilize)
			TicStabilityBegin();

		if (advancedemo)
			D_DoAdvanceDemo();

		G_Ticker();
		MakeConsistencies();
		++gametic;

		if (stabilize)
			TicStabilityEnd();

		if (bCommandsReset)
		{
			bCommandsReset = false;
			break;
		}
	}
	P_PredictPlayer(&players[consoleplayer]);
	S_UpdateSounds(players[consoleplayer].camera);	// Update sounds only after predicting the client's newest position.

	// These should use the actual tics since they're not actually tied to the gameplay logic.
	// Make sure it always comes after so the HUD has the correct game state when updating.
	for (int i = 0; i < totalTics; ++i)
		P_RunClientsideLogic();
}

void Net_NewClientTic()
{
	NetEvents.NewClientTic();
}

void Net_Initialize()
{
	NetEvents.InitializeEventData();
}

void Net_WriteInt8(uint8_t it)
{
	NetEvents << it;
}

void Net_WriteInt16(int16_t it)
{
	NetEvents << it;
}

void Net_WriteInt32(int32_t it)
{
	NetEvents << it;
}

void Net_WriteInt64(int64_t it)
{
	NetEvents << it;
}

void Net_WriteFloat(float it)
{
	NetEvents << it;
}

void Net_WriteDouble(double it)
{
	NetEvents << it;
}

void Net_WriteString(const char *it)
{
	NetEvents << it;
}

void Net_WriteBytes(const uint8_t *block, int len)
{
	while (len--)
		NetEvents << *block++;
}

//==========================================================================
//
// Dynamic buffer interface
//
//==========================================================================

FDynamicBuffer::FDynamicBuffer()
{
	m_Data = nullptr;
	m_Len = m_BufferLen = 0;
}

FDynamicBuffer::~FDynamicBuffer()
{
	if (m_Data != nullptr)
	{
		M_Free(m_Data);
		m_Data = nullptr;
	}
	m_Len = m_BufferLen = 0;
}

void FDynamicBuffer::SetData(const uint8_t *data, int len)
{
	if (len > m_BufferLen)
	{
		m_BufferLen = (len + 255) & ~255;
		m_Data = (uint8_t *)M_Realloc(m_Data, m_BufferLen);
	}

	if (data != nullptr)
	{
		m_Len = len;
		memcpy(m_Data, data, len);
	}
	else 
	{
		m_Len = 0;
	}
}

uint8_t *FDynamicBuffer::GetData(int *len)
{
	if (len != nullptr)
		*len = m_Len;
	return m_Len ? m_Data : nullptr;
}

static int RemoveClass(FLevelLocals *Level, const PClass *cls)
{
	AActor *actor;
	int removecount = 0;
	bool player = false;
	auto iterator = Level->GetThinkerIterator<AActor>(cls->TypeName);
	while ((actor = iterator.Next()))
	{
		if (actor->IsA(cls))
		{
			// [MC]Do not remove LIVE players.
			if (actor->player != nullptr)
			{
				player = true;
				continue;
			}
			// [SP] Don't remove owned inventory objects.
			if (!actor->IsMapActor())
				continue;

			removecount++; 
			actor->ClearCounters();
			actor->Destroy();
		}
	}

	if (player)
		Printf("Cannot remove live players!\n");

	return removecount;

}
// [RH] Execute a special "ticcmd". The type byte should
//		have already been read, and the stream is positioned
//		at the beginning of the command's actual data.
void Net_DoCommand(int cmd, uint8_t **stream, int player)
{
	uint8_t pos = 0;
	const char* s = nullptr;
	int i = 0;

	switch (cmd)
	{
	case DEM_SAY:
		{
			const char *name = players[player].userinfo.GetName();
			uint8_t who = ReadInt8(stream);

			s = ReadStringConst(stream);
			// If chat is disabled, there's nothing else to do here since the stream has been advanced.
			if (cl_showchat == CHAT_DISABLED || (MutedClients & ((uint64_t)1u << player)))
				break;

			constexpr int MSG_TEAM = 1;
			constexpr int MSG_BOLD = 2;
			if (!(who & MSG_TEAM))
			{
				if (cl_showchat < CHAT_GLOBAL)
					break;

				// Said to everyone
				if (deathmatch && teamplay)
					Printf(PRINT_CHAT, "(All) ");
				if ((who & MSG_BOLD) && !cl_noboldchat)
					Printf(PRINT_CHAT, TEXTCOLOR_BOLD "* %s" TEXTCOLOR_BOLD "%s" TEXTCOLOR_BOLD "\n", name, s);
				else
					Printf(PRINT_CHAT, "%s" TEXTCOLOR_CHAT ": %s" TEXTCOLOR_CHAT "\n", name, s);

				if (!cl_nochatsound)
					S_Sound(CHAN_VOICE, CHANF_UI, gameinfo.chatSound, 1.0f, ATTN_NONE);
			}
			else if (!deathmatch || players[player].userinfo.GetTeam() == players[consoleplayer].userinfo.GetTeam())
			{
				if (cl_showchat < CHAT_TEAM_ONLY)
					break;

				// Said only to members of the player's team
				if (deathmatch && teamplay)
					Printf(PRINT_TEAMCHAT, "(Team) ");
				if ((who & MSG_BOLD) && !cl_noboldchat)
					Printf(PRINT_TEAMCHAT, TEXTCOLOR_BOLD "* %s" TEXTCOLOR_BOLD "%s" TEXTCOLOR_BOLD "\n", name, s);
				else
					Printf(PRINT_TEAMCHAT, "%s" TEXTCOLOR_TEAMCHAT ": %s" TEXTCOLOR_TEAMCHAT "\n", name, s);

				if (!cl_nochatsound)
					S_Sound(CHAN_VOICE, CHANF_UI, gameinfo.chatSound, 1.0f, ATTN_NONE);
			}
		}
		break;

	case DEM_MUSICCHANGE:
		S_ChangeMusic(ReadStringConst(stream));
		break;

	case DEM_PRINT:
		Printf("%s", ReadStringConst(stream));
		break;

	case DEM_CENTERPRINT:
		C_MidPrint(nullptr, ReadStringConst(stream));
		break;

	case DEM_UINFCHANGED:
		D_ReadUserInfoStrings(player, stream, true);
		break;

	case DEM_SINFCHANGED:
		D_DoServerInfoChange(stream, false);
		break;

	case DEM_SINFCHANGEDXOR:
		D_DoServerInfoChange(stream, true);
		break;

	case DEM_GIVECHEAT:
		s = ReadStringConst(stream);
		cht_Give(&players[player], s, ReadInt32(stream));
		if (player != consoleplayer)
		{
			FString message = GStrings.GetString("TXT_X_CHEATS");
			message.Substitute("%s", players[player].userinfo.GetName());
			Printf("%s: give %s\n", message.GetChars(), s);
		}
		break;

	case DEM_TAKECHEAT:
		s = ReadStringConst(stream);
		cht_Take(&players[player], s, ReadInt32(stream));
		break;

	case DEM_SETINV:
		s = ReadStringConst(stream);
		i = ReadInt32(stream);
		cht_SetInv(&players[player], s, i, !!ReadInt8(stream));
		break;

	case DEM_WARPCHEAT:
		{
			int x = ReadInt16(stream);
			int y = ReadInt16(stream);
			int z = ReadInt16(stream);
			P_TeleportMove(players[player].mo, DVector3(x, y, z), true);
		}
		break;

	case DEM_GENERICCHEAT:
		cht_DoCheat(&players[player], ReadInt8(stream));
		break;

	case DEM_CHANGEMAP2:
		pos = ReadInt8(stream);
		[[fallthrough]];
	case DEM_CHANGEMAP:
		// Change to another map without disconnecting other players
		s = ReadStringConst(stream);
		// Using LEVEL_NOINTERMISSION tends to throw the game out of sync.
		// That was a long time ago. Maybe it works now?
		primaryLevel->flags |= LEVEL_CHANGEMAPCHEAT;
		primaryLevel->ChangeLevel(s, pos, 0);
		break;

	case DEM_SUICIDE:
		cht_Suicide(&players[player]);
		break;

	case DEM_ADDBOT:
		primaryLevel->BotInfo.TryAddBot(primaryLevel, stream, player);
		break;

	case DEM_KILLBOTS:
		primaryLevel->BotInfo.RemoveAllBots(primaryLevel, true);
		Printf ("Removed all bots\n");
		break;

	case DEM_CENTERVIEW:
		players[player].centering = true;
		break;

	case DEM_INVUSEALL:
		if (gamestate == GS_LEVEL && !paused
			&& players[player].playerstate != PST_DEAD)
		{
			AActor *item = players[player].mo->Inventory;
			auto pitype = PClass::FindActor(NAME_PuzzleItem);
			while (item != nullptr)
			{
				AActor *next = item->Inventory;
				IFVIRTUALPTRNAME(item, NAME_Inventory, UseAll)
				{
					VMValue param[] = { item, players[player].mo };
					VMCall(func, param, 2, nullptr, 0);
				}
				item = next;
			}
		}
		break;

	case DEM_INVUSE:
	case DEM_INVDROP:
		{
			uint32_t which = ReadInt32(stream);
			int amt = -1;
			if (cmd == DEM_INVDROP)
				amt = ReadInt32(stream);

			if (gamestate == GS_LEVEL && !paused
				&& players[player].playerstate != PST_DEAD)
			{
				auto item = players[player].mo->Inventory;
				while (item != nullptr && item->InventoryID != which)
					item = item->Inventory;

				if (item != nullptr)
				{
					if (cmd == DEM_INVUSE)
						players[player].mo->UseInventory(item);
					else
						players[player].mo->DropInventory(item, amt);
				}
			}
		}
		break;

	case DEM_SUMMON:
	case DEM_SUMMONFRIEND:
	case DEM_SUMMONFOE:
	case DEM_SUMMONMBF:
	case DEM_SUMMON2:
	case DEM_SUMMONFRIEND2:
	case DEM_SUMMONFOE2:
		{
			int angle = 0;
			int16_t tid = 0;
			uint8_t special = 0;
			int args[5];

			s = ReadStringConst(stream);
			if (cmd >= DEM_SUMMON2 && cmd <= DEM_SUMMONFOE2)
			{
				angle = ReadInt16(stream);
				tid = ReadInt16(stream);
				special = ReadInt8(stream);
				for (i = 0; i < 5; i++) args[i] = ReadInt32(stream);
			}

			AActor* source = players[player].mo;
			if (source != NULL)
			{
				PClassActor* typeinfo = PClass::FindActor(s);
				if (typeinfo != NULL)
				{
					if (GetDefaultByType(typeinfo)->flags & MF_MISSILE)
					{
						P_SpawnPlayerMissile(source, 0, 0, 0, typeinfo, source->Angles.Yaw);
					}
					else
					{
						const AActor* def = GetDefaultByType(typeinfo);
						DVector3 spawnpos = source->Vec3Angle(def->radius * 2 + source->radius, source->Angles.Yaw, 8.);

						AActor* spawned = Spawn(primaryLevel, typeinfo, spawnpos, ALLOW_REPLACE);
						if (spawned != NULL)
						{
							spawned->SpawnFlags |= MTF_CONSOLETHING;
							if (cmd == DEM_SUMMONFRIEND || cmd == DEM_SUMMONFRIEND2 || cmd == DEM_SUMMONMBF)
							{
								if (spawned->CountsAsKill())
								{
									primaryLevel->total_monsters--;
								}
								spawned->FriendPlayer = player + 1;
								spawned->flags |= MF_FRIENDLY;
								spawned->LastHeard = players[player].mo;
								spawned->health = spawned->SpawnHealth();
								if (cmd == DEM_SUMMONMBF)
									spawned->flags3 |= MF3_NOBLOCKMONST;
							}
							else if (cmd == DEM_SUMMONFOE || cmd == DEM_SUMMONFOE2)
							{
								spawned->FriendPlayer = 0;
								spawned->flags &= ~MF_FRIENDLY;
								spawned->health = spawned->SpawnHealth();
							}

							if (cmd >= DEM_SUMMON2 && cmd <= DEM_SUMMONFOE2)
							{
								spawned->Angles.Yaw = source->Angles.Yaw - DAngle::fromDeg(angle);
								spawned->special = special;
								for (i = 0; i < 5; i++) {
									spawned->args[i] = args[i];
								}
								if (tid) spawned->SetTID(tid);
							}
						}
					}
				}
				else
				{ // not an actor, must be a visualthinker
					PClass* typeinfo = PClass::FindClass(s);
					if (typeinfo && typeinfo->IsDescendantOf("VisualThinker"))
					{
						DVector3 spawnpos = source->Vec3Angle(source->radius * 4, source->Angles.Yaw, 8.);
						auto vt = DVisualThinker::NewVisualThinker(source->Level, typeinfo);
						if (vt)
						{
							vt->PT.Pos = spawnpos;
							vt->UpdateSector();
						}
					}
				}
			}
		}
		break;

	case DEM_SPRAY:
		s = ReadStringConst(stream);
		SprayDecal(players[player].mo, s);
		break;

	case DEM_MDK:
		s = ReadStringConst(stream);
		cht_DoMDK(&players[player], s);
		break;

	case DEM_PAUSE:
		if (gamestate == GS_LEVEL)
		{
			if (paused)
			{
				paused = 0;
				S_ResumeSound(false);
			}
			else
			{
				paused = player + 1;
				S_PauseSound(false, false);
			}
		}
		break;

	case DEM_SAVEGAME:
		if (gamestate == GS_LEVEL)
		{
			savegamefile = ReadStringConst(stream);
			savedescription = ReadStringConst(stream);
			if (player != consoleplayer)
			{
				// Paths sent over the network will be valid for the system that sent
				// the save command. For other systems, the path needs to be changed.
				FString basename = ExtractFileBase(savegamefile.GetChars(), true);
				savegamefile = G_BuildSaveName(basename.GetChars());
			}
		}
		gameaction = ga_savegame;
		break;

	case DEM_CHECKAUTOSAVE:
		// Do not autosave in multiplayer games or when dead.
		// For demo playback, DEM_DOAUTOSAVE already exists in the demo if the
		// autosave happened. And if it doesn't, we must not generate it.
		if (!netgame && !demoplayback && disableautosave < 2 && autosavecount
			&& players[player].playerstate == PST_LIVE)
		{
			Net_WriteInt8(DEM_DOAUTOSAVE);
		}
		break;

	case DEM_DOAUTOSAVE:
		gameaction = ga_autosave;
		break;

	case DEM_FOV:
		{
			float newfov = ReadFloat(stream);
			if (newfov != players[player].DesiredFOV)
			{
				Printf("FOV%s set to %g\n",
					player == Net_Arbitrator ? " for everyone" : "",
					newfov);
			}

			for (auto client : NetworkClients)
				players[client].DesiredFOV = newfov;
		}
		break;

	case DEM_MYFOV:
		players[player].DesiredFOV = ReadFloat(stream);
		break;

	case DEM_RUNSCRIPT:
	case DEM_RUNSCRIPT2:
		{
			int snum = ReadInt16(stream);
			int argn = ReadInt8(stream);
			RunScript(stream, players[player].mo, snum, argn, (cmd == DEM_RUNSCRIPT2) ? ACS_ALWAYS : 0);
		}
		break;

	case DEM_RUNNAMEDSCRIPT:
		{
			s = ReadStringConst(stream);
			int argn = ReadInt8(stream);
			RunScript(stream, players[player].mo, -FName(s).GetIndex(), argn & 127, (argn & 128) ? ACS_ALWAYS : 0);
		}
		break;

	case DEM_RUNSPECIAL:
		{
			int snum = ReadInt16(stream);
			int argn = ReadInt8(stream);
			int arg[5] = {};

			for (i = 0; i < argn; ++i)
			{
				int argval = ReadInt32(stream);
				if ((unsigned)i < countof(arg))
					arg[i] = argval;
			}

			if (!CheckCheatmode(player == consoleplayer))
				P_ExecuteSpecial(primaryLevel, snum, nullptr, players[player].mo, false, arg[0], arg[1], arg[2], arg[3], arg[4]);
		}
		break;

	case DEM_CROUCH:
		if (gamestate == GS_LEVEL && players[player].mo != nullptr
			&& players[player].playerstate == PST_LIVE && !(players[player].oldbuttons & BT_JUMP)
			&& !P_IsPlayerTotallyFrozen(&players[player]))
		{
			players[player].crouching = players[player].crouchdir < 0 ? 1 : -1;
		}
		break;

	case DEM_MORPHEX:
		{
			s = ReadStringConst(stream);
			FString msg = cht_Morph(players + player, PClass::FindActor(s), false);
			if (player == consoleplayer)
				Printf("%s\n", msg[0] != '\0' ? msg.GetChars() : "Morph failed.");
		}
		break;

	case DEM_ADDCONTROLLER:
		{
			uint8_t playernum = ReadInt8(stream);
			players[playernum].settings_controller = true;
			if (consoleplayer == playernum || consoleplayer == Net_Arbitrator)
				Printf("%s has been added to the controller list.\n", players[playernum].userinfo.GetName());
		}
		break;

	case DEM_DELCONTROLLER:
		{
			uint8_t playernum = ReadInt8(stream);
			players[playernum].settings_controller = false;
			if (consoleplayer == playernum || consoleplayer == Net_Arbitrator)
				Printf("%s has been removed from the controller list.\n", players[playernum].userinfo.GetName());
		}
		break;

	case DEM_KILLCLASSCHEAT:
		{
			s = ReadStringConst(stream);
			int killcount = 0;
			PClassActor *cls = PClass::FindActor(s);

			if (cls != nullptr)
			{
				killcount = primaryLevel->Massacre(false, cls->TypeName);
				PClassActor *cls_rep = cls->GetReplacement(primaryLevel);
				if (cls != cls_rep)
					killcount += primaryLevel->Massacre(false, cls_rep->TypeName);

				Printf("Killed %d monsters of type %s.\n", killcount, s);
			}
			else
			{
				Printf("%s is not an actor class.\n", s);
			}
		}
		break;

	case DEM_REMOVE:
		{
			s = ReadStringConst(stream);
			int removecount = 0;
			PClassActor *cls = PClass::FindActor(s);
			if (cls != nullptr && cls->IsDescendantOf(RUNTIME_CLASS(AActor)))
			{
				removecount = RemoveClass(primaryLevel, cls);
				const PClass *cls_rep = cls->GetReplacement(primaryLevel);
				if (cls != cls_rep)
					removecount += RemoveClass(primaryLevel, cls_rep);

				Printf("Removed %d actors of type %s.\n", removecount, s);
			}
			else
			{
				Printf("%s is not an actor class.\n", s);
			}
		}
		break;

	case DEM_CONVREPLY:
	case DEM_CONVCLOSE:
	case DEM_CONVNULL:
		P_ConversationCommand(cmd, player, stream);
		break;

	case DEM_SETSLOT:
	case DEM_SETSLOTPNUM:
		{
			int pnum = player;
			if (cmd == DEM_SETSLOTPNUM)
				pnum = ReadInt8(stream);

			unsigned int slot = ReadInt8(stream);
			int count = ReadInt8(stream);
			if (slot < NUM_WEAPON_SLOTS)
				players[pnum].weapons.ClearSlot(slot);

			for (i = 0; i < count; ++i)
			{
				PClassActor *wpn = Net_ReadWeapon(stream);
				players[pnum].weapons.AddSlot(slot, wpn, pnum == consoleplayer);
			}
		}
		break;

	case DEM_ADDSLOT:
		{
			int slot = ReadInt8(stream);
			PClassActor *wpn = Net_ReadWeapon(stream);
			players[player].weapons.AddSlot(slot, wpn, player == consoleplayer);
		}
		break;

	case DEM_ADDSLOTDEFAULT:
		{
			int slot = ReadInt8(stream);
			PClassActor *wpn = Net_ReadWeapon(stream);
			players[player].weapons.AddSlotDefault(slot, wpn, player == consoleplayer);
		}
		break;

	case DEM_SETPITCHLIMIT:
		players[player].MinPitch = DAngle::fromDeg(-ReadInt8(stream));		// up
		players[player].MaxPitch = DAngle::fromDeg(ReadInt8(stream));		// down
		break;

	case DEM_REVERTCAMERA:
		players[player].camera = players[player].mo;
		break;

	case DEM_FINISHGAME:
		// Simulate an end-of-game action
		primaryLevel->ChangeLevel(nullptr, 0, 0);
		break;

	case DEM_NETEVENT:
		{
			s = ReadStringConst(stream);
			int argn = ReadInt8(stream);
			int arg[3] = { 0, 0, 0 };
			for (int i = 0; i < 3; i++)
				arg[i] = ReadInt32(stream);
			bool manual = !!ReadInt8(stream);
			primaryLevel->localEventManager->Console(player, s, arg[0], arg[1], arg[2], manual, false);
		}
		break;

	case DEM_ENDSCREENJOB:
		EndScreenJob();
		break;

	case DEM_ZSC_CMD:
		{
			FName cmd = ReadStringConst(stream);
			unsigned int size = ReadInt16(stream);

			TArray<uint8_t> buffer;
			if (size)
			{
				buffer.Grow(size);
				for (unsigned int i = 0u; i < size; ++i)
					buffer.Push(ReadInt8(stream));
			}

			FNetworkCommand netCmd = { player, cmd, buffer };
			primaryLevel->localEventManager->NetCommand(netCmd);
		}
		break;

	case DEM_CHANGESKILL:
		NextSkill = ReadInt32(stream);
		break;

	case DEM_KICK:
		{
			const int pNum = ReadInt8(stream);
			if (pNum == consoleplayer)
			{
				I_Error("You have been kicked from the game");
			}
			else
			{
				Printf("%s has been kicked from the game\n", players[pNum].userinfo.GetName());
				if (NetworkClients.InGame(pNum))
					DisconnectClient(pNum);
			}
		}
		break;
		
	default:
		I_Error("Unknown net command: %d", cmd);
		break;
	}
}

// Used by DEM_RUNSCRIPT, DEM_RUNSCRIPT2, and DEM_RUNNAMEDSCRIPT
static void RunScript(uint8_t **stream, AActor *pawn, int snum, int argn, int always)
{
	// Scripts can be invoked without a level loaded, e.g. via puke(name) CCMD in fullscreen console
	if (pawn == nullptr)
		return;

	int arg[4] = {};
	for (int i = 0; i < argn; ++i)
	{
		int argval = ReadInt32(stream);
		if ((unsigned)i < countof(arg))
			arg[i] = argval;
	}

	P_StartScript(pawn->Level, pawn, nullptr, snum, primaryLevel->MapName.GetChars(), arg, min<int>(countof(arg), argn), ACS_NET | always);
}

// TODO: This really needs to be replaced with some kind of packet system that can simply read through packets and opt
// not to execute them. Right now this is making setting up net commands a nightmare.
// Reads through the network stream but doesn't actually execute any command. Used for getting the size of a stream.
// The skip amount is the number of bytes the command possesses. This should mirror the bytes in Net_DoCommand().
void Net_SkipCommand(int cmd, uint8_t **stream)
{
	size_t skip = 0;
	switch (cmd)
	{
		case DEM_SAY:
			skip = strlen((char *)(*stream + 1)) + 2;
			break;

		case DEM_ADDBOT:
			skip = strlen((char *)(*stream + 1)) + 6;
			break;

		case DEM_GIVECHEAT:
		case DEM_TAKECHEAT:
			skip = strlen((char *)(*stream)) + 5;
			break;

		case DEM_SETINV:
			skip = strlen((char *)(*stream)) + 6;
			break;

		case DEM_NETEVENT:
			skip = strlen((char *)(*stream)) + 15;
			break;

		case DEM_ZSC_CMD:
			skip = strlen((char*)(*stream)) + 1;
			skip += (((*stream)[skip] << 8) | (*stream)[skip + 1]) + 2;
			break;

		case DEM_SUMMON2:
		case DEM_SUMMONFRIEND2:
		case DEM_SUMMONFOE2:
			skip = strlen((char *)(*stream)) + 26;
			break;
		case DEM_CHANGEMAP2:
			skip = strlen((char *)(*stream + 1)) + 2;
			break;
		case DEM_MUSICCHANGE:
		case DEM_PRINT:
		case DEM_CENTERPRINT:
		case DEM_UINFCHANGED:
		case DEM_CHANGEMAP:
		case DEM_SUMMON:
		case DEM_SUMMONFRIEND:
		case DEM_SUMMONFOE:
		case DEM_SUMMONMBF:
		case DEM_REMOVE:
		case DEM_SPRAY:
		case DEM_MORPHEX:
		case DEM_KILLCLASSCHEAT:
		case DEM_MDK:
			skip = strlen((char *)(*stream)) + 1;
			break;

		case DEM_WARPCHEAT:
			skip = 6;
			break;

		case DEM_INVUSE:
		case DEM_FOV:
		case DEM_MYFOV:
		case DEM_CHANGESKILL:
			skip = 4;
			break;

		case DEM_INVDROP:
			skip = 8;
			break;

		case DEM_GENERICCHEAT:
		case DEM_DROPPLAYER:
		case DEM_ADDCONTROLLER:
		case DEM_DELCONTROLLER:
		case DEM_KICK:
			skip = 1;
			break;

		case DEM_SAVEGAME:
			skip = strlen((char *)(*stream)) + 1;
			skip += strlen((char *)(*stream) + skip) + 1;
			break;

		case DEM_SINFCHANGEDXOR:
		case DEM_SINFCHANGED:
			{
				uint8_t t = **stream;
				skip = 1 + (t & 63);
				if (cmd == DEM_SINFCHANGED)
				{
					switch (t >> 6)
					{
					case CVAR_Bool:
						skip += 1;
						break;
					case CVAR_Int:
					case CVAR_Float:
						skip += 4;
						break;
					case CVAR_String:
						skip += strlen((char*)(*stream + skip)) + 1;
						break;
					}
				}
				else
				{
					skip += 1;
				}
			}
			break;

		case DEM_RUNSCRIPT:
		case DEM_RUNSCRIPT2:
			skip = 3 + *(*stream + 2) * 4;
			break;

		case DEM_RUNNAMEDSCRIPT:
			skip = strlen((char *)(*stream)) + 2;
			skip += ((*(*stream + skip - 1)) & 127) * 4;
			break;

		case DEM_RUNSPECIAL:
			skip = 3 + *(*stream + 2) * 4;
			break;

		case DEM_CONVREPLY:
			skip = 3;
			break;

		case DEM_SETSLOT:
		case DEM_SETSLOTPNUM:
			{
				skip = 2 + (cmd == DEM_SETSLOTPNUM);
				for (int numweapons = (*stream)[skip-1]; numweapons > 0; --numweapons)
					skip += 1 + ((*stream)[skip] >> 7);
			}
			break;

		case DEM_ADDSLOT:
		case DEM_ADDSLOTDEFAULT:
			skip = 2 + ((*stream)[1] >> 7);
			break;

		case DEM_SETPITCHLIMIT:
			skip = 2;
			break;
	}

	*stream += skip;
}

// This was taken out of shared_hud, because UI code shouldn't do low level calculations that may change if the backing implementation changes.
int Net_GetLatency(int* localDelay, int* arbitratorDelay)
{
	const int gameDelayMs = (ClientTic - gametic) * 1000 / TICRATE;
	int severity = 0;
	if (gameDelayMs >= 160)
		severity = 3;
	else if (gameDelayMs >= 120)
		severity = 2;
	else if (gameDelayMs >= 80)
		severity = 1;
	
	*localDelay = gameDelayMs;
	*arbitratorDelay = NetMode == NET_PacketServer ? ClientStates[consoleplayer].AverageLatency : ClientStates[Net_Arbitrator].AverageLatency;
	return severity;
}

//==========================================================================
//
//
//
//==========================================================================

// [RH] List "ping" times
CCMD(pings)
{
	if (!netgame)
	{
		Printf("Not currently in a net game\n");
		return;
	}

	if (NetworkClients.Size() <= 1)
		return;

	// In Packet Server mode, this displays the latency each individual client has to the host
	for (auto client : NetworkClients)
	{
		if ((NetMode == NET_PeerToPeer && client != consoleplayer) || (NetMode == NET_PacketServer && client != Net_Arbitrator))
			Printf("%ums %s\n", ClientStates[client].AverageLatency, players[client].userinfo.GetName());
	}
}

CCMD(listplayers)
{
	if (!netgame)
	{
		Printf("Not currently in a net game\n");
		return;
	}

	for (auto client : NetworkClients)
	{
		if (client == consoleplayer)
			Printf("* ");
		Printf("%s - %d\n", players[client].userinfo.GetName(), client);
	}
}

CCMD(kick)
{
	if (argv.argc() == 1)
	{
		Printf("Usage: kick <player number>\n");
		return;
	}

	if (!netgame)
	{
		Printf("Not currently in a net game\n");
		return;
	}

	// Dont give settings controllers access to this. That should be reserved as a separate power
	// the host can grant.
	if (consoleplayer != Net_Arbitrator)
	{
		Printf("Only the host is allowed to kick other players\n");
		return;
	}

	int pNum = -1;
	if (!C_IsValidInt(argv[1], pNum))
	{
		Printf("A player number must be provided. Use listplayers for more information\n");
		return;
	}

	if (pNum == consoleplayer || pNum < 0 || pNum >= MAXPLAYERS)
	{
		Printf("Invalid player number provided\n");
		return;
	}

	if (!NetworkClients.InGame(pNum))
	{
		Printf("Player is not currently in the game\n");
		return;
	}

	Net_WriteInt8(DEM_KICK);
	Net_WriteInt8(pNum);
}

CCMD(mute)
{
	if (argv.argc() == 1)
	{
		Printf("Usage: mute <player number> - Don't receive messages from this player\n");
		return;
	}

	if (!netgame)
	{
		Printf("Not currently in a net game\n");
		return;
	}

	int pNum = -1;
	if (!C_IsValidInt(argv[1], pNum))
	{
		Printf("A player number must be provided. Use listplayers for more information\n");
		return;
	}

	if (pNum == consoleplayer || pNum < 0 || pNum >= MAXPLAYERS)
	{
		Printf("Invalid player number provided\n");
		return;
	}

	if (!NetworkClients.InGame(pNum))
	{
		Printf("Player is not currently in the game\n");
		return;
	}

	MutedClients |= (uint64_t)1u << pNum;
}

CCMD(muteall)
{
	if (!netgame)
	{
		Printf("Not currently in a net game\n");
		return;
	}

	for (auto client : NetworkClients)
	{
		if (client != consoleplayer)
			MutedClients |= (uint64_t)1u << client;
	}
}

CCMD(listmuted)
{
	if (!netgame)
	{
		Printf("Not currently in a net game\n");
		return;
	}

	bool found = false;
	for (auto client : NetworkClients)
	{
		if (MutedClients & ((uint64_t)1u << client))
		{
			found = true;
			Printf("%s - %d\n", players[client].userinfo.GetName(), client);
		}
	}

	if (!found)
		Printf("No one currently muted\n");
}

CCMD(unmute)
{
	if (argv.argc() == 1)
	{
		Printf("Usage: unmute <player number> - Allow messages from this player again\n");
		return;
	}

	if (!netgame)
	{
		Printf("Not currently in a net game\n");
		return;
	}

	int pNum = -1;
	if (!C_IsValidInt(argv[1], pNum))
	{
		Printf("A player number must be provided. Use listplayers for more information\n");
		return;
	}

	if (pNum == consoleplayer || pNum < 0 || pNum >= MAXPLAYERS)
	{
		Printf("Invalid player number provided\n");
		return;
	}

	MutedClients &= ~((uint64_t)1u << pNum);
}

CCMD(unmuteall)
{
	if (!netgame)
	{
		Printf("Not currently in a net game\n");
		return;
	}

	MutedClients = 0u;
}

//==========================================================================
//
// Network_Controller
//
// Implement players who have the ability to change settings in a network
// game.
//
//==========================================================================

static void Network_Controller(int pNum, bool add)
{
	if (!netgame)
	{
		Printf("This command can only be used when playing a net game.\n");
		return;
	}

	if (consoleplayer != Net_Arbitrator)
	{
		Printf("This command is only accessible to the host.\n");
		return;
	}

	if (pNum == Net_Arbitrator)
	{
		Printf("The host cannot change their own settings controller status.\n");
		return;
	}

	if (!NetworkClients.InGame(pNum))
	{
		Printf("Player %d is not a valid client\n", pNum);
		return;
	}

	if (players[pNum].settings_controller && add)
	{
		Printf("%s is already on the setting controller list.\n", players[pNum].userinfo.GetName());
		return;
	}

	if (!players[pNum].settings_controller && !add)
	{
		Printf("%s is not on the setting controller list.\n", players[pNum].userinfo.GetName());
		return;
	}

	Net_WriteInt8(add ? DEM_ADDCONTROLLER : DEM_DELCONTROLLER);
	Net_WriteInt8(pNum);
}

//==========================================================================
//
// CCMD net_addcontroller
//
//==========================================================================

CCMD(net_addcontroller)
{
	if (argv.argc() < 2)
	{
		Printf("Usage: net_addcontroller <player num>\n");
		return;
	}

	Network_Controller(atoi (argv[1]), true);
}

//==========================================================================
//
// CCMD net_removecontroller
//
//==========================================================================

CCMD(net_removecontroller)
{
	if (argv.argc() < 2)
	{
		Printf("Usage: net_removecontroller <player num>\n");
		return;
	}

	Network_Controller(atoi(argv[1]), false);
}

//==========================================================================
//
// CCMD net_listcontrollers
//
//==========================================================================

CCMD(net_listcontrollers)
{
	if (!netgame)
	{
		Printf ("This command can only be used when playing a net game.\n");
		return;
	}

	Printf("The following players can change the game settings:\n");

	for (auto client : NetworkClients)
	{
		if (players[client].settings_controller)
			Printf("- %s\n", players[client].userinfo.GetName());
	}
}
