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

const char
i_net_rcsid[] = "$Id: i_net.c,v 1.2 1997/12/29 19:50:54 pekangas Exp $";

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

#include "doomstat.h"

#ifdef __GNUG__
#pragma implementation "i_net.h"
#endif
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


// For some odd reason...
#define ntohl(x) \
		((unsigned long int)((((unsigned long int)(x) & 0x000000ffU) << 24) | \
							 (((unsigned long int)(x) & 0x0000ff00U) <<  8) | \
							 (((unsigned long int)(x) & 0x00ff0000U) >>  8) | \
							 (((unsigned long int)(x) & 0xff000000U) >> 24)))

#define ntohs(x) \
		((unsigned short int)((((unsigned short int)(x) & 0x00ff) << 8) | \
							  (((unsigned short int)(x) & 0xff00) >> 8))) \
		  
#define htonl(x) ntohl(x)
#define htons(x) ntohs(x)

void	NetSend (void);
boolean NetListen (void);


//
// NETWORKING
//

int 	DOOMPORT =		(IPPORT_USERRESERVED +0x1d );

int 	sendsocket;
int 	insocket;

struct	sockaddr_in 	sendaddress[MAXNETNODES];

void	(*netget) (void);
void	(*netsend) (void);


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
		I_Error ("can't create socket!");
#else
	if (s<0)
		I_Error ("can't create socket: %s",strerror(errno));
#endif	  
				
	return (int) s;
}

