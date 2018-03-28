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

#include "d_netserver.h"

class NetClient : public Network
{
public:
	NetClient();

	void Update() override;

	void SetCurrentTic(int receivetic, int sendtic) override;
	void EndCurrentTic() override;

	int GetSendTick() const override;
	ticcmd_t GetPlayerInput(int player) const override;
	ticcmd_t GetLocalInput(int tic) const override;

	void RunCommands(int player) override;

	void WriteLocalInput(ticcmd_t cmd) override;
	void WriteBotInput(int player, const ticcmd_t &cmd) override;
	void WriteBytes(const uint8_t *block, int len) override;

	int GetPing(int player) const override;
	int GetServerPing() const override;

	void ListPingTimes() override;
	void Network_Controller(int playernum, bool add) override;

private:
	void OnClose(const NetPacket &packet);
	void OnConnectResponse(const NetPacket &packet);
	void OnDisconnect(const NetPacket &packet);
	void OnTic(const NetPacket &packet);

	std::unique_ptr<doomcom_t> mComm;
	int mServerNode = -1;
	int mPlayer = -1;
	NodeStatus mStatus = NodeStatus::Closed;
	int mSendTic = 0;
};
