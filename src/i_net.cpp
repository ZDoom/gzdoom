// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id: i_net.c,v 1.2 1997/12/29 19:50:54 pekangas Exp $
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
#include <stdio.h>

/* [Petteri] Use Winsock for Win32: */
#ifdef __WIN32__
#	define WIN32_LEAN_AND_MEAN
#	include <windows.h>
#	include <winsock.h>
#define USE_WINDOWS_DWORD
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

#include "doomtype.h"
#include "i_system.h"
#include "d_event.h"
#include "d_net.h"
#include "m_argv.h"
#include "m_swap.h"
#include "m_crc32.h"
#include "d_player.h"
#include "templates.h"
#include "c_console.h"
#include "st_start.h"
#include "m_misc.h"
#include "doomstat.h"

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

//
// NETWORKING
//

static u_short DOOMPORT = (IPPORT_USERRESERVED + 29);
static SOCKET mysocket = INVALID_SOCKET;
static sockaddr_in sendaddress[MAXNETNODES];
static BYTE sendplayer[MAXNETNODES];

#ifdef __WIN32__
const char *neterror (void);
#else
#define neterror() strerror(errno)
#endif

enum
{
	PRE_CONNECT,			// Sent from guest to host for initial connection
	PRE_DISCONNECT,			// Sent from guest that aborts the game
	PRE_ALLHERE,			// Sent from host to guest when everybody has connected
	PRE_CONACK,				// Sent from host to guest to acknowledge PRE_CONNECT receipt
	PRE_ALLFULL,			// Sent from host to an unwanted guest
	PRE_ALLHEREACK,			// Sent from guest to host to acknowledge PRE_ALLHEREACK receipt
	PRE_GO					// Sent from host to guest to continue game startup
};

// Set PreGamePacket.fake to this so that the game rejects any pregame packets
// after it starts. This translates to NCMD_SETUP|NCMD_MULTI.
#define PRE_FAKE 0x30

struct PreGamePacket
{
	BYTE Fake;
	BYTE Message;
	BYTE NumNodes;
	union
	{
		BYTE ConsoleNum;
		BYTE NumPresent;
	};
	struct
	{
		u_long	address;
		u_short	port;
		BYTE	player;
		BYTE	pad;
	} machines[MAXNETNODES];
};

BYTE TransmitBuffer[TRANSMIT_SIZE];

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

int FindNode (const sockaddr_in *address)
{
	int i;

	// find remote node number
	for (i = 0; i<doomcom.numnodes; i++)
		if (address->sin_addr.s_addr == sendaddress[i].sin_addr.s_addr
			&& address->sin_port == sendaddress[i].sin_port)
			break;

	if (i == doomcom.numnodes)
	{
		// packet is not from one of the players (new game broadcast?)
		i = -1;
	}
	return i;
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

	uLong size = TRANSMIT_SIZE - 1;
	if (doomcom.datalength >= 10)
	{
		assert(!(doomcom.data[0] & NCMD_COMPRESSED));
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
			0, (sockaddr *)&sendaddress[doomcom.remotenode],
			sizeof(sendaddress[doomcom.remotenode]));
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
				0, (sockaddr *)&sendaddress[doomcom.remotenode],
				sizeof(sendaddress[doomcom.remotenode]));
		}
	}
	//	if (c == -1)
	//			I_Error ("SendPacket error: %s",strerror(errno));
}


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

			if (StartScreen != NULL)
			{
				StartScreen->NetMessage ("The connection from %s was dropped.\n",
					players[sendplayer[node]].userinfo.GetName());
			}
			else
			{
				Printf("The connection from %s was dropped.\n",
					players[sendplayer[node]].userinfo.GetName());
			}

			doomcom.data[0] = 0x80;	// NCMD_EXIT
			c = 1;
		}
		else if (err != WSAEWOULDBLOCK)
		{
			I_Error ("GetPacket: %s", neterror ());
		}
		else
		{
			doomcom.remotenode = -1;		// no packet
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
				doomcom.remotenode = -1;
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
		Printf("Dropped packet: Unknown host (%s:%d)\n", inet_ntoa(fromaddress.sin_addr), fromaddress.sin_port);
		doomcom.remotenode = -1;
		return;
	}

	doomcom.remotenode = node;
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
		I_Error ("PreGet: %s", neterror ());
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
		address->sin_addr.s_addr = inet_addr (target);
		Printf ("Node number %d, address %s\n", doomcom.numnodes, target.GetChars());
	}
	else
	{
		hostentry = gethostbyname (target);
		if (!hostentry)
			I_FatalError ("gethostbyname: couldn't find %s\n%s", target.GetChars(), neterror());
		address->sin_addr.s_addr = *(int *)hostentry->h_addr_list[0];
		Printf ("Node number %d, hostname %s\n",
			doomcom.numnodes, hostentry->h_name);
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

	atterm (CloseNetwork);

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

void SendAbort (void)
{
	BYTE dis[2] = { PRE_FAKE, PRE_DISCONNECT };
	int i, j;

	if (doomcom.numnodes > 1)
	{
		if (consoleplayer == 0)
		{
			// The host needs to let everyone know
			for (i = 1; i < doomcom.numnodes; ++i)
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
				PreSend (dis, 2, &sendaddress[1]);
			}
		}
	}
}

