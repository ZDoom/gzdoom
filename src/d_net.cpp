// Emacs style mode select	 -*- C++ -*- 
//-----------------------------------------------------------------------------
//
// $Id:$
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
// $Log:$
//
// DESCRIPTION:
//		DOOM Network game communication and protocol,
//		all OS independent parts.
//
//-----------------------------------------------------------------------------

#include <stddef.h>

#include "version.h"
#include "m_alloc.h"
#include "m_menu.h"
#include "m_random.h"
#include "i_system.h"
#include "i_video.h"
#include "i_net.h"
#include "g_game.h"
#include "doomdef.h"
#include "doomstat.h"
#include "c_console.h"
#include "d_netinf.h"
#include "cmdlib.h"
#include "s_sound.h"
#include "m_cheat.h"
#include "p_effect.h"
#include "p_local.h"
#include "c_dispatch.h"
#include "sbar.h"
#include "gi.h"

#define NCMD_EXIT				0x80000000
#define NCMD_RETRANSMIT 		0x40000000
#define NCMD_SETUP				0x20000000
#define NCMD_KILL				0x10000000		// kill game
#define NCMD_CHECKSUM			0x0fffffff

extern byte		*demo_p;		// [RH] Special "ticcmds" get recorded in demos
extern char		savedescription[SAVESTRINGSIZE];
extern char		*savegamefile;

doomcom_t*		doomcom;		
doomdata_t* 	netbuffer;		// points inside doomcom


//
// NETWORKING
//
// gametic is the tic about to (or currently being) run
// maketic is the tick that hasn't had control made for it yet
// nettics[] has the maketics for all players 
//
// a gametic cannot be run until nettics[] > gametic for all players
//
#define RESENDCOUNT 	10
#define PL_DRONE		0x80	// bit flag in doomdata->player

ticcmd_t		localcmds[BACKUPTICS];

FDynamicBuffer	NetSpecs[MAXPLAYERS][BACKUPTICS];
ticcmd_t		netcmds[MAXPLAYERS][BACKUPTICS];
int 			nettics[MAXNETNODES];
BOOL 			nodeingame[MAXNETNODES];				// set false as nodes leave game
BOOL	 		remoteresend[MAXNETNODES];				// set when local needs tics
int 			resendto[MAXNETNODES];					// set when remote needs tics
int 			resendcount[MAXNETNODES];

unsigned int	lastrecvtime[MAXPLAYERS];				// [RH] Used for pings
unsigned int	currrecvtime[MAXPLAYERS];

int 			nodeforplayer[MAXPLAYERS];

int 			maketic;
int 			lastnettic;
int 			skiptics;
int 			ticdup; 		
int 			maxsend;		// BACKUPTICS/(2*ticdup)-1

void D_ProcessEvents (void); 
void G_BuildTiccmd (ticcmd_t *cmd); 
void D_DoAdvanceDemo (void);
 
BOOL	 		reboundpacket;
doomdata_t		reboundstore;

int 	frameon;
int 	frameskip[MAXPLAYERS];
int 	oldnettics;

static int 	entertic;
static int	oldentertics;

extern	BOOL	 advancedemo;

// [RH] Special "ticcmds" get stored in here
static struct TicSpecial
{
	byte *streams[BACKUPTICS];
	int	  used[BACKUPTICS];
	byte *streamptr;
	int	  streamoffs;
	int   specialsize;
	int	  lastmaketic;
	BOOL  okay;

	TicSpecial ()
	{
		int i;

		lastmaketic = -1;
		specialsize = 256;

		for (i = 0; i < BACKUPTICS; i++)
			streams[i] = NULL;

		for (i = 0; i < BACKUPTICS; i++)
		{
			streams[i] = (byte *)Malloc (256);
			used[i] = 0;
		}
		okay = true;
	}

	~TicSpecial ()
	{
		int i;

		for (i = 0; i < BACKUPTICS; i++)
		{
			if (streams[i])
			{
				free (streams[i]);
				streams[i] = NULL;
				used[i] = 0;
			}
		}
		okay = false;
	}

	// Make more room for special commands.
	void GetMoreSpace ()
	{
		int i;

		specialsize <<= 1;

		DPrintf ("Expanding special size to %d\n", specialsize);

		for (i = 0; i < BACKUPTICS; i++)
			streams[i] = (byte *)Realloc (streams[i], specialsize);

		streamptr = streams[maketic%BACKUPTICS] + streamoffs;
	}

	void CheckSpace (int needed)
	{
		if (streamoffs >= specialsize - needed)
			GetMoreSpace ();

		streamoffs += needed;
	}

	void NewMakeTic ()
	{
		if (lastmaketic != -1)
			used[lastmaketic%BACKUPTICS] = streamoffs;

		lastmaketic = maketic;
		streamptr = streams[maketic%BACKUPTICS];
		streamoffs = 0;
	}

