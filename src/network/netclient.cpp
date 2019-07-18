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

	NetCommand cmd(NetPacketType::ConnectRequest);
	cmd.AddString("ZDoom Connect Request");
	cmd.WriteToNode(mOutput);
}

void NetClient::Update()
{
	if (mStatus == NodeStatus::InPreGame)
		mOutput.Send(mComm.get(), mServerNode);

	while (true)
	{
		NetInputPacket packet;
		mComm->PacketGet(packet);
		if (packet.node == -1)
			break;

		if (packet.node == mServerNode)
		{
			if (packet.stream.IsAtEnd())
			{
				OnClose();
			}
			else
			{
				mInput.ReceivedPacket(packet, mOutput);
			}
		}
		else
		{
			mComm->Close(packet.node);
		}
	}

	while (mStatus == NodeStatus::InPreGame)
	{
		ByteInputStream message = mInput.ReadMessage();
		if (message.IsAtEnd())
			break;

		NetPacketType type = (NetPacketType)message.ReadByte();
		switch (type)
		{
		default: OnClose(); break;
		case NetPacketType::ConnectResponse: OnConnectResponse(message); break;
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
	}
}

void NetClient::BeginTic()
{
	// [BB] Don't check net packets while we are supposed to load a map.
	// This way the commands from the full update will not be parsed before we loaded the map.
	if ((gameaction == ga_newgame) || (gameaction == ga_newgame2))
		return;

	while (mStatus == NodeStatus::InGame)
	{
		ByteInputStream message = mInput.ReadMessage();
		if (message.IsAtEnd())
			break;

		NetPacketType type = (NetPacketType)message.ReadByte();
		switch (type)
		{
		default: OnClose(); break;
		case NetPacketType::Disconnect: OnDisconnect(); break;
		case NetPacketType::Tic: OnTic(message); break;
		case NetPacketType::SpawnActor: OnSpawnActor(message); break;
		case NetPacketType::DestroyActor: OnDestroyActor(message); break;
		}
	}

	gametic = mReceiveTic;
	mSendTic++;

	mCurrentInput[consoleplayer] = mSentInput[gametic % BACKUPTICS];
}

void NetClient::EndTic()
{
	mOutput.Send(mComm.get(), mServerNode);
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

void NetClient::WriteLocalInput(ticcmd_t ticcmd)
{
	mSentInput[(mSendTic - 1) % BACKUPTICS] = ticcmd;

	if (mStatus == NodeStatus::InGame)
	{
		NetCommand cmd(NetPacketType::Tic);
		cmd.AddByte(mSendTic);
		cmd.AddBuffer(&ticcmd.ucmd, sizeof(usercmd_t));
		cmd.WriteToNode(mOutput, true);
	}
}

void NetClient::WriteBotInput(int player, const ticcmd_t &cmd)
{
	mCurrentInput[player] = cmd;
}

int NetClient::GetPing(int player) const
{
	if (player == consoleplayer)
		return (mSendTic - mReceiveTic) * 1000 / TICRATE;
	else
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

			G_InitNetGame(mPlayer, "e1m1", true);

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
	int inputtic = stream.ReadByte();
	int delta = (mSendTic & 0xff) - inputtic;
	if (delta < -0x7f)
		delta += 0x100;
	else if (delta > 0x7f)
		delta -= 0x100;
	mReceiveTic = std::max(mSendTic - delta, 0);

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
		AActor *pawn = players[consoleplayer].mo;
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

	ANetSyncActor *actor = Spawn<ANetSyncActor>(primaryLevel, DVector3(x, y, z), NO_REPLACE);
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

