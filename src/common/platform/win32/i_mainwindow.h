#pragma once

#include "zstring.h"
#include "printf.h"

#include <functional>

#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

// The WndProc used when the game view is active
LRESULT CALLBACK WndProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam);

class MainWindow
{
public:
	void Create(const FString& title, int x, int y, int width, int height);

	void ShowGameView();
	void RestoreConView();

	void ShowErrorPane(const char* text);
	bool CheckForRestart();

	void PrintStr(const char* cp);
	void GetLog(std::function<bool(const void* data, uint32_t size, uint32_t& written)> writeFile);

	void NetInit(const char* message, bool host);
	void NetMessage(const char* message);
	void NetConnect(int client, const char* name, unsigned flags, int status);
	void NetUpdate(int client, int status);
	void NetDisconnect(int client);
	void NetProgress(int cur, int limit);
	void NetDone();
	void NetClose();
	bool ShouldStartNet();
	int GetNetKickClient();
	int GetNetBanClient();
	bool NetLoop(bool (*loopCallback)(void*), void* data);

	void SetWindowTitle(const char* caption);

	HWND GetHandle() { return Window; }

private:
	static LRESULT CALLBACK LConProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	HWND Window = 0;
	bool restartrequest = false;
	TArray<FString> bufferedConsoleStuff;
};

extern MainWindow mainwindow;
