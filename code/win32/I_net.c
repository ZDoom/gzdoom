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
// $Log: i_net.c,v $
// Revision 1.2  1997/12/29 19:50:54  pekangas
// Ported to WinNT/95 environment using Watcom C 10.6.
// Everything expect joystick support is implemented, but networking is
// untested. Crashes way too often with problems in FixedDiv().
// Compiles with no warnings at warning level 3, uses MIDAS 1.1.1.
//
// Revision 1.1.1.1  1997/12/28 12:59:03  pekangas
// Initial DOOM source release from id Software
//
//
// DESCRIPTION:
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
#endif

#ifdef __WIN32__
#	define IPPORT_USERRESERVED 5000
#endif

static void	NetSend (void);
static BOOL	NetListen (void);


//
// NETWORKING
//

static int 	DOOMPORT =		(IPPORT_USERRESERVED +0x1d );

static int 	sendsocket;
static int 	insocket;

static struct sockaddr_in 	sendaddress[MAXNETNODES];

void	(*netget) (void);
void	(*netsend) (void);

#ifdef __WIN32__
char	*neterror (void);
#endif


//
// UDPsocket
//
int UDPsocket (void)
{
	SOCKET s;
		
	// allocate a socket
	s = socket (PF_INET, SOCK_DGRAM, IPPROTO_UDP);
#ifdef __WIN32__
	if ( s == INVALID_SOCKET )
		I_FatalError ("can't create socket: %s", neterror());
#else
	if (s<0)
		I_FatalError ("can't create socket: %s",strerror(errno));
#endif	  
				
	return (int) s;
}