	TicSpecial &operator << (byte it)
	{
		if (streamptr)
		{
			CheckSpace (1);
			WriteByte (it, &streamptr);
		}
		return *this;
	}

	TicSpecial &operator << (short it)
	{
		if (streamptr)
		{
			CheckSpace (2);
			WriteWord (it, &streamptr);
		}
		return *this;
	}

	TicSpecial &operator << (int it)
	{
		if (streamptr)
		{
			CheckSpace (4);
			WriteLong (it, &streamptr);
		}
		return *this;
	}

	TicSpecial &operator << (float it)
	{
		if (streamptr)
		{
			CheckSpace (4);
			WriteFloat (it, &streamptr);
		}
		return *this;
	}

	TicSpecial &operator << (const char *it)
	{
		if (streamptr)
		{
			CheckSpace (strlen (it) + 1);
			WriteString (it, &streamptr);
		}
		return *this;
	}

} specials;

void Net_ClearBuffers ()
{
	int i, j;

	memset (localcmds, 0, sizeof(localcmds));
	memset (netcmds, 0, sizeof(netcmds));
	memset (nettics, 0, sizeof(nettics));
	memset (nodeingame, 0, sizeof(nodeingame));
	memset (remoteresend, 0, sizeof(remoteresend));
	memset (resendto, 0, sizeof(resendto));
	memset (resendcount, 0, sizeof(resendcount));
	memset (lastrecvtime, 0, sizeof(lastrecvtime));
	memset (currrecvtime, 0, sizeof(currrecvtime));
	nodeingame[0] = true;

	for (i = 0; i < MAXPLAYERS; i++)
	{
		for (j = 0; j < BACKUPTICS; j++)
		{
			NetSpecs[i][j].SetData (NULL, 0);
		}
	}

	oldentertics = entertic;
	gametic = 0;
	maketic = 0;
}

//
// [RH] Rewritten to properly calculate the packet size
//		with our variable length commands.
//
int NetbufferSize (void)
{
	byte *skipper = &(netbuffer->cmds[0]);
	if ((netbuffer->checksum & NCMD_EXIT) == 0)
	{
		int count = ReadByte (&skipper);
		for (; count; count--)
		{
			ReadByte (&skipper);
			SkipTicCmd (&skipper, netbuffer->numtics);
		}
	}
	return skipper - (byte *)netbuffer;
}

//
// Checksum 
//
unsigned NetbufferChecksum (int len)
{
	unsigned	c;
	int 		i,l;

	c = 0x1234567;

	// FIXME -endianess? [RH] Is endian-ness still a problem?
	l = (len - myoffsetof(doomdata_t, retransmitfrom))/4;
	for (i=0 ; i<l ; i++)
		c += ((unsigned *)&netbuffer->retransmitfrom)[i] * (i+1);

	return c & NCMD_CHECKSUM;
}

//
//
//
int ExpandTics (int low)
{
	int delta;
		
	delta = low - (maketic&0xff);
		
	if (delta >= -64 && delta <= 64)
		return (maketic&~0xff) + low;
	if (delta > 64)
		return (maketic&~0xff) - 256 + low;
	if (delta < -64)
		return (maketic&~0xff) + 256 + low;
				
	I_Error ("ExpandTics: strange value %i at maketic %i",low,maketic);
	return 0;
}



//
// HSendPacket
//
void HSendPacket (int node, int flags, int len)
{
	netbuffer->checksum = NetbufferChecksum (len) | flags;

	if (!node)
	{
		reboundstore = *netbuffer;
		reboundpacket = true;
		return;
	}

	if (demoplayback)
		return;

	if (!netgame)
		I_Error ("Tried to transmit to another node");
				
	doomcom->command = CMD_SEND;
	doomcom->remotenode = node;
	doomcom->datalength = len;
		
	if (debugfile)
	{
		int 	i;
		int 	realretrans;
		if (netbuffer->checksum & NCMD_RETRANSMIT)
			realretrans = ExpandTics (netbuffer->retransmitfrom);
		else
			realretrans = -1;

		fprintf (debugfile,"send (%i + %i, R %i) [%i] ",
				 ExpandTics(netbuffer->starttic),
				 netbuffer->numtics, realretrans, doomcom->datalength);
		
		for (i=0 ; i<doomcom->datalength ; i++)
			fprintf (debugfile,"%i ",((byte *)netbuffer)[i]);

		fprintf (debugfile,"\n");
	}

	I_NetCmd ();
}

