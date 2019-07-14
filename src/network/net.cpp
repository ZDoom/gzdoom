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

EXTERN_CVAR (Int, disableautosave)
EXTERN_CVAR (Int, autosavecount)

std::unique_ptr<Network> network;
std::unique_ptr<Network> netconnect;

//#define SIMULATEERRORS		(RAND_MAX/3)
#define SIMULATEERRORS			0

extern uint8_t		*demo_p;		// [RH] Special "ticcmds" get recorded in demos
extern FString	savedescription;
extern FString	savegamefile;

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

void D_ProcessEvents (void); 
ticcmd_t G_BuildTiccmd ();
void D_DoAdvanceDemo (void);

static void RunScript(uint8_t **stream, AActor *pawn, int snum, int argn, int always);

extern	bool	 advancedemo;

CVAR (Bool, cl_capfps, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

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

void Network::WriteByte(uint8_t it)
{
	WriteBytes(&it, 1);
}

void Network::WriteWord(short it)
{
	uint16_t buf;
	uint8_t *streamptr = (uint8_t*)&buf;
	::WriteWord(it, &streamptr);
	WriteBytes((const uint8_t*)&buf, sizeof(uint16_t));
}

void Network::WriteLong(int it)
{
	uint32_t buf;
	uint8_t *streamptr = (uint8_t*)&buf;
	::WriteLong(it, &streamptr);
	WriteBytes((const uint8_t*)&buf, sizeof(uint32_t));
}

void Network::WriteFloat(float it)
{
	float buf;
	uint8_t *streamptr = (uint8_t*)&buf;
	::WriteFloat(it, &streamptr);
	WriteBytes((const uint8_t*)&buf, sizeof(float));
}

void Network::WriteString(const char *it)
{
	int length = (int)strlen(it);
	WriteBytes((const uint8_t*)it, length + 1);
}

int Network::GetHighPingThreshold() const
{
	return ((BACKUPTICS / 2 - 1) * ticdup) * (1000 / TICRATE);
}

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

//==========================================================================
//
// Dynamic buffer interface
//
//==========================================================================

FDynamicBuffer::FDynamicBuffer()
{
}

FDynamicBuffer::FDynamicBuffer(const FDynamicBuffer &src)
{
	SetData(src.GetData(), src.GetSize());
}

FDynamicBuffer::~FDynamicBuffer ()
{
	if (m_Data)
	{
		M_Free (m_Data);
		m_Data = nullptr;
	}
	m_Len = m_BufferLen = 0;
}

FDynamicBuffer &FDynamicBuffer::operator=(const FDynamicBuffer &src)
{
	if (this != &src)
		SetData(src.GetData(), src.GetSize());
	return *this;
}

void FDynamicBuffer::SetData (const uint8_t *data, int len)
{
	if (len > m_BufferLen)
	{
		m_BufferLen = (len + 255) & ~255;
		m_Data = (uint8_t *)M_Realloc (m_Data, m_BufferLen);
	}
	if (data)
	{
		m_Len = len;
		memcpy (m_Data, data, len);
	}
	else 
	{
		m_Len = 0;
	}
}

void FDynamicBuffer::AppendData(const uint8_t *data, int len)
{
	int pos = m_Len;
	int neededlen = m_Len + len;

	if (neededlen > m_BufferLen)
	{
		m_BufferLen = (neededlen + 255) & ~255;
		m_Data = (uint8_t *)M_Realloc(m_Data, m_BufferLen);
	}

	memcpy(m_Data + pos, data, len);
	m_Len += len;
}

static int RemoveClass(const PClass *cls)
{
	AActor *actor;
	int removecount = 0;
	bool player = false;
	TThinkerIterator<AActor> iterator(primaryLevel, cls);
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

void Net_RunCommands(FDynamicBuffer &buffer, int player)
{
	int len = buffer.GetSize();
	uint8_t *stream = buffer.GetData();
	if (stream)
	{
		uint8_t *end = stream + len;
		while (stream < end)
		{
			int type = ReadByte(&stream);
			Net_DoCommand(type, &stream, player);
		}
		if (!demorecording)
			buffer.SetData(nullptr, 0);
	}
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
		cht_Give (&players[player], s, ReadLong (stream));
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
		network->WriteByte (DEM_DOAUTOSAVE);
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
			char *sname = ReadString(stream);
			int argn = ReadByte(stream);

			RunScript(stream, players[player].mo, -FName(sname), argn & 127, (argn & 128) ? ACS_ALWAYS : 0);
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
			char *classname = ReadString (stream);
			int killcount = 0;
			PClassActor *cls = PClass::FindActor(classname);

			if (cls != NULL)
			{
				killcount = primaryLevel->Massacre(false, cls->TypeName);
				PClassActor *cls_rep = cls->GetReplacement(primaryLevel);
				if (cls != cls_rep)
				{
					killcount += primaryLevel->Massacre(false, cls_rep->TypeName);
				}
				Printf ("Killed %d monsters of type %s.\n",killcount, classname);
			}
			else
			{
				Printf ("%s is not an actor class.\n", classname);
			}

		}
		break;
	case DEM_REMOVE:
	{
		char *classname = ReadString(stream);
		int removecount = 0;
		PClassActor *cls = PClass::FindActor(classname);
		if (cls != NULL && cls->IsDescendantOf(RUNTIME_CLASS(AActor)))
		{
			removecount = RemoveClass(cls);
			const PClass *cls_rep = cls->GetReplacement(primaryLevel);
			if (cls != cls_rep)
			{
				removecount += RemoveClass(cls_rep);
			}
			Printf("Removed %d actors of type %s.\n", removecount, classname);
		}
		else
		{
			Printf("%s is not an actor class.\n", classname);
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
	P_StartScript(pawn->Level, pawn, NULL, snum, primaryLevel->MapName, arg, MIN<int>(countof(arg), argn), ACS_NET | always);
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
	network.reset(new NetServer());
}