//
// BindToLocalPort
//
void BindToLocalPort (int s, int port)
{
	int 				v;
	struct sockaddr_in	address;
		
	memset (&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = (USHORT)port;
						
	v = bind ((SOCKET) s, (void *)&address, sizeof(address));
#ifdef __WIN32__
	if (v == -1)
		I_FatalError ("BindToPort: bind: %s", neterror());
#else
	if (v == -1)
		I_FatalError ("BindToPort: bind: %s", strerror(errno));
#endif	  
}


//
// PacketSend
//
void PacketSend (void)
{
	int c;
								
	// byte swap
	netbuffer->checksum = LONG(netbuffer->checksum);
				
	//printf ("sending %i\n",gametic);			
	c = sendto (sendsocket , (const char*)netbuffer, doomcom->datalength
				,0,(void *)&sendaddress[doomcom->remotenode]
				,sizeof(sendaddress[doomcom->remotenode]));

	netbuffer->checksum = LONG(netbuffer->checksum);

	//	if (c == -1)
	//			I_Error ("SendPacket error: %s",strerror(errno));
}


//
// PacketGet
//
void PacketGet (void)
{
	int 				i;
	int 				c;
	struct sockaddr_in	fromaddress;
	int 				fromlen;
								
	fromlen = sizeof(fromaddress);
	c = recvfrom (insocket, (char*)netbuffer, sizeof(doomdata_t), 0
				  , (struct sockaddr *)&fromaddress, &fromlen );
	if (c == -1 )
	{
#ifdef __WIN32__
		if ( WSAGetLastError() != WSAEWOULDBLOCK )
			I_Error("GetPacket: %s",neterror());
#else		 
		if (errno != EWOULDBLOCK)
			I_Error ("GetPacket: %s",strerror(errno));
#endif		  
		doomcom->remotenode = -1;		// no packet
		return;
	}

	{
		static int first=1;
		if (first)
			Printf("len=%d:p=[0x%x]\n", c, netbuffer);
		first = 0;
	}

	// find remote node number
	for (i = 0; i<doomcom->numnodes; i++)
		if ( fromaddress.sin_addr.s_addr == sendaddress[i].sin_addr.s_addr )
			break;

	if (i == doomcom->numnodes)
	{
		// packet is not from one of the players (new game broadcast)
		doomcom->remotenode = -1;		// no packet
		return;
	}
		
	doomcom->remotenode = (short)i;			// good packet from a game player
	doomcom->datalength = (short)c;
		
	// byte swap
	netbuffer->checksum = LONG(netbuffer->checksum);
}



int GetLocalAddress (void)
{
	char				hostname[1024];
	struct hostent* 	hostentry;		// host information entry
	int 				v;

	// get local address
	v = gethostname (hostname, sizeof(hostname));
#ifdef __WIN32__
	if (v == -1)
		I_FatalError ("GetLocalAddress : gethostname: %s", neterror());
#else
	if (v == -1)
		I_FatalError ("GetLocalAddress : gethostname: errno %d",errno);
#endif	  
		
	hostentry = gethostbyname (hostname);
#ifdef __WIN32__
	if (!hostentry)
		I_FatalError ("GetLocalAddress : gethostbyname: %s", neterror());
#else
	if (!hostentry)
		I_FatalError ("GetLocalAddress : gethostbyname: couldn't get local host");
#endif	  
				
	return *(int *)hostentry->h_addr_list[0];
}


//
// I_InitNetwork
//
void I_InitNetwork (void)
{
	u_long	 			trueval = true;
	int 				i;
	int 				p;
	struct hostent* 	hostentry;		// host information entry

#ifdef __WIN32__
	WSADATA				wsad;
#endif
	
	doomcom = Malloc (sizeof (*doomcom) );
	memset (doomcom, 0, sizeof(*doomcom) );
	
	// set up for network
	i = M_CheckParm ("-dup");
	if (i && i< myargc-1)
	{
		doomcom->ticdup = (short)(myargv[i+1][0]-'0');
		if (doomcom->ticdup < 1)
			doomcom->ticdup = 1;
		if (doomcom->ticdup > 9)
			doomcom->ticdup = 9;
	}
	else
		doomcom-> ticdup = 1;
		
	if (M_CheckParm ("-extratic"))
		doomcom-> extratics = 1;
	else
		doomcom-> extratics = 0;
				
	p = M_CheckParm ("-port");
	if (p && p<myargc-1)
	{
		DOOMPORT = atoi (myargv[p+1]);
		Printf ("using alternate port %i\n",DOOMPORT);
	}
	
	// parse network game options,
	//	-net <consoleplayer> <host> <host> ...
	if ( !(i = M_CheckParm ("-net")) ) {
		// single player game
		netgame = false;
		doomcom->id = DOOMCOM_ID;
		doomcom->numplayers = doomcom->numnodes = 1;
		doomcom->consoleplayer = 0;
		return;
	}

#ifdef __WIN32__
	WSAStartup( 0x0101, &wsad );
#endif

	netsend = PacketSend;
	netget = PacketGet;
	netgame = true;

	// parse player number and host list
	doomcom->consoleplayer = (short)(myargv[i+1][0]-'1');
	Printf ("Console player number: %d\n", doomcom->consoleplayer);

	doomcom->numnodes = 1;		// this node for sure
		
	i++;
	while (++i < myargc && myargv[i][0] != '-' && myargv[i][0] != '+')
	{
		int port;
		char *portpart;
		BOOL isnamed = false;
		int curchar;
		char c;

		sendaddress[doomcom->numnodes].sin_family = AF_INET;

		if (portpart = strchr (myargv[i], ':')) {
			*portpart = 0;
			port = atoi (portpart + 1);
			if (!port) {
				Printf ("Weird port: %s (using %d)\n", portpart + 1, DOOMPORT);
				port = DOOMPORT;
			}
		} else {
			port = DOOMPORT;
		}
		sendaddress[doomcom->numnodes].sin_port = (USHORT)port;

		for (curchar = 0; c = myargv[i][curchar]; curchar++) {
			if ((c < '0' || c > '9') && c != '.') {
				isnamed = true;
				break;
			}
		}

		if (!isnamed)
		{
			sendaddress[doomcom->numnodes].sin_addr.s_addr 
				= inet_addr (myargv[i]);
			Printf ("Node number %d address %s\n", doomcom->numnodes, myargv[i]);
		}
		else
		{
			hostentry = gethostbyname (myargv[i]);
			if (!hostentry)
#ifdef __WIN32__
				I_FatalError ("gethostbyname: couldn't find %s\n%s", myargv[i], neterror());
#else
				I_FatalError ("gethostbyname: couldn't find %s\n%s", myargv[i], strerror(errno));
#endif
			sendaddress[doomcom->numnodes].sin_addr.s_addr 
				= *(int *)hostentry->h_addr_list[0];
			Printf ("Node number %d hostname %s\n", doomcom->numnodes, hostentry->h_name);
		}

		if (portpart)
			*portpart = ':';

		doomcom->numnodes++;
	}

	Printf ("Total players: %d\n", doomcom->numnodes);
		
	doomcom->id = DOOMCOM_ID;
	doomcom->numplayers = doomcom->numnodes;
	
	// build message to receive
	insocket = UDPsocket ();
	BindToLocalPort (insocket, DOOMPORT);
#ifdef __WIN32__
	ioctlsocket (insocket, FIONBIO, &trueval);
#else
	ioctl (insocket, FIONBIO, &trueval);
#endif	  

	sendsocket = UDPsocket ();
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
#ifdef _DEBUG
char *neterror (void)
{
	static char neterr[16];
	int			code;

	switch (code = WSAGetLastError ()) {
		case WSAEACCES:				return "Permission denied";
		case WSAEADDRINUSE:			return "Address already in use";
		case WSAEADDRNOTAVAIL:		return "Cannot assign requested address";
		case WSAEAFNOSUPPORT:		return "Address family not supported by protocol family";
		case WSAEALREADY:			return "Operation already in progress";
		case WSAECONNABORTED:		return "Software caused connection abort";
		case WSAECONNREFUSED:		return "Connection refused";
		case WSAECONNRESET:			return "Connection reset by peer";
		case WSAEDESTADDRREQ:		return "Destination address required";
		case WSAEFAULT:				return "Bad address";
		case WSAEHOSTDOWN:			return "Host is down";
		case WSAEHOSTUNREACH:		return "No route to host";
		case WSAEINPROGRESS:		return "Operation now in progress";
		case WSAEINTR:				return "Interrupted function call";
		case WSAEINVAL:				return "Invalid argument";
		case WSAEISCONN:			return "Socket is already connected";
		case WSAEMFILE:				return "Too many open files";
		case WSAEMSGSIZE:			return "Message too long";
		case WSAENETDOWN:			return "Network is down";
		case WSAENETRESET:			return "Network dropped connection or reset";
		case WSAENETUNREACH:		return "Network is unreachable";
		case WSAENOBUFS:			return "No buffer space available";
		case WSAENOPROTOOPT:		return "Bad protocol option";
		case WSAENOTCONN:			return "Socket is not connected";
		case WSAENOTSOCK:			return "Socket operation on non-socket";
		case WSAEOPNOTSUPP:			return "Operation not supported";
		case WSAEPFNOSUPPORT:		return "Protocol family not supported";
		case WSAEPROCLIM:			return "Too many processes";
		case WSAEPROTONOSUPPORT:	return "Protocol not supported";
		case WSAEPROTOTYPE:			return "Protocol wrong type for socket";
		case WSAESHUTDOWN:			return "Cannot send after socket shutdown";
		case WSAESOCKTNOSUPPORT:	return "Socket type not supported";
		case WSAETIMEDOUT:			return "Connection timed out";
		case WSAEWOULDBLOCK:		return "Resource temporarily unavailable";
		case WSAHOST_NOT_FOUND:		return "Host not found";
		case WSANOTINITIALISED:		return "Successful WSAStartup not yet performed";
		case WSANO_DATA:			return "Valid name, no data record of requested type";
		case WSANO_RECOVERY:		return "This is a non-recoverable error";
		case WSASYSNOTREADY:		return "Network subsystem is unavailable";
		case WSATRY_AGAIN:			return "Non-authoritative host not found";
		case WSAVERNOTSUPPORTED:	return "WINSOCK.DLL version out of range";
		case WSAEDISCON:			return "Graceful shutdown in progress";

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
#endif
