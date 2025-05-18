#ifndef __I_NET_H__
#define __I_NET_H__

#include <stdint.h>
#include "tarray.h"

inline constexpr size_t MAXPLAYERS = 64u;

enum ENetConstants
{
	BACKUPTICS = 35 * 5,	// Remember up to 5 seconds of data.
	MAXTICDUP = 3,
	MAXSENDTICS = 35 * 1,	// Only send up to 1 second of data at a time.
	LOCALCMDTICS = (BACKUPTICS * MAXTICDUP),
	MAX_MSGLEN = 14000,
};

enum ENetCommand
{
	CMD_NONE,
	CMD_SEND,
	CMD_GET,
};

enum ENetFlags
{
	NCMD_EXIT = 0x80,		// Client has left the game
	NCMD_RETRANSMIT = 0x40,		// 
	NCMD_SETUP = 0x20,		// Guest is letting the host know who it is
	NCMD_LEVELREADY = 0x10,		// After loading a level, guests send this over to the host who then sends it back after all are received
	NCMD_QUITTERS = 0x08,		// Client is getting info about one or more players quitting (packet server only)
	NCMD_COMPRESSED = 0x04,		// Remainder of packet is compressed
	NCMD_LATENCYACK = 0x02,		// A latency packet was just read, so let the sender know.
	NCMD_LATENCY = 0x01,		// Latency packet, used for measuring RTT.		
};

enum ENetMode
{
	NET_PeerToPeer,
	NET_PacketServer
};

struct FClientStack : public TArray<int>
{
	inline bool InGame(int i) const { return Find(i) < Size(); }

	void operator+=(const int i)
	{
		if (!InGame(i))
			SortedInsert(i);
	}

	void operator-=(const int i)
	{
		Delete(Find(i));
	}
};

extern bool netgame, multiplayer;
extern int consoleplayer;
extern int Net_Arbitrator;
extern FClientStack NetworkClients;
extern ENetMode NetMode;
extern uint8_t NetBuffer[MAX_MSGLEN];
extern size_t NetBufferLength;
extern uint8_t TicDup;
extern int RemoteClient;
extern int MaxClients;

bool I_InitNetwork();
void I_ClearClient(size_t client);
void I_NetCmd(ENetCommand cmd);
void I_NetDone();
void HandleIncomingConnection();
void CloseNetwork();

#endif
