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
#	define __WIN32__
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
#else
#	include <sys/socket.h>
#	include <netinet/in.h>
#	include <arpa/inet.h>
#	include <errno.h>
#	include <unistd.h>
#	include <netdb.h>
#	include <sys/ioctl.h>
#endif

#include "doomtype.h"
#include "i_system.h"
#include "d_event.h"
#include "d_net.h"
#include "m_argv.h"
#include "m_alloc.h"
#include "m_swap.h"

#include "doomstat.h"

#include "i_net.h"



/* [Petteri] Get more portable: */
#ifndef __WIN32__
typedef int SOCKET;
#define SOCKET_ERROR -1
#define INVALID_SOCKET -1
#define closesocket close
#define ioctlsocket ioctl
#endif

#ifdef __WIN32__
#	define IPPORT_USERRESERVED 5000
#endif

extern BOOL CheckAbort (void);

static void	NetSend (void);
static BOOL	NetListen (void);


//
// NETWORKING
//

static u_short DOOMPORT = (IPPORT_USERRESERVED + 0x1d);
static SOCKET mysocket = INVALID_SOCKET;
static struct sockaddr_in sendaddress[MAXNETNODES];

void (*netget) (void);
void (*netsend) (void);

#ifdef __WIN32__
char	*neterror (void);
#else
#define neterror() strerror(errno)
#endif

enum
{
	PRE_CONNECT,
	PRE_DISCONNECT,
	PRE_ALLHERE,
	PRE_CONACK,
	PRE_ALLHEREACK,
	PRE_GO,
	PRE_GOACK
};

struct PreGamePacket
{
	u_short message;
	u_short numnodes;
	struct
	{
		u_long address;
		u_short port;
	} machines[MAXNETNODES];
};

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
	struct sockaddr_in address;
		
	memset (&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = htons(port);
						
	v = bind (s, (sockaddr *)&address, sizeof(address));
	if (v == SOCKET_ERROR)
		I_FatalError ("BindToPort: %s", neterror ());
}

int FindNode (struct sockaddr_in *address)
{
	int i;

	// find remote node number
	for (i = 0; i<doomcom->numnodes; i++)
		if (address->sin_addr.s_addr == sendaddress[i].sin_addr.s_addr
			&& address->sin_port == sendaddress[i].sin_port)
			break;

	if (i == doomcom->numnodes)
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

	//printf ("sending %i\n",gametic);
	c = sendto (mysocket , (const char*)netbuffer, doomcom->datalength
				,0,(sockaddr *)&sendaddress[doomcom->remotenode]
				,sizeof(sendaddress[doomcom->remotenode]));

	//	if (c == -1)
	//			I_Error ("SendPacket error: %s",strerror(errno));
}


//
// PacketGet
//
void PacketGet (void)
{
	int c, fromlen;
	struct sockaddr_in fromaddress;

	fromlen = sizeof(fromaddress);
	c = recvfrom (mysocket, (char*)netbuffer, sizeof(doomdata_t), 0
				  , (struct sockaddr *)&fromaddress, &fromlen);

	if (c == SOCKET_ERROR)
	{
#ifdef __WIN32__
		if (WSAGetLastError() != WSAEWOULDBLOCK)
			I_Error ("GetPacket: %s", neterror ());
#else		 
		if (errno != EWOULDBLOCK)
			I_Error ("GetPacket: %s", neterror ());
#endif		  
		doomcom->remotenode = -1;		// no packet
		return;
	}

	doomcom->remotenode = (short)FindNode (&fromaddress);
	doomcom->datalength = (short)c;
}

/*
struct sockaddr_in *PreGet (void *buffer, int bufferlen)
{
	static struct sockaddr_in fromaddress;
	int fromlen;
	int c;

	fromlen = sizeof(fromaddress);
	c = recvfrom (mysocket, buffer, bufferlen, 0,
		(struct sockaddr *)&fromaddress, &fromlen);

	if (c == SOCKET_ERROR)
	{
#ifdef __WIN32__
		if (WSAGetLastError() != WSAEWOULDBLOCK)
			I_Error ("PreGet: %s", neterror ());
#else
		if (errno != EWOULDBLOCK)
			I_Error ("PreGet: %s", neterror ());
#endif
		return NULL;		// no packet
	}
	return &fromaddress;
}

void PreSend (void *buffer, int bufferlen, struct sockaddr_in *to)
{
	sendto (mysocket, buffer, bufferlen, 0, (void *)to, sizeof(*to));
}
*/

