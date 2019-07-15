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

#include "net.h"
#include "netcommand.h"

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
	int NodeIndex = -1;
	bool FirstTic = true;

	struct TicUpdate
	{
		bool received = false;
		usercmd_t input;
	};
	TicUpdate TicUpdates[BACKUPTICS];
};

class NetServer : public Network
{
public:
	NetServer();

	void Update() override;

	void SetCurrentTic(int tictime) override;
	void EndCurrentTic() override;

	int GetSendTick() const override;
	ticcmd_t GetPlayerInput(int player) const override;
	ticcmd_t GetSentInput(int tic) const override;

	void WriteLocalInput(ticcmd_t cmd) override;
	void WriteBotInput(int player, const ticcmd_t &cmd) override;

	int GetPing(int player) const override;
	int GetServerPing() const override;

	void ListPingTimes() override;
	void Network_Controller(int playernum, bool add) override;

	void ActorSpawned(AActor *actor) override;
	void ActorDestroyed(AActor *actor) override;

private:
	void OnClose(NetNode &node, ByteInputStream &stream);
	void OnConnectRequest(NetNode &node, ByteInputStream &stream);
	void OnDisconnect(NetNode &node, ByteInputStream &stream);
	void OnTic(NetNode &node, ByteInputStream &packet);

	void CmdSpawnActor(ByteOutputStream &stream, AActor *actor);
	void CmdDestroyActor(ByteOutputStream &stream, AActor *actor);

	std::unique_ptr<doomcom_t> mComm;
	NetNode mNodes[MAXNETNODES];
	int mNodeForPlayer[MAXPLAYERS];

	ticcmd_t mCurrentInput[MAXPLAYERS];

	IDList<AActor> mNetIDList;

	ByteOutputStream mBroadcastCommands; // Playsim events everyone should hear about
};