//
// HGetPacket
// Returns false if no packet is waiting
//
BOOL HGetPacket (void)
{		
	if (reboundpacket)
	{
		*netbuffer = reboundstore;
		doomcom->remotenode = 0;
		reboundpacket = false;
		return true;
	}

	if (!netgame)
		return false;

	if (demoplayback)
		return false;
				
	doomcom->command = CMD_GET;
	I_NetCmd ();
	
	if (doomcom->remotenode == -1)
		return false;

	if (doomcom->datalength != NetbufferSize () && !(netbuffer->checksum & NCMD_SETUP))
	{
		if (debugfile)
			fprintf (debugfile,"bad packet length %i (should be %i)\n",
				doomcom->datalength, NetbufferSize());
		return false;
	}
		
	if (NetbufferChecksum (doomcom->datalength) != (netbuffer->checksum & NCMD_CHECKSUM))
	{
		if (debugfile)
			fprintf (debugfile,"bad packet checksum\n");
		return false;
	}

	if (debugfile)
	{
		int 	realretrans;
		int 	i;
						
		if (netbuffer->checksum & NCMD_SETUP)
			fprintf (debugfile,"setup packet\n");
		else
		{
			if (netbuffer->checksum & NCMD_RETRANSMIT)
				realretrans = ExpandTics (netbuffer->retransmitfrom);
			else
				realretrans = -1;
			
			fprintf (debugfile,"get %i = (%i + %i, R %i)[%i] ",
					 doomcom->remotenode,
					 ExpandTics(netbuffer->starttic),
					 netbuffer->numtics, realretrans, doomcom->datalength);

			for (i = 0; i < doomcom->datalength; i++)
				fprintf (debugfile,"%i ",((byte *)netbuffer)[i]);
			fprintf (debugfile,"\n");
		}
	}
	return true;		
}


//
// GetPackets
//

void GetPackets (void)
{
	int 		netconsole;
	int 		netnode;
	int 		realend;
	int 		realstart;
								 
	while ( HGetPacket() )
	{
		if (netbuffer->checksum & NCMD_SETUP)
			continue;			// extra setup packet
						
		netconsole = netbuffer->player & ~PL_DRONE;
		netnode = doomcom->remotenode;

		// [RH] Get "ping" times
		lastrecvtime[netconsole] = currrecvtime[netconsole];
		currrecvtime[netconsole] = I_MSTime ();
		
		// to save bytes, only the low byte of tic numbers are sent
		// Figure out what the rest of the bytes are
		realstart = ExpandTics (netbuffer->starttic);			
		realend = (realstart+netbuffer->numtics);
		
		// check for exiting the game
		if (netbuffer->checksum & NCMD_EXIT)
		{
			if (!nodeingame[netnode])
				continue;

			nodeingame[netnode] = false;
			playeringame[netconsole] = false;

			if (deathmatch)
			{
				Printf ("%s left the game with %d frags\n",
						 players[netconsole].userinfo.netname,
						 players[netconsole].fragcount);
			}
			else
			{
				Printf ("%s left the game\n", players[netconsole].userinfo.netname);
			}

			// [RH] Make the player disappear
			P_DisconnectEffect (players[netconsole].mo);
			players[netconsole].mo->Destroy ();
			players[netconsole].mo = NULL;
			if (netconsole == Net_Arbitrator)
			{
				bglobal.RemoveAllBots (true);
				Printf ("Removed all bots\n");

				// Pick a new network arbitrator
				for (int i = 0; i < MAXPLAYERS; i++)
				{
					if (playeringame[i] && !players[i].isbot)
					{
						Net_Arbitrator = i;
						Printf ("%s is the new arbitrator\n",
							players[i].userinfo.netname);
						break;
					}
				}
			}

			if (demorecording)
			{
				G_CheckDemoStatus ();

				//WriteByte (DEM_DROPPLAYER, &demo_p);
				//WriteByte ((byte)netconsole, &demo_p);
			}

			continue;
		}
		
		// check for a remote game kill
		if (netbuffer->checksum & NCMD_KILL)
			I_Error ("Killed by network driver");

		nodeforplayer[netconsole] = netnode;
		
		// check for retransmit request
		if (resendcount[netnode] <= 0 && (netbuffer->checksum & NCMD_RETRANSMIT))
		{
			resendto[netnode] = ExpandTics (netbuffer->retransmitfrom);
			if (debugfile)
				fprintf (debugfile,"retransmit from %i\n", resendto[netnode]);
			resendcount[netnode] = RESENDCOUNT;
		}
		else
		{
			resendcount[netnode]--;
		}
		
		// check for out of order / duplicated packet			
		if (realend == nettics[netnode])
			continue;
						
		if (realend < nettics[netnode])
		{
			if (debugfile)
				fprintf (debugfile, "out of order packet (%i + %i)\n" ,
						 realstart,netbuffer->numtics);
			continue;
		}
		
		// check for a missed packet
		if (realstart > nettics[netnode])
		{
			// stop processing until the other system resends the missed tics
			if (debugfile)
				fprintf (debugfile, "missed tics from %i (%i - %i)\n",
						 netnode, realstart, nettics[netnode]);
			remoteresend[netnode] = true;
			continue;
		}

		// update command store from the packet
		{
			byte *start;
			int numplayers;
			int i, tics;
			remoteresend[netnode] = false;

			start = &(netbuffer->cmds[0]);
			numplayers = ReadByte (&start);

			for (i = numplayers; i != 0; i--)
			{
				netconsole = ReadByte (&start);	// Get player # for following commands
				SkipTicCmd (&start, nettics[netnode] - realstart);
				for (tics = nettics[netnode]; tics < realend; tics++)
					ReadTicCmd (&start, netconsole, tics);
			}

			nettics[netnode] = realend;
		}
	}
}