void BuildAddress (struct sockaddr_in *address, char *name)
{
	struct hostent *hostentry;		// host information entry
	u_short port;
	char *portpart;
	BOOL isnamed = false;
	int curchar;
	char c;

	address->sin_family = AF_INET;

	if (portpart = strchr (name, ':'))
	{
		*portpart = 0;
		port = atoi (portpart + 1);
		if (!port)
		{
			Printf (PRINT_HIGH, "Weird port: %s (using %d)\n", portpart + 1, DOOMPORT);
			port = DOOMPORT;
		}
	}
	else
	{
		port = DOOMPORT;
	}
	address->sin_port = htons(port);

	for (curchar = 0; c = name[curchar]; curchar++)
	{
		if ((c < '0' || c > '9') && c != '.')
		{
			isnamed = true;
			break;
		}
	}

	if (!isnamed)
	{
		address->sin_addr.s_addr = inet_addr (name);
		Printf (PRINT_HIGH, "Node number %d address %s\n", doomcom->numnodes, name);
	}
	else
	{
		hostentry = gethostbyname (name);
		if (!hostentry)
			I_FatalError ("gethostbyname: couldn't find %s\n%s", name, neterror());
		address->sin_addr.s_addr = *(int *)hostentry->h_addr_list[0];
		Printf (PRINT_HIGH, "Node number %d hostname %s\n",
			doomcom->numnodes, hostentry->h_name);
	}

	if (portpart)
		*portpart = ':';
}

void STACK_ARGS CloseNetwork (void)
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

void StartNetwork (void)
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

	netsend = PacketSend;
	netget = PacketGet;
	netgame = true;
	multiplayer = true;
	
	// create communication socket
	mysocket = UDPsocket ();
	BindToLocalPort (mysocket, DOOMPORT);
	ioctlsocket (mysocket, FIONBIO, &trueval);

}

void WaitForPlayers (int i)
{
	if (i == Args.NumArgs() - 1)
		I_FatalError ("Not enough parameters after -net");

	StartNetwork ();

	// parse player number and host list
	doomcom->consoleplayer = (short)(Args.GetArg (i+1)[0]-'1');
	Printf (PRINT_HIGH, "Console player number: %d\n", doomcom->consoleplayer);

	doomcom->numnodes = 1;		// this node for sure
		
	i++;
	while (++i < Args.NumArgs() && Args.GetArg (i)[0] != '-' && Args.GetArg (i)[0] != '+')
	{
		BuildAddress (&sendaddress[doomcom->numnodes], Args.GetArg (i));
		doomcom->numnodes++;
	}

	Printf (PRINT_HIGH, "Total players: %d\n", doomcom->numnodes);
		
	doomcom->id = DOOMCOM_ID;
	doomcom->numplayers = doomcom->numnodes;
}

