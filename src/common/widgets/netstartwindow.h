#pragma once

#include <zwidget/core/widget.h>
#include <stdexcept>

class TextLabel;
class PushButton;
class Timer;

class NetStartWindow : public Widget
{
public:
	static void ShowNetStartPane(const char* message, int maxpos);
	static void HideNetStartPane();
	static void SetNetStartProgress(int pos);
	static bool RunMessageLoop(bool (*timer_callback)(void*), void* userdata);
	static void NetClose();
	static bool ShouldStartNetGame();

private:
	NetStartWindow();

	void SetMessage(const std::string& message, int maxpos);
	void SetProgress(int pos);

protected:
	void OnClose() override;
	void OnGeometryChanged() override;
	virtual void ForceStart();

private:
	void OnCallbackTimerExpired();

	TextLabel* MessageLabel = nullptr;
	TextLabel* ProgressLabel = nullptr;
	PushButton* AbortButton = nullptr;
	PushButton* ForceStartButton = nullptr;

	Timer* CallbackTimer = nullptr;

	int pos = 0;
	int maxpos = 1;
	bool (*timer_callback)(void*) = nullptr;
	void* userdata = nullptr;

	bool exitreason = false;
	bool shouldstart = false;

	std::exception_ptr CallbackException;

	static NetStartWindow* Instance;
};
