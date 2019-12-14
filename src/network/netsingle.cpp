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
#include "netsingle.h"
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

NetSinglePlayer::NetSinglePlayer()
{
	netgame = false;
	multiplayer = false;
	multiplayernext = false;
	consoleplayer = 0;
	players[0].settings_controller = true;
	playeringame[0] = true;
}

void NetSinglePlayer::Update()
{
}

void NetSinglePlayer::SendMessages()
{
}

void NetSinglePlayer::BeginTic()
{
}

void NetSinglePlayer::EndTic()
{
	gametic++;
}

int NetSinglePlayer::GetSendTick() const
{
	return gametic;
}

ticcmd_t NetSinglePlayer::GetPlayerInput(int player) const
{
	return mCurrentInput[player];
}

ticcmd_t NetSinglePlayer::GetSentInput(int tic) const
{
	return mCurrentInput[consoleplayer];
}

void NetSinglePlayer::WriteLocalInput(ticcmd_t cmd)
{
	mCurrentInput[consoleplayer] = cmd;
}

void NetSinglePlayer::WriteBotInput(int player, const ticcmd_t &cmd)
{
	mCurrentInput[player] = cmd;
}

int NetSinglePlayer::GetPing(int player) const
{
	return 0;
}

void NetSinglePlayer::ListPingTimes()
{
}

void NetSinglePlayer::Network_Controller(int playernum, bool add)
{
}
