// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: i_net.c,v 1.2 1997/12/29 19:50:54 pekangas Exp $
//
// Copyright (C) 1993-1996 by id Software, Inc.
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 2 of the License, or
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
//
//
// Alternatively the following applies:
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
//
// DESCRIPTION:
//		Low-level networking code. Uses BSD sockets for UDP networking.
//
//-----------------------------------------------------------------------------


/* [Petteri] Check if compiling for Win32:	*/
#if defined(__WINDOWS__) || defined(__NT__) || defined(_MSC_VER) || defined(_WIN32)
#ifndef __WIN32__
#	define __WIN32__
#endif
#endif
/* Follow #ifdef __WIN32__ marks */

#include <stdlib.h>
#include <string.h>

/* [Petteri] Use Winsock for Win32: */
#ifdef __WIN32__
#	define WIN32_LEAN_AND_MEAN
#	include <windows.h>
#	include <winsock.h>
#else
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <arpa/inet.h>
#	include <errno.h>
#	include <unistd.h>
#	include <netdb.h>
#	include <sys/ioctl.h>
#	ifdef __sun
#		include <fcntl.h>
#	endif
#endif

#include "i_system.h"
#include "m_argv.h"
#include "m_crc32.h"
#include "st_start.h"
#include "engineerrors.h"
#include "cmdlib.h"
#include "printf.h"
#include "i_interface.h"
#include "c_cvars.h"
#include "version.h"
#include "i_net.h"

/* [Petteri] Get more portable: */
#ifndef __WIN32__
typedef int SOCKET;
#define SOCKET_ERROR		-1
#define INVALID_SOCKET		-1
#define closesocket			close
#define ioctlsocket			ioctl
#define Sleep(x)			usleep (x * 1000)
#define WSAEWOULDBLOCK		EWOULDBLOCK
#define WSAECONNRESET		ECONNRESET
#define WSAGetLastError()	errno
#endif

#ifndef IPPORT_USERRESERVED
#define IPPORT_USERRESERVED 5000
#endif

#ifdef __WIN32__
typedef int socklen_t;
const char* neterror(void);
#else
#define neterror() strerror(errno)
#endif

// As per http://support.microsoft.com/kb/q192599/ the standard
// size for network buffers is 8k.
constexpr size_t MaxTransmitSize = 8000u;
constexpr size_t MinCompressionSize = 10u;
constexpr size_t MaxPasswordSize = 256u;

enum ENetConnectType : uint8_t
{
	PRE_HEARTBEAT,			// Host and guests are keep each other's connections alive
	PRE_CONNECT,			// Sent from guest to host for initial connection
	PRE_CONNECT_ACK,		// Sent from host to guest to confirm they've been connected
	PRE_DISCONNECT,			// Sent from host to guest when another guest leaves
	PRE_USER_INFO,			// Host and guests are sending each other user infos
	PRE_USER_INFO_ACK,		// Host and guests are confirming sent user infos
	PRE_GAME_INFO,			// Sent from host to guest containing general game info
	PRE_GAME_INFO_ACK,		// Sent from guest to host confirming game info was gotten
	PRE_GO,					// Sent from host to guest telling them to start the game

	PRE_FULL,				// Sent from host to guest if the lobby is full
	PRE_IN_PROGRESS,		// Sent from host to guest if the game has already started
	PRE_WRONG_PASSWORD,		// Sent from host to guest if their provided password was wrong
	PRE_WRONG_ENGINE,		// Sent from host to guest if their engine version doesn't match the host's
	PRE_INVALID_FILES,		// Sent from host to guest if their files do not match the host's
	PRE_KICKED,				// Sent from hsot to guest if the host kicked them from the game
	PRE_BANNED,				// Sent from host to guest if the host banned them from the game
};

enum EConnectionStatus
{
	CSTAT_NONE,			// Guest isn't connected
	CSTAT_CONNECTING,	// Guest is trying to connect
	CSTAT_WAITING,		// Guest is waiting for game info
	CSTAT_READY,		// Guest is ready to start the game
};

// These need to be synced with the window backends so information about each
// client can be properly displayed.
enum EConnectionFlags : unsigned int
{
	CFL_NONE			= 0,
	CFL_CONSOLEPLAYER	= 1,
	CFL_HOST			= 1 << 1,
};

struct FConnection
{
	EConnectionStatus Status = CSTAT_NONE;
	sockaddr_in Address = {};
	uint64_t InfoAck = 0u;
	bool bHasGameInfo = false;
};

bool netgame = false;
bool multiplayer = false;
ENetMode NetMode = NET_PeerToPeer;
int consoleplayer = -1;
int Net_Arbitrator = 0;
FClientStack NetworkClients = {};

uint32_t GameID = DEFAULT_GAME_ID;
uint8_t	TicDup = 1u;
uint8_t	MaxClients = 1u;
int RemoteClient = -1;
size_t NetBufferLength = 0u;
uint8_t NetBuffer[MAX_MSGLEN] = {};

