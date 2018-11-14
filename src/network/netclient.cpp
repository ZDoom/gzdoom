//-----------------------------------------------------------------------------
//
// Copyright 1993-1996 id Software
// Copyright 1999-2016 Randy Heit
// Copyright 2002-2016 Christoph Oelckers
// Copyright 2018 Magnus Norddahl
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

#include <stddef.h>
#include <inttypes.h>

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
#include "netclient.h"
#include "netsingle.h"
#include "cmdlib.h"
#include "s_sound.h"
#include "m_cheat.h"
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
#include "r_utility.h"
#include "a_keys.h"
#include "intermission/intermission.h"
#include "g_levellocals.h"
#include "events.h"
#include "i_time.h"
#include <cmath>

CVAR( Int, cl_showspawnnames, 0, CVAR_ARCHIVE )

NetClient::NetClient(FString server)
{
	Printf("Connecting to %s..\n", server.GetChars());

	mComm = I_InitNetwork(0);
	mServerNode = mComm->Connect(server);
	mStatus = NodeStatus::InPreGame;

	NetOutputPacket packet(mServerNode);

	NetCommand cmd ( NetPacketType::ConnectRequest );
	cmd.addString("ZDoom Connect Request");
	cmd.writeCommandToStream (packet.stream);

	mComm->PacketSend(packet);
}

void NetClient::Update()
{
	while (true)
	{
		// [BB] Don't check net packets while we are supposed to load a map.
		// This way the commands from the full update will not be parsed before we loaded the map.
		if ( ( gameaction == ga_newgame ) || ( gameaction == ga_newgame2 ) )
			return;

		NetInputPacket packet;
		mComm->PacketGet(packet);
		if (packet.node == -1)
			break;

		if (packet.node != mServerNode)
		{
			mComm->Close(packet.node);
		}
		else if (packet.stream.IsAtEnd())
		{
			OnClose();
			break;
		}
		else
		{
			while ( packet.stream.IsAtEnd() == false )
			{
				NetPacketType type = (NetPacketType)packet.stream.ReadByte();
				switch (type)
				{
				default: OnClose(); break;
				case NetPacketType::ConnectResponse: OnConnectResponse(packet.stream); break;
				case NetPacketType::Disconnect: OnDisconnect(); break;
				case NetPacketType::Tic: OnTic(packet.stream); break;
				case NetPacketType::SpawnActor: OnSpawnActor(packet.stream); break;
				case NetPacketType::DestroyActor: OnDestroyActor(packet.stream); break;
				}
			}
			break;
		}

		if (mStatus == NodeStatus::Closed)
		{
			if (network.get() == this)
			{
				network.reset(new NetSinglePlayer());
				G_EndNetGame();
			}
			else
			{
				netconnect.reset();
			}
			return;
		}
	}
}

void NetClient::SetCurrentTic(int tictime)
{
	gametic = tictime;
	mSendTic = gametic + 10;

	int jitter = 2;
	if (mLastReceivedTic == -1)
	{
		mServerTic = 0;
	}
	else
	{
		if (mServerTicDelta == -1 || abs(gametic + mServerTicDelta - mLastReceivedTic) > jitter)
		{
			//Printf("netcable icon! ;)\n");
			mServerTicDelta = mLastReceivedTic - gametic - jitter;
		}

		mServerTic = MAX(gametic + mServerTicDelta, 0);
	}

	mCurrentInput[consoleplayer] = mSentInput[gametic % BACKUPTICS];
}

void NetClient::EndCurrentTic()
{
	mCurrentCommands = mSendCommands;
	mSendCommands.Clear();

	if (mStatus != NodeStatus::InGame)
		return;

	int targettic = (mSendTic + mServerTicDelta);

	NetOutputPacket packet(mServerNode);

	NetCommand cmd ( NetPacketType::Tic );
	cmd.addByte (targettic); // target gametic
	cmd.addBuffer ( &mSentInput[(mSendTic - 1) % BACKUPTICS].ucmd, sizeof(usercmd_t) );
	cmd.writeCommandToStream ( packet.stream );

	mComm->PacketSend(packet);

	TicUpdate &update = mTicUpdates[mServerTic % BACKUPTICS];
	if (update.received)
	{
		if (playeringame[consoleplayer] && players[consoleplayer].mo)
		{
			APlayerPawn *pawn = players[consoleplayer].mo;
			if ((update.Pos - pawn->Pos()).LengthSquared() > 10.0)
				pawn->SetOrigin(update.Pos, false);
			else
				P_TryMove(pawn, update.Pos.XY(), true);
			pawn->Vel = update.Vel;
			pawn->Angles.Yaw = update.yaw;
			pawn->Angles.Pitch = update.pitch;
		}

		for (unsigned int i = 0; i < update.syncUpdates.Size(); i++)
		{
			const TicSyncData &syncdata = update.syncUpdates[i];
			AActor *netactor = mNetIDList.findPointerByID(syncdata.netID);
			if (netactor)
			{
				netactor->SetOrigin(syncdata.x, syncdata.y, syncdata.z, true);
				netactor->Angles.Yaw = syncdata.yaw;
				netactor->Angles.Pitch = syncdata.pitch;
				netactor->sprite = syncdata.sprite;
				netactor->frame = syncdata.frame;
			}
		}

		update = TicUpdate();
	}
}

int NetClient::GetSendTick() const
{
	return mSendTic;
}

ticcmd_t NetClient::GetPlayerInput(int player) const
{
	return mCurrentInput[player];
}

ticcmd_t NetClient::GetSentInput(int tic) const
{
	return mSentInput[tic % BACKUPTICS];
}

