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
#include "menu/menu.h"
#include "m_random.h"
#include "i_system.h"
#include "i_video.h"
#include "i_net.h"
#include "g_game.h"
#include "doomdef.h"
#include "doomstat.h"
#include "c_console.h"
#include "d_netinf.h"
#include "d_net.h"
#include "cmdlib.h"
#include "s_sound.h"
#include "m_cheat.h"
#include "p_effect.h"
#include "p_local.h"
#include "c_dispatch.h"
#include "sbar.h"
#include "gi.h"
#include "m_misc.h"
#include "gameconfigfile.h"
#include "d_gui.h"
#include "templates.h"
#include "p_acs.h"
#include "p_trace.h"
#include "a_sharedglobal.h"
#include "st_start.h"
#include "teaminfo.h"
#include "p_conversation.h"
#include "g_level.h"
#include "d_event.h"
#include "m_argv.h"
#include "p_lnspec.h"
#include "v_video.h"
#include "p_spec.h"
#include "hardware.h"
#include "intermission/intermission.h"

EXTERN_CVAR (Int, disableautosave)
EXTERN_CVAR (Int, autosavecount)

//#define SIMULATEERRORS		(RAND_MAX/3)
#define SIMULATEERRORS			0

extern BYTE		*demo_p;		// [RH] Special "ticcmds" get recorded in demos
extern char		savedescription[SAVESTRINGSIZE];
extern FString	savegamefile;

extern short consistancy[MAXPLAYERS][BACKUPTICS];

doomcom_t		doomcom;
#define netbuffer (doomcom.data)

enum { NET_PeerToPeer, NET_PacketServer };
BYTE NetMode = NET_PeerToPeer;



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

ticcmd_t		localcmds[LOCALCMDTICS];

FDynamicBuffer	NetSpecs[MAXPLAYERS][BACKUPTICS];
ticcmd_t		netcmds[MAXPLAYERS][BACKUPTICS];
int 			nettics[MAXNETNODES];
bool 			nodeingame[MAXNETNODES];				// set false as nodes leave game
bool			nodejustleft[MAXNETNODES];				// set when a node just left
bool	 		remoteresend[MAXNETNODES];				// set when local needs tics
int 			resendto[MAXNETNODES];					// set when remote needs tics
int 			resendcount[MAXNETNODES];

unsigned int	lastrecvtime[MAXPLAYERS];				// [RH] Used for pings
unsigned int	currrecvtime[MAXPLAYERS];
unsigned int	lastglobalrecvtime;						// Identify the last time a packet was recieved.
bool			hadlate;

int 			nodeforplayer[MAXPLAYERS];
int				playerfornode[MAXNETNODES];

int 			maketic;
int 			skiptics;
int 			ticdup; 		

void D_ProcessEvents (void); 
void G_BuildTiccmd (ticcmd_t *cmd); 
void D_DoAdvanceDemo (void);

static void SendSetup (DWORD playersdetected[MAXNETNODES], BYTE gotsetup[MAXNETNODES], int len);
static void RunScript(BYTE **stream, APlayerPawn *pawn, int snum, int argn, int always);

int		reboundpacket;
BYTE	reboundstore[MAX_MSGLEN];

int 	frameon;
int 	frameskip[4];
int 	oldnettics;
int		mastertics;

static int 	entertic;
static int	oldentertics;

extern	bool	 advancedemo;

CUSTOM_CVAR (Bool, cl_capfps, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	// Do not use the separate FPS limit timer if we are limiting FPS with this.
	if (self)
	{
		I_SetFPSLimit(0);
	}
	else
	{
		I_SetFPSLimit(-1);
	}
}

// [RH] Special "ticcmds" get stored in here
static struct TicSpecial
{
	BYTE *streams[BACKUPTICS];
	size_t used[BACKUPTICS];
	BYTE *streamptr;
	size_t streamoffs;
	int   specialsize;
	int	  lastmaketic;
	bool  okay;