static u_short		GamePort = (IPPORT_USERRESERVED + 29);
static SOCKET		MySocket = INVALID_SOCKET;
static FConnection	Connected[MAXPLAYERS] = {};
static uint8_t		TransmitBuffer[MaxTransmitSize] = {};
static TArray<sockaddr_in> BannedConnections = {};
static bool bGameStarted = false;

CUSTOM_CVAR(String, net_password, "", CVAR_IGNORE)
{
	if (strlen(self) + 1 > MaxPasswordSize)
	{
		self = "";
		Printf(TEXTCOLOR_RED "Password cannot be greater than 255 characters\n");
	}
}

void Net_SetupUserInfo();
const char* Net_GetClientName(int client, unsigned int charLimit);
int Net_SetUserInfo(int client, uint8_t*& stream);
int Net_ReadUserInfo(int client, uint8_t*& stream);
int Net_ReadGameInfo(uint8_t*& stream);
int Net_SetGameInfo(uint8_t*& stream);

static SOCKET CreateUDPSocket()
{
	SOCKET s = socket(PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET)
		I_FatalError("Couldn't create socket: %s", neterror());

	return s;
}

static void BindToLocalPort(SOCKET s, u_short port)
{
	sockaddr_in address = {};
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	int v = bind(s, (sockaddr *)&address, sizeof(address));
	if (v == SOCKET_ERROR)
		I_FatalError("Couldn't bind to port: %s", neterror());
}

static void BuildAddress(sockaddr_in& address, const char* addrName)
{
	FString target = {};
	u_short port = GamePort;
	const char* portName = strchr(addrName, ':');
	if (portName != nullptr)
	{
		target = FString(addrName, portName - addrName);
		u_short portConversion = atoi(portName + 1);
		if (!portConversion)
			Printf("Malformed port: %s (using %d)\n", portName + 1, GamePort);
		else
			port = portConversion;
	}
	else
	{
		target = addrName;
	}

	bool isNamed = false;
	char c = 0;
	for (size_t curChar = 0u; (c = target[curChar]); ++curChar)
	{
		if ((c < '0' || c > '9') && c != '.')
		{
			isNamed = true;
			break;
		}
	}

	address.sin_family = AF_INET;
	address.sin_port = htons(port);
	if (!isNamed)
	{
		address.sin_addr.s_addr = inet_addr(target.GetChars());
	}
	else
	{
		hostent* hostEntry = gethostbyname(target.GetChars());
		if (hostEntry == nullptr)
			I_FatalError("gethostbyname: Couldn't find %s\n%s", target.GetChars(), neterror());

		address.sin_addr.s_addr = *(int*)hostEntry->h_addr_list[0];
	}
}

static void StartNetwork(bool autoPort)
{
#ifdef __WIN32__
	WSADATA data;
	if (WSAStartup(0x0101, &data))
		I_FatalError("Couldn't initialize Windows sockets");
#endif

	netgame = true;
	multiplayer = true;
	MySocket = CreateUDPSocket();
	BindToLocalPort(MySocket, autoPort ? 0 : GamePort);

	u_long trueVal = 1u;
#ifndef __sun
	ioctlsocket(MySocket, FIONBIO, &trueVal);
#else
	fcntl(mysocket, F_SETFL, trueval | O_NONBLOCK);
#endif
}

void CloseNetwork()
{
	if (MySocket != INVALID_SOCKET)
	{
		closesocket(MySocket);
		MySocket = INVALID_SOCKET;
		netgame = false;
	}
#ifdef __WIN32__
	WSACleanup();
#endif
}

static int PrivateNetOf(const in_addr& in)
{
	int addr = ntohl(in.s_addr);
	if ((addr & 0xFFFF0000) == 0xC0A80000)		// 192.168.0.0
	{
		return 0xC0A80000;
	}
	else if ((addr & 0xFFFF0000) >= 0xAC100000 && (addr & 0xFFFF0000) <= 0xAC1F0000)	// 172.16.0.0 - 172.31.0.0
	{
		return 0xAC100000;
	}
	else if ((addr & 0xFF000000) == 0x0A000000)		// 10.0.0.0
	{
		return 0x0A000000;
	}
	else if ((addr & 0xFF000000) == 0x7F000000)		// 127.0.0.0 (localhost)
	{
		return 0x7F000000;
	}
	// Not a private IP
	return 0;
}

// The best I can really do here is check if the others are on the same
// private network, since that means we (probably) are too.
static bool ClientsOnSameNetwork()
{
	size_t start = 1u;
	for (; start < MaxClients; ++start)
	{
		if (Connected[start].Status != CSTAT_NONE)
			break;
	}

	if (start >= MaxClients)
		return false;

	const int firstClient = PrivateNetOf(Connected[start].Address.sin_addr);
	if (!firstClient)
		return false;

	for (size_t i = 1u; i < MaxClients; ++i)
	{
		if (i == start)
			continue;

		if (Connected[i].Status == CSTAT_NONE || PrivateNetOf(Connected[i].Address.sin_addr) != firstClient)
			return false;
	}

	return true;
}

