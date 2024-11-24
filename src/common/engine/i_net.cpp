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


#include "i_net.h"

// As per http://support.microsoft.com/kb/q192599/ the standard
// size for network buffers is 8k.
#define TRANSMIT_SIZE		8000

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
#endif

constexpr int MaxPlayers = 16; // TODO: This needs to be put in some kind of unified header later
constexpr size_t MaxPasswordSize = 256u;

bool netgame, multiplayer;
int consoleplayer; // i.e. myconnectindex in Build. 
doomcom_t doomcom;

CUSTOM_CVAR(String, net_password, "", CVAR_IGNORE)
{
	if (strlen(self) + 1 > MaxPasswordSize)
	{
		self = "";
		Printf(TEXTCOLOR_RED "Password cannot be greater than 255 characters\n");
	}
}

FClientStack NetworkClients;

//
// NETWORKING
//

static u_short DOOMPORT = (IPPORT_USERRESERVED + 29);
static SOCKET mysocket = INVALID_SOCKET;
static sockaddr_in sendaddress[MaxPlayers];

#ifdef __WIN32__
const char *neterror (void);
#else
#define neterror() strerror(errno)
#endif

enum
{
	PRE_CONNECT,			// Sent from guest to host for initial connection
	PRE_KEEPALIVE,
	PRE_DISCONNECT,			// Sent from guest that aborts the game
	PRE_ALLHERE,			// Sent from host to guest when everybody has connected
	PRE_CONACK,				// Sent from host to guest to acknowledge PRE_CONNECT receipt
	PRE_ALLFULL,			// Sent from host to an unwanted guest
	PRE_ALLHEREACK,			// Sent from guest to host to acknowledge PRE_ALLHEREACK receipt
	PRE_GO,					// Sent from host to guest to continue game startup
	PRE_IN_PROGRESS,		// Sent from host to guest if the game has already started
	PRE_WRONG_PASSWORD,		// Sent from host to guest if their provided password was wrong
};

// Set PreGamePacket.fake to this so that the game rejects any pregame packets
// after it starts. This translates to NCMD_SETUP|NCMD_MULTI.
#define PRE_FAKE 0x30

struct PreGameConnectPacket
{
	uint8_t Fake;
	uint8_t Message;
	char Password[MaxPasswordSize];
};

struct PreGamePacket
{
	uint8_t Fake;
	uint8_t Message;
	uint8_t NumNodes;
	union
	{
		uint8_t ConsoleNum;
		uint8_t NumPresent;
	};
	struct
	{
		uint32_t address;
		uint16_t port;
		uint16_t pad;
	} machines[MaxPlayers];
};

uint8_t TransmitBuffer[TRANSMIT_SIZE];

FString GetPlayerName(int num)
{
	if (sysCallbacks.GetPlayerName) return sysCallbacks.GetPlayerName(num);
	else return FStringf("Player %d", num + 1);
}

//
// UDPsocket
//
SOCKET UDPsocket (void)
{
	SOCKET s;

	// allocate a socket
	s = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP);
	if (s == INVALID_SOCKET)
		I_FatalError ("can't create socket: %s", neterror ());

	return s;
}

//
// BindToLocalPort
//
void BindToLocalPort (SOCKET s, u_short port)
{
	int v;
	sockaddr_in address;

	memset (&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);

	v = bind (s, (sockaddr *)&address, sizeof(address));
	if (v == SOCKET_ERROR)
		I_FatalError ("BindToPort: %s", neterror ());
}

void I_ClearNode(int node)
{
	memset(&sendaddress[node], 0, sizeof(sendaddress[node]));
}

static bool I_ShouldStartNetGame()
{
	if (doomcom.consoleplayer != 0)
		return false;

	return StartWindow->ShouldStartNet();
}

int FindNode (const sockaddr_in *address)
{
	int i = 0;

	// find remote node number
	for (; i < doomcom.numplayers; ++i)
	{
		if (address->sin_addr.s_addr == sendaddress[i].sin_addr.s_addr
			&& address->sin_port == sendaddress[i].sin_port)
		{
			break;
		}
	}

	return (i == doomcom.numplayers) ? -1 : i;
}

