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

#include "network/net.h"

class NetSinglePlayer : public Network
{
public:
	NetSinglePlayer();

	void Update() override;

	void SendMessages() override;

	void BeginTic() override;
	void EndTic() override;

	int GetSendTick() const override;
	ticcmd_t GetPlayerInput(int player) const override;
	ticcmd_t GetSentInput(int tic) const override;

	void WriteLocalInput(ticcmd_t cmd) override;
	void WriteBotInput(int player, const ticcmd_t &cmd) override;

	int GetPing(int player) const override;

	void ListPingTimes() override;
	void Network_Controller(int playernum, bool add) override;

private:
	ticcmd_t mCurrentInput[MAXPLAYERS];
};