// Print a network-related message to the console. This doesn't print to the window so should
// not be used for that and is mainly for logging.
static void I_NetLog(const char* text, ...)
{
	// todo: use better abstraction once everything is migrated to in-game start screens.
#if defined _WIN32 || defined __APPLE__
	va_list ap;
	va_start(ap, text);
	VPrintf(PRINT_HIGH, text, ap);
	Printf("\n");
	va_end(ap);
#else
	FString str;
	va_list argptr;

	va_start(argptr, text);
	str.VFormat(text, argptr);
	va_end(argptr);
	fprintf(stderr, "\r%-40s\n", str.GetChars());
#endif
}

// Gracefully closes the net window so that any error messaging can be properly displayed.
static void I_NetError(const char* error)
{
	StartWindow->NetClose();
	I_FatalError("%s", error);
}

static void I_NetInit(const char* msg, bool host)
{
	StartWindow->NetInit(msg, host);
}

// todo: later these must be dispatched by the main menu, not the start screen.
// Updates the general status of the lobby.
static void I_NetMessage(const char* msg)
{
	StartWindow->NetMessage(msg);
}

// Listen for incoming connections while the lobby is active. The main thread needs to be locked up
// here to prevent the engine from continuing to start the game until everyone is ready.
static bool I_NetLoop(bool (*loopCallback)(void*), void* data)
{
	return StartWindow->NetLoop(loopCallback, data);
}

// A new client has just entered the game, so add them to the player list.
static void I_NetClientConnected(size_t client, unsigned int charLimit = 0u)
{
	const char* name = Net_GetClientName(client, charLimit);
	unsigned int flags = CFL_NONE;
	if (client == 0)
		flags |= CFL_HOST;
	if (client == consoleplayer)
		flags |= CFL_CONSOLEPLAYER;

	StartWindow->NetConnect(client, name, flags, Connected[client].Status);
}

// A client changed ready state.
static void I_NetClientUpdated(size_t client)
{
	StartWindow->NetUpdate(client, Connected[client].Status);
}

static void I_NetClientDisconnected(size_t client)
{
	StartWindow->NetDisconnect(client);
}

static void I_NetUpdatePlayers(size_t current, size_t limit)
{
	StartWindow->NetProgress(current, limit);
}

static bool I_ShouldStartNetGame()
{
	return StartWindow->ShouldStartNet();
}

static void I_GetKickClients(TArray<int>& clients)
{
	clients.Clear();

	int c = -1;
	while ((c = StartWindow->GetNetKickClient()) != -1)
		clients.Push(c);
}

static void I_GetBanClients(TArray<int>& clients)
{
	clients.Clear();

	int c = -1;
	while ((c = StartWindow->GetNetBanClient()) != -1)
		clients.Push(c);
}

void I_NetDone()
{
	StartWindow->NetDone();
}

void I_ClearClient(size_t client)
{
	memset(&Connected[client], 0, sizeof(Connected[client]));
}

static int FindClient(const sockaddr_in& address)
{
	int i = 0;
	for (; i < MaxClients; ++i)
	{
		if (Connected[i].Status == CSTAT_NONE)
			continue;

		if (address.sin_addr.s_addr == Connected[i].Address.sin_addr.s_addr
			&& address.sin_port == Connected[i].Address.sin_port)
		{
			break;
		}
	}

	return i >= MaxClients ? -1 : i;
}

static void SendPacket(const sockaddr_in& to)
{
	// Huge packets should be sent out as sequences, not as one big packet, otherwise it's prone
	// to high amounts of congestion and reordering needed.
	if (NetBufferLength > MAX_MSGLEN)
		I_FatalError("Netbuffer overflow: Tried to send %u bytes of data", NetBufferLength);

	assert(!(NetBuffer[0] & NCMD_COMPRESSED));

	uLong size = MaxTransmitSize - 1u;
	int res = -1;
	if (NetBufferLength >= MinCompressionSize)
	{
		TransmitBuffer[0] = NetBuffer[0] | NCMD_COMPRESSED;
		res = compress2(TransmitBuffer + 1, &size, NetBuffer + 1, NetBufferLength - 1u, 9);
		++size;
	}

	if (res == Z_OK && size < static_cast<uLong>(NetBufferLength))
		res = sendto(MySocket, (const char*)TransmitBuffer, size, 0, (const sockaddr*)&to, sizeof(to));
	else if (NetBufferLength > MaxTransmitSize)
		I_Error("Net compression failed (zlib error %d)", res);
	else
		res = sendto(MySocket, (const char*)NetBuffer, NetBufferLength, 0, (const sockaddr*)&to, sizeof(to));
}