//
// PacketSend
//
void PacketSend (void)
{
	int c;

	// FIXME: Catch this before we've overflown the buffer. With long chat
	// text and lots of backup tics, it could conceivably happen. (Though
	// apparently it hasn't yet, which is good.)
	if (doomcom.datalength > MAX_MSGLEN)
	{
		I_FatalError("Netbuffer overflow!");
	}
	assert(!(doomcom.data[0] & NCMD_COMPRESSED));

	uLong size = TRANSMIT_SIZE - 1;
	if (doomcom.datalength >= 10)
	{
		TransmitBuffer[0] = doomcom.data[0] | NCMD_COMPRESSED;
		c = compress2(TransmitBuffer + 1, &size, doomcom.data + 1, doomcom.datalength - 1, 9);
		size += 1;
	}
	else
	{
		c = -1;	// Just some random error code to avoid sending the compressed buffer.
	}
	if (c == Z_OK && size < (uLong)doomcom.datalength)
	{
//		Printf("send %lu/%d\n", size, doomcom.datalength);
		c = sendto(mysocket, (char *)TransmitBuffer, size,
			0, (sockaddr *)&sendaddress[doomcom.remoteplayer],
			sizeof(sendaddress[doomcom.remoteplayer]));
	}
	else
	{
		if (doomcom.datalength > TRANSMIT_SIZE)
		{
			I_Error("Net compression failed (zlib error %d)", c);
		}
		else
		{
//			Printf("send %d\n", doomcom.datalength);
			c = sendto(mysocket, (char *)doomcom.data, doomcom.datalength,
				0, (sockaddr *)&sendaddress[doomcom.remoteplayer],
				sizeof(sendaddress[doomcom.remoteplayer]));
		}
	}
	//	if (c == -1)
	//			I_Error ("SendPacket error: %s",strerror(errno));
}

void PreSend(const void* buffer, int bufferlen, const sockaddr_in* to);
void SendConAck(int num_connected, int num_needed);

//
// PacketGet
//
void PacketGet (void)
{
	int c;
	socklen_t fromlen;
	sockaddr_in fromaddress;
	int node;

	fromlen = sizeof(fromaddress);
	c = recvfrom (mysocket, (char*)TransmitBuffer, TRANSMIT_SIZE, 0,
				  (sockaddr *)&fromaddress, &fromlen);
	node = FindNode (&fromaddress);

	if (node >= 0 && c == SOCKET_ERROR)
	{
		int err = WSAGetLastError();

		if (err == WSAECONNRESET)
		{ // The remote node aborted unexpectedly, so pretend it sent an exit packet

			if (StartWindow != NULL)
			{
				I_NetMessage ("The connection from %s was dropped.\n",
					GetPlayerName(node).GetChars());
			}
			else
			{
				Printf("The connection from %s was dropped.\n",
					GetPlayerName(node).GetChars());
			}

			doomcom.data[0] = NCMD_EXIT;
			c = 1;
		}
		else if (err != WSAEWOULDBLOCK)
		{
			I_Error ("GetPacket: %s", neterror ());
		}
		else
		{
			doomcom.remoteplayer = -1;		// no packet
			return;
		}
	}
	else if (node >= 0 && c > 0)
	{
		doomcom.data[0] = TransmitBuffer[0] & ~NCMD_COMPRESSED;
		if (TransmitBuffer[0] & NCMD_COMPRESSED)
		{
			uLongf msgsize = MAX_MSGLEN - 1;
			int err = uncompress(doomcom.data + 1, &msgsize, TransmitBuffer + 1, c - 1);
//			Printf("recv %d/%lu\n", c, msgsize + 1);
			if (err != Z_OK)
			{
				Printf("Net decompression failed (zlib error %s)\n", M_ZLibError(err).GetChars());
				// Pretend no packet
				doomcom.remoteplayer = -1;
				return;
			}
			c = msgsize + 1;
		}
		else
		{
//			Printf("recv %d\n", c);
			memcpy(doomcom.data + 1, TransmitBuffer + 1, c - 1);
		}
	}
	else if (c > 0)
	{	//The packet is not from any in-game node, so we might as well discard it.
		if (TransmitBuffer[0] == PRE_FAKE)
		{
			// If it's someone waiting in the lobby, let them know the game already started
			uint8_t msg[] = { PRE_FAKE, PRE_IN_PROGRESS };
			PreSend(msg, 2, &fromaddress);
		}
		doomcom.remoteplayer = -1;
		return;
	}

	doomcom.remoteplayer = node;
	doomcom.datalength = (short)c;
}

