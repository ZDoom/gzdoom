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
			UpdateLastReceivedTic(packet.stream.ReadByte());

			if (mStatus == NodeStatus::InPreGame)
			{
				ProcessCommands(packet.stream);
			}
			else
			{
				auto &ticUpdate = mTicUpdates[mLastReceivedTic % BACKUPTICS];
				ticUpdate.Resize(packet.stream.BytesLeft());
				packet.stream.ReadBuffer(ticUpdate.Data(), ticUpdate.Size());
			}
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

	if (abs(gametic + mServerTicDelta - mLastReceivedTic) > jitter)
	{
		mServerTicDelta = mLastReceivedTic - gametic - jitter;
	}

	mServerTic = MAX(gametic + mServerTicDelta, 0);

	mCurrentInput[consoleplayer] = mSentInput[gametic % BACKUPTICS];

	// [BB] Don't check net packets while we are supposed to load a map.
	// This way the commands from the full update will not be parsed before we loaded the map.
	//if ((gameaction == ga_newgame) || (gameaction == ga_newgame2))
	//	return;

	TArray<uint8_t> &update = mTicUpdates[mServerTic % BACKUPTICS];
	if (update.Size() > 0)
	{
		ByteInputStream stream(update.Data(), update.Size());
		ProcessCommands(stream);
		update.Clear();
	}
}

void NetClient::EndCurrentTic()
{
	mCurrentCommands = mSendCommands;
	mSendCommands.Clear();
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

void NetClient::WriteLocalInput(ticcmd_t ticcmd)
{
	mSentInput[(mSendTic - 1) % BACKUPTICS] = ticcmd;

	if (mStatus == NodeStatus::InGame)
	{
		int targettic = (mSendTic + mServerTicDelta);

		NetOutputPacket packet(mServerNode);

		NetCommand cmd(NetPacketType::Tic);
		cmd.addByte(targettic); // target gametic
		cmd.addBuffer(&ticcmd.ucmd, sizeof(usercmd_t));
		cmd.writeCommandToStream(packet.stream);

		mComm->PacketSend(packet);
	}
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

void NetClient::ProcessCommands(ByteInputStream &stream)
{
	while (stream.IsAtEnd() == false)
	{
		NetPacketType type = (NetPacketType)stream.ReadByte();
		switch (type)
		{
		default: OnClose(); break;
		case NetPacketType::ConnectResponse: OnConnectResponse(stream); break;
		case NetPacketType::Disconnect: OnDisconnect(); break;
		case NetPacketType::Tic: OnTic(stream); break;
		case NetPacketType::SpawnActor: OnSpawnActor(stream); break;
		case NetPacketType::DestroyActor: OnDestroyActor(stream); break;
		}
	}
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
			mServerTicDelta = mLastReceivedTic - gametic - jitter;

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

void NetClient::OnTic(ByteInputStream &stream)
{
	DVector3 Pos, Vel;
	float yaw, pitch;
	Pos.X = stream.ReadFloat();
	Pos.Y = stream.ReadFloat();
	Pos.Z = stream.ReadFloat();
	Vel.X = stream.ReadFloat();
	Vel.Y = stream.ReadFloat();
	Vel.Z = stream.ReadFloat();
	yaw = stream.ReadFloat();
	pitch = stream.ReadFloat();

	if (playeringame[consoleplayer] && players[consoleplayer].mo)
	{
		APlayerPawn *pawn = players[consoleplayer].mo;
		if ((Pos - pawn->Pos()).LengthSquared() > 10.0)
			pawn->SetOrigin(Pos, false);
		else
			P_TryMove(pawn, Pos.XY(), true);
		pawn->Vel = Vel;
		pawn->Angles.Yaw = yaw;
		pawn->Angles.Pitch = pitch;
	}

	while (true)
	{
		int netID = stream.ReadShort();
		if (netID == -1)
			break;

		float x = stream.ReadFloat();
		float y = stream.ReadFloat();
		float z = stream.ReadFloat();
		float yaw = stream.ReadFloat();
		float pitch = stream.ReadFloat();
		int sprite = stream.ReadShort();
		uint8_t frame = stream.ReadByte();

		AActor *netactor = mNetIDList.findPointerByID(netID);
		if (netactor)
		{
			netactor->SetOrigin(x, y, z, true);
			netactor->Angles.Yaw = yaw;
			netactor->Angles.Pitch = pitch;
			netactor->sprite = sprite;
			netactor->frame = frame;
		}
	}
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
	mNetIDList.freeID(netID);
}

IMPLEMENT_CLASS(ANetSyncActor, false, false)