static void GetPacket(sockaddr_in* const from = nullptr)
{
	sockaddr_in fromAddress;
	socklen_t fromSize = sizeof(fromAddress);

	int msgSize = recvfrom(MySocket, (char *)TransmitBuffer, MaxTransmitSize, 0,
				  (sockaddr *)&fromAddress, &fromSize);

	int client = FindClient(fromAddress);
	if (client >= 0 && msgSize == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err == WSAECONNRESET)
		{
			if (consoleplayer == -1)
			{
				client = -1;
				msgSize = 0;
			}
			else
			{
				// The remote node aborted unexpectedly, so pretend it sent an exit packet. If in packet server
				// mode and it was the host, just consider the game too bricked to continue since the host has
				// to determine the new host properly.
				if (NetMode == NET_PacketServer && client == Net_Arbitrator)
					I_NetError("Host unexpectedly disconnected");

				NetBuffer[0] = NCMD_EXIT;
				msgSize = 1;
			}
		}
		else if (err != WSAEWOULDBLOCK)
		{
			I_Error("Failed to get packet: %s", neterror());
		}
		else
		{
			client = -1;
			msgSize = 0;
		}
	}
	else if (msgSize > 0)
	{
		if (client == -1 && !(TransmitBuffer[0] & NCMD_SETUP))
		{
			msgSize = 0;
		}
		else if (client == -1 && bGameStarted)
		{
			NetBuffer[0] = NCMD_SETUP;
			NetBuffer[1] = PRE_IN_PROGRESS;
			NetBufferLength = 2u;
			SendPacket(fromAddress);
			msgSize = 0;
		}
		else
		{
			NetBuffer[0] = (TransmitBuffer[0] & ~NCMD_COMPRESSED);
			if (TransmitBuffer[0] & NCMD_COMPRESSED)
			{
				uLongf size = MAX_MSGLEN - 1;
				int err = uncompress(NetBuffer + 1, &size, TransmitBuffer + 1, msgSize - 1);
				if (err != Z_OK)
				{
					Printf("Net decompression failed (zlib error %s)\n", M_ZLibError(err).GetChars());
					client = -1;
					msgSize = 0;
				}
				else
				{
					msgSize = size + 1;
				}
			}
			else
			{
				memcpy(NetBuffer + 1, TransmitBuffer + 1, msgSize - 1);
			}
		}
	}
	else
	{
		client = -1;
	}

	RemoteClient = client;
	NetBufferLength = max<int>(msgSize, 0);
	if (from != nullptr)
		*from = fromAddress;
}

void I_NetCmd(ENetCommand cmd)
{
	if (cmd == CMD_SEND)
	{
		if (RemoteClient >= 0)
			SendPacket(Connected[RemoteClient].Address);
	}
	else if (cmd == CMD_GET)
	{
		GetPacket();
	}
}

static void SetClientAck(size_t client, size_t from, bool add)
{
	const uint64_t bit = (uint64_t)1u << from;
	if (add)
		Connected[client].InfoAck |= bit;
	else
		Connected[client].InfoAck &= ~bit;
}

static bool ClientGotAck(size_t client, size_t from)
{
	return (Connected[client].InfoAck & ((uint64_t)1u << from));
}

static bool GetConnection(sockaddr_in& from)
{
	GetPacket(&from);
	return NetBufferLength > 0;
}

static void RejectConnection(const sockaddr_in& to, ENetConnectType reason)
{
	NetBuffer[0] = NCMD_SETUP;
	NetBuffer[1] = reason;
	NetBufferLength = 2u;

	SendPacket(to);
}

static void AddClientConnection(const sockaddr_in& from, size_t client)
{
	Connected[client].Status = CSTAT_CONNECTING;
	Connected[client].Address = from;
	NetworkClients += client;
	I_NetLog("Client %u joined the lobby", client);
	I_NetClientUpdated(client);

	// Make sure any ready clients are marked as needing the new client's info.
	for (size_t i = 1u; i < MaxClients; ++i)
	{
		if (Connected[i].Status == CSTAT_READY)
		{
			Connected[i].Status = CSTAT_WAITING;
			I_NetClientUpdated(i);
		}
	}
}

static void RemoveClientConnection(size_t client)
{
	I_NetClientDisconnected(client);
	I_ClearClient(client);
	NetworkClients -= client;
	I_NetLog("Client %u left the lobby", client);

	// Let everyone else know the user left as well.
	NetBuffer[0] = NCMD_SETUP;
	NetBuffer[1] = PRE_DISCONNECT;
	NetBuffer[2] = client;
	NetBufferLength = 3u;

	for (size_t i = 1u; i < MaxClients; ++i)
	{
		if (Connected[i].Status == CSTAT_NONE)
			continue;

		SetClientAck(i, client, false);
		for (int i = 0; i < 4; ++i)
			SendPacket(Connected[i].Address);
	}
}

void HandleIncomingConnection()
{
	if (consoleplayer != Net_Arbitrator || RemoteClient == -1)
		return;

	if (Connected[RemoteClient].Status == CSTAT_READY)
	{
		NetBuffer[0] = NCMD_SETUP;
		NetBuffer[1] = PRE_GO;
		NetBuffer[2] = NetMode;
		NetBufferLength = 3u;
		SendPacket(Connected[RemoteClient].Address);
	}
}