/*
void SendAbort (void)
{
	u_short dis = htons (PRE_DISCONNECT);

	doomcom->numnodes--;
	while (doomcom->numnodes)
	{
		PreSend (&dis, 2, &sendaddress[--doomcom->numnodes]);
		PreSend (&dis, 2, &sendaddress[doomcom->numnodes]);
		PreSend (&dis, 2, &sendaddress[doomcom->numnodes]);
		PreSend (&dis, 2, &sendaddress[doomcom->numnodes]);
	}
	I_FatalError ("Network game synchronization aborted.");
}

void HostGame (int i)
{
	struct PreGamePacket packet;
	int numplayers;
	BOOL gotack[MAXNETNODES];
	int ackcount;
	struct sockaddr_in *from;
	int node;

	if ((i == myargc - 1) || !(numplayers = atoi (myargv[i+1])))
	{	// No player count specified, assume 2
		numplayers = 2;
	}

	StartNetwork ();
	doomcom->numnodes = 1;
	Printf (PRINT_HIGH, "Waiting for players...\n");

	// Wait for numplayers-1 different connections
	while (doomcom->numnodes < numplayers)
	{
		while (doomcom->numnodes < numplayers)
		{
			if (CheckAbort ())
			{
				SendAbort ();
			}

			while (from = PreGet (&packet, sizeof(packet)))
			{
				switch (ntohs (packet.message))
				{
					case PRE_CONNECT:
						node = FindNode (from);
						if (node == -1)
						{
							node = doomcom->numnodes++;
							sendaddress[node] = *from;
						}
						Printf (PRINT_HIGH, "Got connect from node %d\n", node);
						packet.message = htons (PRE_CONACK);
						PreSend (&packet, 2, from);
						break;
					case PRE_DISCONNECT:
						node = FindNode (from);
						if (node >= 0)
						{
							Printf (PRINT_HIGH, "Got disconnect from node %d\n", node);
							doomcom->numnodes--;
							while (node < doomcom->numnodes)
							{
								sendaddress[node] = sendaddress[node+1];
								node++;
							}
						}
						break;
				}
			}
		}

		// It's possible somebody bailed out after all players were found.
		// Unfortunately, this isn't guaranteed to catch all of them.
		// Oh well. Better than nothing.
		while ( (from = PreGet (&packet, sizeof(packet))) )
		{
			if (ntohs (packet.message) == PRE_DISCONNECT)
			{
				node = FindNode (from);
				if (node >= 0)
				{
					doomcom->numnodes--;
					while (node < doomcom->numnodes)
					{
						sendaddress[node] = sendaddress[node+1];
						node++;
					}
				}
				break;
			}
		}
	}

	// Now inform everyone of all machines involved in the game
	ackcount = 0;
	memset (gotack, 0, sizeof(gotack));
	Printf (PRINT_HIGH, "Sending all here\n");
	while (ackcount < doomcom->numnodes - 1)
	{
		packet.message = htons (PRE_ALLHERE);
		packet.numnodes = htons ((u_short)(doomcom->numnodes - 1));
		for (node = 0; node < doomcom->numnodes - 1; node++)
		{
			int machine, spot;

			if (!gotack[node])
			{
				for (spot = machine = 0; machine < doomcom->numnodes; machine++)
				{
					if (node != machine)
					{
						packet.machines[spot].address =
							sendaddress[machine].sin_addr.s_addr;
						packet.machines[spot].port =
							sendaddress[machine].sin_port;
					}
				}
			}
			PreSend (&packet, sizeof(packet), &sendaddress[node]);
		}
		if (CheckAbort ())
			SendAbort ();

		while (from = PreGet (&packet, sizeof(packet)))
		{
			if (ntohs (packet.message) == PRE_ALLHEREACK)
			{
				node = FindNode (from);
				if (node >= 0)
				{
					if (!gotack[node])
					{
						gotack[node] = true;
						ackcount++;
					}
				}
				PreSend (&packet, 2, from);
			}
		}
	}

	// Now go
	Printf (PRINT_HIGH, "Go\n");
	packet.message = htons (PRE_GO);
	for (node = 0; node < doomcom->numnodes - 1; node++)
	{
		PreSend (&packet, sizeof(packet), &sendaddress[node]);
		PreSend (&packet, sizeof(packet), &sendaddress[node]);
		PreSend (&packet, sizeof(packet), &sendaddress[node]);
		PreSend (&packet, sizeof(packet), &sendaddress[node]);
	}
}

void SendToHost (u_short message, u_short ackmess, BOOL abortable)
{
	struct sockaddr_in *from;
	BOOL waiting = true;
	u_short packet;

	message = htons (message);
	ackmess = htons (ackmess);
	while (waiting)
	{
		if (abortable && CheckAbort ())
			SendAbort ();

		// Let host know we are here
		PreSend (&message, 2, &sendaddress[0]);

		Sleep (300);
		// Listen for acknowledgement
		while ( (from = PreGet (&packet, sizeof(packet))) )
		{
			if (packet == ackmess)
				waiting = false;
		}
	}
}

void JoinGame (int i)
{
	struct sockaddr_in *from;
	BOOL waiting;
	struct PreGamePacket packet;

	if ((i == myargc - 1) || (myargv[i+1][0] == '-') || (myargv[i+1][0] == '+'))
		I_FatalError ("You need to specify the host machine's address");

	StartNetwork ();

	// Host is always node 0
	BuildAddress (&sendaddress[0], myargv[i+1]);

	// Let host know we are here
	SendToHost (PRE_CONNECT, PRE_CONACK, true);

	// Wait for everyone else to connect
	waiting = true;
	doomcom->numnodes = 0;
	while (waiting)
	{
		if (CheckAbort ())
			SendAbort ();
		Sleep (300);
		while (from = PreGet (&packet, sizeof(packet)))
		{
			switch (ntohs (packet.message))
			{
				case PRE_ALLHERE:
					if (doomcom->numnodes == 0)
					{
						int node;

						packet.numnodes = ntohs (packet.numnodes);
						doomcom->numnodes = packet.numnodes + 1;
						for (node = 0; node < packet.numnodes; node++)
						{
							sendaddress[node+1].sin_addr.s_addr =
								packet.machines[node].address;
							sendaddress[node+1].sin_port =
								packet.machines[node].port;
						}
					}
					packet.message = htons (PRE_ALLHEREACK);
					PreSend (&packet, 2, &sendaddress[0]);
					break;
				case PRE_GO:
					waiting = false;
					break;
				case PRE_DISCONNECT:
					I_FatalError ("Host cancelled the game");
					break;
			}
		}
	}

	// Clear out any waiting packets
	while (PreGet (&packet, sizeof(packet)))
		;
}
*/