static void SendConAck (int num_connected, int num_needed)
{
	PreGamePacket packet;

	packet.Fake = PRE_FAKE;
	packet.Message = PRE_CONACK;
	packet.NumNodes = num_needed;
	packet.NumPresent = num_connected;
	for (int node = 1; node < doomcom.numnodes; ++node)
	{
		PreSend (&packet, 4, &sendaddress[node]);
	}
	StartScreen->NetProgress (doomcom.numnodes);
}

bool Host_CheckForConnects (void *userdata)
{
	PreGamePacket packet;
	int numplayers = (int)(intptr_t)userdata;
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
			if (doomcom.numnodes == numplayers)
			{
				if (node == -1)
				{
					const BYTE *s_addr_bytes = (const BYTE *)&from->sin_addr;
					StartScreen->NetMessage ("Got extra connect from %d.%d.%d.%d:%d",
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
					node = doomcom.numnodes++;
					sendaddress[node] = *from;
					StartScreen->NetMessage ("Got connect from node %d.", node);
				}

				// Let the new guest (and everyone else) know we got their message.
				SendConAck (doomcom.numnodes, numplayers);
			}
			break;

		case PRE_DISCONNECT:
			node = FindNode (from);
			if (node >= 0)
			{
				StartScreen->NetMessage ("Got disconnect from node %d.", node);
				doomcom.numnodes--;
				while (node < doomcom.numnodes)
				{
					sendaddress[node] = sendaddress[node+1];
					node++;
				}

				// Let remaining guests know that somebody left.
				SendConAck (doomcom.numnodes, numplayers);
			}
			break;
		}
	}
	if (doomcom.numnodes < numplayers)
	{
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
				doomcom.numnodes--;
				while (node < doomcom.numnodes)
				{
					sendaddress[node] = sendaddress[node+1];
					node++;
				}
				// Let remaining guests know that somebody left.
				SendConAck (doomcom.numnodes, numplayers);
			}
			break;
		}
	}
	return doomcom.numnodes >= numplayers;
}

