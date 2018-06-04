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
#include "d_netserver.h"
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

	mComm = I_InitNetwork(DOOMPORT);

	G_InitServerNetGame("e1m1");
}

void NetServer::Update()
{
	while (true)
	{
		NetPacket packet;
		mComm->PacketGet(packet);
		if (packet.node == -1)
			break;

		NetNode &node = mNodes[packet.node];

		if (packet.size == 0)
		{
			OnClose(node, packet);
		}
		else
		{
			NetPacketType type = (NetPacketType)packet[0];
			switch (type)
			{
			default: OnClose(node, packet); break;
			case NetPacketType::ConnectRequest: OnConnectRequest(node, packet); break;
			case NetPacketType::Disconnect: OnDisconnect(node, packet); break;
			case NetPacketType::Tic: OnTic(node, packet); break;
			}
		}
	}
}

void NetServer::SetCurrentTic(int receivetic, int sendtic)
{
	gametic = receivetic;
	mSendTic = sendtic;
}

void NetServer::EndCurrentTic()
{
	for (int i = 0; i < MAXNETNODES; i++)
	{
		if (mNodes[i].Status == NodeStatus::InGame)
		{
			NetPacket packet;
			packet.node = i;
			packet.size = 2 + sizeof(float) * 5;
			packet[0] = (uint8_t)NetPacketType::Tic;
			packet[1] = gametic;

			int player = mNodes[i].Player;
			if (playeringame[player] && players[player].mo)
			{
				*(float*)&packet[2] = (float)players[player].mo->X();
				*(float*)&packet[6] = (float)players[player].mo->Y();
				*(float*)&packet[10] = (float)players[player].mo->Z();
				*(float*)&packet[14] = (float)players[player].mo->Angles.Yaw.Degrees;
				*(float*)&packet[18] = (float)players[player].mo->Angles.Pitch.Degrees;
			}
			else
			{
				*(float*)&packet[2] = 0.0f;
				*(float*)&packet[6] = 0.0f;
				*(float*)&packet[10] = 0.0f;
				*(float*)&packet[14] = 0.0f;
				*(float*)&packet[18] = 0.0f;
			}

			mComm->PacketSend(packet);
		}
	}

	mCurrentCommands = mSendCommands;
	mSendCommands.Clear();
}

int NetServer::GetSendTick() const
{
	return mSendTic;
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

void NetServer::OnClose(NetNode &node, const NetPacket &packet)
{
	if (node.Status == NodeStatus::InGame)
	{
		Printf("Player %d left the server", node.Player);

		playeringame[node.Player] = false;
		players[node.Player].settings_controller = false;
		node.Player = -1;
	}

	node.Status = NodeStatus::Closed;
	mComm->Close(packet.node);
}

void NetServer::OnConnectRequest(NetNode &node, const NetPacket &packet)
{
	// Search for a spot in the player list
	if (node.Status != NodeStatus::InGame)
	{
		for (int i = 0; i < MAXPLAYERS; i++)
		{
			if (!playeringame[i])
			{
				node.Player = i;
				playeringame[i] = true;
				players[i].settings_controller = false;
				break;
			}
		}
	}

	if (node.Player != -1) // Join accepted.
	{
		Printf("Player %d joined the server", node.Player);

		mNodeForPlayer[node.Player] = packet.node;

		NetPacket response;
		response.node = packet.node;
		response[0] = (uint8_t)NetPacketType::ConnectResponse;
		response[1] = 1; // Protocol version
		response[2] = node.Player;
		response.size = 3;
		mComm->PacketSend(response);

		node.Status = NodeStatus::InGame;
	}
	else // Server is full.
	{
		node.Status = NodeStatus::Closed;

		NetPacket response;
		response.node = packet.node;
		response[0] = (uint8_t)NetPacketType::ConnectResponse;
		response[1] = 1; // Protocol version
		response[2] = 255;
		response.size = 3;
		mComm->PacketSend(response);

		node.Status = NodeStatus::Closed;
		mComm->Close(packet.node);
	}
}

void NetServer::OnDisconnect(NetNode &node, const NetPacket &packet)
{
	if (node.Status == NodeStatus::InGame)
	{
		Printf("Player %d left the server", node.Player);

		playeringame[node.Player] = false;
		players[node.Player].settings_controller = false;
		node.Player = -1;
	}

	node.Status = NodeStatus::Closed;
	mComm->Close(packet.node);
}

void NetServer::OnTic(NetNode &node, const NetPacket &packet)
{
	if (node.Status == NodeStatus::InGame)
	{
		if (packet.size != 2 + sizeof(usercmd_t))
			return;

		memcpy(&mCurrentInput[node.Player].ucmd, &packet[2], sizeof(usercmd_t));
	}
	else
	{
		node.Status = NodeStatus::Closed;
		mComm->Close(packet.node);
	}
}
