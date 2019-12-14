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

	for (int i = 0; i < MAXNETNODES; i++)
		mNodes[i].NodeIndex = i;

	mComm = I_InitNetwork(DOOMPORT);
}

void NetServer::Init()
{
	G_InitNetGame(0, "e1m1", false);
}

void NetServer::Update()
{
	while (true)
	{
		NetInputPacket packet;
		mComm->PacketGet(packet);
		if (packet.node == -1)
			break;

		NetNode& node = mNodes[packet.node];
		if (packet.stream.IsAtEnd())
		{
			// Connection to node closed (timed out)
			Close(node);
		}
		else
		{
			node.Input.ReceivedPacket(packet, node.Output);

			if (node.Status == NodeStatus::Closed)
				node.Status = NodeStatus::InPreGame;
		}
	}
}

void NetServer::BeginTic()
{
	for (int nodeIndex = 0; nodeIndex < MAXNETNODES; nodeIndex++)
	{
		NetNode& node = mNodes[nodeIndex];
		while (node.Status != NodeStatus::Closed)
		{
			ByteInputStream message = node.Input.ReadMessage();
			if (message.IsAtEnd())
				break;

			NetPacketType type = (NetPacketType)message.ReadByte();
			switch (type)
			{
			default: Close(node); break;
			case NetPacketType::ConnectRequest: OnConnectRequest(node, message); break;
			case NetPacketType::Disconnect: OnDisconnect(node, message); break;
			case NetPacketType::BeginTic: OnBeginTic(node, message); break;
			}
		}
	}

	for (int i = 0; i < MAXNETNODES; i++)
	{
		if (mNodes[i].Status == NodeStatus::InGame)
		{
			if (mNodes[i].FirstTic)
			{
				int player = mNodes[i].Player;

				TThinkerIterator<AActor> it = primaryLevel->GetThinkerIterator<AActor>();
				AActor* mo;
				while (mo = it.Next())
				{
					if (mo != players[player].mo)
					{
						CmdSpawnActor(i, mo);
					}
				}
				mNodes[i].FirstTic = false;
			}

			CmdBeginTic(i);
		}
	}
}

void NetServer::EndTic()
{
	for (int i = 0; i < MAXNETNODES; i++)
	{
		if (mNodes[i].Status == NodeStatus::InGame)
		{
			CmdEndTic(i);
		}
	}

	gametic++;
}

void NetServer::SendMessages()
{
	for (int i = 0; i < MAXNETNODES; i++)
	{
		mNodes[i].Output.Send(mComm.get(), i);
	}
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

void NetServer::WriteLocalInput(ticcmd_t cmd)
{
	mCurrentInput[consoleplayer] = cmd;
}

void NetServer::WriteBotInput(int player, const ticcmd_t &cmd)
{
	mCurrentInput[player] = cmd;
}

int NetServer::GetPing(int player) const
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
			Close(node);
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

		node.FirstTic = true;
		node.Status = NodeStatus::InGame;
		mNodeForPlayer[node.Player] = node.NodeIndex;

		playeringame[node.Player] = true;
		players[node.Player].settings_controller = false;

		CmdConnectResponse(node.NodeIndex);
	}
	else // Server is full.
	{
		CmdConnectResponse(node.NodeIndex);
		node.Output.Send(mComm.get(), node.NodeIndex);
		Close(node);
	}
}

void NetServer::OnDisconnect(NetNode &node, ByteInputStream &stream)
{
	node.Output.Send(mComm.get(), node.NodeIndex);
	Close(node);
}

void NetServer::OnBeginTic(NetNode &node, ByteInputStream &stream)
{
	if (node.Status != NodeStatus::InGame)
		return;

	mCurrentInputTic[node.Player] = stream.ReadByte();
	stream.ReadBuffer(&mCurrentInput[node.Player].ucmd, sizeof(usercmd_t));
}

void NetServer::CmdConnectResponse(int nodeIndex)
{
	int player = mNodes[nodeIndex].Player;
	if (player == -1)
		player = 255;

	NetCommand cmd(NetPacketType::ConnectResponse);
	cmd.AddByte(1); // Protocol version
	cmd.AddByte(player);
	WriteCommand(nodeIndex, cmd);
}

