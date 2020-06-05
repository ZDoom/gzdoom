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
#include "netnode.h"
#include "playsim/a_dynlight.h"

class NetClient : public Network
{
public:
	NetClient(FString server);

	void Update() override;

	void SendMessages() override;

	bool TicAvailable(int count);
	void BeginTic() override;
	void EndTic() override;

	int GetSendTick() const override;
	ticcmd_t GetPlayerInput(int player) const override;
	ticcmd_t GetSentInput(int tic) const override;

	void WriteLocalInput(ticcmd_t cmd) override;
	void WriteBotInput(int player, const ticcmd_t &cmd) override;

	int GetPing(int player) const override;
	FString GetStats() override;

	void ListPingTimes() override;
	void Network_Controller(int playernum, bool add) override;

	void ActorSpawned(AActor *actor) override;
	void ActorDestroyed(AActor *actor) override;

private:
	void OnClose();
	void OnConnectResponse(ByteInputStream &stream);
	void OnDisconnect();
	void OnBeginTic(ByteInputStream &stream);
	void OnEndTic(ByteInputStream& stream);
	void OnSpawnActor(ByteInputStream &stream);
	void OnDestroyActor(ByteInputStream &stream);

	std::unique_ptr<doomcom_t> mComm;
	NetNodeInput mInput;
	NetNodeOutput mOutput;
	int mServerNode = -1;
	int mPlayer = -1;
	NodeStatus mStatus = NodeStatus::Closed;

	int mReceiveTic = 0;
	int mSendTic = 0;

	ticcmd_t mCurrentInput[MAXPLAYERS];
	ticcmd_t mSentInput[BACKUPTICS];

	IDList<AActor> mNetIDList;
};

class ANetSyncActor : public AActor
{
	DECLARE_CLASS(ANetSyncActor, AActor)
public:
};