sockaddr_in *PreGet (void *buffer, int bufferlen, bool noabort)
{
	static sockaddr_in fromaddress;
	socklen_t fromlen;
	int c;

	fromlen = sizeof(fromaddress);
	c = recvfrom (mysocket, (char *)buffer, bufferlen, 0,
		(sockaddr *)&fromaddress, &fromlen);

	if (c == SOCKET_ERROR)
	{
		int err = WSAGetLastError();
		if (err == WSAEWOULDBLOCK || (noabort && err == WSAECONNRESET))
			return NULL;	// no packet
	}
	return &fromaddress;
}

void PreSend (const void *buffer, int bufferlen, const sockaddr_in *to)
{
	sendto (mysocket, (const char *)buffer, bufferlen, 0, (const sockaddr *)to, sizeof(*to));
}

void BuildAddress (sockaddr_in *address, const char *name)
{
	hostent *hostentry;		// host information entry
	u_short port;
	const char *portpart;
	bool isnamed = false;
	int curchar;
	char c;
	FString target;

	address->sin_family = AF_INET;

	if ( (portpart = strchr (name, ':')) )
	{
		target = FString(name, portpart - name);
		port = atoi (portpart + 1);
		if (!port)
		{
			Printf ("Weird port: %s (using %d)\n", portpart + 1, DOOMPORT);
			port = DOOMPORT;
		}
	}
	else
	{
		target = name;
		port = DOOMPORT;
	}
	address->sin_port = htons(port);

	for (curchar = 0; (c = target[curchar]) ; curchar++)
	{
		if ((c < '0' || c > '9') && c != '.')
		{
			isnamed = true;
			break;
		}
	}

	if (!isnamed)
	{
		address->sin_addr.s_addr = inet_addr (target.GetChars());
		Printf ("Node number %d, address %s\n", doomcom.numplayers, target.GetChars());
	}
	else
	{
		hostentry = gethostbyname (target.GetChars());
		if (!hostentry)
			I_FatalError ("gethostbyname: couldn't find %s\n%s", target.GetChars(), neterror());
		address->sin_addr.s_addr = *(int *)hostentry->h_addr_list[0];
		Printf ("Node number %d, hostname %s\n",
			doomcom.numplayers, hostentry->h_name);
	}
}

void CloseNetwork (void)
{
	if (mysocket != INVALID_SOCKET)
	{
		closesocket (mysocket);
		mysocket = INVALID_SOCKET;
	}
#ifdef __WIN32__
	WSACleanup ();
#endif
}

void StartNetwork (bool autoPort)
{
	u_long trueval = 1;
#ifdef __WIN32__
	WSADATA wsad;

	if (WSAStartup (0x0101, &wsad))
	{
		I_FatalError ("Could not initialize Windows Sockets");
	}
#endif

	netgame = true;
	multiplayer = true;

	// create communication socket
	mysocket = UDPsocket ();
	BindToLocalPort (mysocket, autoPort ? 0 : DOOMPORT);
#ifndef __sun
	ioctlsocket (mysocket, FIONBIO, &trueval);
#else
	fcntl(mysocket, F_SETFL, trueval | O_NONBLOCK);
#endif
}

void SendAbort (int connected)
{
	uint8_t dis[2] = { PRE_FAKE, PRE_DISCONNECT };
	int i, j;

	if (connected > 1)
	{
		if (doomcom.consoleplayer == 0)
		{
			// The host needs to let everyone know
			for (i = 1; i < connected; ++i)
			{
				for (j = 4; j > 0; --j)
				{
					PreSend (dis, 2, &sendaddress[i]);
				}
			}
		}
		else
		{
			// Guests only need to let the host know.
			for (i = 4; i > 0; --i)
			{
				PreSend (dis, 2, &sendaddress[0]);
			}
		}
	}
}