void NetServer::CmdBeginTic(int nodeIndex)
{
	int player = mNodes[nodeIndex].Player;

	NetCommand cmd(NetPacketType::BeginTic);

	cmd.AddByte(mCurrentInputTic[player]++);

	if (playeringame[player] && players[player].mo)
	{
		cmd.AddFloat(static_cast<float> (players[player].mo->X()));
		cmd.AddFloat(static_cast<float> (players[player].mo->Y()));
		cmd.AddFloat(static_cast<float> (players[player].mo->Z()));
		cmd.AddFloat(static_cast<float> (players[player].mo->Vel.X));
		cmd.AddFloat(static_cast<float> (players[player].mo->Vel.Y));
		cmd.AddFloat(static_cast<float> (players[player].mo->Vel.Z));
		cmd.AddFloat(static_cast<float> (players[player].mo->Angles.Yaw.Degrees));
		cmd.AddFloat(static_cast<float> (players[player].mo->Angles.Pitch.Degrees));
	}
	else
	{
		cmd.AddFloat(0.0f);
		cmd.AddFloat(0.0f);
		cmd.AddFloat(0.0f);
		cmd.AddFloat(0.0f);
		cmd.AddFloat(0.0f);
		cmd.AddFloat(0.0f);
		cmd.AddFloat(0.0f);
		cmd.AddFloat(0.0f);
	}

	TThinkerIterator<AActor> it = primaryLevel->GetThinkerIterator<AActor>();
	AActor* mo;
	while (mo = it.Next())
	{
		if (mo != players[player].mo && mo->syncdata.NetID)
		{
			cmd.AddShort(mo->syncdata.NetID);
			cmd.AddFloat(static_cast<float> (mo->X()));
			cmd.AddFloat(static_cast<float> (mo->Y()));
			cmd.AddFloat(static_cast<float> (mo->Z()));
			cmd.AddFloat(static_cast<float> (mo->Angles.Yaw.Degrees));
			cmd.AddFloat(static_cast<float> (mo->Angles.Pitch.Degrees));
			cmd.AddShort(mo->sprite);
			cmd.AddByte(mo->frame);
		}
	}
	cmd.AddShort(-1);

	WriteCommand(nodeIndex, cmd, true);
}

void NetServer::CmdEndTic(int nodeIndex)
{
	NetCommand cmd(NetPacketType::EndTic);
	WriteCommand(nodeIndex, cmd);
}

void NetServer::CmdSpawnActor(int nodeIndex, AActor *actor)
{
	NetCommand cmd(NetPacketType::SpawnActor);
	cmd.AddShort(actor->syncdata.NetID);
	cmd.AddFloat(static_cast<float>(actor->X()));
	cmd.AddFloat(static_cast<float>(actor->Y()));
	cmd.AddFloat(static_cast<float>(actor->Z()));
	WriteCommand(nodeIndex, cmd);
}

void NetServer::CmdDestroyActor(int nodeIndex, AActor *actor)
{
	NetCommand cmd(NetPacketType::DestroyActor);
	cmd.AddShort(actor->syncdata.NetID);
	WriteCommand(nodeIndex, cmd);
}

void NetServer::ActorSpawned(AActor *actor)
{
	actor->syncdata.NetID = mNetIDList.getNewID();
	mNetIDList.useID(actor->syncdata.NetID, actor);

	CmdSpawnActor(-1, actor);
}

void NetServer::ActorDestroyed(AActor *actor)
{
	CmdDestroyActor(-1, actor);
	mNetIDList.freeID(actor->syncdata.NetID);
}

void NetServer::Close(NetNode &node)
{
	if (node.Status == NodeStatus::InGame)
	{
		Printf("Player %d left the server\n", node.Player);

		playeringame[node.Player] = false;
		players[node.Player].settings_controller = false;
		node.Player = -1;
	}

	node.Status = NodeStatus::Closed;
	node.Input = {};
	node.Output = {};
	mComm->Close(node.NodeIndex);
}

void NetServer::WriteCommand(int nodeIndex, NetCommand& command, bool unreliable)
{
	if (nodeIndex == -1)
	{
		for (int i = 0; i < MAXNETNODES; i++)
		{
			if (mNodes[i].Status == NodeStatus::InGame)
			{
				command.WriteToNode(mNodes[i].Output, unreliable);
			}
		}
	}
	else
	{
		command.WriteToNode(mNodes[nodeIndex].Output, unreliable);
	}
}
