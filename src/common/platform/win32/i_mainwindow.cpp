
#include "i_mainwindow.h"
#include "resource.h"
#include "startupinfo.h"
#include "gstrings.h"
#include "palentry.h"
#include "st_start.h"
#include "i_input.h"
#include "version.h"
#include "utf8.h"
#include "v_font.h"
#include "i_net.h"
#include "engineerrors.h"
#include "common/widgets/errorwindow.h"
#include "common/widgets/netstartwindow.h"
#include <richedit.h>
#include <shellapi.h>
#include <commctrl.h>
#include <dwmapi.h>

#pragma comment(lib, "dwmapi.lib")

MainWindow mainwindow;

void MainWindow::Create(const FString& caption, int x, int y, int width, int height)
{
	static const WCHAR WinClassName[] = L"MainWindow";

	HINSTANCE hInstance = GetModuleHandle(0);

	WNDCLASS WndClass;
	WndClass.style = 0;
	WndClass.lpfnWndProc = LConProc;
	WndClass.cbClsExtra = 0;
	WndClass.cbWndExtra = 0;
	WndClass.hInstance = hInstance;
	WndClass.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_ICON1));
	WndClass.hCursor = LoadCursor(NULL, IDC_ARROW);
	WndClass.hbrBackground = CreateSolidBrush(RGB(0,0,0));
	WndClass.lpszMenuName = NULL;
	WndClass.lpszClassName = WinClassName;

	/* register this new class with Windows */
	if (!RegisterClass((LPWNDCLASS)&WndClass))
	{
		MessageBoxA(nullptr, "Could not register window class", "Fatal", MB_ICONEXCLAMATION | MB_OK);
		exit(-1);
	}

	std::wstring wcaption = caption.WideString();
	Window = CreateWindowExW(
		WS_EX_APPWINDOW,
		WinClassName,
		wcaption.c_str(),
		WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN,
		x, y, width, height,
		(HWND)NULL,
		(HMENU)NULL,
		hInstance,
		NULL);

	if (!Window)
	{
		MessageBoxA(nullptr, "Unable to create main window", "Fatal", MB_ICONEXCLAMATION | MB_OK);
		exit(-1);
	}

	uint32_t bordercolor = RGB(51, 51, 51);
	uint32_t captioncolor = RGB(33, 33, 33);
	uint32_t textcolor = RGB(226, 223, 219);

	// Don't error check these as they only exist on Windows 11, and if they fail then that is OK.
	DwmSetWindowAttribute(Window, 34/*DWMWA_BORDER_COLOR*/, &bordercolor, sizeof(uint32_t));
	DwmSetWindowAttribute(Window, 35/*DWMWA_CAPTION_COLOR*/, &captioncolor, sizeof(uint32_t));
	DwmSetWindowAttribute(Window, 36/*DWMWA_TEXT_COLOR*/, &textcolor, sizeof(uint32_t));
}

// Sets the main WndProc, hides all the child windows, and starts up in-game input.
void MainWindow::ShowGameView()
{
	if (GetWindowLongPtr(Window, GWLP_USERDATA) == 0)
	{
		SetWindowLongPtr(Window, GWLP_USERDATA, 1);
		SetWindowLongPtr(Window, GWLP_WNDPROC, (LONG_PTR)WndProc);
		I_InitInput(Window);
	}
}

// Returns the main window to its startup state.
void MainWindow::RestoreConView()
{
	I_ShutdownInput();		// Make sure the mouse pointer is available.
	ShowWindow(Window, SW_HIDE);

	// Make sure the progress bar isn't visible.
	DeleteStartupScreen();
}

// Shows an error message, preferably in the main window, but it can use a normal message box too.
void MainWindow::ShowErrorPane(const char* text)
{
	if (StartWindow)	// Ensure that the network pane is hidden.
	{
		I_NetDone();
	}

	// PrintStr(text);

	size_t totalsize = 0;
	for (const FString& line : bufferedConsoleStuff)
		totalsize += line.Len();

	std::string alltext;
	alltext.reserve(totalsize);
	for (const FString& line : bufferedConsoleStuff)
		alltext.append(line.GetChars(), line.Len());

	restartrequest = ErrorWindow::ExecModal(text, alltext);
}

void MainWindow::ShowNetStartPane(const char* message, int maxpos)
{
	NetStartWindow::ShowNetStartPane(message, maxpos);
}

void MainWindow::HideNetStartPane()
{
	NetStartWindow::HideNetStartPane();
}

void MainWindow::SetNetStartProgress(int pos)
{
	NetStartWindow::SetNetStartProgress(pos);
}

bool MainWindow::RunMessageLoop(bool (*timer_callback)(void*), void* userdata)
{
	return NetStartWindow::RunMessageLoop(timer_callback, userdata);
}

bool MainWindow::CheckForRestart()
{
	bool result = restartrequest;
	restartrequest = false;
	return result;
}

// The main window's WndProc during startup. During gameplay, the WndProc in i_input.cpp is used instead.
LRESULT MainWindow::LConProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

void MainWindow::PrintStr(const char* cp)
{
	bufferedConsoleStuff.Push(cp);
}

void MainWindow::GetLog(std::function<bool(const void* data, uint32_t size, uint32_t& written)> writeData)
{
	for (const FString& line : bufferedConsoleStuff)
	{
		size_t pos = 0;
		size_t len = line.Len();
		while (pos < len)
		{
			uint32_t size = (uint32_t)std::min(len - pos, 0x0fffffffULL);
			uint32_t written = 0;
			if (!writeData(&line[pos], size, written))
				return;
			pos += written;
		}
	}
}

// each platform has its own specific version of this function.
void MainWindow::SetWindowTitle(const char* caption)
{
	std::wstring widecaption;
	if (!caption)
	{
		FStringf default_caption("" GAMENAME " %s (%s)", GetVersionString(), GetGitTime());
		widecaption = default_caption.WideString();
	}
	else
	{
		widecaption = WideString(caption);
	}
	SetWindowText(Window, widecaption.c_str());
}
