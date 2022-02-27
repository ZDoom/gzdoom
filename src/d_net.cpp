//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
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
//-----------------------------------------------------------------------------
//
// DESCRIPTION:
//		DOOM Network game communication and protocol,
//		all OS independent parts.
//
//-----------------------------------------------------------------------------

#include <stddef.h>
#define __STDC_FORMAT_MACROS
#include <inttypes.h>

#include "version.h"
#include "menu.h"
#include "i_video.h"
#include "i_net.h"
#include "g_game.h"
#include "c_console.h"
#include "d_netinf.h"
#include "d_net.h"
#include "cmdlib.h"
#include "m_cheat.h"
#include "p_local.h"
#include "c_dispatch.h"
#include "sbar.h"
#include "gi.h"
#include "m_misc.h"
#include "gameconfigfile.h"
#include "p_acs.h"
#include "p_trace.h"
#include "a_sharedglobal.h"
#include "st_start.h"
#include "teaminfo.h"
#include "p_conversation.h"
#include "d_event.h"
#include "p_enemy.h"
#include "m_argv.h"
#include "p_lnspec.h"
#include "p_spec.h"
#include "hardware.h"
#include "r_utility.h"
#include "a_keys.h"
#include "intermission/intermission.h"
#include "g_levellocals.h"
#include "actorinlines.h"
#include "events.h"
#include "i_time.h"
#include "i_system.h"
#include "vm.h"
#include "gstrings.h"
#include "s_music.h"

EXTERN_CVAR (Int, disableautosave)
EXTERN_CVAR (Int, autosavecount)
EXTERN_CVAR(Bool, cl_capfps)

//#define SIMULATEERRORS		(RAND_MAX/3)
#define SIMULATEERRORS			0

extern uint8_t		*demo_p;		// [RH] Special "ticcmds" get recorded in demos
extern FString	savedescription;
extern FString	savegamefile;

extern short consistancy[MAXPLAYERS][BACKUPTICS];

#define netbuffer (doomcom.data)

enum { NET_PeerToPeer, NET_PacketServer };
uint8_t NetMode = NET_PeerToPeer;



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

uint64_t		lastrecvtime[MAXPLAYERS];				// [RH] Used for pings
uint64_t		currrecvtime[MAXPLAYERS];
uint64_t		lastglobalrecvtime;						// Identify the last time a packet was received.
bool			hadlate;
int				netdelay[MAXNETNODES][BACKUPTICS];		// Used for storing network delay times.
int				lastaverage;

int 			nodeforplayer[MAXPLAYERS];
int				playerfornode[MAXNETNODES];

int 			maketic;
int 			skiptics;
int 			ticdup;

void D_ProcessEvents (void); 
void G_BuildTiccmd (ticcmd_t *cmd); 
void D_DoAdvanceDemo (void);

static void SendSetup (uint32_t playersdetected[MAXNETNODES], uint8_t gotsetup[MAXNETNODES], int len);
static void RunScript(uint8_t **stream, AActor *pawn, int snum, int argn, int always);

int		reboundpacket;
uint8_t	reboundstore[MAX_MSGLEN];

int 	frameon;
int 	frameskip[4];
int 	oldnettics;
int		mastertics;

static int 	entertic;
static int	oldentertics;

extern	bool	 advancedemo;

CVAR(Bool, net_ticbalance, false, CVAR_SERVERINFO | CVAR_NOSAVE)
CUSTOM_CVAR(Int, net_extratic, 0, CVAR_SERVERINFO | CVAR_NOSAVE)
{
	if (self < 0)
	{
		self = 0;
	}
	else if (self > 2)
	{
		self = 2;
	}
}

#ifdef _DEBUG
CVAR(Int, net_fakelatency, 0, 0);

struct PacketStore
{
	int timer;
	doomcom_t message;
};

static TArray<PacketStore> InBuffer;
static TArray<PacketStore> OutBuffer;
#endif

// [RH] Special "ticcmds" get stored in here
static struct TicSpecial
{
	uint8_t *streams[BACKUPTICS];
	size_t used[BACKUPTICS];
	uint8_t *streamptr;
	size_t streamoffs;
	size_t specialsize;
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
			streams[i] = (uint8_t *)M_Malloc (256);
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
	void GetMoreSpace (size_t needed)
	{
		int i;

		specialsize = max(specialsize * 2, needed + 30);

		DPrintf (DMSG_NOTIFY, "Expanding special size to %zu\n", specialsize);

		for (i = 0; i < BACKUPTICS; i++)
			streams[i] = (uint8_t *)M_Realloc (streams[i], specialsize);

		streamptr = streams[(maketic/ticdup)%BACKUPTICS] + streamoffs;
	}