//
// I_InitNetwork
//
void I_InitNetwork (void)
{
	int i;
	char *v;

	doomcom = new doomcom_t;
	memset (doomcom, 0, sizeof(*doomcom));
	
	// set up for network
	v = Args.CheckValue ("-dup");
	if (v)
	{
		doomcom->ticdup = (short)(v[0]-'0');
		if (doomcom->ticdup < 1)
			doomcom->ticdup = 1;
		if (doomcom->ticdup > 9)
			doomcom->ticdup = 9;
	}
	else
		doomcom-> ticdup = 1;
		
	if (Args.CheckParm ("-extratic"))
		doomcom->extratics = 1;
	else
		doomcom->extratics = 0;
				
	v = Args.CheckValue ("-port");
	if (v)
	{
		DOOMPORT = atoi (v);
		Printf (PRINT_HIGH, "using alternate port %i\n", DOOMPORT);
	}
	
	// parse network game options,
	//		-net <consoleplayer> <host> <host> ...
	// -or-
	//		player 1: -host <numplayers>
	//		player x: -join <player 1's address>
	if ( (i = Args.CheckParm ("-net")) )
		WaitForPlayers (i);
/*
	else if ( (i = M_CheckParm ("-host")) )
		HostGame (i);
	else if ( (i = M_CheckParm ("-join")) )
		JoinGame (i);
*/
	else
	{
		// single player game
		netgame = false;
		multiplayer = false;
		doomcom->id = DOOMCOM_ID;
		doomcom->numplayers = doomcom->numnodes = 1;
		doomcom->consoleplayer = 0;
		return;
	}
}


void I_NetCmd (void)
{
	if (doomcom->command == CMD_SEND)
	{
		netsend ();
	}
	else if (doomcom->command == CMD_GET)
	{
		netget ();
	}
	else
		I_Error ("Bad net cmd: %i\n",doomcom->command);
}

#ifdef __WIN32__
char *neterror (void)
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
			sprintf (neterr, "%d", code);
			return neterr;
	}
}
#else
char *neterror (void)
{
	static char neterr[16];

	sprintf (neterr, "%d", WSAGetLastError ());
	return neterr;
}
#endif