void NetClient::RunCommands(int player)
{
	if (player == consoleplayer)
	{
		Net_RunCommands(mCurrentCommands, consoleplayer);
	}
}

void NetClient::WriteLocalInput(ticcmd_t cmd)
{
	mSentInput[(mSendTic - 1) % BACKUPTICS] = cmd;
}

void NetClient::WriteBotInput(int player, const ticcmd_t &cmd)
{
	mCurrentInput[player] = cmd;
}

void NetClient::WriteBytes(const uint8_t *block, int len)
{
	mSendCommands.AppendData(block, len);
}

int NetClient::GetPing(int player) const
{
	return 0;
}

int NetClient::GetServerPing() const
{
	return 0;
}

void NetClient::ListPingTimes()
{
}

void NetClient::Network_Controller(int playernum, bool add)
{
}

void NetClient::ActorSpawned(AActor *actor)
{
	actor->syncdata.NetID = -1;
}

void NetClient::ActorDestroyed(AActor *actor)
{
}

void NetClient::OnClose()
{
	mComm->Close(mServerNode);
	mServerNode = -1;
	mStatus = NodeStatus::Closed;

	if (network.get() == this)
	{
		Printf("Disconnected\n");
	}
	else
	{
		Printf("Could not connect\n");
	}
}

void NetClient::OnConnectResponse(ByteInputStream &stream)
{
	int version = stream.ReadByte(); // Protocol version
	if (version == 1)
	{
		int playernum = stream.ReadByte();
		if (playernum > 0 && playernum < MAXPLAYERS) // Join accepted
		{
			mPlayer = playernum;
			mStatus = NodeStatus::InGame;

			G_InitClientNetGame(mPlayer, "e1m1");

			network = std::move(netconnect);
		}
		else // Server full
		{
			mComm->Close(mServerNode);
			mServerNode = -1;
			mStatus = NodeStatus::Closed;

			Printf("Could not connect: server is full!\n");
		}
	}
	else
	{
		Printf("Could not connect: version mismatch.\n");
		mComm->Close(mServerNode);
		mServerNode = -1;
		mStatus = NodeStatus::Closed;
	}
}

void NetClient::OnDisconnect()
{
	mComm->Close(mServerNode);
	mServerNode = -1;
	mStatus = NodeStatus::Closed;
}

void NetClient::UpdateLastReceivedTic(int tic)
{
	if (mLastReceivedTic != -1)
	{
		int delta = tic - (mLastReceivedTic & 0xff);
		if (delta > 128) delta -= 256;
		else if (delta < -128) delta += 256;
		mLastReceivedTic += delta;
	}
	else
	{
		mLastReceivedTic = tic;
	}
	mLastReceivedTic = MAX(mLastReceivedTic, 0);
}

void NetClient::OnTic(ByteInputStream &stream)
{
	UpdateLastReceivedTic(stream.ReadByte());

	TicUpdate update;
	update.received = true;
	update.Pos.X = stream.ReadFloat();
	update.Pos.Y = stream.ReadFloat();
	update.Pos.Z = stream.ReadFloat();
	update.Vel.X = stream.ReadFloat();
	update.Vel.Y = stream.ReadFloat();
	update.Vel.Z = stream.ReadFloat();
	update.yaw = stream.ReadFloat();
	update.pitch = stream.ReadFloat();

	while (true)
	{
		TicSyncData syncdata;
		syncdata.netID = stream.ReadShort();
		if (syncdata.netID == -1)
			break;

		syncdata.x = stream.ReadFloat();
		syncdata.y = stream.ReadFloat();
		syncdata.z = stream.ReadFloat();
		syncdata.yaw = stream.ReadFloat();
		syncdata.pitch = stream.ReadFloat();
		syncdata.sprite = stream.ReadShort();
		syncdata.frame = stream.ReadByte();
		update.syncUpdates.Push(syncdata);
	}

	mTicUpdates[mLastReceivedTic % BACKUPTICS] = update;
}

void NetClient::OnSpawnActor(ByteInputStream &stream)
{
	const int16_t netID = stream.ReadShort();
	const float x = stream.ReadFloat();
	const float y = stream.ReadFloat();
	const float z = stream.ReadFloat();

	AActor *oldNetActor = mNetIDList.findPointerByID(netID);

	// If there's already an actor with this net ID, destroy it.
	if (oldNetActor)
	{
		oldNetActor->Destroy();
		mNetIDList.freeID(netID);
	}

	ANetSyncActor *actor = Spawn<ANetSyncActor>(DVector3(x, y, z), NO_REPLACE);
	mNetIDList.useID(netID, actor);
}

void NetClient::OnDestroyActor(ByteInputStream &stream)
{
	const int16_t netID = stream.ReadShort();
	AActor *actor = mNetIDList.findPointerByID(netID);
	actor->Destroy();
}

#if 0 // Use playerpawn, problematic as client pawn behavior may have to deviate from server side
	// This player is now in the game.
	playeringame[player] = true;
	player_t &p = players[player];

	if ( cl_showspawnnames )
		Printf ( "Spawning player %d at %f,%f,%f (id %d)\n", player, x, y, z, netID );

	DVector3 spawn ( x, y, z );

	p.mo = static_cast<APlayerPawn *>(Spawn (p.cls, spawn, NO_REPLACE));

	if ( p.mo )
	{
		// Set the network ID.
		p.mo->syncdata.NetID = netID;
		mNetIDList.useID ( netID, p.mo );
	}
#endif

IMPLEMENT_CLASS(ANetSyncActor, false, false)