void SendConAck (int num_connected, int num_needed)
{
	PreGamePacket packet;

	packet.Fake = PRE_FAKE;
	packet.Message = PRE_CONACK;
	packet.NumNodes = num_needed;
	packet.NumPresent = num_connected;
	for (int node = 1; node < num_connected; ++node)
	{
		PreSend (&packet, 4, &sendaddress[node]);
	}
	I_NetProgress (num_connected);
}

bool Host_CheckForConnects (void *userdata)
{
	PreGameConnectPacket packet;
	int* connectedPlayers = (int*)userdata;
	sockaddr_in *from;
	int node;

	while ( (from = PreGet (&packet, sizeof(packet), false)) )
	{
		if (packet.Fake != PRE_FAKE)
		{
			continue;
		}
		switch (packet.Message)
		{
		case PRE_CONNECT:
			node = FindNode (from);
			if (doomcom.numplayers == *connectedPlayers)
			{
				if (node == -1)
				{
					const uint8_t *s_addr_bytes = (const uint8_t *)&from->sin_addr;
					I_NetMessage ("Got extra connect from %d.%d.%d.%d:%d",
						s_addr_bytes[0], s_addr_bytes[1], s_addr_bytes[2], s_addr_bytes[3],
						from->sin_port);
					packet.Message = PRE_ALLFULL;
					PreSend (&packet, 2, from);
				}
			}
			else
			{
				if (node == -1)
				{
					if (strlen(net_password) > 0 && strcmp(net_password, packet.Password))
					{
						packet.Message = PRE_WRONG_PASSWORD;
						PreSend(&packet, 2, from);
						break;
					}

					node = *connectedPlayers;
					++*connectedPlayers;
					sendaddress[node] = *from;
					I_NetMessage ("Got connect from node %d.", node);
				}

				// Let the new guest (and everyone else) know we got their message.
				SendConAck (*connectedPlayers, doomcom.numplayers);
			}
			break;

		case PRE_DISCONNECT:
			node = FindNode (from);
			if (node >= 0)
			{
				I_ClearNode(node);
				I_NetMessage ("Got disconnect from node %d.", node);
				--*connectedPlayers;
				while (node < *connectedPlayers)
				{
					sendaddress[node] = sendaddress[node+1];
					++node;
				}

				// Let remaining guests know that somebody left.
				SendConAck (*connectedPlayers, doomcom.numplayers);
			}
			break;
		}
	}
	if (*connectedPlayers < doomcom.numplayers && !I_ShouldStartNetGame())
	{
		// Send message to everyone as a keepalive
		SendConAck(*connectedPlayers, doomcom.numplayers);
		return false;
	}

	// It's possible somebody bailed out after all players were found.
	// Unfortunately, this isn't guaranteed to catch all of them.
	// Oh well. Better than nothing.
	while ( (from = PreGet (&packet, sizeof(packet), false)) )
	{
		if (packet.Fake == PRE_FAKE && packet.Message == PRE_DISCONNECT)
		{
			node = FindNode (from);
			if (node >= 0)
			{
				I_ClearNode(node);
				--*connectedPlayers;
				while (node < *connectedPlayers)
				{
					sendaddress[node] = sendaddress[node+1];
					++node;
				}
				// Let remaining guests know that somebody left.
				SendConAck (*connectedPlayers, doomcom.numplayers);
			}
			break;
		}
	}

	// TODO: This will need a much better solution later.
	if (I_ShouldStartNetGame())
		doomcom.numplayers = *connectedPlayers;

	return *connectedPlayers >= doomcom.numplayers;
}