static bool Host_CheckForConnections(void* connected)
{
	const bool forceStarting = I_ShouldStartNetGame();
	const bool hasPassword = strlen(net_password) > 0;
	size_t* connectedPlayers = (size_t*)connected;

	TArray<int> toBoot = {};
	I_GetKickClients(toBoot);
	for (auto client : toBoot)
	{
		if (client <= 0 || Connected[client].Status == CSTAT_NONE)
			continue;

		sockaddr_in booted = Connected[client].Address;

		RemoveClientConnection(client);
		--*connectedPlayers;
		I_NetUpdatePlayers(*connectedPlayers, MaxClients);

		RejectConnection(booted, PRE_KICKED);
	}

	I_GetBanClients(toBoot);
	for (auto client : toBoot)
	{
		if (client <= 0 || Connected[client].Status == CSTAT_NONE)
			continue;

		sockaddr_in booted = Connected[client].Address;
		BannedConnections.Push(booted);

		RemoveClientConnection(client);
		--*connectedPlayers;
		I_NetUpdatePlayers(*connectedPlayers, MaxClients);

		RejectConnection(booted, PRE_BANNED);
	}

	sockaddr_in from;
	while (GetConnection(from))
	{
		if (NetBuffer[0] == NCMD_EXIT)
		{
			if (RemoteClient >= 0)
			{
				RemoveClientConnection(RemoteClient);
				--*connectedPlayers;
				I_NetUpdatePlayers(*connectedPlayers, MaxClients);
			}

			continue;
		}

		if (NetBuffer[0] != NCMD_SETUP)
			continue;

		if (NetBuffer[1] == PRE_CONNECT)
		{
			if (RemoteClient >= 0)
				continue;

			size_t banned = 0u;
			for (; banned < BannedConnections.Size(); ++banned)
			{
				if (BannedConnections[banned].sin_addr.s_addr == from.sin_addr.s_addr)
					break;
			}

			if (banned < BannedConnections.Size())
			{
				RejectConnection(from, PRE_BANNED);
			}
			else if (NetBuffer[2] % 256 != VER_MAJOR || NetBuffer[3] % 256 != VER_MINOR || NetBuffer[4] % 256 != VER_REVISION)
			{
				RejectConnection(from, PRE_WRONG_ENGINE);
			}
			else if (*connectedPlayers >= MaxClients)
			{
				RejectConnection(from, PRE_FULL);
			}
			else if (forceStarting)
			{
				RejectConnection(from, PRE_IN_PROGRESS);
			}
			else if (hasPassword && strcmp(net_password, (const char*)&NetBuffer[5]))
			{
				RejectConnection(from, PRE_WRONG_PASSWORD);
			}
			else
			{
				size_t free = 1u;
				for (; free < MaxClients; ++free)
				{
					if (Connected[free].Status == CSTAT_NONE)
						break;
				}

				AddClientConnection(from, free);
				++*connectedPlayers;
				I_NetUpdatePlayers(*connectedPlayers, MaxClients);
			}
		}
		else if (NetBuffer[1] == PRE_USER_INFO)
		{
			if (Connected[RemoteClient].Status == CSTAT_CONNECTING)
			{
				uint8_t* stream = &NetBuffer[2];
				Net_ReadUserInfo(RemoteClient, stream);
				Connected[RemoteClient].Status = CSTAT_WAITING;
				I_NetClientConnected(RemoteClient, 16u);
			}
		}
		else if (NetBuffer[1] == PRE_USER_INFO_ACK)
		{
			SetClientAck(RemoteClient, NetBuffer[2], true);
		}
		else if (NetBuffer[1] == PRE_GAME_INFO_ACK)
		{
			Connected[RemoteClient].bHasGameInfo = true;
		}
	}

	const size_t addrSize = sizeof(sockaddr_in);
	bool ready = true;
	NetBuffer[0] = NCMD_SETUP;
	for (size_t client = 1u; client < MaxClients; ++client)
	{
		auto& con = Connected[client];
		// If we're starting before the lobby is full, only check against connected clients.
		if (con.Status != CSTAT_READY && (!forceStarting || con.Status != CSTAT_NONE))
			ready = false;

		if (con.Status == CSTAT_CONNECTING)
		{
			NetBuffer[1] = PRE_CONNECT_ACK;
			NetBuffer[2] = client;
			NetBuffer[3] = *connectedPlayers;
			NetBuffer[4] = MaxClients;
			NetBufferLength = 5u;
			SendPacket(con.Address);
		}
		else if (con.Status == CSTAT_WAITING)
		{
			bool clientReady = true;
			if (!ClientGotAck(client, client))
			{
				NetBuffer[1] = PRE_USER_INFO_ACK;
				NetBufferLength = 2u;
				SendPacket(con.Address);
				clientReady = false;
			}

			if (!con.bHasGameInfo)
			{
				NetBuffer[1] = PRE_GAME_INFO;
				NetBuffer[2] = TicDup;
				NetBufferLength = 3u;

				uint8_t* stream = &NetBuffer[NetBufferLength];
				NetBufferLength += Net_SetGameInfo(stream);
				SendPacket(con.Address);
				clientReady = false;
			}

			NetBuffer[1] = PRE_USER_INFO;
			for (size_t i = 0u; i < MaxClients; ++i)
			{
				if (i == client || Connected[i].Status == CSTAT_NONE)
					continue;

				if (!ClientGotAck(client, i))
				{
					if (Connected[i].Status >= CSTAT_WAITING)
					{
						NetBuffer[2] = i;
						NetBufferLength = 3u;
						// Client will already have the host connection information.
						if (i > 0)
						{
							memcpy(&NetBuffer[NetBufferLength], &Connected[i].Address, addrSize);
							NetBufferLength += addrSize;
						}

						uint8_t* stream = &NetBuffer[NetBufferLength];
						NetBufferLength += Net_SetUserInfo(i, stream);
						SendPacket(con.Address);
					}
					clientReady = false;
				}
			}

			if (clientReady)
			{
				con.Status = CSTAT_READY;
				I_NetClientUpdated(client);
			}
		}
		else if (con.Status == CSTAT_READY)
		{
			NetBuffer[1] = PRE_HEARTBEAT;
			NetBuffer[2] = *connectedPlayers;
			NetBuffer[3] = MaxClients;
			NetBufferLength = 4u;
			SendPacket(con.Address);
		}
	}

	return ready && (*connectedPlayers >= MaxClients || forceStarting);
}

