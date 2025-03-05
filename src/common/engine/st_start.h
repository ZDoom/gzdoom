#pragma once
/*
** st_start.h
** Interface for the startup screen.
**
**---------------------------------------------------------------------------
** Copyright 2006-2007 Randy Heit
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
** The startup screen interface is based on a mix of Heretic and Hexen.
** Actual implementation is system-specific.
*/
#include <stdint.h>

class FStartupScreen
{
public:
	static FStartupScreen *CreateInstance(int max_progress);

	FStartupScreen(int max_progress)
	{
		MaxPos = max_progress;
		CurPos = 0;
		NotchPos = 0;
	}

	virtual ~FStartupScreen() = default;

	virtual void Progress() {}
	virtual void AppendStatusLine(const char* status) {}
	virtual void LoadingStatus(const char* message, int colors) {}

	virtual void NetInit(const char* message, bool host) {}
	virtual void NetMessage(const char* message) {}
	virtual void NetConnect(int client, const char* name, unsigned flags, int status) {}
	virtual void NetUpdate(int client, int status) {}
	virtual void NetDisconnect(int client) {}
	virtual void NetProgress(int cur, int limit) {}
	virtual void NetDone() {}
	virtual void NetClose() {}
	virtual bool ShouldStartNet() { return false; }
	virtual int GetNetKickClient() { return -1; }
	virtual int GetNetBanClient() { return -1; }
	virtual bool NetLoop(bool (*loopCallback)(void *), void *data) { return false; }

protected:
	int MaxPos, CurPos, NotchPos;
};

class FBasicStartupScreen : public FStartupScreen
{
public:
	FBasicStartupScreen(int max_progress);
	~FBasicStartupScreen();

	void Progress() override;

	void NetInit(const char* message, bool host) override;
	void NetMessage(const char* message) override;
	void NetConnect(int client, const char* name, unsigned flags, int status) override;
	void NetUpdate(int client, int status) override;
	void NetDisconnect(int client) override;
	void NetProgress(int cur, int limit) override;
	void NetDone() override;
	void NetClose() override;
	bool ShouldStartNet() override;
	int GetNetKickClient() override;
	int GetNetBanClient() override;
	bool NetLoop(bool (*loopCallback)(void*), void* data) override;

protected:
	int NetMaxPos, NetCurPos;
};



extern FStartupScreen *StartWindow;

//===========================================================================
//
// DeleteStartupScreen
//
// Makes sure the startup screen has been deleted before quitting.
//
//===========================================================================

inline void DeleteStartupScreen()
{
	if (StartWindow != nullptr)
	{
		delete StartWindow;
		StartWindow = nullptr;
	}
}


