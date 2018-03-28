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
}

int NetServer::GetSendTick() const
{
	return mSendTic;
}

ticcmd_t NetServer::GetPlayerInput(int player) const
{
	return ticcmd_t();
}

ticcmd_t NetServer::GetLocalInput(int tic) const
{
	return ticcmd_t();
}

void NetServer::RunCommands(int player)
{
}

void NetServer::WriteLocalInput(ticcmd_t cmd)
{
}

void NetServer::WriteBotInput(int player, const ticcmd_t &cmd)
{
}

void NetServer::WriteBytes(const uint8_t *block, int len)
{
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
	node.Status = NodeStatus::Closed;
	mComm->Close(packet.node);
}

void NetServer::OnConnectRequest(NetNode &node, const NetPacket &packet)
{
	if (node.Player == -1)
	{
		// To do: search for free player spot
		node.Player = 1;
	}

	if (node.Player != -1) // Join accepted.
	{
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
	node.Status = NodeStatus::Closed;
	mComm->Close(packet.node);
}

void NetServer::OnTic(NetNode &node, const NetPacket &packet)
{
	if (node.Status == NodeStatus::InGame)
	{
	}
	else
	{
		node.Status = NodeStatus::Closed;
		mComm->Close(packet.node);
	}
}
