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

#include "doomtype.h"
#include "doomdef.h"
#include "d_protocol.h"
#include "i_net.h"
#include <memory>

#define MAXNETNODES		8	// max computers in a game
#define BACKUPTICS		36	// number of tics to remember

class AActor;

class Network
{
public:
	virtual ~Network() { }

	// Check for incoming packets
	virtual void Update() = 0;

	// Set current tic time
	virtual void SetCurrentTic(int localtic) = 0;

	// Send any pending outgoing data
	virtual void EndCurrentTic() = 0;

	// Retrieve data about the current tic
	virtual int GetSendTick() const = 0;
	virtual ticcmd_t GetPlayerInput(int player) const = 0;
	virtual ticcmd_t GetSentInput(int tic) const = 0;

	// Write outgoing data for the current tic
	virtual void WriteLocalInput(ticcmd_t cmd) = 0;
	virtual void WriteBotInput(int player, const ticcmd_t &cmd) = 0;

	// Statistics
	virtual int GetPing(int player) const = 0;
	virtual int GetServerPing() const = 0;
	int GetHighPingThreshold() const { return ((BACKUPTICS / 2 - 1)) * (1000 / TICRATE); }

	// CCMDs
	virtual void ListPingTimes() = 0;
	virtual void Network_Controller(int playernum, bool add) = 0;

	// Playsim events
	virtual void ActorSpawned(AActor *actor) { }
	virtual void ActorDestroyed(AActor *actor) { }

	// Old init/deinit stuff
	void Startup() { }
	void Net_ClearBuffers() { }
	void D_QuitNetGame() { }
};

extern std::unique_ptr<Network> network;
extern std::unique_ptr<Network> netconnect;