	TicSpecial ()
	{
		int i;

		lastmaketic = -1;
		specialsize = 256;

		for (i = 0; i < BACKUPTICS; i++)
			streams[i] = NULL;

		for (i = 0; i < BACKUPTICS; i++)
		{
			streams[i] = (BYTE *)M_Malloc (256);
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
				M_Free (streams[i]);
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
			streams[i] = (BYTE *)M_Realloc (streams[i], specialsize);

		streamptr = streams[(maketic/ticdup)%BACKUPTICS] + streamoffs;
	}

	void CheckSpace (size_t needed)
	{
		if (streamoffs >= specialsize - needed)
			GetMoreSpace ();

		streamoffs += needed;
	}

	void NewMakeTic ()
	{
		int mt = maketic / ticdup;
		if (lastmaketic != -1)
		{
			if (lastmaketic == mt)
				return;
			used[lastmaketic%BACKUPTICS] = streamoffs;
		}

		lastmaketic = mt;
		streamptr = streams[mt%BACKUPTICS];
		streamoffs = 0;
	}

	TicSpecial &operator << (BYTE it)
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
	memset (consistancy, 0, sizeof(consistancy));
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

	lastglobalrecvtime = 0;
}

//
// [RH] Rewritten to properly calculate the packet size
//		with our variable length commands.
//
int NetbufferSize ()
{
	if (netbuffer[0] & (NCMD_EXIT | NCMD_SETUP))
	{
		return doomcom.datalength;
	}

	int k = 2, count, numtics;

	if (netbuffer[0] & NCMD_RETRANSMIT)
		k++;

	if (NetMode == NET_PacketServer && doomcom.remotenode == nodeforplayer[Net_Arbitrator])
		k++;

	numtics = netbuffer[0] & NCMD_XTICS;
	if (numtics == 3)
	{
		numtics += netbuffer[k++];
	}

	if (netbuffer[0] & NCMD_QUITTERS)
	{
		k += netbuffer[k] + 1;
	}

	if (netbuffer[0] & NCMD_MULTI)
	{
		count = netbuffer[k];
		k += count;
	}
	else
	{
		count = 1;
	}

	// Need at least 3 bytes per tic per player
	if (doomcom.datalength < k + 3 * count * numtics)
	{
		return k + 3 * count * numtics;
	}

	BYTE *skipper = &netbuffer[k];
	if ((netbuffer[0] & NCMD_EXIT) == 0)
	{
		while (count-- > 0)
		{
			SkipTicCmd (&skipper, numtics);
		}
	}
	return int(skipper - netbuffer);
}

//
//
//
int ExpandTics (int low)
{
	int delta;
	int mt = maketic / ticdup;

	delta = low - (mt&0xff);
		
	if (delta >= -64 && delta <= 64)
		return (mt&~0xff) + low;
	if (delta > 64)
		return (mt&~0xff) - 256 + low;
	if (delta < -64)
		return (mt&~0xff) + 256 + low;
				
	I_Error ("ExpandTics: strange value %i at maketic %i", low, maketic);
	return 0;
}



//
// HSendPacket
//
void HSendPacket (int node, int len)
{
	if (debugfile && node != 0)
	{
		int i, k, realretrans;

		if (netbuffer[0] & NCMD_SETUP)
		{
			fprintf (debugfile,"%i/%i send %i = SETUP [%3i]", gametic, maketic, node, len);
			for (i = 0; i < len; i++)
				fprintf (debugfile," %2x", ((BYTE *)netbuffer)[i]);
		}
		else if (netbuffer[0] & NCMD_EXIT)
		{
			fprintf (debugfile,"%i/%i send %i = EXIT [%3i]", gametic, maketic, node, len);
			for (i = 0; i < len; i++)
				fprintf (debugfile," %2x", ((BYTE *)netbuffer)[i]);
		}
		else
		{
			k = 2;

			if (NetMode == NET_PacketServer && consoleplayer == Net_Arbitrator &&
				node != 0)
			{
				k++;
			}

			if (netbuffer[0] & NCMD_RETRANSMIT)
				realretrans = ExpandTics (netbuffer[k++]);
			else
				realretrans = -1;

			int numtics = netbuffer[0] & 3;
			if (numtics == 3)
				numtics += netbuffer[k++];

			fprintf (debugfile,"%i/%i send %i = (%i + %i, R %i) [%3i]",
					gametic, maketic,
					node,
					ExpandTics(netbuffer[1]),
					numtics, realretrans, len);
			
			for (i = 0; i < len; i++)
				fprintf (debugfile, "%c%2x", i==k?'|':' ', ((BYTE *)netbuffer)[i]);
		}
		fprintf (debugfile, " [[ ");
		for (i = 0; i < doomcom.numnodes; ++i)
		{
			if (nodeingame[i])
			{
				fprintf (debugfile, "%d ", nettics[i]);
			}
			else
			{
				fprintf (debugfile, "--- ");
			}
		}
		fprintf (debugfile, "]]\n");
	}

	if (node == 0)
	{
		memcpy (reboundstore, netbuffer, len);
		reboundpacket = len;
		return;
	}

	if (demoplayback)
		return;

	if (!netgame)
		I_Error ("Tried to transmit to another node");

#if SIMULATEERRORS
	if (rand() < SIMULATEERRORS)
	{
		if (debugfile)
			fprintf (debugfile, "Drop!\n");
		return;
	}
#endif

	doomcom.command = CMD_SEND;
	doomcom.remotenode = node;
	doomcom.datalength = len;

	I_NetCmd ();
}

//
// HGetPacket
// Returns false if no packet is waiting
//
bool HGetPacket (void)
{
	if (reboundpacket)
	{
		memcpy (netbuffer, reboundstore, reboundpacket);
		doomcom.remotenode = 0;
		reboundpacket = 0;
		return true;
	}

	if (!netgame)
		return false;

	if (demoplayback)
		return false;
				
	doomcom.command = CMD_GET;
	I_NetCmd ();
	
	if (doomcom.remotenode == -1)
		return false;
		
	if (debugfile)
	{
		int i, k, realretrans;

		if (netbuffer[0] & NCMD_SETUP)
		{
			fprintf (debugfile,"%i/%i  get %i = SETUP [%3i]", gametic, maketic, doomcom.remotenode, doomcom.datalength);
			for (i = 0; i < doomcom.datalength; i++)
				fprintf (debugfile, " %2x", ((BYTE *)netbuffer)[i]);
			fprintf (debugfile, "\n");
		}
		else if (netbuffer[0] & NCMD_EXIT)
		{
			fprintf (debugfile,"%i/%i  get %i = EXIT [%3i]", gametic, maketic, doomcom.remotenode, doomcom.datalength);
			for (i = 0; i < doomcom.datalength; i++)
				fprintf (debugfile, " %2x", ((BYTE *)netbuffer)[i]);
			fprintf (debugfile, "\n");
		}
		else		{
			k = 2;

			if (NetMode == NET_PacketServer &&
				doomcom.remotenode == nodeforplayer[Net_Arbitrator])
			{
				k++;
			}

			if (netbuffer[0] & NCMD_RETRANSMIT)
				realretrans = ExpandTics (netbuffer[k++]);
			else
				realretrans = -1;

			int numtics = netbuffer[0] & 3;
			if (numtics == 3)
				numtics += netbuffer[k++];

			fprintf (debugfile,"%i/%i  get %i = (%i + %i, R %i) [%3i]",
					gametic, maketic,
					doomcom.remotenode,
					ExpandTics(netbuffer[1]),
					numtics, realretrans, doomcom.datalength);
			
			for (i = 0; i < doomcom.datalength; i++)
				fprintf (debugfile, "%c%2x", i==k?'|':' ', ((BYTE *)netbuffer)[i]);
			if (numtics)
				fprintf (debugfile, " <<%4x>>\n",
					consistancy[playerfornode[doomcom.remotenode]][nettics[doomcom.remotenode]%BACKUPTICS] & 0xFFFF);
			else
				fprintf (debugfile, "\n");
		}
	}

	if (doomcom.datalength != NetbufferSize ())
	{
		if (debugfile)
			fprintf (debugfile,"---bad packet length %i (calculated %i)\n",
				doomcom.datalength, NetbufferSize());
		return false;
	}

	return true;		
}

void PlayerIsGone (int netnode, int netconsole)
{
	int i;

	for (i = netnode + 1; i < doomcom.numnodes; ++i)
	{
		if (nodeingame[i])
			break;
	}
	if (i == doomcom.numnodes)
	{
		doomcom.numnodes = netnode;
	}

	nodeingame[netnode] = false;
	playeringame[netconsole] = false;
	nodejustleft[netnode] = false;

	if (deathmatch)
	{
		Printf ("%s left the game with %d frags\n",
			players[netconsole].userinfo.GetName(),
			players[netconsole].fragcount);
	}
	else
	{
		Printf ("%s left the game\n", players[netconsole].userinfo.GetName());
	}

	// [RH] Revert each player to their own view if spying through the player who left
	for (int ii = 0; ii < MAXPLAYERS; ++ii)
	{
		if (playeringame[ii] && players[ii].camera == players[netconsole].mo)
		{
			players[ii].camera = players[ii].mo;
			if (ii == consoleplayer && StatusBar != NULL)
			{
				StatusBar->AttachToPlayer (&players[ii]);
			}
		}
	}

	// [RH] Make the player disappear
	FBehavior::StaticStopMyScripts (players[netconsole].mo);
	if (players[netconsole].mo != NULL)
	{
		P_DisconnectEffect (players[netconsole].mo);
		players[netconsole].mo->player = NULL;
		players[netconsole].mo->Destroy ();
		if (!(players[netconsole].mo->ObjectFlags & OF_EuthanizeMe))
		{ // We just destroyed a morphed player, so now the original player
		  // has taken their place. Destroy that one too.
			players[netconsole].mo->Destroy();
		}
		players[netconsole].mo = NULL;
		players[netconsole].camera = NULL;
	}
	// [RH] Let the scripts know the player left
	FBehavior::StaticStartTypedScripts (SCRIPT_Disconnect, NULL, true, netconsole);
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
				players[i].settings_controller = true;
				Printf ("%s is the new arbitrator\n", players[i].userinfo.GetName());
				break;
			}
		}
		if (debugfile && NetMode == NET_PacketServer)
		{
			if (Net_Arbitrator == consoleplayer)
			{
				fprintf (debugfile, "I am the new master!\n");
			}
			else
			{
				fprintf (debugfile, "Node %d is the new master!\n", nodeforplayer[Net_Arbitrator]);
			}
		}
	}

	if (demorecording)
	{
		G_CheckDemoStatus ();

		//WriteByte (DEM_DROPPLAYER, &demo_p);
		//WriteByte ((BYTE)netconsole, &demo_p);
	}
}

//
// GetPackets
//

void GetPackets (void)
{
	int netconsole;
	int netnode;
	int realend;
	int realstart;
	int numtics;
	int retransmitfrom;
	int k;
	BYTE playerbytes[MAXNETNODES];
	int numplayers;
								 
	while ( HGetPacket() )
	{
		if (netbuffer[0] & NCMD_SETUP)
		{
			if (consoleplayer == Net_Arbitrator)
			{
				// This player apparantly doesn't realise the game has started
				netbuffer[0] = NCMD_SETUP+3;
				HSendPacket (doomcom.remotenode, 1);
			}
			continue;			// extra setup packet
		}
						
		netnode = doomcom.remotenode;
		netconsole = playerfornode[netnode] & ~PL_DRONE;

		// [RH] Get "ping" times - totally useless, since it's bound to the frequency
		// packets go out at.
		lastrecvtime[netconsole] = currrecvtime[netconsole];
		currrecvtime[netconsole] = I_MSTime ();

		// check for exiting the game
		if (netbuffer[0] & NCMD_EXIT)
		{
			if (!nodeingame[netnode])
				continue;

			if (NetMode != NET_PacketServer || netconsole == Net_Arbitrator)
			{
				PlayerIsGone (netnode, netconsole);
				if (NetMode == NET_PacketServer)
				{
					BYTE *foo = &netbuffer[2];
					for (int i = 0; i < MAXPLAYERS; ++i)
					{
						if (playeringame[i])
						{
							int resend = ReadLong (&foo);
							if (i != consoleplayer)
							{
								resendto[nodeforplayer[i]] = resend;
							}
						}
					}
				}
			}
			else
			{
				nodeingame[netnode] = false;
				playeringame[netconsole] = false;
				nodejustleft[netnode] = true;
			}
			continue;
		}

		k = 2;

		if (NetMode == NET_PacketServer &&
			netconsole == Net_Arbitrator &&
			netconsole != consoleplayer)
		{
			mastertics = ExpandTics (netbuffer[k++]);
		}

		if (netbuffer[0] & NCMD_RETRANSMIT)
		{
			retransmitfrom = netbuffer[k++];
		}
		else
		{
			retransmitfrom = 0;
		}

		numtics = (netbuffer[0] & NCMD_XTICS);
		if (numtics == 3)
		{
			numtics += netbuffer[k++];
		}

		if (netbuffer[0] & NCMD_QUITTERS)

		{
			numplayers = netbuffer[k++];
			for (int i = 0; i < numplayers; ++i)
			{
				PlayerIsGone (nodeforplayer[netbuffer[k]], netbuffer[k]);
				k++;
			}
		}

		playerbytes[0] = netconsole;
		if (netbuffer[0] & NCMD_MULTI)
		{
			numplayers = netbuffer[k++];
			memcpy (playerbytes+1, &netbuffer[k], numplayers - 1);
			k += numplayers - 1;
		}
		else
		{
			numplayers = 1;
		}

		// to save bytes, only the low byte of tic numbers are sent
		// Figure out what the rest of the bytes are
		realstart = ExpandTics (netbuffer[1]);
		realend = (realstart + numtics);
		
		nodeforplayer[netconsole] = netnode;
		
		// check for retransmit request
		if (resendcount[netnode] <= 0 && (netbuffer[0] & NCMD_RETRANSMIT))
		{
			resendto[netnode] = ExpandTics (retransmitfrom);
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
						 realstart, numtics);
			continue;
		}
		
		// check for a missed packet
		if (realstart > nettics[netnode])
		{
			// stop processing until the other system resends the missed tics
			if (debugfile)
				fprintf (debugfile, "missed tics from %i (%i to %i)\n",
						 netnode, nettics[netnode], realstart);
			remoteresend[netnode] = true;
			continue;
		}

		// update command store from the packet
		{
			BYTE *start;
			int i, tics;
			remoteresend[netnode] = false;

			start = &netbuffer[k];

			for (i = 0; i < numplayers; ++i)
			{
				int node = !players[playerbytes[i]].isbot ?
					nodeforplayer[playerbytes[i]] : netnode;

				SkipTicCmd (&start, nettics[node] - realstart);
				for (tics = nettics[node]; tics < realend; tics++)
					ReadTicCmd (&start, playerbytes[i], tics);
			}
			// Update the number of tics received from each node. This must
			// be separate from the above loop in case the master is also
			// sending bot movements. If it's not separate, then the bots
			// will only move on the master, because the other players will
			// read the master's tics and then think they already got all
			// the tics for the bots and skip the bot tics included in the
			// packet.
			for (i = 0; i < numplayers; ++i)
			{
				if (!players[playerbytes[i]].isbot)
				{
					nettics[nodeforplayer[playerbytes[i]]] = realend;
				}
			}
		}
	}
}