bool Host_SendAllHere (void *userdata)
{
	int *gotack = (int *)userdata;	// ackcount is at gotack[MAXNETNODES]
	PreGamePacket packet;
	int node;
	sockaddr_in *from;

	// Send out address information to all guests. Guests that have already
	// acknowledged receipt effectively get just a heartbeat packet.
	packet.Fake = PRE_FAKE;
	packet.Message = PRE_ALLHERE;
	for (node = 1; node < doomcom.numnodes; node++)
	{
		int machine, spot = 0;

		packet.ConsoleNum = node;
		if (!gotack[node])
		{
			for (spot = 0, machine = 1; machine < doomcom.numnodes; machine++)
			{
				if (node != machine)
				{
					packet.machines[spot].address = sendaddress[machine].sin_addr.s_addr;
					packet.machines[spot].port = sendaddress[machine].sin_port;
					packet.machines[spot].player = node;

					spot++;	// fixes problem of new address replacing existing address in
							// array; it's supposed to increment the index before getting
							// and storing in the packet the next address.
				}
			}
			packet.NumNodes = doomcom.numnodes - 2;
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
		if (packet.Fake == PRE_FAKE && packet.Message == PRE_ALLHEREACK)
		{
			node = FindNode (from);
			if (node >= 0)
			{
				if (!gotack[node])
				{
					gotack[node] = true;
					gotack[MAXNETNODES]++;
				}
			}
			PreSend (&packet, 2, from);
		}
	}

	// If everybody has replied, then this loop can end.
	return gotack[MAXNETNODES] == doomcom.numnodes - 1;
}

void HostGame (int i)
{
	PreGamePacket packet;
	int numplayers;
	int node;
	int gotack[MAXNETNODES+1];

	if ((i == Args->NumArgs() - 1) || !(numplayers = atoi (Args->GetArg(i+1))))
	{	// No player count specified, assume 2
		numplayers = 2;
	}

	if (numplayers == 1)
	{ // Special case: Only 1 player, so don't bother starting the network
		netgame = false;
		multiplayer = true;
		doomcom.id = DOOMCOM_ID;
		doomcom.numplayers = doomcom.numnodes = 1;
		doomcom.consoleplayer = 0;
		return;
	}

	StartNetwork (false);

	// [JC] - this computer is starting the game, therefore it should
	// be the Net Arbitrator.
	doomcom.consoleplayer = 0;
	Printf ("Console player number: %d\n", doomcom.consoleplayer);

	doomcom.numnodes = 1;

	atterm (SendAbort);

	StartScreen->NetInit ("Waiting for players", numplayers);

	// Wait for numplayers-1 different connections
	if (!StartScreen->NetLoop (Host_CheckForConnects, (void *)(intptr_t)numplayers))
	{
		exit (0);
	}

	// Now inform everyone of all machines involved in the game
	memset (gotack, 0, sizeof(gotack));
	StartScreen->NetMessage ("Sending all here.");
	StartScreen->NetInit ("Done waiting", 1);

	if (!StartScreen->NetLoop (Host_SendAllHere, (void *)gotack))
	{
		exit (0);
	}

	popterm ();

	// Now go
	StartScreen->NetMessage ("Go");
	packet.Fake = PRE_FAKE;
	packet.Message = PRE_GO;
	for (node = 1; node < doomcom.numnodes; node++)
	{
		// If we send the packets eight times to each guest,
		// hopefully at least one of them will get through.
		for (int i = 8; i != 0; --i)
		{
			PreSend (&packet, 2, &sendaddress[node]);
		}
	}

	StartScreen->NetMessage ("Total players: %d", doomcom.numnodes);

	doomcom.id = DOOMCOM_ID;
	doomcom.numplayers = doomcom.numnodes;

	// On the host, each player's number is the same as its node number
	for (i = 0; i < doomcom.numnodes; ++i)
	{
		sendplayer[i] = i;
	}
}

// This routine is used by a guest to notify the host of its presence.
// Once that host acknowledges receipt of the notification, this routine
// is never called again.

bool Guest_ContactHost (void *userdata)
{
	sockaddr_in *from;
	PreGamePacket packet;

	// Let the host know we are here.
	packet.Fake = PRE_FAKE;
	packet.Message = PRE_CONNECT;
	PreSend (&packet, 2, &sendaddress[1]);

	// Listen for a reply.
	while ( (from = PreGet (&packet, sizeof(packet), true)) )
	{
		if (packet.Fake == PRE_FAKE && FindNode(from) == 1)
		{
			if (packet.Message == PRE_CONACK)
			{
				StartScreen->NetMessage ("Total players: %d", packet.NumNodes);
				StartScreen->NetInit ("Waiting for other players", packet.NumNodes);
				StartScreen->NetProgress (packet.NumPresent);
				return true;
			}
			else if (packet.Message == PRE_DISCONNECT)
			{
				doomcom.numnodes = 0;
				I_FatalError ("The host cancelled the game.");
			}
			else if (packet.Message == PRE_ALLFULL)
			{
				doomcom.numnodes = 0;
				I_FatalError ("The game is full.");
			}
		}
	}

	// In case the progress bar could not be marqueed, bump it.
	StartScreen->NetProgress (0);

	return false;
}

bool Guest_WaitForOthers (void *userdata)
{
	sockaddr_in *from;
	PreGamePacket packet;

	while ( (from = PreGet (&packet, sizeof(packet), false)) )
	{
		if (packet.Fake != PRE_FAKE || FindNode(from) != 1)
		{
			continue;
		}
		switch (packet.Message)
		{
		case PRE_CONACK:
			StartScreen->NetProgress (packet.NumPresent);
			break;

		case PRE_ALLHERE:
			if (doomcom.numnodes == 2)
			{
				int node;

				packet.NumNodes = packet.NumNodes;
				doomcom.numnodes = packet.NumNodes + 2;
				sendplayer[0] = packet.ConsoleNum;	// My player number
				doomcom.consoleplayer = packet.ConsoleNum;
				StartScreen->NetMessage ("Console player number: %d", doomcom.consoleplayer);
				for (node = 0; node < packet.NumNodes; node++)
				{
					sendaddress[node+2].sin_addr.s_addr = packet.machines[node].address;
					sendaddress[node+2].sin_port = packet.machines[node].port;
					sendplayer[node+2] = packet.machines[node].player;

					// [JC] - fixes problem of games not starting due to
					// no address family being assigned to nodes stored in
					// sendaddress[] from the All Here packet.
					sendaddress[node+2].sin_family = AF_INET;
				}
			}

			StartScreen->NetMessage ("Received All Here, sending ACK.");
			packet.Fake = PRE_FAKE;
			packet.Message = PRE_ALLHEREACK;
			PreSend (&packet, 2, &sendaddress[1]);
			break;

		case PRE_GO:
			StartScreen->NetMessage ("Received \"Go.\"");
			return true;

		case PRE_DISCONNECT:
			I_FatalError ("The host cancelled the game.");
			break;
		}
	}

	return false;
}

void JoinGame (int i)
{
	if ((i == Args->NumArgs() - 1) ||
		(Args->GetArg(i+1)[0] == '-') ||
		(Args->GetArg(i+1)[0] == '+'))
		I_FatalError ("You need to specify the host machine's address");

	StartNetwork (true);

	// Host is always node 1
	BuildAddress (&sendaddress[1], Args->GetArg(i+1));
	sendplayer[1] = 0;
	doomcom.numnodes = 2;

	atterm (SendAbort);

	// Let host know we are here
	StartScreen->NetInit ("Contacting host", 0);

	if (!StartScreen->NetLoop (Guest_ContactHost, NULL))
	{
		exit (0);
	}

	// Wait for everyone else to connect
	if (!StartScreen->NetLoop (Guest_WaitForOthers, 0))
	{
		exit (0);
	}

	popterm ();

	StartScreen->NetMessage ("Total players: %d", doomcom.numnodes);

	doomcom.id = DOOMCOM_ID;
	doomcom.numplayers = doomcom.numnodes;
}

static int PrivateNetOf(in_addr in)
{
	int addr = ntohl(in.s_addr);
		 if ((addr & 0xFFFF0000) == 0xC0A80000)		// 192.168.0.0
	{
		return 0xC0A80000;
	}
	else if ((addr & 0xFFF00000) == 0xAC100000)		// 172.16.0.0
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
	int net1;

	net1 = PrivateNetOf(sendaddress[1].sin_addr);
//	Printf("net1 = %08x\n", net1);
	if (net1 == 0)
	{
		return false;
	}
	for (int i = 2; i < doomcom.numnodes; ++i)
	{
		int net = PrivateNetOf(sendaddress[i].sin_addr);
//		Printf("Net[%d] = %08x\n", i, net);
		if (net != net1)
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
bool I_InitNetwork (void)
{
	int i;
	const char *v;

	memset (&doomcom, 0, sizeof(doomcom));

	// set up for network
	v = Args->CheckValue ("-dup");
	if (v)
	{
		doomcom.ticdup = clamp (atoi (v), 1, MAXTICDUP);
	}
	else
	{
		doomcom.ticdup = 1;
	}

	if (Args->CheckParm ("-extratic"))
		doomcom.extratics = 1;
	else
		doomcom.extratics = 0;

	v = Args->CheckValue ("-port");
	if (v)
	{
		DOOMPORT = atoi (v);
		Printf ("using alternate port %i\n", DOOMPORT);
	}

	// parse network game options,
	//		player 1: -host <numplayers>
	//		player x: -join <player 1's address>
	if ( (i = Args->CheckParm ("-host")) )
	{
		HostGame (i);
	}
	else if ( (i = Args->CheckParm ("-join")) )
	{
		JoinGame (i);
	}
	else
	{
		// single player game
		netgame = false;
		multiplayer = false;
		doomcom.id = DOOMCOM_ID;
		doomcom.numplayers = doomcom.numnodes = 1;
		doomcom.consoleplayer = 0;
		return false;
	}
	if (doomcom.numnodes < 3)
	{ // Packet server mode with only two players is effectively the same as
	  // peer-to-peer but with some slightly larger packets.
		return false;
	}
	return doomcom.numnodes > 3 || !NodesOnSameNetwork();
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
