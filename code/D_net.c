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

#include "version.h"
#include "m_alloc.h"
#include "m_menu.h"
#include "i_system.h"
#include "i_video.h"
#include "i_net.h"
#include "g_game.h"
#include "doomdef.h"
#include "doomstat.h"
#include "c_consol.h"
#include "d_netinf.h"
#include "cmdlib.h"
#include "s_sound.h"
#include "m_cheat.h"
#include "p_effect.h"
#include "p_local.h"

#define NCMD_EXIT				0x80000000
#define NCMD_RETRANSMIT 		0x40000000
#define NCMD_SETUP				0x20000000
#define NCMD_KILL				0x10000000		// kill game
#define NCMD_CHECKSUM			0x0fffffff

extern byte		*demo_p;		// [RH] Special "ticcmds" get recorded in demos

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

// [RH] Special "ticcmds" get stored in here
static struct {
	byte *streams[BACKUPTICS];
	int	  used[BACKUPTICS];
	byte *streamptr;
	int	  streamoffs;
	int   specialsize;
	int	  lastmaketic;
	BOOL  okay;
} specials;

void D_ProcessEvents (void); 
void G_BuildTiccmd (ticcmd_t *cmd); 
void D_DoAdvanceDemo (void);
 
BOOL	 		reboundpacket;
doomdata_t		reboundstore;



//
// [RH] Rewritten to properly calculate the packet size
//		with our variable length commands.
//
int NetbufferSize (void)
{
	byte *skipper = &(netbuffer->cmds[0]);

	return (int)&(((doomdata_t *)0)->cmds[0])
			+ SkipTicCmd (&skipper, netbuffer->numtics);
}

