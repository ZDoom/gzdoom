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
	void CheckForRestart();

	void HideGameTitleWindow();
	int GetGameTitleWindowHeight();

	void PrintStr(const char* cp);
	void GetLog(std::function<bool(const void* data, uint32_t size, uint32_t& written)> writeFile);

	void UpdateLayout();

	void ShowProgressBar(int maxpos);
	void HideProgressBar();
	void SetProgressPos(int pos);

	void ShowNetStartPane(const char* message, int maxpos);
	void SetNetStartProgress(int pos);
	bool RunMessageLoop(bool (*timer_callback)(void*), void* userdata);
	void HideNetStartPane();

	void SetWindowTitle(const char* caption);

	HWND GetHandle() { return Window; }

private:
	void LayoutMainWindow(HWND hWnd, HWND pane);
	int LayoutErrorPane(HWND pane, int w);
	int LayoutNetStartPane(HWND pane, int w);

	void DoPrintStr(const char* cpt);
	void FlushBufferedConsoleStuff();

	LRESULT OnMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT OnCreate(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT OnSize(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT OnDrawItem(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT OnCommand(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT OnClose(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	LRESULT OnDestroy(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

	static LRESULT CALLBACK LConProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	static INT_PTR CALLBACK ErrorPaneProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam);

	HWND Window = 0;

	HFONT GameTitleFont = 0;
	LONG GameTitleFontHeight = 0;
	LONG DefaultGUIFontHeight = 0;
	LONG ErrorIconChar = 0;

	bool restartrequest = false;

	HWND GameTitleWindow = 0;
	HWND ErrorPane = 0;
	HWND ErrorIcon = 0;

	bool ConWindowHidden = false;
	HWND ConWindow = 0;
	TArray<FString> bufferedConsoleStuff;

	HWND ProgressBar = 0;

	HWND NetStartPane = 0;
	int NetStartMaxPos = 0;

	HWND StartupScreen = 0;
};

extern MainWindow mainwindow;