//
// NetUpdate
// Builds ticcmds for console player,
// sends out a packet
//
int gametime;

void NetUpdate (void)
{
	int 	nowtime;
	int 	newtics;
	int 	i,j;
	int 	realstart;
	int 	gameticdiv;
	byte	*cmddata;

	// check time
	nowtime = I_GetTime ()/ticdup;
	newtics = nowtime - gametime;
	gametime = nowtime;

	if (newtics <= 0)	// nothing new to update
		goto listen; 

	if (skiptics <= newtics)
	{
		newtics -= skiptics;
		skiptics = 0;
	}
	else
	{
		skiptics -= newtics;
		newtics = 0;
	}

	netbuffer->player = consoleplayer;
	
	// build new ticcmds for console player
	gameticdiv = gametic/ticdup;

	// [RH] This loop adjusts the bots' rotations for ticcmds that have
	// been already created but not yet executed. This way, the bot is still
	// able to create ticcmds that accurately reflect the state it wants to
	// be in even when gametic lags behind maketic.
	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && players[i].isbot && players[i].mo)
		{
			players[i].savedyaw = players[i].mo->angle;
			players[i].savedpitch = players[i].mo->pitch;
			for (int j = gameticdiv; j < maketic; j++)
			{
				players[i].mo->angle += (netcmds[i][j%BACKUPTICS].ucmd.yaw << 16) * ticdup;
				players[i].mo->pitch -= (netcmds[i][j%BACKUPTICS].ucmd.pitch << 16) * ticdup;
			}
		}
	}

	for (i = 0; i < newtics; i++)
	{
		I_StartTic ();
		D_ProcessEvents ();
		if (maketic - gameticdiv >= BACKUPTICS/2-1)
			break;			// can't hold any more
		
		//Printf ("mk:%i ",maketic);
		int block = maketic%BACKUPTICS;
		G_BuildTiccmd (&localcmds[block]);
		//Added by MC: For some of that bot stuff. The main bot function.
		bglobal.Main (block);
		maketic++;
		Net_NewMakeTic ();
	}

	for (i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && players[i].isbot && players[i].mo)
		{
			players[i].mo->angle = players[i].savedyaw;
			players[i].mo->pitch = players[i].savedpitch;
		}
	}

	if (singletics)
		return; 		// singletic update is syncronous
	
	// send the packet to the other nodes
	int count;
	count = 1;

	if (consoleplayer == Net_Arbitrator)
	{
		for (j = 0; j < MAXPLAYERS; j++)
			if (playeringame[j] && players[j].isbot)
				count++;
	}

	for (i = 0; i < doomcom->numnodes; i++)
	{
		if (nodeingame[i])
		{
			netbuffer->starttic = realstart = resendto[i];
			netbuffer->numtics = maketic - realstart;
			if (netbuffer->numtics > BACKUPTICS)
				I_Error ("NetUpdate: netbuffer->numtics > BACKUPTICS");

			resendto[i] = maketic - doomcom->extratics;

			cmddata = &(netbuffer->cmds[0]);
			WriteByte (count, &cmddata);	// # of players whose commands are in this packet

			WriteByte (consoleplayer, &cmddata); // Player number for the following commands
			for (j = 0; j < netbuffer->numtics; j++)
			{
				int start = (realstart+j)%BACKUPTICS;

				WriteWord (localcmds[start].consistancy, &cmddata);
				// [RH] Write out special "ticcmds" before real ticcmd
				if (specials.used[start])
				{
					memcpy (cmddata, specials.streams[start], specials.used[start]);
					cmddata += specials.used[start];
				}
				WriteByte (DEM_USERCMD, &cmddata);
				PackUserCmd (&localcmds[start].ucmd,
					start ? &localcmds[(start-1)%BACKUPTICS].ucmd : NULL, &cmddata);
			}
			if (count > 1)
			{
				for (int k = 0; k < MAXPLAYERS; k++)
				{
					if (playeringame[k] && players[k].isbot)
					{
						WriteByte (k, &cmddata);
						for (j = 0; j < netbuffer->numtics; j++)
						{
							int start = (realstart+j)%BACKUPTICS;
							WriteWord (0, &cmddata);	// fake consistancy word
							WriteByte (DEM_USERCMD, &cmddata);
							PackUserCmd (&netcmds[k][start].ucmd,
								start ? &netcmds[k][(start-1)%BACKUPTICS].ucmd : NULL, &cmddata);
						}
					}
				}
			}

			if (remoteresend[i])
			{
				netbuffer->retransmitfrom = nettics[i];
				HSendPacket (i, NCMD_RETRANSMIT, (ptrdiff_t)cmddata - (ptrdiff_t)netbuffer);
			}
			else
			{
				netbuffer->retransmitfrom = 0;
				HSendPacket (i, 0, (ptrdiff_t)cmddata - (ptrdiff_t)netbuffer);
			}
		}
	}

	// listen for other packets
  listen:
	GetPackets ();
}