bool Host_SendAllHere (void *userdata)
{
	int* gotack = (int*)userdata;
	const int mask = (1 << doomcom.numplayers) - 1;
	PreGamePacket packet;
	int node;
	sockaddr_in *from;

	// Send out address information to all guests. This is will be the final order
	// of the send addresses so each guest will need to rearrange their own in order
	// to keep node -> player numbers synchronized.
	packet.Fake = PRE_FAKE;
	packet.Message = PRE_ALLHERE;
	for (node = 1; node < doomcom.numplayers; node++)
	{
		int machine, spot = 0;

		packet.ConsoleNum = node;
		if (!((*gotack) & (1 << node)))
		{
			for (spot = 0; spot < doomcom.numplayers; spot++)
			{
				packet.machines[spot].address = sendaddress[spot].sin_addr.s_addr;
				packet.machines[spot].port = sendaddress[spot].sin_port;
			}
			packet.NumNodes = doomcom.numplayers;
		}
		else
		{
			packet.NumNodes = 0;
		}
		PreSend (&packet, 4 + spot*8, &sendaddress[node]);
	}

	// Check for replies.
	while ( (from = PreGet (&packet, sizeof(packet), false)) )
	{
		if (packet.Fake != PRE_FAKE)
			continue;

		if (packet.Message == PRE_ALLHEREACK)
		{
			node = FindNode (from);
			if (node >= 0)
				*gotack |= 1 << node;
		}
		else if (packet.Message == PRE_CONNECT)
		{
			// If someone is still trying to connect, let them know it's either too
			// late or that they need to move on to the next stage.
			node = FindNode(from);
			if (node == -1)
			{
				packet.Message = PRE_ALLFULL;
				PreSend(&packet, 2, from);
			}
			else
			{
				packet.Message = PRE_CONACK;
				packet.NumNodes = packet.NumPresent = doomcom.numplayers;
				PreSend(&packet, 4, from);
			}
		}
	}

	// If everybody has replied, then this loop can end.
	return ((*gotack) & mask) == mask;
}

bool HostGame (int i)
{
	PreGamePacket packet;
	int numplayers;
	int node;

	if ((i == Args->NumArgs() - 1) || !(numplayers = atoi (Args->GetArg(i+1))))
	{	// No player count specified, assume 2
		numplayers = 2;
	}

	if (numplayers > MaxPlayers)
	{
		I_FatalError("You cannot host a game with %d players. The limit is currently %d.", numplayers, MaxPlayers);
	}

	if (numplayers == 1)
	{ // Special case: Only 1 player, so don't bother starting the network
		NetworkClients += 0;
		netgame = false;
		multiplayer = true;
		doomcom.id = DOOMCOM_ID;
		doomcom.numplayers = 1;
		doomcom.consoleplayer = 0;
		return true;
	}

	StartNetwork (false);

	// [JC] - this computer is starting the game, therefore it should
	// be the Net Arbitrator.
	doomcom.consoleplayer = 0;
	doomcom.numplayers = numplayers;

	I_NetInit ("Hosting game", numplayers);

	// Wait for the lobby to be full.
	int connectedPlayers = 1;
	if (!I_NetLoop (Host_CheckForConnects, (void *)&connectedPlayers))
	{
		SendAbort(connectedPlayers);
		throw CExitEvent(0);
		return false;
	}

	// If the player force started with only themselves in the lobby, start the game
	// immediately.
	if (doomcom.numplayers <= 1)
	{
		NetworkClients += 0;
		netgame = false;
		multiplayer = true;
		doomcom.id = DOOMCOM_ID;
		doomcom.numplayers = 1;
		I_NetDone();
		return true;
	}

	// Now inform everyone of all machines involved in the game
	int gotack = 1;
	I_NetMessage ("Sending all here.");
	I_NetInit ("Done waiting", 1);

	if (!I_NetLoop (Host_SendAllHere, (void *)&gotack))
	{
		SendAbort(doomcom.numplayers);
		throw CExitEvent(0);
		return false;
	}

	// Now go
	I_NetMessage ("Go");
	packet.Fake = PRE_FAKE;
	packet.Message = PRE_GO;
	for (node = 1; node < doomcom.numplayers; node++)
	{
		// If we send the packets eight times to each guest,
		// hopefully at least one of them will get through.
		for (int ii = 8; ii != 0; --ii)
		{
			PreSend (&packet, 2, &sendaddress[node]);
		}
	}

	I_NetMessage ("Total players: %d", doomcom.numplayers);

	doomcom.id = DOOMCOM_ID;

	return true;
}

