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
#include "netserver.h"
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

NetServer::NetServer()
{
	Printf("Started hosting multiplayer game..\n");

	mBroadcastCommands.SetBuffer(MAX_MSGLEN);

	for (int i = 0; i < MAXNETNODES; i++)
		mNodes[i].NodeIndex = i;

	mComm = I_InitNetwork(DOOMPORT);

	G_InitServerNetGame("e1m1");
}

void NetServer::Update()
{
	// Read all packets currently available from clients
	while (true)
	{
		NetInputPacket packet;
		mComm->PacketGet(packet);
		if (packet.node == -1)
			break; // No more packets. We are done.

		NetNode &node = mNodes[packet.node];

		if (packet.stream.IsAtEnd()) // Connection to node closed (timed out)
		{
			OnClose(node, packet.stream);
		}
		else
		{
			while (packet.stream.IsAtEnd() == false)
			{
				NetPacketType type = (NetPacketType)packet.stream.ReadByte();
				switch (type)
				{
				default: OnClose(node, packet.stream); break;
				case NetPacketType::ConnectRequest: OnConnectRequest(node, packet.stream); break;
				case NetPacketType::Disconnect: OnDisconnect(node, packet.stream); break;
				case NetPacketType::Tic: OnTic(node, packet.stream); break;
				}
			}
		}
	}
}

void NetServer::SetCurrentTic(int tictime)
{
	gametic = tictime;

	for (int i = 0; i < MAXNETNODES; i++)
	{
		NetNode &node = mNodes[i];
		if (node.Status == NodeStatus::InGame && node.Player != -1)
		{
			NetNode::TicUpdate &update = node.TicUpdates[gametic % BACKUPTICS];
			if (update.received)
			{
				mCurrentInput[node.Player].ucmd = update.input;
				update.received = false;
			}
		}
	}
}

void NetServer::EndCurrentTic()
{
	for (int i = 0; i < MAXNETNODES; i++)
	{
		if (mNodes[i].Status == NodeStatus::InGame)
		{
			int player = mNodes[i].Player;

			NetOutputPacket packet(i);
			packet.stream.WriteByte(gametic + 1);

			if (mNodes[i].FirstTic)
			{
				TThinkerIterator<AActor> it = primaryLevel->GetThinkerIterator<AActor>();
				AActor *mo;
				while (mo = it.Next())
				{
					if (mo != players[player].mo)
					{
						CmdSpawnActor(packet.stream, mo);
					}
				}
				mNodes[i].FirstTic = false;
			}

			packet.stream.WriteBuffer(mBroadcastCommands.GetData(), mBroadcastCommands.GetSize());

			NetCommand cmd ( NetPacketType::Tic);

			if (playeringame[player] && players[player].mo)
			{
				cmd.addFloat(static_cast<float> (players[player].mo->X()));
				cmd.addFloat(static_cast<float> (players[player].mo->Y()));
				cmd.addFloat(static_cast<float> (players[player].mo->Z()));
				cmd.addFloat(static_cast<float> (players[player].mo->Vel.X));
				cmd.addFloat(static_cast<float> (players[player].mo->Vel.Y));
				cmd.addFloat(static_cast<float> (players[player].mo->Vel.Z));
				cmd.addFloat(static_cast<float> (players[player].mo->Angles.Yaw.Degrees));
				cmd.addFloat(static_cast<float> (players[player].mo->Angles.Pitch.Degrees));
			}
			else
			{
				cmd.addFloat(0.0f);
				cmd.addFloat(0.0f);
				cmd.addFloat(0.0f);
				cmd.addFloat(0.0f);
				cmd.addFloat(0.0f);
				cmd.addFloat(0.0f);
				cmd.addFloat(0.0f);
				cmd.addFloat(0.0f);
			}

			TThinkerIterator<AActor> it = primaryLevel->GetThinkerIterator<AActor>();
			AActor *mo;
			while (mo = it.Next())
			{
				if (mo != players[player].mo && mo->syncdata.NetID)
				{
					cmd.addShort(mo->syncdata.NetID);
					cmd.addFloat(static_cast<float> (mo->X()));
					cmd.addFloat(static_cast<float> (mo->Y()));
					cmd.addFloat(static_cast<float> (mo->Z()));
					cmd.addFloat(static_cast<float> (mo->Angles.Yaw.Degrees));
					cmd.addFloat(static_cast<float> (mo->Angles.Pitch.Degrees));
					cmd.addShort(mo->sprite);
					cmd.addByte(mo->frame);
				}
			}
			cmd.addShort(-1);

			cmd.writeCommandToStream(packet.stream);
			mComm->PacketSend(packet);
		}
	}

	mBroadcastCommands.ResetPos();

	mCurrentCommands = mSendCommands;
	mSendCommands.Clear();
}

int NetServer::GetSendTick() const
{
	return gametic;
}

ticcmd_t NetServer::GetPlayerInput(int player) const
{
	return mCurrentInput[player];
}

ticcmd_t NetServer::GetSentInput(int tic) const
{
	return mCurrentInput[consoleplayer];
}

void NetServer::RunCommands(int player)
{
	if (player == consoleplayer)
	{
		Net_RunCommands(mCurrentCommands, consoleplayer);
	}
}

void NetServer::WriteLocalInput(ticcmd_t cmd)
{
	mCurrentInput[consoleplayer] = cmd;
}

void NetServer::WriteBotInput(int player, const ticcmd_t &cmd)
{
	mCurrentInput[player] = cmd;
}