//
// CheckAbort
//
BOOL CheckAbort (void)
{
	event_t *ev;

	Printf ("");	// [RH] Give the console a chance to redraw itself
	// This WaitForTic is to avoid flooding the network with packets on startup.
	I_WaitForTic (I_GetTime () + TICRATE/4);
	I_StartTic ();
	for ( ; eventtail != eventhead 
		  ; eventtail = (++eventtail)&(MAXEVENTS-1) ) 
	{ 
		ev = &events[eventtail]; 
		if (ev->type == EV_KeyDown && ev->data1 == KEY_ESCAPE)
			return true;
	}
	return false;
}


//
// D_ArbitrateNetStart
//
void D_ArbitrateNetStart (void)
{
	int 		i;
	BOOL	 	nodesdetected[MAXNETNODES];
	BOOL		gotsetup;
	char		*s;
	byte		*stream;

	// Return right away if we're just playing with ourselves.
	if (doomcom->numnodes == 1)
		return;

	autostart = true;
	gotsetup = false;
	bool theresmore = false;

	memset (nodesdetected, 0, sizeof(nodesdetected));
	nodesdetected[0] = 1;	// Detect ourselves

	// [RH] Rewrote this loop based on Doom Legacy 1.11's code.
	Printf ("Waiting for %d more player%s...\n",
		doomcom->numnodes - 1, (doomcom->numnodes == 2) ? "" : "s");
	do 
	{
		if (CheckAbort ())
			I_FatalError ("Network game synchronization aborted.");

		for (i = 10; i > 0; i--)
		{
			if (HGetPacket ())
			{
				switch (netbuffer->checksum & (NCMD_SETUP|NCMD_KILL))
				{
				case NCMD_SETUP:
					// game info
					if (!gotsetup)
					{
						gotsetup = true;
						if (netbuffer->player != VERSION)
							I_Error ("Different DOOM versions cannot play a net game!");

						stream = &(netbuffer->cmds[0]);

						s = ReadString (&stream);
						strncpy (startmap, s, 8);
						delete[] s;
						rngseed = ReadLong (&stream);
						C_ReadCVars (&stream);
					}
					break;

				case NCMD_SETUP|NCMD_KILL:
					// user info
					if (doomcom->remotenode)
					{
						nodesdetected[doomcom->remotenode] = netbuffer->starttic;

						if (!nodeingame[doomcom->remotenode])
						{
							stream = &(netbuffer->cmds[0]);
							playeringame[netbuffer->player] = true;
							nodeingame[doomcom->remotenode] = true;
							nodesdetected[0]++;

							D_ReadUserInfoStrings (netbuffer->player, &stream, false);

							Printf ("%s connected (node %d, player %d)\n",
									players[netbuffer->player].userinfo.netname,
									doomcom->remotenode,
									netbuffer->player);
						}
					}
					break;
				}
			}
		}

		// Loop until everyone knows about everyone else.
		for (i = 0; i < doomcom->numnodes; i++)
			if (nodesdetected[i] < doomcom->numnodes)
				break;

		theresmore = i < doomcom->numnodes;

		// Send user info for local node
		for (i = 1; i < doomcom->numnodes; i++)
		{
			netbuffer->player = consoleplayer;
			netbuffer->starttic = nodesdetected[0];
			stream = &(netbuffer->cmds[0]);

			D_WriteUserInfoStrings (consoleplayer, &stream, true);

			HSendPacket (i, NCMD_SETUP|NCMD_KILL, (ptrdiff_t)stream - (ptrdiff_t)netbuffer);
		}

		// If we're the arbitrator, also send the game info packet
		if (consoleplayer == Net_Arbitrator)
		{
			for (i = 1; i < doomcom->numnodes; i++)
			{
				netbuffer->retransmitfrom = 0;
				netbuffer->player = VERSION;
				netbuffer->numtics = 0;

				stream = &(netbuffer->cmds[0]);

				WriteString (startmap, &stream);
				WriteLong (rngseed, &stream);
				C_WriteCVars (&stream, CVAR_SERVERINFO, true);

				HSendPacket (i, NCMD_SETUP, (ptrdiff_t)stream - (ptrdiff_t)netbuffer);
			}
		}
	} while (theresmore);
}

//
// D_CheckNetGame
// Works out player numbers among the net participants
//
extern int viewangleoffset;