// This routine is used by a guest to notify the host of its presence.
// Once that host acknowledges receipt of the notification, this routine
// is never called again.

bool Guest_ContactHost (void *userdata)
{
	sockaddr_in *from;
	PreGamePacket packet;
	PreGameConnectPacket sendPacket;

	// Let the host know we are here.
	sendPacket.Fake = PRE_FAKE;
	sendPacket.Message = PRE_CONNECT;
	memcpy(sendPacket.Password, net_password, strlen(net_password) + 1);
	PreSend (&sendPacket, sizeof(sendPacket), &sendaddress[0]);

	// Listen for a reply.
	while ( (from = PreGet (&packet, sizeof(packet), true)) )
	{
		if (packet.Fake == PRE_FAKE && !FindNode(from))
		{
			if (packet.Message == PRE_CONACK)
			{
				I_NetMessage ("Total players: %d", packet.NumNodes);
				I_NetInit ("Waiting for other players", packet.NumNodes);
				I_NetProgress (packet.NumPresent);
				return true;
			}
			else if (packet.Message == PRE_DISCONNECT)
			{
				I_NetError("The host cancelled the game.");
			}
			else if (packet.Message == PRE_ALLFULL)
			{
				I_NetError("The game is full.");
			}
			else if (packet.Message == PRE_IN_PROGRESS)
			{
				I_NetError("The game was already started.");
			}
			else if (packet.Message == PRE_WRONG_PASSWORD)
			{
				I_NetError("Invalid password.");
			}
		}
	}

	// In case the progress bar could not be marqueed, bump it.
	I_NetProgress (0);

	return false;
}

bool Guest_WaitForOthers (void *userdata)
{
	sockaddr_in *from;
	PreGamePacket packet;

	while ( (from = PreGet (&packet, sizeof(packet), false)) )
	{
		if (packet.Fake != PRE_FAKE || FindNode(from))
		{
			continue;
		}
		switch (packet.Message)
		{
		case PRE_CONACK:
			I_NetProgress (packet.NumPresent);
			break;

		case PRE_ALLHERE:
			if (doomcom.consoleplayer == -1)
			{
				doomcom.numplayers = packet.NumNodes;
				doomcom.consoleplayer = packet.ConsoleNum;
				// Don't use the address sent from the host for our own machine.
				sendaddress[doomcom.consoleplayer] = sendaddress[1];

				I_NetMessage ("Console player number: %d", doomcom.consoleplayer);

				for (int node = 1; node < doomcom.numplayers; ++node)
				{
					if (node == doomcom.consoleplayer)
						continue;

					sendaddress[node].sin_addr.s_addr = packet.machines[node].address;
					sendaddress[node].sin_port = packet.machines[node].port;

					// [JC] - fixes problem of games not starting due to
					// no address family being assigned to nodes stored in
					// sendaddress[] from the All Here packet.
					sendaddress[node].sin_family = AF_INET;
				}

				I_NetMessage("Received All Here, sending ACK.");
			}

			packet.Fake = PRE_FAKE;
			packet.Message = PRE_ALLHEREACK;
			PreSend (&packet, 2, &sendaddress[0]);
			break;

		case PRE_GO:
			I_NetMessage ("Received \"Go.\"");
			return true;

		case PRE_DISCONNECT:
			I_NetError("The host cancelled the game.");
			break;
		}
	}

	return false;
}