//
// BindToLocalPort
//
void
BindToLocalPort
( int	s,
  int	port )
{
	int 				v;
	struct sockaddr_in	address;
		
	memset (&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = INADDR_ANY;
	address.sin_port = port;
						
	v = bind ((SOCKET) s, (void *)&address, sizeof(address));
#ifdef __WIN32__
	if (v == -1)
		I_Error ("BindToPort: bind failed!");
#else
	if (v == -1)
		I_Error ("BindToPort: bind: %s", strerror(errno));
#endif	  
}


//
// PacketSend
//
void PacketSend (void)
{
	int 		c;
	doomdata_t	sw;
								
	// byte swap
	sw.checksum = htonl(netbuffer->checksum);
	sw.player = netbuffer->player;
	sw.retransmitfrom = netbuffer->retransmitfrom;
	sw.starttic = netbuffer->starttic;
	sw.numtics = netbuffer->numtics;
	for (c=0 ; c< netbuffer->numtics ; c++)
	{
		sw.cmds[c].ucmd.msec = netbuffer->cmds[c].ucmd.msec;
		sw.cmds[c].ucmd.buttons = netbuffer->cmds[c].ucmd.buttons;
		sw.cmds[c].ucmd.pitch = htons(netbuffer->cmds[c].ucmd.pitch);
		sw.cmds[c].ucmd.yaw = htons(netbuffer->cmds[c].ucmd.yaw);
		sw.cmds[c].ucmd.roll = htons(netbuffer->cmds[c].ucmd.roll);
		sw.cmds[c].ucmd.forwardmove = htons(netbuffer->cmds[c].ucmd.forwardmove);
		sw.cmds[c].ucmd.sidemove = htons(netbuffer->cmds[c].ucmd.sidemove);
		sw.cmds[c].ucmd.upmove = htons(netbuffer->cmds[c].ucmd.upmove);
		sw.cmds[c].ucmd.impulse = netbuffer->cmds[c].ucmd.impulse;
		sw.cmds[c].ucmd.lightlevel = netbuffer->cmds[c].ucmd.lightlevel;
		sw.cmds[c].consistancy = htons(netbuffer->cmds[c].consistancy);
		sw.cmds[c].chatchar = netbuffer->cmds[c].chatchar;
	}
				
	//printf ("sending %i\n",gametic);			
	c = sendto (sendsocket , (const char*) &sw, doomcom->datalength
				,0,(void *)&sendaddress[doomcom->remotenode]
				,sizeof(sendaddress[doomcom->remotenode]));
		
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
	doomdata_t			sw;
								
	fromlen = sizeof(fromaddress);
	c = recvfrom (insocket, (char*) &sw, sizeof(sw), 0
				  , (struct sockaddr *)&fromaddress, &fromlen );
	if (c == -1 )
	{
#ifdef __WIN32__
		if ( WSAGetLastError() != WSAEWOULDBLOCK )
			I_Error("GetPacket: recvfrom() failed");
#else		 
		if (errno != EWOULDBLOCK)
			I_Error ("GetPacket: %s",strerror(errno));
#endif		  
		doomcom->remotenode = -1;				// no packet
		return;
	}

	{
		static int first=1;
		if (first)
			Printf("len=%d:p=[0x%x 0x%x] \n", c, *(int*)&sw, *((int*)&sw+1));
		first = 0;
	}

	// find remote node number
	for (i=0 ; i<doomcom->numnodes ; i++)
		if ( fromaddress.sin_addr.s_addr == sendaddress[i].sin_addr.s_addr )
			break;

	if (i == doomcom->numnodes)
	{
		// packet is not from one of the players (new game broadcast)
		doomcom->remotenode = -1;				// no packet
		return;
	}
		
	doomcom->remotenode = i;					// good packet from a game player
	doomcom->datalength = c;
		
	// byte swap
	netbuffer->checksum = ntohl(sw.checksum);
	netbuffer->player = sw.player;
	netbuffer->retransmitfrom = sw.retransmitfrom;
	netbuffer->starttic = sw.starttic;
	netbuffer->numtics = sw.numtics;

	for (c=0 ; c< netbuffer->numtics ; c++)
	{
		netbuffer->cmds[c].ucmd.msec = sw.cmds[c].ucmd.msec;
		netbuffer->cmds[c].ucmd.buttons = sw.cmds[c].ucmd.buttons;
		netbuffer->cmds[c].ucmd.pitch = ntohs(sw.cmds[c].ucmd.pitch);
		netbuffer->cmds[c].ucmd.yaw = ntohs(sw.cmds[c].ucmd.yaw);
		netbuffer->cmds[c].ucmd.roll = ntohs(sw.cmds[c].ucmd.roll);
		netbuffer->cmds[c].ucmd.forwardmove = ntohs(sw.cmds[c].ucmd.forwardmove);
		netbuffer->cmds[c].ucmd.sidemove = ntohs(sw.cmds[c].ucmd.sidemove);
		netbuffer->cmds[c].ucmd.upmove = ntohs(sw.cmds[c].ucmd.upmove);
		netbuffer->cmds[c].ucmd.impulse = sw.cmds[c].ucmd.impulse;
		netbuffer->cmds[c].ucmd.lightlevel = sw.cmds[c].ucmd.lightlevel;
		netbuffer->cmds[c].consistancy = ntohs(sw.cmds[c].consistancy);
		netbuffer->cmds[c].chatchar = sw.cmds[c].chatchar;
	}
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
		I_Error ("GetLocalAddress : gethostname() failed!");
#else
	if (v == -1)
		I_Error ("GetLocalAddress : gethostname: errno %d",errno);
#endif	  
		
	hostentry = gethostbyname (hostname);
#ifdef __WIN32__
	if (!hostentry)
		I_Error ("GetLocalAddress : gethostbyname() failed!");
#else
	if (!hostentry)
		I_Error ("GetLocalAddress : gethostbyname: couldn't get local host");
#endif	  
				
	return *(int *)hostentry->h_addr_list[0];
}


//
// I_InitNetwork
//
void I_InitNetwork (void)
{
	boolean 			trueval = true;
	int 				i;
	int 				p;
	struct hostent* 	hostentry;		// host information entry

#ifdef __WIN32__
	WSADATA 	wsad;
#endif
	
	doomcom = Malloc (sizeof (*doomcom) );
	memset (doomcom, 0, sizeof(*doomcom) );
	
	// set up for network
	i = M_CheckParm ("-dup");
	if (i && i< myargc-1)
	{
		doomcom->ticdup = myargv[i+1][0]-'0';
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
	i = M_CheckParm ("-net");
	if (!i)
	{
		// single player game
		netgame = false;
		doomcom->id = DOOMCOM_ID;
		doomcom->numplayers = doomcom->numnodes = 1;
		doomcom->deathmatch = false;
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
	doomcom->consoleplayer = myargv[i+1][0]-'1';
	Printf ("Console player number: %d\n", doomcom->consoleplayer);

	doomcom->numnodes = 1;		// this node for sure
		
	i++;
	while (++i < myargc && myargv[i][0] != '-')
	{
		sendaddress[doomcom->numnodes].sin_family = AF_INET;
		sendaddress[doomcom->numnodes].sin_port = htons(DOOMPORT);
		if (myargv[i][0] == '.')
		{
			sendaddress[doomcom->numnodes].sin_addr.s_addr 
				= inet_addr (myargv[i]+1);
			Printf ("Node number %d address %s\n", doomcom->numnodes, myargv[i]+1);
		}
		else
		{
			hostentry = gethostbyname (myargv[i]);
			if (!hostentry)
				I_Error ("gethostbyname: couldn't find %s", myargv[i]);
			sendaddress[doomcom->numnodes].sin_addr.s_addr 
				= *(int *)hostentry->h_addr_list[0];
			Printf ("Node number %d hostname %s\n", doomcom->numnodes, myargv[i]);
		}
		doomcom->numnodes++;
	}

	Printf ("Total players: %d\n", doomcom->numnodes);
		
	doomcom->id = DOOMCOM_ID;
	doomcom->numplayers = doomcom->numnodes;
	
	// build message to receive
	insocket = UDPsocket ();
	BindToLocalPort (insocket,htons(DOOMPORT));
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