void NetServer::WriteBytes(const uint8_t *block, int len)
{
	mSendCommands.AppendData(block, len);
}

int NetServer::GetPing(int player) const
{
	return 0;
}

int NetServer::GetServerPing() const
{
	return 0;
}

void NetServer::ListPingTimes()
{
#if 0
	for (int i = 0; i < MAXPLAYERS; i++)
		if (playeringame[i])
			Printf("% 4" PRId64 " %s\n", currrecvtime[i] - lastrecvtime[i], players[i].userinfo.GetName());
#endif
}

void NetServer::Network_Controller(int playernum, bool add)
{
}

void NetServer::OnClose(NetNode &node, ByteInputStream &stream)
{
	if (node.Status == NodeStatus::InGame)
	{
		Printf("Player %d left the server\n", node.Player);

		playeringame[node.Player] = false;
		players[node.Player].settings_controller = false;
		node.Player = -1;
	}

	node.Status = NodeStatus::Closed;
	mComm->Close(node.NodeIndex);
}

void NetServer::OnConnectRequest(NetNode &node, ByteInputStream &stream)
{
	// Make the initial connect packet a bit more complex than a bunch of zeros..
	if (strcmp(stream.ReadString(), "ZDoom Connect Request") != 0)
	{
		if (node.Status == NodeStatus::InGame)
		{
			Printf("Junk data received from a joined player\n");
		}
		else
		{
			node.Status = NodeStatus::Closed;
			mComm->Close(node.NodeIndex);
			return;
		}
	}

	if (node.Status == NodeStatus::InGame)
		return;

	// Search for a spot in the player list
	for (int i = 0; i < MAXPLAYERS; i++)
	{
		if (!playeringame[i])
		{
			node.Player = i;
			break;
		}
	}

	if (node.Player != -1) // Join accepted.
	{
		Printf("Player %d joined the server\n", node.Player);

		for (int i = 0; i < BACKUPTICS; i++)
			node.TicUpdates[i].received = false;

		node.FirstTic = true;
		node.Status = NodeStatus::InGame;
		mNodeForPlayer[node.Player] = node.NodeIndex;

		playeringame[node.Player] = true;
		players[node.Player].settings_controller = false;

		NetOutputPacket response(node.NodeIndex);
		response.stream.WriteByte(gametic);

		NetCommand cmd ( NetPacketType::ConnectResponse );
		cmd.addByte ( 1 ); // Protocol version
		cmd.addByte ( node.Player );
		cmd.writeCommandToStream ( response.stream );

		mComm->PacketSend(response);
	}
	else // Server is full.
	{
		node.Status = NodeStatus::Closed;

		NetOutputPacket response(node.NodeIndex);
		response.stream.WriteByte(gametic);

		NetCommand cmd ( NetPacketType::ConnectResponse );
		cmd.addByte ( 1 ); // Protocol version
		cmd.addByte ( 255 );
		cmd.writeCommandToStream (response.stream);

		mComm->PacketSend(response);

		node.Status = NodeStatus::Closed;
		mComm->Close(node.NodeIndex);
	}
}

void NetServer::OnDisconnect(NetNode &node, ByteInputStream &stream)
{
	if (node.Status == NodeStatus::InGame)
	{
		Printf("Player %d left the server\n", node.Player);

		playeringame[node.Player] = false;
		players[node.Player].settings_controller = false;
		node.Player = -1;
	}

	node.Status = NodeStatus::Closed;
	mComm->Close(node.NodeIndex);
}

void NetServer::OnTic(NetNode &node, ByteInputStream &stream)
{
	if (node.Status != NodeStatus::InGame)
		return;

	int tic = stream.ReadByte();
	int delta = tic - (gametic & 0xff);
	if (delta > 128) delta -= 256;
	else if (delta < -128) delta += 256;
	tic = gametic + delta;

	NetNode::TicUpdate update;
	update.received = true;
	stream.ReadBuffer(&update.input, sizeof(usercmd_t));

	if (tic <= gametic)
	{
		// Packet arrived too late.
		tic = gametic + 1;

		if (tic < 0 || node.TicUpdates[tic % BACKUPTICS].received)
			return; // We already received the proper packet.
	}

	node.TicUpdates[tic % BACKUPTICS] = update;
}

void NetServer::CmdSpawnActor(ByteOutputStream &stream, AActor *actor)
{
	NetCommand cmd(NetPacketType::SpawnActor);
	cmd.addShort(actor->syncdata.NetID);
	cmd.addFloat(static_cast<float>(actor->X()));
	cmd.addFloat(static_cast<float>(actor->Y()));
	cmd.addFloat(static_cast<float>(actor->Z()));
	cmd.writeCommandToStream(stream);
}

void NetServer::CmdDestroyActor(ByteOutputStream &stream, AActor *actor)
{
	NetCommand cmd(NetPacketType::DestroyActor);
	cmd.addShort(actor->syncdata.NetID);
	cmd.writeCommandToStream(stream);
}

void NetServer::ActorSpawned(AActor *actor)
{
	actor->syncdata.NetID = mNetIDList.getNewID();
	mNetIDList.useID(actor->syncdata.NetID, actor);

	CmdSpawnActor(mBroadcastCommands, actor);
}

void NetServer::ActorDestroyed(AActor *actor)
{
	CmdDestroyActor(mBroadcastCommands, actor);
	mNetIDList.freeID(actor->syncdata.NetID);
}
