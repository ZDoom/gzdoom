/*
** richpresence.cpp
**
** handles rich presence for Discord. does nothing but transmit the appid,
** game, and current level.
**
**---------------------------------------------------------------------------
** Copyright 2022 Rachael Alexanderson
** Copyright 2017 Discord
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#include <time.h>
#include <string.h>

#include "common/engine/printf.h"
#include "discord_rpc.h"
#include "version.h"

static int64_t StartTime = 0;
static bool didInit = false;

static void handleDiscordReady(const DiscordUser* connectedUser)
{
	Printf("\nDiscord: connected to user %s#%s - %s\n",
		connectedUser->username,
		connectedUser->discriminator,
		connectedUser->userId);
}

static void handleDiscordDisconnected(int errcode, const char* message)
{
	Printf("\nDiscord: disconnected (%d: %s)\n", errcode, message);
}

static void handleDiscordError(int errcode, const char* message)
{
	Printf("\nDiscord: error (%d: %s)\n", errcode, message);
}

static void handleDiscordSpectate(const char* secret)
{
	Printf("\nDiscord: spectate (%s)\n", secret);
}

static void handleDiscordJoin(const char* secret)
{
	Printf("\nDiscord: join (%s)\n", secret);
}

static void handleDiscordJoinRequest(const DiscordUser* request)
{
	// we can't join in-game
	int response = DISCORD_REPLY_NO;
	Discord_Respond(request->userId, response);
	Printf("\nDiscord: join request from %s#%s - %s\n",
		request->username,
		request->discriminator,
		request->userId);
}

void I_UpdateDiscordPresence(bool SendPresence, const char* curstatus, const char* appid, const char* steamappid)
{
	const char* curappid = DEFAULT_DISCORD_APP_ID;

	if (appid && appid[0] != '\0')
		curappid = appid;

	if (!didInit && !SendPresence)
		return;	// we haven't initted, there's nothing to do here, just exit

	if (!didInit)
	{
		didInit = true;
		DiscordEventHandlers handlers;
		memset(&handlers, 0, sizeof(handlers));
		handlers.ready = handleDiscordReady;
		handlers.disconnected = handleDiscordDisconnected;
		handlers.errored = handleDiscordError;
		handlers.joinGame = handleDiscordJoin;
		handlers.spectateGame = handleDiscordSpectate;
		handlers.joinRequest = handleDiscordJoinRequest;
		Discord_Initialize(curappid, &handlers, 1, steamappid);
	}

	if (SendPresence)
	{
		DiscordRichPresence discordPresence;
		memset(&discordPresence, 0, sizeof(discordPresence));
		discordPresence.state = "Playing";
		if (!StartTime)
			StartTime = time(0);
		discordPresence.startTimestamp = StartTime;
		discordPresence.details = curstatus;
		discordPresence.largeImageKey = "game-image";
		discordPresence.instance = 0;
		Discord_UpdatePresence(&discordPresence);
	}
	else
	{
		Discord_ClearPresence();
	}
}