void D_CheckNetGame (void)
{
	int i;

	for (i = 0; i < MAXNETNODES; i++)
	{
		nodeingame[i] = false;
		nettics[i] = 0;
		remoteresend[i] = false;		// set when local needs tics
		resendto[i] = 0;				// which tic to start sending
	}

	// I_InitNetwork sets doomcom and netgame
	I_InitNetwork ();
	if (doomcom->id != DOOMCOM_ID)
		I_FatalError ("Doomcom buffer invalid!");
	
	netbuffer = &doomcom->data;
	consoleplayer = displayplayer = doomcom->consoleplayer;

	// [RH] Setup user info
	D_SetupUserInfo ();

	if (netgame)
		D_ArbitrateNetStart ();

	// read values out of doomcom
	ticdup = doomcom->ticdup;
	maxsend = BACKUPTICS/(2*ticdup)-1;
	if (maxsend<1)
		maxsend = 1;

	for (i = 0; i < doomcom->numplayers; i++)
		playeringame[i] = true;
	for (i = 0; i < doomcom->numnodes; i++)
		nodeingame[i] = true;

	Printf ("player %i of %i (%i nodes)\n",
			consoleplayer+1, doomcom->numplayers, doomcom->numnodes);
}


//
// D_QuitNetGame
// Called before quitting to leave a net game
// without hanging the other players
//
void STACK_ARGS D_QuitNetGame (void)
{
	int i, j;

	if (debugfile)
		fclose (debugfile);

	if (!netgame || !usergame || consoleplayer == -1 || demoplayback)
		return;

	// send a bunch of packets for security
	netbuffer->player = consoleplayer;
	netbuffer->numtics = 0;
	for (i = 0; i < 4; i++)
	{
		for (j = 1; j < doomcom->numnodes; j++)
			if (nodeingame[j])
				HSendPacket (j, NCMD_EXIT, myoffsetof(doomdata_t,cmds));
		I_WaitVBL (1);
	}
}



//
// TryRunTics
//
void TryRunTics (void)
{
	int 		i;
	int 		lowtic;
	int 		realtics;
	int 		availabletics;
	int 		counts;
	int 		numplaying;

	// get real tics
	entertic = I_WaitForTic (oldentertics * ticdup) / ticdup;
	realtics = entertic - oldentertics;
	oldentertics = entertic;

	// get available tics
	NetUpdate ();

	lowtic = INT_MAX;
	numplaying = 0;
	for (i=0 ; i<doomcom->numnodes ; i++)
	{
		if (nodeingame[i])
		{
			numplaying++;
			if (nettics[i] < lowtic)
				lowtic = nettics[i];
		}
	}
	availabletics = lowtic - gametic/ticdup;

	// decide how many tics to run
	if (realtics < availabletics-1)
		counts = realtics+1;
	else if (realtics < availabletics)
		counts = realtics;
	else
		counts = availabletics;
	
	if (counts < 1)
		counts = 1;

	frameon++;

	if (debugfile)
		fprintf (debugfile,
				 "=======real: %i  avail: %i  game: %i\n",
				 realtics, availabletics,counts);

	if (!demoplayback)
	{
		// ideally nettics[0] should be 1 - 3 tics above lowtic
		// if we are consistantly slower, speed up time
		for (i=0 ; i<MAXPLAYERS ; i++)
			if (playeringame[i] && !players[i].isbot)
				break;
		if (consoleplayer == i)
		{
			// the key player does not adapt
		}
		else
		{
			if (nettics[0] <= nettics[nodeforplayer[i]])
			{
				gametime--;
				// DPrintf ("-");
			}
			frameskip[frameon&(MAXPLAYERS-1)] = (oldnettics > nettics[nodeforplayer[i]]);
			oldnettics = nettics[0];
			{
				int k;

				// [RH] Handle all players without hardcoding it
				skiptics = 1;
				for (k = 0; k < MAXPLAYERS; k++)
					if (!frameskip[k]) {
						skiptics = 0;
						break;
					}
			}
		}
	}// !demoplayback

	// wait for new tics if needed
	while (lowtic < gametic/ticdup + counts)
	{
		NetUpdate ();
		lowtic = INT_MAX;

		for (i=0 ; i<doomcom->numnodes ; i++)
			if (nodeingame[i] && nettics[i] < lowtic)
				lowtic = nettics[i];

		if (lowtic < gametic/ticdup)
			I_Error ("TryRunTics: lowtic < gametic");

		// don't stay in here forever -- give the menu a chance to work
		if (I_GetTime ()/ticdup - entertic >= 20)
		{
			C_Ticker ();
			M_Ticker ();
			return;
		}
	}

	// run the count * ticdup tics
	while (counts--)
	{
		for (i = 0; i < ticdup; i++)
		{
			if (gametic/ticdup > lowtic)
				I_Error ("gametic>lowtic");
			if (advancedemo)
				D_DoAdvanceDemo ();
			DObject::BeginFrame ();
			C_Ticker ();
			M_Ticker ();
			G_Ticker ();
			DObject::EndFrame ();
			gametic++;

			// modify command for duplicated tics
			if (i != ticdup-1)
			{
				ticcmd_t		*cmd;
				int 			buf;
				int 			j;

				buf = (gametic/ticdup)%BACKUPTICS; 
				for (j = 0; j < MAXPLAYERS; j++)
				{
					cmd = &netcmds[j][buf];
				}
			}
		}
		NetUpdate ();	// check for new console commands
	}
}

