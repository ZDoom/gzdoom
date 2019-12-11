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
#include "menu/menu.h"
#include "i_video.h"
#include "i_net.h"
#include "g_game.h"
#include "c_console.h"
#include "d_netinf.h"
#include "net.h"
#include "netclient.h"
#include "netserver.h"
#include "netsingle.h"
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
#include "events.h"
#include "i_time.h"
#include "vm.h"

std::unique_ptr<Network> network;
std::unique_ptr<Network> netconnect;

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

#if 0
CVAR(Int, net_fakelatency, 0, 0);

struct PacketStore
{
	int timer;
	NetPacket message;
};

static TArray<PacketStore> InBuffer;
static TArray<PacketStore> OutBuffer;
#endif

#if 0 // For reference. Remove when c/s migration is complete

void Network::ReadTicCmd(uint8_t **stream, int player, int tic)
{
	int type;
	uint8_t *start;
	ticcmd_t *tcmd;

	int ticmod = tic % BACKUPTICS;

	tcmd = &netcmds[player][ticmod];
	tcmd->consistency = ReadWord(stream);

	start = *stream;

	while ((type = ReadByte(stream)) != DEM_USERCMD && type != DEM_EMPTYUSERCMD)
		Net_SkipCommand(type, stream);

	NetSpecs[player][ticmod].SetData(start, int(*stream - start - 1));

	if (type == DEM_USERCMD)
	{
		UnpackUserCmd(&tcmd->ucmd, tic ? &netcmds[player][(tic - 1) % BACKUPTICS].ucmd : NULL, stream);
	}
	else
	{
		if (tic)
		{
			memcpy(&tcmd->ucmd, &netcmds[player][(tic - 1) % BACKUPTICS].ucmd, sizeof(tcmd->ucmd));
		}
		else
		{
			memset(&tcmd->ucmd, 0, sizeof(tcmd->ucmd));
		}
	}

	if (player == consoleplayer && tic>BACKUPTICS)
		assert(consistency[player][ticmod] == tcmd->consistency);
}

// Works out player numbers among the net participants
void Network::D_CheckNetGame()
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

	int port = DOOMPORT;
	v = Args->CheckValue("-port");
	if (v)
	{
		port = atoi(v);
		Printf("using alternate port %i\n", port);
	}

	netgame = false;
	multiplayer = false;
	consoleplayer = 0;

	v = Args->CheckValue("-dup");
	if (v)
	{
		ticdup = clamp(atoi(v), 1, MAXTICDUP);
	}
	else
	{
		ticdup = 1;
	}

	players[0].settings_controller = true;

	/*
	consoleplayer = doomcom->consoleplayer;

	if (consoleplayer == Net_Arbitrator)
	{
		v = Args->CheckValue("-netmode");
		if (v != NULL)
		{
			NetMode = atoi(v) != 0 ? NET_PacketServer : NET_PeerToPeer;
		}
		if (doomcom->numnodes > 1)
		{
			Printf("Selected " TEXTCOLOR_BLUE "%s" TEXTCOLOR_NORMAL " networking mode. (%s)\n", NetMode == NET_PeerToPeer ? "peer to peer" : "packet server",
				v != NULL ? "forced" : "auto");
		}

		if (Args->CheckParm("-extratic"))
		{
			net_extratic = 1;
		}
	}
	*/

	// [RH] Setup user info
	D_SetupUserInfo();

	if (Args->CheckParm("-debugfile"))
	{
		char filename[20];
		mysnprintf(filename, countof(filename), "debug%i.txt", consoleplayer);
		Printf("debug output to: %s\n", filename);
		debugfile = fopen(filename, "w");
	}

	if (netgame)
	{
		GameConfig->ReadNetVars();	// [RH] Read network ServerInfo cvars
		//D_ArbitrateNetStart();
	}

	consoleplayer = 0;
	playeringame[0] = true;
	nodeingame[0] = true;

	/*
	for (i = 0; i < doomcom->numplayers; i++)
		playeringame[i] = true;
	for (i = 0; i < doomcom->numnodes; i++)
		nodeingame[i] = true;

	if (consoleplayer != Net_Arbitrator && doomcom->numnodes > 1)
	{
		Printf("Arbitrator selected " TEXTCOLOR_BLUE "%s" TEXTCOLOR_NORMAL " networking mode.\n", NetMode == NET_PeerToPeer ? "peer to peer" : "packet server");
	}

	if (!batchrun) Printf("player %i of %i (%i nodes)\n",
		consoleplayer + 1, doomcom->numplayers, doomcom->numnodes);
	*/
}
#endif

// This was taken out of shared_hud, because UI code shouldn't do low level calculations that may change if the backing implementation changes.
int Net_GetLatency(int *ld, int *ad)
{
	*ld = 123;
	*ad = 123;
	return 0;
#if 0
	int i, localdelay = 0, arbitratordelay = 0;

	for (i = 0; i < BACKUPTICS; i++) localdelay += netdelay[0][i];
	for (i = 0; i < BACKUPTICS; i++) arbitratordelay += netdelay[nodeforplayer[Net_Arbitrator]][i];
	arbitratordelay = ((arbitratordelay / BACKUPTICS) * ticdup) * (1000 / TICRATE);
	localdelay = ((localdelay / BACKUPTICS) * ticdup) * (1000 / TICRATE);
	int severity = 0;

	if (MAX(localdelay, arbitratordelay) > 200)
	{
		severity = 1;
	}
	if (MAX(localdelay, arbitratordelay) > 400)
	{
		severity = 2;
	}
	if (MAX(localdelay, arbitratordelay) >= ((BACKUPTICS / 2 - 1) * ticdup) * (1000 / TICRATE))
	{
		severity = 3;
	}
	*ld = localdelay;
	*ad = arbitratordelay;
	return severity;
#endif
}

CCMD (pings)
{
	network->ListPingTimes();
}

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

	network->Network_Controller (atoi (argv[1]), true);
}

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

	network->Network_Controller (atoi (argv[1]), false);
}

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

CCMD(connect)
{
	if (argv.argc() < 2)
	{
		Printf("Usage: connect <server>\n");
		return;
	}

	netconnect.reset(new NetClient(argv[1]));
}

CCMD(hostgame)
{
	netconnect.reset();
	std::unique_ptr<NetServer> netgame(new NetServer());
	network = std::move(netgame);
	netgame->Init();
}

void Startup()
{
}

void Net_ClearBuffers()
{
}

bool D_CheckNetGame()
{
	if (!network)
	{
		network.reset(new NetSinglePlayer());
		D_SetupUserInfo();

		if (Args->CheckParm("-debugfile"))
		{
			char filename[20];
			mysnprintf(filename, countof(filename), "debug%i.txt", consoleplayer);
			Printf("debug output to: %s\n", filename);
			debugfile = fopen(filename, "w");
		}

		if (netgame)
		{
			GameConfig->ReadNetVars();	// [RH] Read network ServerInfo cvars
		}
	}

	return true;
}

void D_QuitNetGame()
{
}

void NetUpdate()
{
}

void CloseNetwork()
{
}