// Boon TODO: Add cool down between sends
static void SendAbort()
{
	NetBuffer[0] = NCMD_EXIT;
	NetBufferLength = 1u;

	if (consoleplayer == 0)
	{
		for (size_t client = 1u; client < MaxClients; ++client)
		{
			if (Connected[client].Status != CSTAT_NONE)
				SendPacket(Connected[client].Address);
		}
	}
	else
	{
		SendPacket(Connected[0].Address);
	}
}

static bool HostGame(int arg, bool forcedNetMode)
{
	if (arg >= Args->NumArgs() || !(MaxClients = atoi(Args->GetArg(arg))))
	{	// No player count specified, assume 2
		MaxClients = 2u;
	}

	if (MaxClients > MAXPLAYERS)
		I_FatalError("Cannot host a game with %u players. The limit is currently %u", MaxClients, MAXPLAYERS);

	consoleplayer = 0;
	NetworkClients += 0;
	Connected[consoleplayer].Status = CSTAT_READY;
	Net_SetupUserInfo();

	// If only 1 player, don't bother starting the network
	if (MaxClients == 1u)
	{
		TicDup = 1u;
		multiplayer = true;
		return true;
	}

	StartNetwork(false);
	I_NetInit("Waiting for other players...", true);
	I_NetUpdatePlayers(1u, MaxClients);
	I_NetClientConnected(0u, 16u);

	// Wait for the lobby to be full.
	size_t connectedPlayers = 1u;
	if (!I_NetLoop(Host_CheckForConnections, (void*)&connectedPlayers))
	{
		SendAbort();
		throw CExitEvent(0);
	}

	// Now go
	I_NetMessage("Starting game");
	I_NetDone();

	// If the player force started with only themselves in the lobby, start the game
	// immediately.
	if (connectedPlayers == 1u)
	{
		CloseNetwork();
		MaxClients = TicDup = 1u;
		return true;
	}

	if (!forcedNetMode)
	{
		if (MaxClients < 3)
			NetMode = NET_PeerToPeer;
		else if (!ClientsOnSameNetwork())
			NetMode = NET_PacketServer;
	}

	I_NetLog("Go");

	NetBuffer[0] = NCMD_SETUP;
	NetBuffer[1] = PRE_GO;
	NetBuffer[2] = NetMode;
	NetBufferLength = 3u;
	for (size_t client = 1u; client < MaxClients; ++client)
	{
		if (Connected[client].Status != CSTAT_NONE)
			SendPacket(Connected[client].Address);
	}

	I_NetLog("Total players: %u", connectedPlayers);

	return true;
}

