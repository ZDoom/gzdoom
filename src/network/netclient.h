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

#include "netserver.h"
#include "netcommand.h"
#include "playsim/a_dynlight.h"

class NetClient : public Network
{
public:
	NetClient(FString server);

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
	void OnClose();
	void OnConnectResponse(ByteInputStream &stream);
	void OnDisconnect();
	void OnTic(ByteInputStream &stream);
	void OnSpawnActor(ByteInputStream &stream);
	void OnDestroyActor(ByteInputStream &stream);

	void ProcessCommands(ByteInputStream &stream);
	void UpdateLastReceivedTic(int tic);

	std::unique_ptr<doomcom_t> mComm;
	int mServerNode = -1;
	int mPlayer = -1;
	NodeStatus mStatus = NodeStatus::Closed;

	int mSendTic = 0;
	int mServerTic = 0;
	int mServerTicDelta = -1;
	int mLastReceivedTic = -1;

	int jitter = 2;

	TArray<uint8_t> mTicUpdates[BACKUPTICS];

	ticcmd_t mCurrentInput[MAXPLAYERS];
	ticcmd_t mSentInput[BACKUPTICS];

	IDList<AActor> mNetIDList;
};

class ANetSyncActor : public AActor
{
	DECLARE_CLASS(ANetSyncActor, AActor)
public:
};