void Net_NewMakeTic (void)
{
	specials.NewMakeTic ();
}

void Net_WriteByte (byte it)
{
	specials << it;
}

void Net_WriteWord (short it)
{
	specials << it;
}

void Net_WriteLong (int it)
{
	specials << it;
}

void Net_WriteFloat (float it)
{
	specials << it;
}

void Net_WriteString (const char *it)
{
	specials << it;
}

void Net_WriteBytes (const byte *block, int len)
{
	while (len--)
		specials << *block++;
}

//==========================================================================
//
// Dynamic buffer interface
//
//==========================================================================

FDynamicBuffer::FDynamicBuffer ()
{
	m_Data = NULL;
	m_Len = m_BufferLen = 0;
}

FDynamicBuffer::~FDynamicBuffer ()
{
	if (m_Data)
	{
		free (m_Data);
		m_Data = NULL;
	}
	m_Len = m_BufferLen = 0;
}

void FDynamicBuffer::SetData (const byte *data, int len)
{
	if (len > m_BufferLen)
	{
		m_BufferLen = (len + 255) & ~255;
		m_Data = (byte *)Realloc (m_Data, m_BufferLen);
	}
	if (data)
	{
		m_Len = len;
		memcpy (m_Data, data, len);
	}
	else
	{
		len = 0;
	}
}

byte *FDynamicBuffer::GetData (int *len)
{
	if (len)
		*len = m_Len;
	return m_Len ? m_Data : NULL;
}


