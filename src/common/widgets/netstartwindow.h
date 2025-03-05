#pragma once

#include <zwidget/core/widget.h>
#include <stdexcept>

class TextLabel;
class ListView;
class PushButton;
class Timer;

class NetStartWindow : public Widget
{
public:
	static void NetInit(const char* message, bool host);
	static void NetMessage(const char* message);
	static void NetConnect(int client, const char* name, unsigned flags, int status);
	static void NetUpdate(int client, int status);
	static void NetDisconnect(int client);
	static void NetProgress(int cur, int limit);
	static void NetDone();
	static void NetClose();
	static bool ShouldStartNet();
	static int GetNetKickClient();
	static int GetNetBanClient();
	static bool NetLoop(bool (*timer_callback)(void*), void* userdata);

private:
	NetStartWindow(bool host);

	void SetMessage(const std::string& message);
	void SetProgress(int pos);

protected:
	void OnClose() override;
	void OnGeometryChanged() override;
	virtual void ForceStart();
	virtual void OnKick();
	virtual void OnBan();

private:
	void OnCallbackTimerExpired();

	TextLabel* MessageLabel = nullptr;
	TextLabel* ProgressLabel = nullptr;
	ListView* LobbyWindow = nullptr;
	PushButton* AbortButton = nullptr;
	PushButton* ForceStartButton = nullptr;
	PushButton* KickButton = nullptr;
	PushButton* BanButton = nullptr;

	Timer* CallbackTimer = nullptr;

	int pos = 0;
	int maxpos = 1;
	bool (*timer_callback)(void*) = nullptr;
	void* userdata = nullptr;

	bool exitreason = false;
	bool shouldstart = false;
	bool hosting = false;
	std::vector<int> kickclients;
	std::vector<int> banclients;

	std::exception_ptr CallbackException;

	static NetStartWindow* Instance;
};