	void CheckSpace (size_t needed)
	{
		if (streamoffs + needed >= specialsize)
			GetMoreSpace (streamoffs + needed);

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

	TicSpecial &operator << (uint8_t it)
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
	memset (nodeforplayer, 0, sizeof(nodeforplayer));
	memset (playerfornode, 0, sizeof(playerfornode));
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

	// Network delay byte
	k++;

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

	uint8_t *skipper = &netbuffer[k];
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
				fprintf (debugfile," %2x", ((uint8_t *)netbuffer)[i]);
		}
		else if (netbuffer[0] & NCMD_EXIT)
		{
			fprintf (debugfile,"%i/%i send %i = EXIT [%3i]", gametic, maketic, node, len);
			for (i = 0; i < len; i++)
				fprintf (debugfile," %2x", ((uint8_t *)netbuffer)[i]);
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
				fprintf (debugfile, "%c%2x", i==k?'|':' ', ((uint8_t *)netbuffer)[i]);
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

#ifdef _DEBUG
	if (net_fakelatency / 2 > 0)
	{
		PacketStore store;
		store.message = doomcom;
		store.timer = I_GetTime() + ((net_fakelatency / 2) / (1000 / TICRATE));
		OutBuffer.Push(store);
	}
	else
		I_NetCmd();

	for (unsigned int i = 0; i < OutBuffer.Size(); i++)
	{
		if (OutBuffer[i].timer <= I_GetTime())
		{
			doomcom = OutBuffer[i].message;
			I_NetCmd();
			OutBuffer.Delete(i);
			i = -1;
		}
	}
#else
	I_NetCmd();
#endif
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

#ifdef _DEBUG
	if (net_fakelatency / 2 > 0 && doomcom.remotenode != -1)
	{
		PacketStore store;
		store.message = doomcom;
		store.timer = I_GetTime() + ((net_fakelatency / 2) / (1000 / TICRATE));
		InBuffer.Push(store);
		doomcom.remotenode = -1;
	}
	
	if (doomcom.remotenode == -1)
	{
		bool gotmessage = false;
		for (unsigned int i = 0; i < InBuffer.Size(); i++)
		{
			if (InBuffer[i].timer <= I_GetTime())
			{
				doomcom = InBuffer[i].message;
				InBuffer.Delete(i);
				gotmessage = true;
				break;
			}
		}
		if (!gotmessage)
			return false;
	}
#else
	if (doomcom.remotenode == -1)
	{
		return false;
	}
#endif
		
	if (debugfile)
	{
		int i, k, realretrans;

		if (netbuffer[0] & NCMD_SETUP)
		{
			fprintf (debugfile,"%i/%i  get %i = SETUP [%3i]", gametic, maketic, doomcom.remotenode, doomcom.datalength);
			for (i = 0; i < doomcom.datalength; i++)
				fprintf (debugfile, " %2x", ((uint8_t *)netbuffer)[i]);
			fprintf (debugfile, "\n");
		}
		else if (netbuffer[0] & NCMD_EXIT)
		{
			fprintf (debugfile,"%i/%i  get %i = EXIT [%3i]", gametic, maketic, doomcom.remotenode, doomcom.datalength);
			for (i = 0; i < doomcom.datalength; i++)
				fprintf (debugfile, " %2x", ((uint8_t *)netbuffer)[i]);
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
				fprintf (debugfile, "%c%2x", i==k?'|':' ', ((uint8_t *)netbuffer)[i]);
			if (numtics)
				fprintf (debugfile, " <<%4x>>\n",
					consistancy[playerfornode[doomcom.remotenode]][nettics[doomcom.remotenode]%BACKUPTICS] & 0xFFFF);
			else
				fprintf (debugfile, "\n");
		}
	}

	if (doomcom.datalength != NetbufferSize ())
	{
		Printf("Bad packet length %i (calculated %i)\n",
			doomcom.datalength, NetbufferSize());

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

	if (nodeingame[netnode])
	{
		for (i = netnode + 1; i < doomcom.numnodes; ++i)
		{
			if (nodeingame[i])
				break;
		}
		if (i == doomcom.numnodes)
		{
			doomcom.numnodes = netnode;
		}

		if (playeringame[netconsole])
		{
			players[netconsole].playerstate = PST_GONE;
		}
		nodeingame[netnode] = false;
		nodejustleft[netnode] = false;
	}
	else if (nodejustleft[netnode]) // Packet Server
	{
		if (netnode + 1 == doomcom.numnodes)
		{
			doomcom.numnodes = netnode;
		}
		if (playeringame[netconsole])
		{
			players[netconsole].playerstate = PST_GONE;
		}
		nodejustleft[netnode] = false;
	}
	else return;

	if (netconsole == Net_Arbitrator)
	{
		// Pick a new network arbitrator
		for (int i = 0; i < MAXPLAYERS; i++)
		{
			if (i != netconsole && playeringame[i] && players[i].Bot == NULL)
			{
				Net_Arbitrator = i;
				players[i].settings_controller = true;
				Printf("%s is the new arbitrator\n", players[i].userinfo.GetName());
				break;
			}
		}
	}

	if (debugfile && NetMode == NET_PacketServer)
	{
		if (Net_Arbitrator == consoleplayer)
		{
			fprintf(debugfile, "I am the new master!\n");
		}
		else
		{
			fprintf(debugfile, "Node %d is the new master!\n", nodeforplayer[Net_Arbitrator]);
		}
	}

	if (demorecording)
	{
		G_CheckDemoStatus ();

		//WriteByte (DEM_DROPPLAYER, &demo_p);
		//WriteByte ((uint8_t)netconsole, &demo_p);
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
	uint8_t playerbytes[MAXNETNODES];
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
		currrecvtime[netconsole] = I_msTime ();

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
					uint8_t *foo = &netbuffer[2];
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

		// Pull current network delay from node
		netdelay[netnode][(nettics[netnode]+1) % BACKUPTICS] = netbuffer[k++];

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
			uint8_t *start;
			int i, tics;
			remoteresend[netnode] = false;

			start = &netbuffer[k];

			for (i = 0; i < numplayers; ++i)
			{
				int node = nodeforplayer[playerbytes[i]];

				SkipTicCmd (&start, nettics[node] - realstart);
				for (tics = nettics[node]; tics < realend; tics++)
					ReadTicCmd (&start, playerbytes[i], tics);

				nettics[nodeforplayer[playerbytes[i]]] = realend;
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
	int		lowtic;
	int 	nowtime;
	int 	newtics;
	int 	i,j;
	int 	realstart;
	uint8_t	*cmddata;
	bool	resendOnly;

	GC::CheckGC();

	if (ticdup == 0)
	{
		return;
	}

	// check time
	nowtime = I_GetTime ();
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

	// build new ticcmds for console player
	for (i = 0; i < newtics; i++)
	{
		I_StartTic ();
		D_ProcessEvents ();
		if (pauseext || (maketic - gametic) / ticdup >= BACKUPTICS/2-1)
			break;			// can't hold any more
		
		//Printf ("mk:%i ",maketic);
		G_BuildTiccmd (&localcmds[maketic % LOCALCMDTICS]);
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

	if (singletics)
		return; 		// singletic update is synchronous

	if (demoplayback)
	{
		resendto[0] = nettics[0] = (maketic / ticdup);
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
		if (NetMode == NET_PacketServer)
		{
			for (j = 0; j < MAXPLAYERS; j++)
			{
				if (playeringame[j] && players[j].Bot == NULL)
				{
					count++;
				}
			}

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
		uint8_t playerbytes[MAXPLAYERS];

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

		numtics = max(0, lowtic - realstart);
		if (numtics > BACKUPTICS)
			I_Error ("NetUpdate: Node %d missed too many tics", i);

		switch (net_extratic)
		{
		case 0:
		default: 
			resendto[i] = lowtic; break;
		case 1: resendto[i] = max(0, lowtic - 1); break;
		case 2: resendto[i] = nettics[i]; break;
		}

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

		// Send current network delay
		// The number of tics we just made should be removed from the count.
		netbuffer[k++] = ((maketic - numtics - gametic) / ticdup);

		if (numtics > 0)
		{
			int l;

			if (count > 1 && i != 0 && consoleplayer == Net_Arbitrator)
			{
				netbuffer[0] |= NCMD_MULTI;
				netbuffer[k++] = count;

				if (NetMode == NET_PacketServer)
				{
					for (l = 1, j = 0; j < MAXPLAYERS; j++)
					{
						if (playeringame[j] && players[j].Bot == NULL && j != playerfornode[i] && j != consoleplayer)
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
					// the other players.
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
						int len;
						uint8_t *spec;

						WriteWord (netcmds[playerbytes[l]][start].consistancy, &cmddata);
						spec = NetSpecs[playerbytes[l]][start].GetData (&len);
						if (spec != NULL)
						{
							memcpy (cmddata, spec, len);
							cmddata += len;
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

	if (!resendOnly)
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

			if (NetMode == NET_PeerToPeer)
			{
				int totalavg = 0;
				if (net_ticbalance)
				{
					// Try to guess ahead the time it takes to send responses to the slowest node
					int nodeavg = 0, arbavg = 0;

					for (j = 0; j < BACKUPTICS; j++)
					{
						arbavg += netdelay[nodeforplayer[Net_Arbitrator]][j];
						nodeavg += netdelay[0][j];
					}
					arbavg /= BACKUPTICS;
					nodeavg /= BACKUPTICS;

					// We shouldn't adapt if we are already the arbitrator isn't what we are waiting for, otherwise it just adds more latency
					if (arbavg > nodeavg)
					{
						lastaverage = totalavg = ((arbavg + nodeavg) / 2);
					}
					else
					{
						// Allow room to guess two tics ahead
						if (nodeavg > (arbavg + 2) && lastaverage > 0)
							lastaverage--;
						totalavg = lastaverage;
					}
				}
					
				mastertics = nettics[nodeforplayer[Net_Arbitrator]] + totalavg;
			}
			if (nettics[0] <= mastertics)
			{
				gametime--;
				if (debugfile) fprintf(debugfile, "-");
			}
			if (NetMode != NET_PacketServer)
			{
				frameskip[(maketic / ticdup) & 3] = (oldnettics > mastertics);
			}
			else
			{
				frameskip[(maketic / ticdup) & 3] = (oldnettics - mastertics) > 3;
			}
			if (frameskip[0] && frameskip[1] && frameskip[2] && frameskip[3])
			{
				skiptics = 1;
				if (debugfile) fprintf(debugfile, "+");
			}
			oldnettics = nettics[0];
		}
	}
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
//  2 One byte for NetMode setting
//  3 String with starting map's name
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
	uint32_t playersdetected[MAXNETNODES];
	uint8_t  gotsetup[MAXNETNODES];
};

bool DoArbitrate (void *userdata)
{
	ArbitrateData *data = (ArbitrateData *)userdata;
	char *s;
	uint8_t *stream;
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
			NetMode = netbuffer[2];

			stream = &netbuffer[3];
			s = ReadString (&stream);
			startmap = s;
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
			if (data->playersdetected[i] != uint32_t(1 << doomcom.numnodes) - 1 || !data->gotsetup[i])
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
		netbuffer[1] = (uint8_t)doomcom.ticdup;
		netbuffer[2] = NetMode;
		stream = &netbuffer[3];
		WriteString (startmap, &stream);
		WriteLong (rngseed, &stream);
		C_WriteCVars (&stream, CVAR_SERVERINFO, true);

		SendSetup (data->playersdetected, data->gotsetup, int(stream - netbuffer));
	}
	return false;
}

bool D_ArbitrateNetStart (void)
{
	ArbitrateData data;
	int i;

	// Return right away if we're just playing with ourselves.
	if (doomcom.numnodes == 1)
		return true;

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
		return false;
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
	return true;
}

static void SendSetup (uint32_t playersdetected[MAXNETNODES], uint8_t gotsetup[MAXNETNODES], int len)
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

bool D_CheckNetGame (void)
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

	// Packet server has proven to be rather slow over the internet. Print a warning about it.
	v = Args->CheckValue("-netmode");
	if (v != NULL && (atoi(v) != 0))
	{
		Printf(TEXTCOLOR_YELLOW "Notice: Using PacketServer (netmode 1) over the internet is prone to running too slow on some internet configurations."
			"\nIf the game is running well below expected speeds, use netmode 0 (P2P) instead.\n");
	}

	int result = I_InitNetwork ();
	// I_InitNetwork sets doomcom and netgame
	if (result == -1)
	{
		return false;
	}
	else if (result > 0)
	{
		// For now, stop auto selecting PacketServer, as it's more likely to cause confusion.
		//NetMode = NET_PacketServer;
	}
	if (doomcom.id != DOOMCOM_ID)
	{
		I_FatalError ("Doomcom buffer invalid!");
	}
	players[0].settings_controller = true;

	consoleplayer = doomcom.consoleplayer;

	if (consoleplayer == Net_Arbitrator)
	{
		v = Args->CheckValue("-netmode");
		if (v != NULL)
		{
			NetMode = atoi(v) != 0 ? NET_PacketServer : NET_PeerToPeer;
		}
		if (doomcom.numnodes > 1)
		{
			Printf("Selected " TEXTCOLOR_BLUE "%s" TEXTCOLOR_NORMAL " networking mode. (%s)\n", NetMode == NET_PeerToPeer ? "peer to peer" : "packet server",
				v != NULL ? "forced" : "auto");
		}

		if (Args->CheckParm("-extratic"))
		{
			net_extratic = 1;
		}
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
		if (!D_ArbitrateNetStart ()) return false;
	}

	// read values out of doomcom
	ticdup = doomcom.ticdup;

	for (i = 0; i < doomcom.numplayers; i++)
		playeringame[i] = true;
	for (i = 0; i < doomcom.numnodes; i++)
		nodeingame[i] = true;

	if (consoleplayer != Net_Arbitrator && doomcom.numnodes > 1)
	{
		Printf("Arbitrator selected " TEXTCOLOR_BLUE "%s" TEXTCOLOR_NORMAL " networking mode.\n", NetMode == NET_PeerToPeer ? "peer to peer" : "packet server");
	}

	if (!batchrun) Printf ("player %i of %i (%i nodes)\n",
			consoleplayer+1, doomcom.numplayers, doomcom.numnodes);
	
	return true;
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
		uint8_t *foo = &netbuffer[2];

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

// Forces playsim processing time to be consistent across frames.
// This improves interpolation for frames in between tics.
//
// With this cvar off the mods with a high playsim processing time will appear
// less smooth as the measured time used for interpolation will vary.

CVAR(Bool, r_ticstability, true, CVAR_ARCHIVE | CVAR_GLOBALCONFIG)

static uint64_t stabilityticduration = 0;
static uint64_t stabilitystarttime = 0;

static void TicStabilityWait()
{
	using namespace std::chrono;
	using namespace std::this_thread;

	if (!r_ticstability)
		return;

	uint64_t start = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
	while (true)
	{
		uint64_t cur = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
		if (cur - start > stabilityticduration)
			break;
	}
}

static void TicStabilityBegin()
{
	using namespace std::chrono;
	stabilitystarttime = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
}

static void TicStabilityEnd()
{
	using namespace std::chrono;
	uint64_t stabilityendtime = duration_cast<microseconds>(steady_clock::now().time_since_epoch()).count();
	stabilityticduration = min(stabilityendtime - stabilitystarttime, (uint64_t)1'000'000);
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

	bool doWait = (cl_capfps || pauseext || (r_NoInterpolate && !M_IsAnimated() /*&& gamestate != GS_INTERMISSION && gamestate != GS_INTRO*/));

	// get real tics
	if (doWait)
	{
		entertic = I_WaitForTic (oldentertics);
	}
	else
	{
		entertic = I_GetTime ();
	}
	realtics = entertic - oldentertics;
	oldentertics = entertic;

	// get available tics
	NetUpdate ();

	if (pauseext)
		return;

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
	
	// Uncapped framerate needs seprate checks
	if (counts == 0 && !doWait)
	{
		TicStabilityWait();

		// Check possible stall conditions
		Net_CheckLastReceived(counts);
		if (realtics >= 1)
		{
			C_Ticker();
			M_Ticker();
			// Repredict the player for new buffered movement
			P_UnPredictPlayer();
			P_PredictPlayer(&players[consoleplayer]);
		}
		return;
	}

	if (counts < 1)
		counts = 1;

	if (debugfile)
		fprintf (debugfile,
				 "=======real: %i  avail: %i  game: %i\n",
				 realtics, availabletics, counts);

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
		Net_CheckLastReceived (counts);

		// Update time returned by I_GetTime, but only if we are stuck in this loop
		if (lowtic < gametic + counts)
			I_SetFrameTime();

		// don't stay in here forever -- give the menu a chance to work
		if (I_GetTime () - entertic >= 1)
		{
			C_Ticker ();
			M_Ticker ();
			// Repredict the player for new buffered movement
			P_UnPredictPlayer();
			P_PredictPlayer(&players[consoleplayer]);
			return;
		}
	}

	//Tic lowtic is high enough to process this gametic. Clear all possible waiting info
	hadlate = false;
	for (i = 0; i < MAXPLAYERS; i++)
		players[i].waiting = false;
	lastglobalrecvtime = I_GetTime (); //Update the last time the game tic'd over

	// run the count tics
	if (counts > 0)
	{
		P_UnPredictPlayer();
		while (counts--)
		{
			TicStabilityBegin();
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
			G_Ticker();
			gametic++;

			NetUpdate ();	// check for new console commands
			TicStabilityEnd();
		}
		P_PredictPlayer(&players[consoleplayer]);
		S_UpdateSounds (players[consoleplayer].camera);	// move positional sounds
	}
	else
	{
		TicStabilityWait();
	}
}

void Net_CheckLastReceived (int counts)
{
	// [Ed850] Check to see the last time a packet was received.
	// If it's longer then 3 seconds, a node has likely stalled.
	if (I_GetTime() - lastglobalrecvtime >= TICRATE * 3)
	{
		lastglobalrecvtime = I_GetTime(); //Bump the count

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
			if (debugfile && !players[Net_Arbitrator].waiting)
				fprintf(debugfile, "Arbitrator is slow (%i to %i)\n",
				nettics[nodeforplayer[Net_Arbitrator]], gametic + counts);
			//Send resend request to the Arbitrator. Also mark the Arbitrator as waiting to display it in the hud.
			remoteresend[nodeforplayer[Net_Arbitrator]] = players[Net_Arbitrator].waiting = hadlate = true;
		}
	}
}

void Net_NewMakeTic (void)
{
	specials.NewMakeTic ();
}

void Net_WriteByte (uint8_t it)
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

void Net_WriteBytes (const uint8_t *block, int len)
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
		M_Free (m_Data);
		m_Data = NULL;
	}
	m_Len = m_BufferLen = 0;
}

void FDynamicBuffer::SetData (const uint8_t *data, int len)
{
	if (len > m_BufferLen)
	{
		m_BufferLen = (len + 255) & ~255;
		m_Data = (uint8_t *)M_Realloc (m_Data, m_BufferLen);
	}
	if (data != NULL)
	{
		m_Len = len;
		memcpy (m_Data, data, len);
	}
	else 
	{
		m_Len = 0;
	}
}

uint8_t *FDynamicBuffer::GetData (int *len)
{
	if (len)
		*len = m_Len;
	return m_Len ? m_Data : NULL;
}


static int RemoveClass(FLevelLocals *Level, const PClass *cls)
{
	AActor *actor;
	int removecount = 0;
	bool player = false;
	auto iterator = Level->GetThinkerIterator<AActor>(cls->TypeName);
	while ((actor = iterator.Next()))
	{
		if (actor->IsA(cls))
		{
			// [MC]Do not remove LIVE players.
			if (actor->player != NULL)
			{
				player = true;
				continue;
			}
			// [SP] Don't remove owned inventory objects.
			if (!actor->IsMapActor())
			{
				continue;
			}
			removecount++; 
			actor->ClearCounters();
			actor->Destroy();
		}
	}
	if (player)
		Printf("Cannot remove live players!\n");
	return removecount;

}
// [RH] Execute a special "ticcmd". The type byte should
//		have already been read, and the stream is positioned
//		at the beginning of the command's actual data.
void Net_DoCommand (int type, uint8_t **stream, int player)
{
	uint8_t pos = 0;
	char *s = NULL;
	int i;

	switch (type)
	{
	case DEM_SAY:
		{
			const char *name = players[player].userinfo.GetName();
			uint8_t who = ReadByte (stream);

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
				S_Sound (CHAN_VOICE, CHANF_UI, gameinfo.chatSound, 1, ATTN_NONE);
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
				S_Sound (CHAN_VOICE, CHANF_UI, gameinfo.chatSound, 1, ATTN_NONE);
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
		C_MidPrint (nullptr, s);
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
		cht_Give (&players[player], s, ReadLong (stream));
		if (player != consoleplayer)
		{
			FString message = GStrings("TXT_X_CHEATS");
			message.Substitute("%s", players[player].userinfo.GetName());
			Printf("%s: give %s\n", message.GetChars(), s);
		}

		break;

	case DEM_TAKECHEAT:
		s = ReadString (stream);
		cht_Take (&players[player], s, ReadLong (stream));
		break;

	case DEM_SETINV:
		s = ReadString(stream);
		i = ReadLong(stream);
		cht_SetInv(&players[player], s, i, !!ReadByte(stream));
		break;

	case DEM_WARPCHEAT:
		{
			int x, y, z;
			x = ReadWord (stream);
			y = ReadWord (stream);
			z = ReadWord (stream);
			P_TeleportMove (players[player].mo, DVector3(x, y, z), true);
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
		primaryLevel->flags |= LEVEL_CHANGEMAPCHEAT;
		primaryLevel->ChangeLevel(s, pos, 0);
		break;

	case DEM_SUICIDE:
		cht_Suicide (&players[player]);
		break;

	case DEM_ADDBOT:
		primaryLevel->BotInfo.TryAddBot (primaryLevel, stream, player);
		break;

	case DEM_KILLBOTS:
		primaryLevel->BotInfo.RemoveAllBots (primaryLevel, true);
		Printf ("Removed all bots\n");
		break;

	case DEM_CENTERVIEW:
		players[player].centering = true;
		break;

	case DEM_INVUSEALL:
		if (gamestate == GS_LEVEL && !paused)
		{
			AActor *item = players[player].mo->Inventory;
			auto pitype = PClass::FindActor(NAME_PuzzleItem);
			while (item != nullptr)
			{
				AActor *next = item->Inventory;
				IFVIRTUALPTRNAME(item, NAME_Inventory, UseAll)
				{
					VMValue param[] = { item, players[player].mo };
					VMCall(func, param, 2, nullptr, 0);
				}
				item = next;
			}
		}
		break;

	case DEM_INVUSE:
	case DEM_INVDROP:
		{
			uint32_t which = ReadLong (stream);
			int amt = -1;

			if (type == DEM_INVDROP) amt = ReadLong(stream);

			if (gamestate == GS_LEVEL && !paused
				&& players[player].playerstate != PST_DEAD)
			{
				auto item = players[player].mo->Inventory;
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
						players[player].mo->DropInventory (item, amt);
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
			PClassActor *typeinfo;
			int angle = 0;
			int16_t tid = 0;
			uint8_t special = 0;
			int args[5];

			s = ReadString (stream);
			if (type >= DEM_SUMMON2 && type <= DEM_SUMMONFOE2)
			{
				angle = ReadWord(stream);
				tid = ReadWord(stream);
				special = ReadByte(stream);
				for(i = 0; i < 5; i++) args[i] = ReadLong(stream);
			}

			typeinfo = PClass::FindActor(s);
			if (typeinfo != NULL)
			{
				AActor *source = players[player].mo;
				if (source != NULL)
				{
					if (GetDefaultByType (typeinfo)->flags & MF_MISSILE)
					{
						P_SpawnPlayerMissile (source, 0, 0, 0, typeinfo, source->Angles.Yaw);
					}
					else
					{
						const AActor *def = GetDefaultByType (typeinfo);
						DVector3 spawnpos = source->Vec3Angle(def->radius * 2 + source->radius, source->Angles.Yaw, 8.);

						AActor *spawned = Spawn (primaryLevel, typeinfo, spawnpos, ALLOW_REPLACE);
						if (spawned != NULL)
						{
							if (type == DEM_SUMMONFRIEND || type == DEM_SUMMONFRIEND2 || type == DEM_SUMMONMBF)
							{
								if (spawned->CountsAsKill()) 
								{
									primaryLevel->total_monsters--;
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

							if (type >= DEM_SUMMON2 && type <= DEM_SUMMONFOE2)
							{
								spawned->Angles.Yaw = source->Angles.Yaw - angle;
								spawned->special = special;
								for(i = 0; i < 5; i++) {
									spawned->args[i] = args[i];
								}
								if(tid) spawned->SetTID(tid);
							}
						}
					}
				}
			}
		}
		break;

	case DEM_SPRAY:
		s = ReadString(stream);
		SprayDecal(players[player].mo, s);
		break;

	case DEM_MDK:
		s = ReadString(stream);
		cht_DoMDK(&players[player], s);
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
		}
		break;

	case DEM_SAVEGAME:
		if (gamestate == GS_LEVEL)
		{
			s = ReadString (stream);
			savegamefile = s;
			delete[] s;
			s = ReadString (stream);
			savedescription = s;
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
			float newfov = ReadFloat (stream);

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
		players[player].DesiredFOV = ReadFloat (stream);
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
			s = ReadString(stream);
			int argn = ReadByte(stream);

			RunScript(stream, players[player].mo, -FName(s).GetIndex(), argn & 127, (argn & 128) ? ACS_ALWAYS : 0);
		}
		break;

	case DEM_RUNSPECIAL:
		{
			int snum = ReadWord(stream);
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
				P_ExecuteSpecial(primaryLevel, snum, NULL, players[player].mo, false, arg[0], arg[1], arg[2], arg[3], arg[4]);
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
			FString msg = cht_Morph (players + player, PClass::FindActor (s), false);
			if (player == consoleplayer)
			{
				Printf ("%s\n", msg[0] != '\0' ? msg.GetChars() : "Morph failed.");
			}
		}
		break;

	case DEM_ADDCONTROLLER:
		{
			uint8_t playernum = ReadByte (stream);
			players[playernum].settings_controller = true;

			if (consoleplayer == playernum || consoleplayer == Net_Arbitrator)
				Printf ("%s has been added to the controller list.\n", players[playernum].userinfo.GetName());
		}
		break;

	case DEM_DELCONTROLLER:
		{
			uint8_t playernum = ReadByte (stream);
			players[playernum].settings_controller = false;

			if (consoleplayer == playernum || consoleplayer == Net_Arbitrator)
				Printf ("%s has been removed from the controller list.\n", players[playernum].userinfo.GetName());
		}
		break;

	case DEM_KILLCLASSCHEAT:
		{
			s = ReadString (stream);
			int killcount = 0;
			PClassActor *cls = PClass::FindActor(s);

			if (cls != NULL)
			{
				killcount = primaryLevel->Massacre(false, cls->TypeName);
				PClassActor *cls_rep = cls->GetReplacement(primaryLevel);
				if (cls != cls_rep)
				{
					killcount += primaryLevel->Massacre(false, cls_rep->TypeName);
				}
				Printf ("Killed %d monsters of type %s.\n",killcount, s);
			}
			else
			{
				Printf ("%s is not an actor class.\n", s);
			}

		}
		break;
	case DEM_REMOVE:
	{
		s = ReadString(stream);
		int removecount = 0;
		PClassActor *cls = PClass::FindActor(s);
		if (cls != NULL && cls->IsDescendantOf(RUNTIME_CLASS(AActor)))
		{
			removecount = RemoveClass(primaryLevel, cls);
			const PClass *cls_rep = cls->GetReplacement(primaryLevel);
			if (cls != cls_rep)
			{
				removecount += RemoveClass(primaryLevel, cls_rep);
			}
			Printf("Removed %d actors of type %s.\n", removecount, s);
		}
		else
		{
			Printf("%s is not an actor class.\n", s);
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
				players[pnum].weapons.ClearSlot(slot);
			}
			for(i = 0; i < count; ++i)
			{
				PClassActor *wpn = Net_ReadWeapon(stream);
				players[pnum].weapons.AddSlot(slot, wpn, pnum == consoleplayer);
			}
		}
		break;

	case DEM_ADDSLOT:
		{
			int slot = ReadByte(stream);
			PClassActor *wpn = Net_ReadWeapon(stream);
			players[player].weapons.AddSlot(slot, wpn, player == consoleplayer);
		}
		break;

	case DEM_ADDSLOTDEFAULT:
		{
			int slot = ReadByte(stream);
			PClassActor *wpn = Net_ReadWeapon(stream);
			players[player].weapons.AddSlotDefault(slot, wpn, player == consoleplayer);
		}
		break;

	case DEM_SETPITCHLIMIT:
		players[player].MinPitch = -(double)ReadByte(stream);		// up
		players[player].MaxPitch = (double)ReadByte(stream);		// down
		break;

	case DEM_ADVANCEINTER:
		F_AdvanceIntermission();
		break;

	case DEM_REVERTCAMERA:
		players[player].camera = players[player].mo;
		break;

	case DEM_FINISHGAME:
		// Simulate an end-of-game action
		primaryLevel->ChangeLevel(NULL, 0, 0);
		break;

	case DEM_NETEVENT:
		{
			s = ReadString(stream);
			int argn = ReadByte(stream);
			int arg[3] = { 0, 0, 0 };
			for (int i = 0; i < 3; i++)
				arg[i] = ReadLong(stream);
			bool manual = !!ReadByte(stream);
			primaryLevel->localEventManager->Console(player, s, arg[0], arg[1], arg[2], manual);
		}
		break;

	default:
		I_Error ("Unknown net command: %d", type);
		break;
	}

	if (s)
		delete[] s;
}

// Used by DEM_RUNSCRIPT, DEM_RUNSCRIPT2, and DEM_RUNNAMEDSCRIPT
static void RunScript(uint8_t **stream, AActor *pawn, int snum, int argn, int always)
{
	if (pawn == nullptr)
	{
		// Scripts can be invoked without a level loaded, e.g. via puke(name) CCMD in fullscreen console
		return;
	}

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
	P_StartScript(pawn->Level, pawn, NULL, snum, primaryLevel->MapName, arg, min<int>(countof(arg), argn), ACS_NET | always);
}

void Net_SkipCommand (int type, uint8_t **stream)
{
	uint8_t t;
	size_t skip;

	switch (type)
	{
		case DEM_SAY:
			skip = strlen ((char *)(*stream + 1)) + 2;
			break;

		case DEM_ADDBOT:
			skip = strlen ((char *)(*stream + 1)) + 6;
			break;

		case DEM_GIVECHEAT:
		case DEM_TAKECHEAT:
			skip = strlen ((char *)(*stream)) + 5;
			break;

		case DEM_SETINV:
			skip = strlen((char *)(*stream)) + 6;
			break;

		case DEM_NETEVENT:
			skip = strlen((char *)(*stream)) + 15;
			break;

		case DEM_SUMMON2:
		case DEM_SUMMONFRIEND2:
		case DEM_SUMMONFOE2:
			skip = strlen ((char *)(*stream)) + 26;
			break;
		case DEM_CHANGEMAP2:
			skip = strlen((char *)(*stream + 1)) + 2;
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
		case DEM_REMOVE:
		case DEM_SPRAY:
		case DEM_MORPHEX:
		case DEM_KILLCLASSCHEAT:
		case DEM_MDK:
			skip = strlen ((char *)(*stream)) + 1;
			break;

		case DEM_WARPCHEAT:
			skip = 6;
			break;

		case DEM_INVUSE:
		case DEM_FOV:
		case DEM_MYFOV:
			skip = 4;
			break;

		case DEM_INVDROP:
			skip = 8;
			break;

		case DEM_GENERICCHEAT:
		case DEM_DROPPLAYER:
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
			skip = 3 + *(*stream + 2) * 4;
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

// This was taken out of shared_hud, because UI code shouldn't do low level calculations that may change if the backing implementation changes.
int Net_GetLatency(int *ld, int *ad)
{
	int i, localdelay = 0, arbitratordelay = 0;

	for (i = 0; i < BACKUPTICS; i++) localdelay += netdelay[0][i];
	for (i = 0; i < BACKUPTICS; i++) arbitratordelay += netdelay[nodeforplayer[Net_Arbitrator]][i];
	arbitratordelay = ((arbitratordelay / BACKUPTICS) * ticdup) * (1000 / TICRATE);
	localdelay = ((localdelay / BACKUPTICS) * ticdup) * (1000 / TICRATE);
	int severity = 0;

	if (max(localdelay, arbitratordelay) > 200)
	{
		severity = 1;
	}
	if (max(localdelay, arbitratordelay) > 400)
	{
		severity = 2;
	}
	if (max(localdelay, arbitratordelay) >= ((BACKUPTICS / 2 - 1) * ticdup) * (1000 / TICRATE))
	{
		severity = 3;
	}
	*ld = localdelay;
	*ad = arbitratordelay;
	return severity;
}

// [RH] List "ping" times
CCMD (pings)
{
	int i;
	for (i = 0; i < MAXPLAYERS; i++)
		if (playeringame[i])
			Printf ("% 4" PRId64 " %s\n", currrecvtime[i] - lastrecvtime[i],
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

	if (players[playernum].Bot != NULL)
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