static bool Guest_ContactHost(void* unused)
{
	// Listen for a reply.
	const size_t addrSize = sizeof(sockaddr_in);
	sockaddr_in from;
	while (GetConnection(from))
	{
		if (RemoteClient != 0)
			continue;

		if (NetBuffer[0] == NCMD_EXIT)
			I_NetError("The host cancelled the game");

		if (NetBuffer[0] != NCMD_SETUP)
			continue;

		if (NetBuffer[1] == PRE_HEARTBEAT)
		{
			MaxClients = NetBuffer[3];
			I_NetUpdatePlayers(NetBuffer[2], MaxClients);
		}
		else if (NetBuffer[1] == PRE_DISCONNECT)
		{
			I_ClearClient(NetBuffer[2]);
			NetworkClients -= NetBuffer[2];
			SetClientAck(consoleplayer, NetBuffer[2], false);
			I_NetClientDisconnected(NetBuffer[2]);
		}
		else if (NetBuffer[1] == PRE_FULL)
		{
			I_NetError("The game is full");
		}
		else if (NetBuffer[1] == PRE_IN_PROGRESS)
		{
			I_NetError("The game has already started");
		}
		else if (NetBuffer[1] == PRE_WRONG_PASSWORD)
		{
			I_NetError("Invalid password");
		}
		else if (NetBuffer[1] == PRE_WRONG_ENGINE)
		{
			I_NetError("Engine version does not match the host's engine version");
		}
		else if (NetBuffer[1] == PRE_INVALID_FILES)
		{
			I_NetError("Files do not match the host's files");
		}
		else if (NetBuffer[1] == PRE_KICKED)
		{
			I_NetError("You have been kicked from the game");
		}
		else if (NetBuffer[1] == PRE_BANNED)
		{
			I_NetError("You have been banned from the game");
		}
		else if (NetBuffer[1] == PRE_CONNECT_ACK)
		{
			if (consoleplayer == -1)
			{
				NetworkClients += 0;
				Connected[0].Status = CSTAT_WAITING;
				I_NetClientUpdated(0);

				consoleplayer = NetBuffer[2];
				NetworkClients += consoleplayer;
				Connected[consoleplayer].Status = CSTAT_CONNECTING;
				Net_SetupUserInfo();

				MaxClients = NetBuffer[4];
				I_NetMessage("Sending game information");
				I_NetUpdatePlayers(NetBuffer[3], MaxClients);
				I_NetClientConnected(consoleplayer, 16u);
			}
		}
		else if (NetBuffer[1] == PRE_USER_INFO_ACK)
		{
			// The host will only ever send us this to confirm they've gotten our data.
			SetClientAck(consoleplayer, consoleplayer, true);
			if (Connected[consoleplayer].Status == CSTAT_CONNECTING)
			{
				Connected[consoleplayer].Status = CSTAT_WAITING;
				I_NetClientUpdated(consoleplayer);
				I_NetMessage("Waiting for game to start");
			}

			NetBuffer[0] = NCMD_SETUP;
			NetBuffer[1] = PRE_USER_INFO_ACK;
			NetBuffer[2] = consoleplayer;
			NetBufferLength = 3u;
			SendPacket(from);
		}
		else if (NetBuffer[1] == PRE_GAME_INFO)
		{
			if (!Connected[consoleplayer].bHasGameInfo)
			{
				TicDup = clamp<int>(NetBuffer[2], 1, MAXTICDUP);
				uint8_t* stream = &NetBuffer[3];
				Net_ReadGameInfo(stream);
				Connected[consoleplayer].bHasGameInfo = true;
			}

			NetBuffer[0] = NCMD_SETUP;
			NetBuffer[1] = PRE_GAME_INFO_ACK;
			NetBufferLength = 2u;
			SendPacket(from);
		}
		else if (NetBuffer[1] == PRE_USER_INFO)
		{
			const size_t c = NetBuffer[2];
			if (!ClientGotAck(consoleplayer, c))
			{
				NetworkClients += c;
				size_t byte = 3u;
				if (c > 0)
				{
					Connected[c].Status = CSTAT_WAITING;
					memcpy(&Connected[c].Address, &NetBuffer[byte], addrSize);
					byte += addrSize;
				}
				else
				{
					Connected[c].Status = CSTAT_READY;
				}
				uint8_t* stream = &NetBuffer[byte];
				Net_ReadUserInfo(c, stream);
				SetClientAck(consoleplayer, c, true);

				I_NetClientConnected(c, 16u);
			}

			NetBuffer[0] = NCMD_SETUP;
			NetBuffer[1] = PRE_USER_INFO_ACK;
			NetBuffer[2] = c;
			NetBufferLength = 3u;
			SendPacket(from);
		}
		else if (NetBuffer[1] == PRE_GO)
		{
			NetMode = static_cast<ENetMode>(NetBuffer[2]);
			I_NetMessage("Starting game");
			I_NetLog("Received GO");
			return true;
		}
	}

	NetBuffer[0] = NCMD_SETUP;
	if (consoleplayer == -1)
	{
		NetBuffer[1] = PRE_CONNECT;
		NetBuffer[2] = VER_MAJOR % 256;
		NetBuffer[3] = VER_MINOR % 256;
		NetBuffer[4] = VER_REVISION % 256;
		const size_t passSize = strlen(net_password) + 1;
		memcpy(&NetBuffer[5], net_password, passSize);
		NetBufferLength = 5u + passSize;
		SendPacket(Connected[0].Address);
	}
	else
	{
		auto& con = Connected[consoleplayer];
		if (con.Status == CSTAT_CONNECTING)
		{
			NetBuffer[1] = PRE_USER_INFO;
			NetBufferLength = 2u;

			uint8_t* stream = &NetBuffer[NetBufferLength];
			NetBufferLength += Net_SetUserInfo(consoleplayer, stream);
			SendPacket(Connected[0].Address);
		}
		else if (con.Status == CSTAT_WAITING)
		{
			NetBuffer[1] = PRE_HEARTBEAT;
			NetBufferLength = 2u;
			SendPacket(Connected[0].Address);
		}
	}

	return false;
}

