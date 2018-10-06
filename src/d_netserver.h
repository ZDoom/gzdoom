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

#pragma once

#include "d_net.h"

enum class NodeStatus
{
	Closed,
	InPreGame,
	InGame
};

struct NetNode
{
	NodeStatus Status = NodeStatus::Closed;

	int Ping = 0;
	int Gametic = 0;
	int Player = -1;

	ticcmd_t PlayerMovement;
	FDynamicBuffer Commands; // "NetSpecs"
};

class NetServer : public Network
{
public:
	NetServer();

	void Update() override;

	void SetCurrentTic(int receivetic, int sendtic) override;
	void EndCurrentTic() override;

	int GetSendTick() const override;
	ticcmd_t GetPlayerInput(int player) const override;
	ticcmd_t GetSentInput(int tic) const override;

	void RunCommands(int player) override;

	void WriteLocalInput(ticcmd_t cmd) override;
	void WriteBotInput(int player, const ticcmd_t &cmd) override;
	void WriteBytes(const uint8_t *block, int len) override;

	int GetPing(int player) const override;
	int GetServerPing() const override;

	void ListPingTimes() override;
	void Network_Controller(int playernum, bool add) override;

private:
	void OnClose(NetNode &node, const NetPacket &packet);
	void OnConnectRequest(NetNode &node, const NetPacket &packet);
	void OnDisconnect(NetNode &node, const NetPacket &packet);
	void OnTic(NetNode &node, NetPacket &packet);

	void CmdSpawnPlayer(NetNode &node, int player);
	void FullUpdate(NetNode &node);

	std::unique_ptr<doomcom_t> mComm;
	NetNode mNodes[MAXNETNODES];
	int mNodeForPlayer[MAXPLAYERS];
	int mSendTic = 0;

	ticcmd_t mCurrentInput[MAXPLAYERS];
	FDynamicBuffer mCurrentCommands;
	FDynamicBuffer mSendCommands;
};