//
// Checksum 
//
unsigned NetbufferChecksum (int len)
{
	unsigned	c;
	int 		i,l;

	c = 0x1234567;

	// FIXME -endianess?
//#ifdef NORMALUNIX
//	return 0;					// byte order problems
//#endif

	l = (len - (int)&(((doomdata_t *)0)->retransmitfrom))/4;
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
			fprintf (debugfile,"bad packet length %i\n",doomcom->datalength);
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

			for (i=0 ; i<doomcom->datalength ; i++)
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
	ticcmd_t	*dest;
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

			if (deathmatch->value) {
				Printf (PRINT_HIGH, "%s left the game with %d frags\n",
						 players[netconsole].userinfo.netname,
						 players[netconsole].fragcount);
			} else {
				Printf (PRINT_HIGH, "%s left the game\n", players[netconsole].userinfo.netname);
			}

			// [RH] Make the player disappear
			P_DisconnectEffect (players[netconsole].mo);
			P_RemoveMobj (players[netconsole].mo);
			players[netconsole].mo = NULL;

			if (demorecording) {
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
		if ( resendcount[netnode] <= 0 
			 && (netbuffer->checksum & NCMD_RETRANSMIT) )
		{
			resendto[netnode] = ExpandTics(netbuffer->retransmitfrom);
			if (debugfile)
				fprintf (debugfile,"retransmit from %i\n", resendto[netnode]);
			resendcount[netnode] = RESENDCOUNT;
		}
		else
			resendcount[netnode]--;
		
		// check for out of order / duplicated packet			
		if (realend == nettics[netnode])
			continue;
						
		if (realend < nettics[netnode])
		{
			if (debugfile)
				fprintf (debugfile,
						 "out of order packet (%i + %i)\n" ,
						 realstart,netbuffer->numtics);
			continue;
		}
		
		// check for a missed packet
		if (realstart > nettics[netnode])
		{
			// stop processing until the other system resends the missed tics
			if (debugfile)
				fprintf (debugfile,
						 "missed tics from %i (%i - %i)\n",
						 netnode, realstart, nettics[netnode]);
			remoteresend[netnode] = true;
			continue;
		}

		// update command store from the packet
		{
			byte *start;

			remoteresend[netnode] = false;

			start = &(netbuffer->cmds[0]);
			SkipTicCmd (&start, nettics[netnode] - realstart);

			while (nettics[netnode] < realend)
			{
				dest = &netcmds[netconsole][nettics[netnode]%BACKUPTICS];
				nettics[netnode]++;
				ReadTicCmd (dest, &start, netconsole);
			}
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
	for (i=0 ; i<newtics ; i++)
	{
		I_StartTic ();
		D_ProcessEvents ();
		if (maketic - gameticdiv >= BACKUPTICS/2-1)
			break;			// can't hold any more
		
		//Printf (PRINT_HIGH, "mk:%i ",maketic);
		G_BuildTiccmd (&localcmds[maketic%BACKUPTICS]);
		maketic++;
		Net_NewMakeTic ();
	}


	if (singletics)
		return; 		// singletic update is syncronous
	
	// send the packet to the other nodes
	for (i=0 ; i<doomcom->numnodes ; i++)
		if (nodeingame[i])
		{
			netbuffer->starttic = realstart = resendto[i];
			netbuffer->numtics = maketic - realstart;
			if (netbuffer->numtics > BACKUPTICS)
				I_Error ("NetUpdate: netbuffer->numtics > BACKUPTICS");

			resendto[i] = maketic - doomcom->extratics;

			cmddata = &(netbuffer->cmds[0]);
			for (j=0; j < netbuffer->numtics; j++) {
				int start = (realstart+j)%BACKUPTICS;

				WriteWord (localcmds[start].consistancy, &cmddata);
				// [RH] Write out special "ticcmds" before real ticcmd
				if (specials.used[start]) {
					memcpy (cmddata, specials.streams[start], specials.used[start]);
					cmddata += specials.used[start];
				}
				WriteByte (DEM_USERCMD, &cmddata);
				PackUserCmd (&localcmds[start].ucmd, &cmddata);
			}

			if (remoteresend[i])
			{
				netbuffer->retransmitfrom = nettics[i];
				HSendPacket (i, NCMD_RETRANSMIT, (int)cmddata - (int)netbuffer);
			}
			else
			{
				netbuffer->retransmitfrom = 0;
				HSendPacket (i, 0, (int)cmddata - (int)netbuffer);
			}
		}
	
	// listen for other packets
  listen:
	GetPackets ();
}



//
// CheckAbort
//
void CheckAbort (void)
{
	event_t *ev;
	int stoptic;

	Printf (PRINT_HIGH, "");	// [RH] Give the console a chance to redraw itself
	stoptic = I_GetTime () + 2; 
	while (I_GetTime() < stoptic) 
		I_StartTic (); 

	I_StartTic ();
	for ( ; eventtail != eventhead 
		  ; eventtail = (++eventtail)&(MAXEVENTS-1) ) 
	{ 
		ev = &events[eventtail]; 
		if (ev->type == ev_keydown && ev->data1 == KEY_ESCAPE)
			I_Error ("Network game synchronization aborted.");
	} 
}


//
// D_ArbitrateNetStart
//
void D_ArbitrateNetStart (void)
{
	extern int	rngseed;
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

	memset (nodesdetected, 0, sizeof(nodesdetected));
	nodesdetected[0] = 1;	// Detect ourselves

	// [RH] Rewrote this loop based on Doom Legacy 1.11's code.
	Printf (PRINT_HIGH, "Waiting for %d more player%s...\n",
		doomcom->numnodes - 1, (doomcom->numnodes == 2) ? "" : "s");
	do {
		CheckAbort ();

		for (i = 10; i > 0; i--) {
			if (HGetPacket ()) {
				switch (netbuffer->checksum & (NCMD_SETUP|NCMD_KILL)) {
					case NCMD_SETUP:
						// game info
						if (!gotsetup) {
							gotsetup = true;
							if (netbuffer->player != VERSION)
								I_Error ("Different DOOM versions cannot play a net game!");

							stream = &(netbuffer->cmds[0]);

							s = ReadString (&stream);
							strncpy (startmap, s, 8);
							free (s);
							rngseed = ReadLong (&stream);
							C_ReadCVars (&stream);
						}
						break;

					case NCMD_SETUP|NCMD_KILL:
						// user info
						if (doomcom->remotenode) {
							nodesdetected[doomcom->remotenode] = netbuffer->starttic;

							if (!nodeingame[doomcom->remotenode]) {
								stream = &(netbuffer->cmds[0]);
								playeringame[netbuffer->player] = true;
								nodeingame[doomcom->remotenode] = true;
								nodesdetected[0]++;

								D_ReadUserInfoStrings (netbuffer->player, &stream, false);

								Printf (PRINT_HIGH, "%s joined the game (node %d, player %d)\n",
										players[netbuffer->player].userinfo.netname,
										doomcom->remotenode,
										netbuffer->player);
							}
						}
						break;
				}
			}
		}

		// Send user info for local node
		for (i = 1; i < doomcom->numnodes; i++) {
			netbuffer->player = consoleplayer;
			netbuffer->starttic = nodesdetected[0];
			stream = &(netbuffer->cmds[0]);

			D_WriteUserInfoStrings (consoleplayer, &stream);

			HSendPacket (i, NCMD_SETUP|NCMD_KILL, (int)stream - (int)netbuffer);
		}

		// If we're the key player, also send the game info packet
		if (!consoleplayer) {
			for (i = 1; i < doomcom->numnodes; i++) {
				netbuffer->retransmitfrom = 0;
				netbuffer->player = VERSION;
				netbuffer->numtics = 0;

				stream = &(netbuffer->cmds[0]);

				WriteString (startmap, &stream);
				WriteLong (rngseed, &stream);
				C_WriteCVars (&stream, CVAR_SERVERINFO);

				HSendPacket (i, NCMD_SETUP, (int)stream - (int)netbuffer);
			}
		}

		// Loop until everyone knows about everyone else.
		for (i = 0; i < doomcom->numnodes; i++)
			if (nodesdetected[i] < doomcom->numnodes)
				break;
	} while (i < doomcom->numnodes);
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

	// [RH] Prepare special "ticcmds" for use.
	specials.lastmaketic = -1;
	specials.specialsize = 256;
	for (i = 0; i < BACKUPTICS; i++) {
		specials.streams[i] = Malloc (256);
		specials.used[i] = 0;
	}
	specials.okay = true;

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
		
	Printf (PRINT_HIGH, "player %i of %i (%i nodes)\n",
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
int 	frameon;
int 	frameskip[MAXPLAYERS];
int 	oldnettics;

extern	BOOL	 advancedemo;

void TryRunTics (void)
{
	int 		i;
	int 		lowtic;
	int 		entertic;
	static int	oldentertics;
	int 		realtics;
	int 		availabletics;
	int 		counts;
	int 		numplaying;
	
	// get real tics
	entertic = I_GetTime ()/ticdup;
	realtics = entertic - oldentertics;
	oldentertics = entertic;
	
	// get available tics
	NetUpdate ();

	lowtic = MAXINT;
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
			if (playeringame[i])
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
	}// demoplayback

	// wait for new tics if needed
	while (lowtic < gametic/ticdup + counts)
	{
		NetUpdate ();
		lowtic = MAXINT;

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
		for (i=0 ; i<ticdup ; i++)
		{
			if (gametic/ticdup > lowtic)
				I_Error ("gametic>lowtic");
			if (advancedemo)
				D_DoAdvanceDemo ();
			C_Ticker ();
			M_Ticker ();
			G_Ticker ();
			gametic++;

			// modify command for duplicated tics
			if (i != ticdup-1)
			{
				ticcmd_t		*cmd;
				int 			buf;
				int 			j;

				buf = (gametic/ticdup)%BACKUPTICS; 
				for (j=0 ; j<MAXPLAYERS ; j++)
				{
					cmd = &netcmds[j][buf];
					if (cmd->ucmd.buttons & BT_SPECIAL)
						cmd->ucmd.buttons = 0;
				}
			}
		}
		NetUpdate ();	// check for new console commands
	}
}

// Make more room for special commands.
static void BuyMoreSpecialSpace (void)
{
	int i;

	specials.specialsize <<= 1;

	DPrintf ("Expanding special size to %d\n", specials.specialsize);

	for (i = 0; i < BACKUPTICS; i++)
		specials.streams[i] = Realloc (specials.streams[i], specials.specialsize);

	specials.streamptr = specials.streams[maketic%BACKUPTICS] + specials.streamoffs;
}

static void CheckSpecialSpace (int needed)
{
	if (specials.streamoffs >= specials.specialsize - needed)
		BuyMoreSpecialSpace ();

	specials.streamoffs += needed;
}

void Net_NewMakeTic (void)
{
	if (specials.lastmaketic != -1)
		specials.used[specials.lastmaketic%BACKUPTICS] = specials.streamoffs;

	specials.lastmaketic = maketic;
	specials.streamptr = specials.streams[maketic%BACKUPTICS];
	specials.streamoffs = 0;
}

void Net_WriteByte (byte it)
{
	if (specials.okay) {
		CheckSpecialSpace (1);
		WriteByte (it, &specials.streamptr);
	}
}

void Net_WriteWord (short it)
{
	if (specials.okay) {
		CheckSpecialSpace (2);
		WriteWord (it, &specials.streamptr);
	}
}

void Net_WriteLong (int it)
{
	if (specials.okay) {
		CheckSpecialSpace (4);
		WriteLong (it, &specials.streamptr);
	}
}

void Net_WriteString (const char *it)
{
	if (specials.okay) {
		CheckSpecialSpace (strlen (it) + 1);
		WriteString (it, &specials.streamptr);
	}
}

// [RH] Execute a special "ticcmd". The type byte should
//		have already been read, and the stream is positioned
//		at the beginning of the command's actual data.
void Net_DoCommand (int type, byte **stream, int player)
{
	char *s = NULL;

	switch (type) {
		case DEM_SAY:
			{
				byte who = ReadByte (stream);

				s = ReadString (stream);
				if ((who == 0) || players[player].userinfo.team[0] == 0) {
					// Said to everyone
					Printf (PRINT_CHAT, "%s: %s\n", players[player].userinfo.netname, s);
					
					if (gamemode == commercial) {
						S_Sound (NULL, CHAN_VOICE, "misc/chat", 1, ATTN_NONE);
					} else {
						S_Sound (NULL, CHAN_VOICE, "misc/chat2", 1, ATTN_NONE);
					}
				} else if (!stricmp (players[player].userinfo.team,
									 players[consoleplayer].userinfo.team)) {
					// Said only to members of the player's team
					Printf (PRINT_TEAMCHAT, "(%s): %s\n", players[player].userinfo.netname, s);
					
					if (gamemode == commercial) {
						S_Sound (NULL, CHAN_VOICE, "misc/chat", 1, ATTN_NONE);
					} else {
						S_Sound (NULL, CHAN_VOICE, "misc/chat2", 1, ATTN_NONE);
					}
				}
			}
			break;

		case DEM_MUSICCHANGE:
			s = ReadString (stream);
			S_ChangeMusic (s, true);
			break;

		case DEM_PRINT:
			s = ReadString (stream);
			Printf (PRINT_HIGH, s);
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
			cht_Give (players + player, s = ReadString (stream));
			break;

		case DEM_GENERICCHEAT:
			cht_DoCheat (players + player, ReadByte (stream));
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
			cht_Suicide (players + player);
			break;

		default:
			I_Error ("Unknown net command: %d", type);
			break;
	}

	if (s)
		free (s);
}

// [RH] List ping times
void Cmd_Pings (void *plyr, int argc, char **argv)
{
	int i;

	for (i = 0; i < MAXPLAYERS; i++)
		if (playeringame[i])
			Printf (PRINT_HIGH, "% 4u %s\n", currrecvtime[i] - lastrecvtime[i],
					players[i].userinfo.netname);
}