// [RH] Execute a special "ticcmd". The type byte should
//		have already been read, and the stream is positioned
//		at the beginning of the command's actual data.
void Net_DoCommand (int type, byte **stream, int player)
{
	char *s = NULL;

	switch (type)
	{
	case DEM_SAY:
		{
			byte who = ReadByte (stream);

			s = ReadString (stream);
			if ((who == 0) || players[player].userinfo.team == TEAM_None)
			{ // Said to everyone
				Printf (PRINT_CHAT, "%s: %s\n", players[player].userinfo.netname, s);
				S_Sound (CHAN_VOICE, gameinfo.chatSound, 1, ATTN_NONE);
			}
			else if (players[player].userinfo.team == players[consoleplayer].userinfo.team)
			{ // Said only to members of the player's team
				Printf (PRINT_TEAMCHAT, "(%s): %s\n", players[player].userinfo.netname, s);
				S_Sound (CHAN_VOICE, gameinfo.chatSound, 1, ATTN_NONE);
			}
		}
		break;

	case DEM_MUSICCHANGE:
		s = ReadString (stream);
		S_ChangeMusic (s);
		break;

	case DEM_PRINT:
		s = ReadString (stream);
		Printf (s);
		break;

	case DEM_CENTERPRINT:
		s = ReadString (stream);
		C_MidPrint (s);
		break;

	case DEM_UINFCHANGED:
		D_ReadUserInfoStrings (player, stream, true);
		break;

	case DEM_SINFCHANGED:
		D_DoServerInfoChange (stream);
		break;

	case DEM_GIVECHEAT:
		s = ReadString (stream);
		cht_Give (&players[player], s, ReadByte (stream));
		break;

	case DEM_GENERICCHEAT:
		cht_DoCheat (&players[player], ReadByte (stream));
		break;

	case DEM_CHANGEMAP:
		// Change to another map without disconnecting other players
		s = ReadString (stream);
		strncpy (level.nextmap, s, 8);
		// Using LEVEL_NOINTERMISSION tends to throw the game out of sync.
		level.flags |= LEVEL_CHANGEMAPCHEAT;
		G_ExitLevel (0);
		break;

	case DEM_SUICIDE:
		cht_Suicide (&players[player]);
		break;

	case DEM_ADDBOT:
		{
			byte num = ReadByte (stream);
			bglobal.DoAddBot (num, s = ReadString (stream));
		}
		break;

	case DEM_KILLBOTS:
		bglobal.RemoveAllBots (true);
		Printf ("Removed all bots\n");
		break;

	case DEM_INVSEL:
		{
			byte which = ReadByte (stream);

			if (which & 0x80)
			{
				players[player].inventorytics = 5*TICRATE;
			}
			which &= 0x7f;
			if (players[player].inventory[which] > 0)
			{
				players[player].readyArtifact = (artitype_t)which;
			}
		}
		break;

	case DEM_INVUSE:
		{
			byte which = ReadByte (stream);

			if (which == arti_none)
			{ // Use one of each artifact (except puzzle artifacts)
				int i;

				for (i = 1; i < arti_firstpuzzitem; i++)
				{
					P_PlayerUseArtifact (&players[player], (artitype_t)i);
				}
			}
			else
			{
				if (players[player].inventorytics)
				{
					players[player].inventorytics = 0;
				}
				else
				{
					P_PlayerUseArtifact (&players[player], (artitype_t)which);
				}
			}
		}
		break;

	case DEM_WEAPSEL:
		{
			byte which = ReadByte (stream);
			FWeaponInfo **infos = (players[player].powers[pw_weaponlevel2]
				&& deathmatch) ? wpnlev2info : wpnlev1info;

			if (which < NUMWEAPONS
				&& which != players[player].readyweapon
				&& players[player].weaponowned[which]
				&& (infos[which]->ammo >= NUMAMMO ||
				    players[player].ammo[infos[which]->ammo] >= infos[which]->ammouse))
			{
				// The actual changing of the weapon is done when the weapon
				// psprite can do it (A_WeaponReady), so it doesn't happen in
				// the middle of an attack.
				players[player].pendingweapon = (weapontype_t)which;
			}
		}
		break;

	case DEM_WEAPSLOT:
		{
			byte slot = ReadByte (stream);

			if (slot < NUM_WEAPON_SLOTS)
			{
				weapontype_t newweap = WeaponSlots[slot].PickWeapon (&players[player]);
				if (newweap < NUMWEAPONS && newweap != players[player].readyweapon)
				{
					players[player].pendingweapon = newweap;
				}
			}
		}
		break;

	case DEM_WEAPNEXT:
	case DEM_WEAPPREV:
		{
			weapontype_t newweap;
			
			if (type == DEM_WEAPNEXT)
				newweap = PickNextWeapon (&players[player]);
			else
				newweap = PickPrevWeapon (&players[player]);

			if (newweap != players[player].readyweapon)
			{
				players[player].pendingweapon = newweap;
			}
		}
		break;

	case DEM_SUMMON:
		{
			const TypeInfo *type;

			s = ReadString (stream);
			type = TypeInfo::FindType (s);
			if (type != NULL && type->ActorInfo != NULL)
			{
				AActor *source = players[player].mo;
				Spawn (type, source->x + 64 * finecosine[source->angle>>ANGLETOFINESHIFT],
							 source->y + 64 * finesine[source->angle>>ANGLETOFINESHIFT],
							 source->z + 8 * FRACUNIT);
			}
		}
		break;

	case DEM_PAUSE:
		if (paused)
		{
			paused = 0;
			S_ResumeSound ();
		}
		else
		{
			paused = player + 1;
			S_PauseSound ();
		}
		BorderNeedRefresh = screen->GetPageCount ();
		break;

	case DEM_SAVEGAME:
		savegamefile = ReadString (stream);
		s = ReadString (stream);
		memset (savedescription, 0, sizeof(savedescription));
		strcpy (savedescription, s);
		gameaction = ga_savegame;
		break;

	default:
		I_Error ("Unknown net command: %d", type);
		break;
	}

	if (s)
		delete[] s;
}

void Net_SkipCommand (int type, byte **stream)
{
	BYTE t;
	int skip;

	switch (type)
	{
		case DEM_SAY:
		case DEM_ADDBOT:
			skip = strlen ((char *)(*stream + 1)) + 2;
			break;

		case DEM_GIVECHEAT:
			skip = strlen ((char *)(*stream)) + 2;
			break;

		case DEM_MUSICCHANGE:
		case DEM_PRINT:
		case DEM_CENTERPRINT:
		case DEM_UINFCHANGED:
		case DEM_CHANGEMAP:
		case DEM_SUMMON:
			skip = strlen ((char *)(*stream)) + 1;
			break;

		case DEM_GENERICCHEAT:
		case DEM_DROPPLAYER:
		case DEM_INVSEL:
		case DEM_INVUSE:
		case DEM_WEAPSEL:
		case DEM_WEAPSLOT:
			skip = 1;
			break;

		case DEM_SAVEGAME:
			skip = strlen ((char *)(*stream)) + 1;
			skip += strlen ((char *)(*stream) + skip) + 1;
			break;

		case DEM_SINFCHANGED:
			t = **stream;
			skip = 1 + (t & 63);
			switch (t >> 6)
			{
			case CVAR_Bool:
				skip += 1;
				break;
			case CVAR_Int: case CVAR_Float:
				skip += 4;
				break;
			case CVAR_String:
				skip += strlen ((char *)(*stream + skip)) + 1;
				break;
			}
			break;

		default:
			return;
	}

	*stream += skip;
}

// [RH] List "ping" times
CCMD (pings)
{
	int i;

	for (i = 0; i < MAXPLAYERS; i++)
		if (playeringame[i])
			Printf ("% 4d %s\n", currrecvtime[i] - lastrecvtime[i],
					players[i].userinfo.netname);
}