static bool JoinGame(int arg)
{
	if (arg >= Args->NumArgs()
		|| Args->GetArg(arg)[0] == '-' || Args->GetArg(arg)[0] == '+')
	{
		I_FatalError("You need to specify the host machine's address");
	}

	StartNetwork(true);

	// Host is always client 0.
	BuildAddress(Connected[0].Address, Args->GetArg(arg));
	Connected[0].Status = CSTAT_CONNECTING;

	I_NetInit("Contacting host...", false);
	I_NetUpdatePlayers(0u, MaxClients);
	I_NetClientUpdated(0);

	if (!I_NetLoop(Guest_ContactHost, nullptr))
	{
		SendAbort();
		throw CExitEvent(0);
	}

	for (size_t i = 1u; i < MaxClients; ++i)
	{
		if (Connected[i].Status != CSTAT_NONE)
			Connected[i].Status = CSTAT_READY;
	}

	I_NetLog("Total players: %u", MaxClients);
	I_NetDone();

	return true;
}

//
// I_InitNetwork
//
// Returns true if packet server mode might be a good idea.
//
bool I_InitNetwork()
{
	// set up for network
	const char* v = Args->CheckValue("-dup");
	if (v != nullptr)
		TicDup = clamp<int>(atoi(v), 1, MAXTICDUP);

	v = Args->CheckValue("-port");
	if (v != nullptr)
	{
		GamePort = atoi(v);
		Printf("Using alternate port %d\n", GamePort);
	}

	v = Args->CheckValue("-netmode");
	if (v != nullptr)
		NetMode = atoi(v) ? NET_PacketServer : NET_PeerToPeer;

	net_password = Args->CheckValue("-password");

	// parse network game options,
	//		player 1: -host <numplayers>
	//		player x: -join <player 1's address>
	int arg = -1;
	if ((arg = Args->CheckParm("-host")))
	{
		if (!HostGame(arg + 1, v != nullptr))
			return false;
	}
	else if ((arg = Args->CheckParm("-join")))
	{
		if (!JoinGame(arg + 1))
			return false;
	}
	else
	{
		// single player game
		TicDup = 1;
		consoleplayer = 0;
		NetworkClients += 0;
		Connected[0].Status = CSTAT_READY;
		Net_SetupUserInfo();
	}

	bGameStarted = true;
	return true;
}

#ifdef __WIN32__
const char* neterror()
{
	static char neterr[16];
	int			code;

	switch (code = WSAGetLastError()) {
		case WSAEACCES:				return "EACCES";
		case WSAEADDRINUSE:			return "EADDRINUSE";
		case WSAEADDRNOTAVAIL:		return "EADDRNOTAVAIL";
		case WSAEAFNOSUPPORT:		return "EAFNOSUPPORT";
		case WSAEALREADY:			return "EALREADY";
		case WSAECONNABORTED:		return "ECONNABORTED";
		case WSAECONNREFUSED:		return "ECONNREFUSED";
		case WSAECONNRESET:			return "ECONNRESET";
		case WSAEDESTADDRREQ:		return "EDESTADDRREQ";
		case WSAEFAULT:				return "EFAULT";
		case WSAEHOSTDOWN:			return "EHOSTDOWN";
		case WSAEHOSTUNREACH:		return "EHOSTUNREACH";
		case WSAEINPROGRESS:		return "EINPROGRESS";
		case WSAEINTR:				return "EINTR";
		case WSAEINVAL:				return "EINVAL";
		case WSAEISCONN:			return "EISCONN";
		case WSAEMFILE:				return "EMFILE";
		case WSAEMSGSIZE:			return "EMSGSIZE";
		case WSAENETDOWN:			return "ENETDOWN";
		case WSAENETRESET:			return "ENETRESET";
		case WSAENETUNREACH:		return "ENETUNREACH";
		case WSAENOBUFS:			return "ENOBUFS";
		case WSAENOPROTOOPT:		return "ENOPROTOOPT";
		case WSAENOTCONN:			return "ENOTCONN";
		case WSAENOTSOCK:			return "ENOTSOCK";
		case WSAEOPNOTSUPP:			return "EOPNOTSUPP";
		case WSAEPFNOSUPPORT:		return "EPFNOSUPPORT";
		case WSAEPROCLIM:			return "EPROCLIM";
		case WSAEPROTONOSUPPORT:	return "EPROTONOSUPPORT";
		case WSAEPROTOTYPE:			return "EPROTOTYPE";
		case WSAESHUTDOWN:			return "ESHUTDOWN";
		case WSAESOCKTNOSUPPORT:	return "ESOCKTNOSUPPORT";
		case WSAETIMEDOUT:			return "ETIMEDOUT";
		case WSAEWOULDBLOCK:		return "EWOULDBLOCK";
		case WSAHOST_NOT_FOUND:		return "HOST_NOT_FOUND";
		case WSANOTINITIALISED:		return "NOTINITIALISED";
		case WSANO_DATA:			return "NO_DATA";
		case WSANO_RECOVERY:		return "NO_RECOVERY";
		case WSASYSNOTREADY:		return "SYSNOTREADY";
		case WSATRY_AGAIN:			return "TRY_AGAIN";
		case WSAVERNOTSUPPORTED:	return "VERNOTSUPPORTED";
		case WSAEDISCON:			return "EDISCON";

		default:
			mysnprintf(neterr, countof(neterr), "%d", code);
			return neterr;
	}
}
#endif