bool JoinGame (int i)
{
	if ((i == Args->NumArgs() - 1) ||
		(Args->GetArg(i+1)[0] == '-') ||
		(Args->GetArg(i+1)[0] == '+'))
		I_FatalError ("You need to specify the host machine's address");

	StartNetwork (true);

	// Host is always node 0
	BuildAddress (&sendaddress[0], Args->GetArg(i+1));
	doomcom.numplayers = 2;
	doomcom.consoleplayer = -1;

	// Let host know we are here
	I_NetInit ("Contacting host", 0);

	if (!I_NetLoop (Guest_ContactHost, nullptr))
	{
		SendAbort(2);
		throw CExitEvent(0);
		return false;
	}

	// Wait for everyone else to connect
	if (!I_NetLoop (Guest_WaitForOthers, nullptr))
	{
		SendAbort(2);
		throw CExitEvent(0);
		return false;
	}

	I_NetMessage ("Total players: %d", doomcom.numplayers);

	doomcom.id = DOOMCOM_ID;
	return true;
}

static int PrivateNetOf(in_addr in)
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

//
// NodesOnSameNetwork
//
// The best I can really do here is check if the others are on the same
// private network, since that means we (probably) are too.
//

static bool NodesOnSameNetwork()
{
	if (doomcom.consoleplayer != 0)
		return false;

	const int firstClient = PrivateNetOf(sendaddress[1].sin_addr);
//	Printf("net1 = %08x\n", net1);
	if (firstClient == 0)
	{
		return false;
	}
	for (int i = 2; i < doomcom.numplayers; ++i)
	{
		const int net = PrivateNetOf(sendaddress[i].sin_addr);
//		Printf("Net[%d] = %08x\n", i, net);
		if (net != firstClient)
		{
			return false;
		}
	}
	return true;
}

//
// I_InitNetwork
//
// Returns true if packet server mode might be a good idea.
//
int I_InitNetwork (void)
{
	int i;
	const char *v;

	memset (&doomcom, 0, sizeof(doomcom));

	// set up for network
	v = Args->CheckValue ("-dup");
	if (v)
	{
		doomcom.ticdup = clamp<int> (atoi (v), 1, MAXTICDUP);
	}
	else
	{
		doomcom.ticdup = 1;
	}

	v = Args->CheckValue ("-port");
	if (v)
	{
		DOOMPORT = atoi (v);
		Printf ("using alternate port %i\n", DOOMPORT);
	}

	net_password = Args->CheckValue("-password");

	// parse network game options,
	//		player 1: -host <numplayers>
	//		player x: -join <player 1's address>
	if ( (i = Args->CheckParm ("-host")) )
	{
		if (!HostGame (i)) return -1;
	}
	else if ( (i = Args->CheckParm ("-join")) )
	{
		if (!JoinGame (i)) return -1;
	}
	else
	{
		// single player game
		NetworkClients += 0;
		netgame = false;
		multiplayer = false;
		doomcom.id = DOOMCOM_ID;
		doomcom.ticdup = 1;
		doomcom.numplayers = 1;
		doomcom.consoleplayer = 0;
		return false;
	}

	if (doomcom.numplayers < 3)
	{ // Packet server mode with only two players is effectively the same as
	  // peer-to-peer but with some slightly larger packets.
		return false;
	}
	return !NodesOnSameNetwork();
}


void I_NetCmd (void)
{
	if (doomcom.command == CMD_SEND)
	{
		PacketSend ();
	}
	else if (doomcom.command == CMD_GET)
	{
		PacketGet ();
	}
	else
		I_Error ("Bad net cmd: %i\n",doomcom.command);
}

void I_NetMessage(const char* text, ...)
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

void I_NetError(const char* error)
{
	doomcom.numplayers = 0;
	StartWindow->NetClose();
	I_FatalError("%s", error);
}

// todo: later these must be dispatched by the main menu, not the start screen.
void I_NetProgress(int val)
{
	StartWindow->NetProgress(val);
}
void I_NetInit(const char* msg, int num)
{
	StartWindow->NetInit(msg, num);
}
bool I_NetLoop(bool (*timer_callback)(void*), void* userdata)
{
	return StartWindow->NetLoop(timer_callback, userdata);
}
void I_NetDone()
{
	StartWindow->NetDone();
}
#ifdef __WIN32__
const char *neterror (void)
{
	static char neterr[16];
	int			code;

	switch (code = WSAGetLastError ()) {
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
			mysnprintf (neterr, countof(neterr), "%d", code);
			return neterr;
	}
}
#endif