void AdjustBots (int gameticdiv)
{
	// [RH] This loop adjusts the bots' rotations for ticcmds that have
	// been already created but not yet executed. This way, the bot is still
	// able to create ticcmds that accurately reflect the state it wants to
	// be in even when gametic lags behind maketic.
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && players[i].isbot && players[i].mo)
		{
			players[i].savedyaw = players[i].mo->angle;
			players[i].savedpitch = players[i].mo->pitch;
			for (int j = gameticdiv; j < maketic/ticdup; j++)
			{
				players[i].mo->angle += (netcmds[i][j%BACKUPTICS].ucmd.yaw << 16) * ticdup;
				players[i].mo->pitch -= (netcmds[i][j%BACKUPTICS].ucmd.pitch << 16) * ticdup;
			}
		}
	}
}

void UnadjustBots ()
{
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (playeringame[i] && players[i].isbot && players[i].mo)
		{
			players[i].mo->angle = players[i].savedyaw;
			players[i].mo->pitch = players[i].savedpitch;
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
	int		lowtic;
	int 	nowtime;
	int 	newtics;
	int 	i,j;
	int 	realstart;
	BYTE	*cmddata;
	bool	resendOnly;

	GC::CheckGC();

	if (ticdup == 0)
	{
		return;
	}

	// check time
	nowtime = I_GetTime (false);
	newtics = nowtime - gametime;
	gametime = nowtime;

	if (newtics <= 0)	// nothing new to update
	{
		GetPackets ();
		return;
	}

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

	// build new ticcmds for console player (and bots if I am the arbitrator)
	AdjustBots (gametic / ticdup);

	for (i = 0; i < newtics; i++)
	{
		I_StartTic ();
		D_ProcessEvents ();
		if ((maketic - gametic) / ticdup >= BACKUPTICS/2-1)
			break;			// can't hold any more
		
		//Printf ("mk:%i ",maketic);
		G_BuildTiccmd (&localcmds[maketic % LOCALCMDTICS]);
		if (maketic % ticdup == 0)
		{
			//Added by MC: For some of that bot stuff. The main bot function.
			bglobal.Main ((maketic / ticdup) % BACKUPTICS);
		}
		maketic++;

		if (ticdup == 1 || maketic == 0)
		{
			Net_NewMakeTic ();
		}
		else
		{
			// Once ticdup tics have been collected, average their movements
			// and combine their buttons, since they will all be sent as a
			// single tic that gets duplicated ticdup times. Even with ticdup,
			// tics are still collected at the normal rate so that, with the
			// help of prediction, the game seems as responsive as normal.
			if (maketic % ticdup != 0)
			{
				int mod = maketic - maketic % ticdup;
				int j;

				// Update the buttons for all tics in this ticdup set as soon as
				// possible so that the prediction shows jumping as correctly as
				// possible. (If you press +jump in the middle of a ticdup set,
				// the jump will actually begin at the beginning of the set, not
				// in the middle.)
				for (j = maketic-2; j >= mod; --j)
				{
					localcmds[j % LOCALCMDTICS].ucmd.buttons |=
						localcmds[(j + 1) % LOCALCMDTICS].ucmd.buttons;
				}
			}
			else
			{
				// Average the ticcmds between these tics to get the
				// movement that is actually sent across the network. We
				// need to update them in all the localcmds slots that
				// are dupped so that prediction works properly.
				int mod = maketic - ticdup;
				int modp, j;

				int pitch = 0;
				int yaw = 0;
				int roll = 0;
				int forwardmove = 0;
				int sidemove = 0;
				int upmove = 0;

				for (j = 0; j < ticdup; ++j)
				{
					modp = (mod + j) % LOCALCMDTICS;
					pitch += localcmds[modp].ucmd.pitch;
					yaw += localcmds[modp].ucmd.yaw;
					roll += localcmds[modp].ucmd.roll;
					forwardmove += localcmds[modp].ucmd.forwardmove;
					sidemove += localcmds[modp].ucmd.sidemove;
					upmove += localcmds[modp].ucmd.upmove;
				}

				pitch /= ticdup;
				yaw /= ticdup;
				roll /= ticdup;
				forwardmove /= ticdup;
				sidemove /= ticdup;
				upmove /= ticdup;

				for (j = 0; j < ticdup; ++j)
				{
					modp = (mod + j) % LOCALCMDTICS;
					localcmds[modp].ucmd.pitch = pitch;
					localcmds[modp].ucmd.yaw = yaw;
					localcmds[modp].ucmd.roll = roll;
					localcmds[modp].ucmd.forwardmove = forwardmove;
					localcmds[modp].ucmd.sidemove = sidemove;
					localcmds[modp].ucmd.upmove = upmove;
				}

				Net_NewMakeTic ();
			}
		}
	}

	UnadjustBots ();

	if (singletics)
		return; 		// singletic update is synchronous

	if (demoplayback)
	{
		nettics[0] = (maketic / ticdup);
		return;			// Don't touch netcmd data while playing a demo, as it'll already exist.
	}

	// If maketic didn't cross a ticdup boundary, only send packets
	// to players waiting for resends.
	resendOnly = (maketic / ticdup) == (maketic - i) / ticdup;

	// send the packet to the other nodes
	int count = 1;
	int quitcount = 0;

	if (consoleplayer == Net_Arbitrator)
	{
		for (j = 0; j < MAXPLAYERS; j++)
		{
			if (playeringame[j])
			{
				if (players[j].isbot || NetMode == NET_PacketServer)
				{
					count++;
				}
			}
		}

		if (NetMode == NET_PacketServer)
		{
			// The loop above added the local player to the count a second time,
			// and it also added the player being sent the packet to the count.
			count -= 2;

			for (j = 0; j < doomcom.numnodes; ++j)
			{
				if (nodejustleft[j])
				{
					if (count == 0)
					{
						PlayerIsGone (j, playerfornode[j]);
					}
					else
					{
						quitcount++;
					}
				}
			}

			if (count == 0)
			{
				count = 1;
			}
		}
	}

	for (i = 0; i < doomcom.numnodes; i++)
	{
		BYTE playerbytes[MAXPLAYERS];

		if (!nodeingame[i])
		{
			continue;
		}
		if (NetMode == NET_PacketServer &&
			consoleplayer != Net_Arbitrator &&
			i != nodeforplayer[Net_Arbitrator] &&
			i != 0)
		{
			continue;
		}
		if (resendOnly && resendcount[i] <= 0 && !remoteresend[i] && nettics[i])
		{
			continue;
		}

		int numtics;
		int k;

		lowtic = maketic / ticdup;

		netbuffer[0] = 0;
		netbuffer[1] = realstart = resendto[i];
		k = 2;

		if (NetMode == NET_PacketServer &&
			consoleplayer == Net_Arbitrator &&
			i != 0)
		{
			for (j = 1; j < doomcom.numnodes; ++j)
			{
				if (nodeingame[j] && nettics[j] < lowtic && j != i)
				{
					lowtic = nettics[j];
				}
			}
			netbuffer[k++] = lowtic;
		}

		numtics = lowtic - realstart;
		if (numtics > BACKUPTICS)
			I_Error ("NetUpdate: Node %d missed too many tics", i);

		resendto[i] = MAX (0, lowtic - doomcom.extratics);

		if (numtics == 0 && resendOnly && !remoteresend[i] && nettics[i])
		{
			continue;
		}

		if (remoteresend[i])
		{
			netbuffer[0] |= NCMD_RETRANSMIT;
			netbuffer[k++] = nettics[i];
		}

		if (numtics < 3)
		{
			netbuffer[0] |= numtics;
		}
		else
		{
			netbuffer[0] |= NCMD_XTICS;
			netbuffer[k++] = numtics - 3;
		}

		if (quitcount > 0)
		{
			netbuffer[0] |= NCMD_QUITTERS;
			netbuffer[k++] = quitcount;
			for (int l = 0; l < doomcom.numnodes; ++l)
			{
				if (nodejustleft[l])
				{
					netbuffer[k++] = playerfornode[l];
				}
			}
		}

		if (numtics > 0)
		{
			int l;

			if (count > 1 && i != 0 && consoleplayer == Net_Arbitrator)
			{
				netbuffer[0] |= NCMD_MULTI;
				netbuffer[k++] = count;

				for (l = 1, j = 0; j < MAXPLAYERS; j++)
				{
					if (playeringame[j] && j != playerfornode[i] && j != consoleplayer)
					{
						if (players[j].isbot || NetMode == NET_PacketServer)
						{
							playerbytes[l++] = j;
							netbuffer[k++] = j;
						}
					}
				}
			}

			cmddata = &netbuffer[k];

			for (l = 0; l < count; ++l)
			{
				for (j = 0; j < numtics; j++)
				{
					int start = realstart + j, prev = start - 1;
					int localstart, localprev;

					localstart = (start * ticdup) % LOCALCMDTICS;
					localprev = (prev * ticdup) % LOCALCMDTICS;
					start %= BACKUPTICS;
					prev %= BACKUPTICS;

					// The local player has their tics sent first, followed by
					// the other players/bots.
					if (l == 0)
					{
						WriteWord (localcmds[localstart].consistancy, &cmddata);
						// [RH] Write out special "ticcmds" before real ticcmd
						if (specials.used[start])
						{
							memcpy (cmddata, specials.streams[start], specials.used[start]);
							cmddata += specials.used[start];
						}
						WriteUserCmdMessage (&localcmds[localstart].ucmd,
							localprev >= 0 ? &localcmds[localprev].ucmd : NULL, &cmddata);
					}
					else if (i != 0)
					{
						if (players[playerbytes[l]].isbot)
						{

							WriteWord (0, &cmddata);	// fake consistancy word
						}
						else
						{
							int len;
							BYTE *spec;

							WriteWord (netcmds[playerbytes[l]][start].consistancy, &cmddata);
							spec = NetSpecs[playerbytes[l]][start].GetData (&len);
							if (spec != NULL)
							{
								memcpy (cmddata, spec, len);
								cmddata += len;
							}
						}
						WriteUserCmdMessage (&netcmds[playerbytes[l]][start].ucmd,
							prev >= 0 ? &netcmds[playerbytes[l]][prev].ucmd : NULL, &cmddata);
					}
				}
			}
			HSendPacket (i, int(cmddata - netbuffer));
		}
		else
		{
			HSendPacket (i, k);
		}
	}

	// listen for other packets
	GetPackets ();
}


//
// D_ArbitrateNetStart
//
// User info packets look like this:
//
//  0 One byte set to NCMD_SETUP or NCMD_SETUP+1; if NCMD_SETUP+1, omit byte 9
//  1 One byte for the player's number
//2-4 Three bytes for the game version (255,high byte,low byte)
//5-8 A bit mask for each player the sender knows about
//  9 The high bit is set if the sender got the game info
// 10 A stream of bytes with the user info
//
//    The guests always send NCMD_SETUP packets, and the host always
//    sends NCMD_SETUP+1 packets.
//
// Game info packets look like this:
//
//  0 One byte set to NCMD_SETUP+2
//  1 One byte for ticdup setting
//  2 One byte for extratics setting
//  3 One byte for NetMode setting
//  4 String with starting map's name
//  . Four bytes for the RNG seed
//  . Stream containing remaining game info
//
// Finished packet looks like this:
//
//  0 One byte set to NCMD_SETUP+3
//
// Each machine sends user info packets to the host. The host sends user
// info packets back to the other machines as well as game info packets.
// Negotiation is done when all the guests have reported to the host that
// they know about the other nodes.

struct ArbitrateData
{
	DWORD playersdetected[MAXNETNODES];
	BYTE  gotsetup[MAXNETNODES];
};

bool DoArbitrate (void *userdata)
{
	ArbitrateData *data = (ArbitrateData *)userdata;
	char *s;
	BYTE *stream;
	int version;
	int node;
	int i, j;

	while (HGetPacket ())
	{
		if (netbuffer[0] == NCMD_EXIT)
		{
			I_FatalError ("The game was aborted.");
		}

		if (doomcom.remotenode == 0)
		{
			continue;
		}

		if (netbuffer[0] == NCMD_SETUP || netbuffer[0] == NCMD_SETUP+1)		// got user info
		{
			node = (netbuffer[0] == NCMD_SETUP) ? doomcom.remotenode : nodeforplayer[netbuffer[1]];

			data->playersdetected[node] =
				(netbuffer[5] << 24) | (netbuffer[6] << 16) | (netbuffer[7] << 8) | netbuffer[8];

			if (netbuffer[0] == NCMD_SETUP)
			{ // Sent to host
				data->gotsetup[node] = netbuffer[9] & 0x80;
				stream = &netbuffer[10];
			}
			else
			{ // Sent from host
				stream = &netbuffer[9];
			}

			D_ReadUserInfoStrings (netbuffer[1], &stream, false);
			if (!nodeingame[node])
			{
				version = (netbuffer[2] << 16) | (netbuffer[3] << 8) | netbuffer[4];
				if (version != (0xFF0000 | NETGAMEVERSION))
				{
					I_Error ("Different " GAMENAME " versions cannot play a net game");
				}

				playeringame[netbuffer[1]] = true;
				nodeingame[node] = true;

				data->playersdetected[0] |= 1 << netbuffer[1];

				StartScreen->NetMessage ("Found %s (node %d, player %d)",
						players[netbuffer[1]].userinfo.GetName(),
						node, netbuffer[1]+1);
			}
		}
		else if (netbuffer[0] == NCMD_SETUP+2)	// got game info
		{
			data->gotsetup[0] = 0x80;

			ticdup = doomcom.ticdup = netbuffer[1];
			doomcom.extratics = netbuffer[2];
			NetMode = netbuffer[3];

			stream = &netbuffer[4];
			s = ReadString (&stream);
			startmap = FString(s, 8);
			delete[] s;
			rngseed = ReadLong (&stream);
			C_ReadCVars (&stream);
		}
		else if (netbuffer[0] == NCMD_SETUP+3)
		{
			return true;
		}
	}

	// If everybody already knows everything, it's time to go
	if (consoleplayer == Net_Arbitrator)
	{
		for (i = 0; i < doomcom.numnodes; ++i)
			if (data->playersdetected[i] != DWORD(1 << doomcom.numnodes) - 1 || !data->gotsetup[i])
				break;

		if (i == doomcom.numnodes)
			return true;
	}

	netbuffer[2] = 255;
	netbuffer[3] = (NETGAMEVERSION >> 8) & 255;
	netbuffer[4] = NETGAMEVERSION & 255;
	netbuffer[5] = data->playersdetected[0] >> 24;
	netbuffer[6] = data->playersdetected[0] >> 16;
	netbuffer[7] = data->playersdetected[0] >> 8;
	netbuffer[8] = data->playersdetected[0];

	if (consoleplayer != Net_Arbitrator)
	{ // Send user info for the local node
		netbuffer[0] = NCMD_SETUP;
		netbuffer[1] = consoleplayer;
		netbuffer[9] = data->gotsetup[0];
		stream = &netbuffer[10];
		D_WriteUserInfoStrings (consoleplayer, &stream, true);
		SendSetup (data->playersdetected, data->gotsetup, int(stream - netbuffer));
	}
	else
	{ // Send user info for all nodes
		netbuffer[0] = NCMD_SETUP+1;
		for (i = 1; i < doomcom.numnodes; ++i)
		{
			for (j = 0; j < doomcom.numnodes; ++j)
			{
				// Send info about player j to player i?
				if ((data->playersdetected[0] & (1<<j)) && !(data->playersdetected[i] & (1<<j)))
				{
					netbuffer[1] = j;
					stream = &netbuffer[9];
					D_WriteUserInfoStrings (j, &stream, true);
					HSendPacket (i, int(stream - netbuffer));
				}
			}
		}
	}

	// If we're the host, send the game info, too
	if (consoleplayer == Net_Arbitrator)
	{
		netbuffer[0] = NCMD_SETUP+2;
		netbuffer[1] = (BYTE)doomcom.ticdup;
		netbuffer[2] = (BYTE)doomcom.extratics;
		netbuffer[3] = NetMode;
		stream = &netbuffer[4];
		WriteString (startmap, &stream);
		WriteLong (rngseed, &stream);
		C_WriteCVars (&stream, CVAR_SERVERINFO, true);

		SendSetup (data->playersdetected, data->gotsetup, int(stream - netbuffer));
	}
	return false;
}

void D_ArbitrateNetStart (void)
{
	ArbitrateData data;
	int i;

	// Return right away if we're just playing with ourselves.
	if (doomcom.numnodes == 1)
		return;

	autostart = true;

	memset (data.playersdetected, 0, sizeof(data.playersdetected));
	memset (data.gotsetup, 0, sizeof(data.gotsetup));

	// The arbitrator knows about himself, but the other players must
	// be told about themselves, in case the host had to adjust their
	// userinfo (e.g. assign them to a different team).
	if (consoleplayer == Net_Arbitrator)
	{
		data.playersdetected[0] = 1 << consoleplayer;
	}

	// Assign nodes to players. The local player is always node 0.
	// If the local player is not the host, then the host is node 1.
	// Any remaining players are assigned node numbers in the order
	// they were detected.
	playerfornode[0] = consoleplayer;
	nodeforplayer[consoleplayer] = 0;
	if (consoleplayer == Net_Arbitrator)
	{
		for (i = 1; i < doomcom.numnodes; ++i)
		{
			playerfornode[i] = i;
			nodeforplayer[i] = i;
		}
	}
	else
	{
		playerfornode[1] = 0;
		nodeforplayer[0] = 1;
		for (i = 1; i < doomcom.numnodes; ++i)
		{
			if (i < consoleplayer)
			{
				playerfornode[i+1] = i;
				nodeforplayer[i] = i+1;
			}
			else if (i > consoleplayer)
			{
				playerfornode[i] = i;
				nodeforplayer[i] = i;
			}
		}
	}

	if (consoleplayer == Net_Arbitrator)
	{
		data.gotsetup[0] = 0x80;
	}

	StartScreen->NetInit ("Exchanging game information", 1);
	if (!StartScreen->NetLoop (DoArbitrate, &data))
	{
		exit (0);
	}

	if (consoleplayer == Net_Arbitrator)
	{
		netbuffer[0] = NCMD_SETUP+3;
		SendSetup (data.playersdetected, data.gotsetup, 1);
	}

	if (debugfile)
	{
		for (i = 0; i < doomcom.numnodes; ++i)
		{
			fprintf (debugfile, "player %d is on node %d\n", i, nodeforplayer[i]);
		}
	}
	StartScreen->NetDone();
}

static void SendSetup (DWORD playersdetected[MAXNETNODES], BYTE gotsetup[MAXNETNODES], int len)
{
	if (consoleplayer != Net_Arbitrator)
	{
		if (playersdetected[1] & (1 << consoleplayer))
		{
			HSendPacket (1, 10);
		}
		else
		{
			HSendPacket (1, len);
		}
	}
	else
	{
		for (int i = 1; i < doomcom.numnodes; ++i)
		{
			if (!gotsetup[i] || netbuffer[0] == NCMD_SETUP+3)
			{
				HSendPacket (i, len);
			}
		}
	}
}

//
// D_CheckNetGame
// Works out player numbers among the net participants
//

void D_CheckNetGame (void)
{
	const char *v;
	int i;

	for (i = 0; i < MAXNETNODES; i++)
	{
		nodeingame[i] = false;
		nettics[i] = 0;
		remoteresend[i] = false;		// set when local needs tics
		resendto[i] = 0;				// which tic to start sending
	}

	// I_InitNetwork sets doomcom and netgame
	if (I_InitNetwork ())
	{
		NetMode = NET_PacketServer;
	}
	if (doomcom.id != DOOMCOM_ID)
	{
		I_FatalError ("Doomcom buffer invalid!");
	}
	players[0].settings_controller = true;

	consoleplayer = doomcom.consoleplayer;

	v = Args->CheckValue ("-netmode");
	if (v != NULL)
	{
		NetMode = atoi (v) != 0 ? NET_PacketServer : NET_PeerToPeer;
	}
	if (doomcom.numnodes > 1)
	{
		Printf ("Selected " TEXTCOLOR_BLUE "%s" TEXTCOLOR_NORMAL " networking mode. (%s)\n", NetMode == NET_PeerToPeer ? "peer to peer" : "packet server",
			v != NULL ? "forced" : "auto");
	}

	// [RH] Setup user info
	D_SetupUserInfo ();

	if (Args->CheckParm ("-debugfile"))
	{
		char filename[20];
		mysnprintf (filename, countof(filename), "debug%i.txt", consoleplayer);
		Printf ("debug output to: %s\n", filename);
		debugfile = fopen (filename, "w");
	}

	if (netgame)
	{
		GameConfig->ReadNetVars ();	// [RH] Read network ServerInfo cvars
		D_ArbitrateNetStart ();
	}

	// read values out of doomcom
	ticdup = doomcom.ticdup;

	for (i = 0; i < doomcom.numplayers; i++)
		playeringame[i] = true;
	for (i = 0; i < doomcom.numnodes; i++)
		nodeingame[i] = true;

	Printf ("player %i of %i (%i nodes)\n",
			consoleplayer+1, doomcom.numplayers, doomcom.numnodes);
}


//
// D_QuitNetGame
// Called before quitting to leave a net game
// without hanging the other players
//
void D_QuitNetGame (void)
{
	int i, j, k;

	if (!netgame || !usergame || consoleplayer == -1 || demoplayback)
		return;

	// send a bunch of packets for security
	netbuffer[0] = NCMD_EXIT;
	netbuffer[1] = 0;

	k = 2;
	if (NetMode == NET_PacketServer && consoleplayer == Net_Arbitrator)
	{
		BYTE *foo = &netbuffer[2];

		// Let the new arbitrator know what resendto counts to use

		for (i = 0; i < MAXPLAYERS; ++i)
		{
			if (playeringame[i] && i != consoleplayer)
				WriteLong (resendto[nodeforplayer[i]], &foo);
		}
		k = int(foo - netbuffer);
	}

	for (i = 0; i < 4; i++)
	{
		if (NetMode == NET_PacketServer && consoleplayer != Net_Arbitrator)
		{
			HSendPacket (nodeforplayer[Net_Arbitrator], 2);
		}
		else
		{
			for (j = 1; j < doomcom.numnodes; j++)
				if (nodeingame[j])
					HSendPacket (j, k);
		}
		I_WaitVBL (1);
	}

	if (debugfile)
		fclose (debugfile);
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

	// If paused, do not eat more CPU time than we need, because it
	// will all be wasted anyway.
	bool doWait = cl_capfps || r_NoInterpolate /*|| netgame*/;

	// get real tics
	if (doWait)
	{
		entertic = I_WaitForTic (oldentertics);
	}
	else
	{
		entertic = I_GetTime (false);
	}
	realtics = entertic - oldentertics;
	oldentertics = entertic;

	// get available tics
	NetUpdate ();

	lowtic = INT_MAX;
	numplaying = 0;
	for (i = 0; i < doomcom.numnodes; i++)
	{
		if (nodeingame[i])
		{
			numplaying++;
			if (nettics[i] < lowtic)
				lowtic = nettics[i];
		}
	}

	if (ticdup == 1)
	{
		availabletics = lowtic - gametic;
	}
	else
	{
		availabletics = lowtic - gametic / ticdup;
	}

	// decide how many tics to run
	if (realtics < availabletics-1)
		counts = realtics+1;
	else if (realtics < availabletics)
		counts = realtics;
	else
		counts = availabletics;
	
	if (counts == 0 && !doWait)
	{
		// Check possible stall conditions
		Net_CheckLastRecieved(counts);
		return;
	}

	if (counts < 1)
		counts = 1;

	frameon++;

	if (debugfile)
		fprintf (debugfile,
				 "=======real: %i  avail: %i  game: %i\n",
				 realtics, availabletics, counts);

	if (!demoplayback)
	{
		// ideally nettics[0] should be 1 - 3 tics above lowtic
		// if we are consistantly slower, speed up time

		// [RH] I had erroneously assumed frameskip[] had 4 entries
		// because there were 4 players, but that's not the case at
		// all. The game is comparing the lag behind the master for
		// four runs of TryRunTics. If our tic count is ahead of the
		// master all 4 times, the next run of NetUpdate will not
		// process any new input. If we have less input than the
		// master, the next run of NetUpdate will process extra tics
		// (because gametime gets decremented here).

		// the key player does not adapt
		if (consoleplayer != Net_Arbitrator)
		{
			// I'm not sure about this when using a packet server, because
			// if left unmodified from the P2P version, it can make the game
			// very jerky. The way I have it written right now basically means
			// that it won't adapt. Fortunately, player prediction helps
			// alleviate the lag somewhat.

			if (NetMode != NET_PacketServer)
			{
				mastertics = nettics[nodeforplayer[Net_Arbitrator]];
			}
			if (nettics[0] <= mastertics)
			{
				gametime--;
				if (debugfile) fprintf (debugfile, "-");
			}
			if (NetMode != NET_PacketServer)
			{
				frameskip[frameon&3] = (oldnettics > mastertics);
			}
			else
			{
				frameskip[frameon&3] = (oldnettics - mastertics) > 3;
			}
			if (frameskip[0] && frameskip[1] && frameskip[2] && frameskip[3])
			{
				skiptics = 1;
				if (debugfile) fprintf (debugfile, "+");
			}
			oldnettics = nettics[0];
		}
	}// !demoplayback

	// wait for new tics if needed
	while (lowtic < gametic + counts)
	{
		NetUpdate ();
		lowtic = INT_MAX;

		for (i = 0; i < doomcom.numnodes; i++)
			if (nodeingame[i] && nettics[i] < lowtic)
				lowtic = nettics[i];

		lowtic = lowtic * ticdup;

		if (lowtic < gametic)
			I_Error ("TryRunTics: lowtic < gametic");

		// Check possible stall conditions
		Net_CheckLastRecieved (counts);

		// don't stay in here forever -- give the menu a chance to work
		if (I_GetTime (false) - entertic >= TICRATE/3)
		{
			C_Ticker ();
			M_Ticker ();
			return;
		}
	}

	//Tic lowtic is high enough to process this gametic. Clear all possible waiting info
	hadlate = false;
	for (i = 0; i < MAXPLAYERS; i++)
		players[i].waiting = false;
	lastglobalrecvtime = I_GetTime (false); //Update the last time the game tic'd over

	// run the count tics
	if (counts > 0)
	{
		while (counts--)
		{
			if (gametic > lowtic)
			{
				I_Error ("gametic>lowtic");
			}
			if (advancedemo)
			{
				D_DoAdvanceDemo ();
			}
			if (debugfile) fprintf (debugfile, "run tic %d\n", gametic);
			C_Ticker ();
			M_Ticker ();
			I_GetTime (true);
			G_Ticker ();
			gametic++;

			NetUpdate ();	// check for new console commands
		}
		S_UpdateSounds (players[consoleplayer].camera);	// move positional sounds
	}
}

void Net_CheckLastRecieved (int counts)
{
	// [Ed850] Check to see the last time a packet was recieved.
	// If it's longer then 3 seconds, a node has likely stalled.
	if (I_GetTime(false) - lastglobalrecvtime >= TICRATE * 3)
	{
		lastglobalrecvtime = I_GetTime(false); //Bump the count

		if (NetMode == NET_PeerToPeer || consoleplayer == Net_Arbitrator)
		{
			//Keep the local node in the for loop so we can still log any cases where the local node is /somehow/ late.
			//However, we don't send a resend request for sanity reasons.
			for (int i = 0; i < doomcom.numnodes; i++)
			{
				if (nodeingame[i] && nettics[i] <= gametic + counts)
				{
					if (debugfile && !players[playerfornode[i]].waiting)
						fprintf(debugfile, "%i is slow (%i to %i)\n",
						i, nettics[i], gametic + counts);
					//Send resend request to the late node. Also mark the node as waiting to display it in the hud.
					if (i != 0)
						remoteresend[i] = players[playerfornode[i]].waiting = hadlate = true;
				}
				else
					players[playerfornode[i]].waiting = false;
			}
		}
		else
		{	//Send a resend request to the Arbitrator, as it's obvious we are stuck here.
			if (debugfile && !players[playerfornode[Net_Arbitrator]].waiting)
				fprintf(debugfile, "Arbitrator is slow (%i to %i)\n",
				nettics[Net_Arbitrator], gametic + counts);
			//Send resend request to the Arbitrator. Also mark the Arbitrator as waiting to display it in the hud.
			remoteresend[Net_Arbitrator] = players[playerfornode[Net_Arbitrator]].waiting = hadlate = true;
		}
	}
}

void Net_NewMakeTic (void)
{
	specials.NewMakeTic ();
}

void Net_WriteByte (BYTE it)
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

void Net_WriteBytes (const BYTE *block, int len)
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

void FDynamicBuffer::SetData (const BYTE *data, int len)
{
	if (len > m_BufferLen)
	{
		m_BufferLen = (len + 255) & ~255;
		m_Data = (BYTE *)M_Realloc (m_Data, m_BufferLen);
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

BYTE *FDynamicBuffer::GetData (int *len)
{
	if (len)
		*len = m_Len;
	return m_Len ? m_Data : NULL;
}


static int KillAll(const PClass *cls)
{
	AActor *actor;
	int killcount = 0;
	TThinkerIterator<AActor> iterator(cls);
	while ( (actor = iterator.Next ()) )
	{
		if (actor->IsA(cls))
		{
			if (!(actor->flags2 & MF2_DORMANT) && (actor->flags3 & MF3_ISMONSTER))
					killcount += actor->Massacre ();
		}
	}
	return killcount;

}
// [RH] Execute a special "ticcmd". The type byte should
//		have already been read, and the stream is positioned
//		at the beginning of the command's actual data.
void Net_DoCommand (int type, BYTE **stream, int player)
{
	BYTE pos = 0;
	char *s = NULL;
	int i;

	switch (type)
	{
	case DEM_SAY:
		{
			const char *name = players[player].userinfo.GetName();
			BYTE who = ReadByte (stream);

			s = ReadString (stream);
			CleanseString (s);
			if (((who & 1) == 0) || players[player].userinfo.GetTeam() == TEAM_NONE)
			{ // Said to everyone
				if (who & 2)
				{
					Printf (PRINT_CHAT, TEXTCOLOR_BOLD "* %s" TEXTCOLOR_BOLD "%s" TEXTCOLOR_BOLD "\n", name, s);
				}
				else
				{
					Printf (PRINT_CHAT, "%s" TEXTCOLOR_CHAT ": %s" TEXTCOLOR_CHAT "\n", name, s);
				}
				S_Sound (CHAN_VOICE | CHAN_UI, gameinfo.chatSound, 1, ATTN_NONE);
			}
			else if (players[player].userinfo.GetTeam() == players[consoleplayer].userinfo.GetTeam())
			{ // Said only to members of the player's team
				if (who & 2)
				{
					Printf (PRINT_TEAMCHAT, TEXTCOLOR_BOLD "* (%s" TEXTCOLOR_BOLD ")%s" TEXTCOLOR_BOLD "\n", name, s);
				}
				else
				{
					Printf (PRINT_TEAMCHAT, "(%s" TEXTCOLOR_TEAMCHAT "): %s" TEXTCOLOR_TEAMCHAT "\n", name, s);
				}
				S_Sound (CHAN_VOICE | CHAN_UI, gameinfo.chatSound, 1, ATTN_NONE);
			}
		}
		break;

	case DEM_MUSICCHANGE:
		s = ReadString (stream);
		S_ChangeMusic (s);
		break;

	case DEM_PRINT:
		s = ReadString (stream);
		Printf ("%s", s);
		break;

	case DEM_CENTERPRINT:
		s = ReadString (stream);
		C_MidPrint (SmallFont, s);
		break;

	case DEM_UINFCHANGED:
		D_ReadUserInfoStrings (player, stream, true);
		break;

	case DEM_SINFCHANGED:
		D_DoServerInfoChange (stream, false);
		break;

	case DEM_SINFCHANGEDXOR:
		D_DoServerInfoChange (stream, true);
		break;

	case DEM_GIVECHEAT:
		s = ReadString (stream);
		cht_Give (&players[player], s, ReadWord (stream));
		break;

	case DEM_TAKECHEAT:
		s = ReadString (stream);
		cht_Take (&players[player], s, ReadWord (stream));
		break;

	case DEM_WARPCHEAT:
		{
			int x, y;
			x = ReadWord (stream);
			y = ReadWord (stream);
			P_TeleportMove (players[player].mo, x * 65536, y * 65536, ONFLOORZ, true);
		}
		break;

	case DEM_GENERICCHEAT:
		cht_DoCheat (&players[player], ReadByte (stream));
		break;

	case DEM_CHANGEMAP2:
		pos = ReadByte (stream);
		/* intentional fall-through */
	case DEM_CHANGEMAP:
		// Change to another map without disconnecting other players
		s = ReadString (stream);
		// Using LEVEL_NOINTERMISSION tends to throw the game out of sync.
		// That was a long time ago. Maybe it works now?
		level.flags |= LEVEL_CHANGEMAPCHEAT;
		G_ChangeLevel(s, pos, 0);
		break;

	case DEM_SUICIDE:
		cht_Suicide (&players[player]);
		break;

	case DEM_ADDBOT:
		{
			BYTE num = ReadByte (stream);
			bglobal.DoAddBot (num, s = ReadString (stream));
		}
		break;

	case DEM_KILLBOTS:
		bglobal.RemoveAllBots (true);
		Printf ("Removed all bots\n");
		break;

	case DEM_CENTERVIEW:
		players[player].centering = true;
		break;

	case DEM_INVUSEALL:
		if (gamestate == GS_LEVEL && !paused)
		{
			AInventory *item = players[player].mo->Inventory;

			while (item != NULL)
			{
				AInventory *next = item->Inventory;
				if (item->ItemFlags & IF_INVBAR && !(item->IsKindOf(RUNTIME_CLASS(APuzzleItem))))
				{
					players[player].mo->UseInventory (item);
				}
				item = next;
			}
		}
		break;

	case DEM_INVUSE:
	case DEM_INVDROP:
		{
			DWORD which = ReadLong (stream);

			if (gamestate == GS_LEVEL && !paused)
			{
				AInventory *item = players[player].mo->Inventory;
				while (item != NULL && item->InventoryID != which)
				{
					item = item->Inventory;
				}
				if (item != NULL)
				{
					if (type == DEM_INVUSE)
					{
						players[player].mo->UseInventory (item);
					}
					else
					{
						players[player].mo->DropInventory (item);
					}
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
			const PClass *typeinfo;
			int angle = 0;
			SWORD tid = 0;
			BYTE special = 0;
			int args[5];

			s = ReadString (stream);
			if (type >= DEM_SUMMON2 && type <= DEM_SUMMONFOE2)
			{
				angle = ReadWord(stream);
				tid = ReadWord(stream);
				special = ReadByte(stream);
				for(i = 0; i < 5; i++) args[i] = ReadLong(stream);
			}

			typeinfo = PClass::FindClass (s);
			if (typeinfo != NULL && typeinfo->ActorInfo != NULL)
			{
				AActor *source = players[player].mo;
				if (source != NULL)
				{
					if (GetDefaultByType (typeinfo)->flags & MF_MISSILE)
					{
						P_SpawnPlayerMissile (source, typeinfo);
					}
					else
					{
						const AActor *def = GetDefaultByType (typeinfo);
						AActor *spawned = Spawn (typeinfo,
							source->x + FixedMul (def->radius * 2 + source->radius, finecosine[source->angle>>ANGLETOFINESHIFT]),
							source->y + FixedMul (def->radius * 2 + source->radius, finesine[source->angle>>ANGLETOFINESHIFT]),
							source->z + 8 * FRACUNIT, ALLOW_REPLACE);
						if (spawned != NULL)
						{
							if (type == DEM_SUMMONFRIEND || type == DEM_SUMMONFRIEND2 || type == DEM_SUMMONMBF)
							{
								if (spawned->CountsAsKill()) 
								{
									level.total_monsters--;
								}
								spawned->FriendPlayer = player + 1;
								spawned->flags |= MF_FRIENDLY;
								spawned->LastHeard = players[player].mo;
								spawned->health = spawned->SpawnHealth();
								if (type == DEM_SUMMONMBF)
									spawned->flags3 |= MF3_NOBLOCKMONST;
							}
							else if (type == DEM_SUMMONFOE || type == DEM_SUMMONFOE2)
							{
								spawned->FriendPlayer = 0;
								spawned->flags &= ~MF_FRIENDLY;
								spawned->health = spawned->SpawnHealth();
							}
						}
						if (type >= DEM_SUMMON2 && type <= DEM_SUMMONFOE2)
						{
							spawned->angle = source->angle - (ANGLE_1 * angle);
							spawned->tid = tid;
							spawned->special = special;
							for(i = 0; i < 5; i++) {
								spawned->args[i] = args[i];
							}
							if(tid) spawned->AddToHash();
						}
					}
				}
			}
		}
		break;

	case DEM_SPRAY:
		{
			FTraceResults trace;

			angle_t ang = players[player].mo->angle  >> ANGLETOFINESHIFT;
			angle_t pitch = (angle_t)(players[player].mo->pitch) >> ANGLETOFINESHIFT;
			fixed_t vx = FixedMul (finecosine[pitch], finecosine[ang]);
			fixed_t vy = FixedMul (finecosine[pitch], finesine[ang]);
			fixed_t vz = -finesine[pitch];

			s = ReadString (stream);

			if (Trace (players[player].mo->x, players[player].mo->y,
				players[player].mo->z + players[player].mo->height - (players[player].mo->height>>2),
				players[player].mo->Sector,
				vx, vy, vz, 172*FRACUNIT, 0, ML_BLOCKEVERYTHING, players[player].mo,
				trace, TRACE_NoSky))
			{
				if (trace.HitType == TRACE_HitWall)
				{
					DImpactDecal::StaticCreate (s,
						trace.X, trace.Y, trace.Z,
						trace.Line->sidedef[trace.Side], NULL);
				}
			}
		}
		break;

	case DEM_PAUSE:
		if (gamestate == GS_LEVEL)
		{
			if (paused)
			{
				paused = 0;
				S_ResumeSound (false);
			}
			else
			{
				paused = player + 1;
				S_PauseSound (false, false);
			}
			V_SetBorderNeedRefresh();
		}
		break;

	case DEM_SAVEGAME:
		if (gamestate == GS_LEVEL)
		{
			s = ReadString (stream);
			savegamefile = s;
			delete[] s;
			s = ReadString (stream);
			memset (savedescription, 0, sizeof(savedescription));
			strncpy (savedescription, s, sizeof(savedescription));
			if (player != consoleplayer)
			{
				// Paths sent over the network will be valid for the system that sent
				// the save command. For other systems, the path needs to be changed.
				const char *fileonly = savegamefile.GetChars();
				const char *slash = strrchr (fileonly, '\\');
				if (slash != NULL)
				{
					fileonly = slash + 1;
				}
				slash = strrchr (fileonly, '/');
				if (slash != NULL)
				{
					fileonly = slash + 1;
				}
				if (fileonly != savegamefile.GetChars())
				{
					savegamefile = G_BuildSaveName (fileonly, -1);
				}
			}
		}
		gameaction = ga_savegame;
		break;

	case DEM_CHECKAUTOSAVE:
		// Do not autosave in multiplayer games or when dead.
		// For demo playback, DEM_DOAUTOSAVE already exists in the demo if the
		// autosave happened. And if it doesn't, we must not generate it.
		if (multiplayer ||
			demoplayback ||
			players[consoleplayer].playerstate != PST_LIVE ||
			disableautosave >= 2 ||
			autosavecount == 0)
		{
			break;
		}
		Net_WriteByte (DEM_DOAUTOSAVE);
		break;

	case DEM_DOAUTOSAVE:
		gameaction = ga_autosave;
		break;

	case DEM_FOV:
		{
			float newfov = (float)ReadByte (stream);

			if (newfov != players[consoleplayer].DesiredFOV)
			{
				Printf ("FOV%s set to %g\n",
					consoleplayer == Net_Arbitrator ? " for everyone" : "",
					newfov);
			}

			for (i = 0; i < MAXPLAYERS; ++i)
			{
				if (playeringame[i])
				{
					players[i].DesiredFOV = newfov;
				}
			}
		}
		break;

	case DEM_MYFOV:
		players[player].DesiredFOV = (float)ReadByte (stream);
		break;

	case DEM_RUNSCRIPT:
	case DEM_RUNSCRIPT2:
		{
			int snum = ReadWord (stream);
			int argn = ReadByte (stream);

			RunScript(stream, players[player].mo, snum, argn, (type == DEM_RUNSCRIPT2) ? ACS_ALWAYS : 0);
		}
		break;

	case DEM_RUNNAMEDSCRIPT:
		{
			char *sname = ReadString(stream);
			int argn = ReadByte(stream);

			RunScript(stream, players[player].mo, -FName(sname), argn & 127, (argn & 128) ? ACS_ALWAYS : 0);
		}
		break;

	case DEM_RUNSPECIAL:
		{
			int snum = ReadByte(stream);
			int argn = ReadByte(stream);
			int arg[5] = { 0, 0, 0, 0, 0 };

			for (i = 0; i < argn; ++i)
			{
				int argval = ReadLong(stream);
				if ((unsigned)i < countof(arg))
				{
					arg[i] = argval;
				}
			}
			if (!CheckCheatmode(player == consoleplayer))
			{
				P_ExecuteSpecial(snum, NULL, players[player].mo, false, arg[0], arg[1], arg[2], arg[3], arg[4]);
			}
		}
		break;

	case DEM_CROUCH:
		if (gamestate == GS_LEVEL && players[player].mo != NULL && 
			players[player].health > 0 && !(players[player].oldbuttons & BT_JUMP) &&
			!P_IsPlayerTotallyFrozen(&players[player]))
		{
			players[player].crouching = players[player].crouchdir < 0 ? 1 : -1;
		}
		break;

	case DEM_MORPHEX:
		{
			s = ReadString (stream);
			const char *msg = cht_Morph (players + player, PClass::FindClass (s), false);
			if (player == consoleplayer)
			{
				Printf ("%s\n", *msg != '\0' ? msg : "Morph failed.");
			}
		}
		break;

	case DEM_ADDCONTROLLER:
		{
			BYTE playernum = ReadByte (stream);
			players[playernum].settings_controller = true;

			if (consoleplayer == playernum || consoleplayer == Net_Arbitrator)
				Printf ("%s has been added to the controller list.\n", players[playernum].userinfo.GetName());
		}
		break;

	case DEM_DELCONTROLLER:
		{
			BYTE playernum = ReadByte (stream);
			players[playernum].settings_controller = false;

			if (consoleplayer == playernum || consoleplayer == Net_Arbitrator)
				Printf ("%s has been removed from the controller list.\n", players[playernum].userinfo.GetName());
		}
		break;

	case DEM_KILLCLASSCHEAT:
		{
			char *classname = ReadString (stream);
			int killcount = 0;
			const PClass *cls = PClass::FindClass(classname);

			if (cls != NULL && cls->ActorInfo != NULL)
			{
				killcount = KillAll(cls);
				const PClass *cls_rep = cls->GetReplacement();
				if (cls != cls_rep)
				{
					killcount += KillAll(cls_rep);
				}
				Printf ("Killed %d monsters of type %s.\n",killcount, classname);
			}
			else
			{
				Printf ("%s is not an actor class.\n", classname);
			}

		}
		break;

	case DEM_CONVREPLY:
	case DEM_CONVCLOSE:
	case DEM_CONVNULL:
		P_ConversationCommand (type, player, stream);
		break;

	case DEM_SETSLOT:
	case DEM_SETSLOTPNUM:
		{
			int pnum;
			if (type == DEM_SETSLOTPNUM)
			{
				pnum = ReadByte(stream);
			}
			else
			{
				pnum = player;
			}
			unsigned int slot = ReadByte(stream);
			int count = ReadByte(stream);
			if (slot < NUM_WEAPON_SLOTS)
			{
				players[pnum].weapons.Slots[slot].Clear();
			}
			for(i = 0; i < count; ++i)
			{
				const PClass *wpn = Net_ReadWeapon(stream);
				players[pnum].weapons.AddSlot(slot, wpn, pnum == consoleplayer);
			}
		}
		break;

	case DEM_ADDSLOT:
		{
			int slot = ReadByte(stream);
			const PClass *wpn = Net_ReadWeapon(stream);
			players[player].weapons.AddSlot(slot, wpn, player == consoleplayer);
		}
		break;

	case DEM_ADDSLOTDEFAULT:
		{
			int slot = ReadByte(stream);
			const PClass *wpn = Net_ReadWeapon(stream);
			players[player].weapons.AddSlotDefault(slot, wpn, player == consoleplayer);
		}
		break;

	case DEM_SETPITCHLIMIT:
		players[player].MinPitch = ReadByte(stream) * -ANGLE_1;		// up
		players[player].MaxPitch = ReadByte(stream) *  ANGLE_1;		// down
		break;

	case DEM_ADVANCEINTER:
		F_AdvanceIntermission();
		break;

	case DEM_REVERTCAMERA:
		players[player].camera = players[player].mo;
		break;

	default:
		I_Error ("Unknown net command: %d", type);
		break;
	}

	if (s)
		delete[] s;
}

// Used by DEM_RUNSCRIPT, DEM_RUNSCRIPT2, and DEM_RUNNAMEDSCRIPT
static void RunScript(BYTE **stream, APlayerPawn *pawn, int snum, int argn, int always)
{
	int arg[4] = { 0, 0, 0, 0 };
	int i;
	
	for (i = 0; i < argn; ++i)
	{
		int argval = ReadLong(stream);
		if ((unsigned)i < countof(arg))
		{
			arg[i] = argval;
		}
	}
	P_StartScript(pawn, NULL, snum, level.mapname, arg, MIN<int>(countof(arg), argn), ACS_NET | always);
}

void Net_SkipCommand (int type, BYTE **stream)
{
	BYTE t;
	size_t skip;

	switch (type)
	{
		case DEM_SAY:
		case DEM_ADDBOT:
			skip = strlen ((char *)(*stream + 1)) + 2;
			break;

		case DEM_GIVECHEAT:
		case DEM_TAKECHEAT:
			skip = strlen ((char *)(*stream)) + 3;
			break;

		case DEM_SUMMON2:
		case DEM_SUMMONFRIEND2:
		case DEM_SUMMONFOE2:
			skip = strlen ((char *)(*stream)) + 26;
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
		case DEM_SPRAY:
		case DEM_MORPHEX:
		case DEM_KILLCLASSCHEAT:
			skip = strlen ((char *)(*stream)) + 1;
			break;

		case DEM_INVUSE:
		case DEM_INVDROP:
		case DEM_WARPCHEAT:
			skip = 4;
			break;

		case DEM_GENERICCHEAT:
		case DEM_DROPPLAYER:
		case DEM_FOV:
		case DEM_MYFOV:
		case DEM_ADDCONTROLLER:
		case DEM_DELCONTROLLER:
			skip = 1;
			break;

		case DEM_SAVEGAME:
			skip = strlen ((char *)(*stream)) + 1;
			skip += strlen ((char *)(*stream) + skip) + 1;
			break;

		case DEM_SINFCHANGEDXOR:
		case DEM_SINFCHANGED:
			t = **stream;
			skip = 1 + (t & 63);
			if (type == DEM_SINFCHANGED)
			{
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
			}
			else
			{
				skip += 1;
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
			skip = 2 + *(*stream + 1) * 4;
			break;

		case DEM_CONVREPLY:
			skip = 3;
			break;

		case DEM_SETSLOT:
		case DEM_SETSLOTPNUM:
			{
				skip = 2 + (type == DEM_SETSLOTPNUM);
				for(int numweapons = (*stream)[skip-1]; numweapons > 0; numweapons--)
				{
					skip += 1 + ((*stream)[skip] >> 7);
				}
			}
			break;

		case DEM_ADDSLOT:
		case DEM_ADDSLOTDEFAULT:
			skip = 2 + ((*stream)[1] >> 7);
			break;

		case DEM_SETPITCHLIMIT:
			skip = 2;
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
					players[i].userinfo.GetName());
}

//==========================================================================
//
// Network_Controller
//
// Implement players who have the ability to change settings in a network
// game.
//
//==========================================================================

static void Network_Controller (int playernum, bool add)
{
	if (consoleplayer != Net_Arbitrator)
	{
		Printf ("This command is only accessible to the net arbitrator.\n");
		return;
	}

	if (players[playernum].settings_controller && add)
	{
		Printf ("%s is already on the setting controller list.\n", players[playernum].userinfo.GetName());
		return;
	}

	if (!players[playernum].settings_controller && !add)
	{
		Printf ("%s is not on the setting controller list.\n", players[playernum].userinfo.GetName());
		return;
	}

	if (!playeringame[playernum])
	{
		Printf ("Player (%d) not found!\n", playernum);
		return;
	}

	if (players[playernum].isbot)
	{
		Printf ("Bots cannot be added to the controller list.\n");
		return;
	}

	if (playernum == Net_Arbitrator)
	{
		Printf ("The net arbitrator cannot have their status changed on this list.\n");
		return;
	}

	if (add)
		Net_WriteByte (DEM_ADDCONTROLLER);
	else
		Net_WriteByte (DEM_DELCONTROLLER);

	Net_WriteByte (playernum);
}

//==========================================================================
//
// CCMD net_addcontroller
//
//==========================================================================

CCMD (net_addcontroller)
{
	if (!netgame)
	{
		Printf ("This command can only be used when playing a net game.\n");
		return;
	}

	if (argv.argc () < 2)
	{
		Printf ("Usage: net_addcontroller <player>\n");
		return;
	}

	Network_Controller (atoi (argv[1]), true);
}

//==========================================================================
//
// CCMD net_removecontroller
//
//==========================================================================

CCMD (net_removecontroller)
{
	if (!netgame)
	{
		Printf ("This command can only be used when playing a net game.\n");
		return;
	}

	if (argv.argc () < 2)
	{
		Printf ("Usage: net_removecontroller <player>\n");
		return;
	}

	Network_Controller (atoi (argv[1]), false);
}

//==========================================================================
//
// CCMD net_listcontrollers
//
//==========================================================================

CCMD (net_listcontrollers)
{
	if (!netgame)
	{
		Printf ("This command can only be used when playing a net game.\n");
		return;
	}

	Printf ("The following players can change the game settings:\n");

	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
			continue;

		if (players[i].settings_controller)
		{
			Printf ("- %s\n", players[i].userinfo.GetName());
		}
	}
}
