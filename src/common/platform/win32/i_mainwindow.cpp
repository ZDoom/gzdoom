
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
#include <richedit.h>
#include <shellapi.h>
#include <commctrl.h>

#ifdef _M_X64
#define X64 " 64-bit"
#elif _M_ARM64
#define X64 " ARM-64"
#else
#define X64 ""
#endif

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
	WndClass.hbrBackground = NULL;
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
		WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_CLIPCHILDREN,
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
}

void MainWindow::HideGameTitleWindow()
{
	if (GameTitleWindow != 0)
	{
		DestroyWindow(GameTitleWindow);
		GameTitleWindow = 0;
	}
}

int MainWindow::GetGameTitleWindowHeight()
{
	if (GameTitleWindow != 0)
	{
		RECT rect;
		GetClientRect(GameTitleWindow, &rect);
		return rect.bottom;
	}
	else
	{
		return 0;
	}
}

// Sets the main WndProc, hides all the child windows, and starts up in-game input.
void MainWindow::ShowGameView()
{
	if (GetWindowLongPtr(Window, GWLP_USERDATA) == 0)
	{
		SetWindowLongPtr(Window, GWLP_USERDATA, 1);
		SetWindowLongPtr(Window, GWLP_WNDPROC, (LONG_PTR)WndProc);
		ShowWindow(ConWindow, SW_HIDE);
		ShowWindow(ProgressBar, SW_HIDE);
		ConWindowHidden = true;
		ShowWindow(GameTitleWindow, SW_HIDE);
		I_InitInput(Window);
	}
}

// Returns the main window to its startup state.
void MainWindow::RestoreConView()
{
	HDC screenDC = GetDC(0);
	int dpi = GetDeviceCaps(screenDC, LOGPIXELSX);
	ReleaseDC(0, screenDC);
	int width = (512 * dpi + 96 / 2) / 96;
	int height = (384 * dpi + 96 / 2) / 96;

	// Make sure the window has a frame in case it was fullscreened.
	SetWindowLongPtr(Window, GWL_STYLE, WS_VISIBLE | WS_OVERLAPPEDWINDOW);
	if (GetWindowLong(Window, GWL_EXSTYLE) & WS_EX_TOPMOST)
	{
		SetWindowPos(Window, HWND_BOTTOM, 0, 0, width, height, SWP_DRAWFRAME | SWP_NOCOPYBITS | SWP_NOMOVE);
		SetWindowPos(Window, HWND_TOP, 0, 0, 0, 0, SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOSIZE);
	}
	else
	{
		SetWindowPos(Window, NULL, 0, 0, width, height, SWP_DRAWFRAME | SWP_NOCOPYBITS | SWP_NOMOVE | SWP_NOZORDER);
	}

	SetWindowLongPtr(Window, GWLP_WNDPROC, (LONG_PTR)LConProc);
	ShowWindow(ConWindow, SW_SHOW);
	ConWindowHidden = false;
	ShowWindow(GameTitleWindow, SW_SHOW);
	I_ShutdownInput();		// Make sure the mouse pointer is available.
	// Make sure the progress bar isn't visible.
	DeleteStartupScreen();

	FlushBufferedConsoleStuff();
}

// Shows an error message, preferably in the main window, but it can use a normal message box too.
void MainWindow::ShowErrorPane(const char* text)
{
	auto widetext = WideString(text);
	if (Window == nullptr || ConWindow == nullptr)
	{
		if (text != NULL)
		{
			FStringf caption("Fatal Error - " GAMENAME " %s " X64 " (%s)", GetVersionString(), GetGitTime());
			MessageBoxW(Window, widetext.c_str(),caption.WideString().c_str(), MB_OK | MB_ICONSTOP | MB_TASKMODAL);
		}
		return;
	}

	if (StartWindow != NULL)	// Ensure that the network pane is hidden.
	{
		I_NetDone();
	}
	if (text != NULL)
	{
		FStringf caption("Fatal Error - " GAMENAME " %s " X64 " (%s)", GetVersionString(), GetGitTime());
		auto wcaption = caption.WideString();
		SetWindowTextW(Window, wcaption.c_str());
		ErrorIcon = CreateWindowExW(WS_EX_NOPARENTNOTIFY, L"STATIC", NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_OWNERDRAW, 0, 0, 0, 0, Window, NULL, GetModuleHandle(0), NULL);
		if (ErrorIcon != NULL)
		{
			SetWindowLong(ErrorIcon, GWL_ID, IDC_ICONPIC);
		}
	}
	ErrorPane = CreateDialogParam(GetModuleHandle(0), MAKEINTRESOURCE(IDD_ERRORPANE), Window, ErrorPaneProc, (LONG_PTR)NULL);

	if (text != NULL)
	{
		CHARRANGE end;
		CHARFORMAT2 oldformat, newformat;
		PARAFORMAT2 paraformat;

		// Append the error message to the log.
		end.cpMax = end.cpMin = GetWindowTextLength(ConWindow);
		SendMessage(ConWindow, EM_EXSETSEL, 0, (LPARAM)&end);

		// Remember current charformat.
		oldformat.cbSize = sizeof(oldformat);
		SendMessage(ConWindow, EM_GETCHARFORMAT, SCF_SELECTION, (LPARAM)&oldformat);

		// Use bigger font and standout colors.
		newformat.cbSize = sizeof(newformat);
		newformat.dwMask = CFM_BOLD | CFM_COLOR | CFM_SIZE;
		newformat.dwEffects = CFE_BOLD;
		newformat.yHeight = oldformat.yHeight * 5 / 4;
		newformat.crTextColor = RGB(255, 170, 170);
		SendMessage(ConWindow, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&newformat);

		// Indent the rest of the text to make the error message stand out a little more.
		paraformat.cbSize = sizeof(paraformat);
		paraformat.dwMask = PFM_STARTINDENT | PFM_OFFSETINDENT | PFM_RIGHTINDENT;
		paraformat.dxStartIndent = paraformat.dxOffset = paraformat.dxRightIndent = 120;
		SendMessage(ConWindow, EM_SETPARAFORMAT, 0, (LPARAM)&paraformat);
		SendMessageW(ConWindow, EM_REPLACESEL, FALSE, (LPARAM)L"\n");

		// Find out where the error lines start for the error icon display control.
		SendMessage(ConWindow, EM_EXGETSEL, 0, (LPARAM)&end);
		ErrorIconChar = end.cpMax;

		// Now start adding the actual error message.
		SendMessageW(ConWindow, EM_REPLACESEL, FALSE, (LPARAM)L"Execution could not continue.\n\n");

		// Restore old charformat but with light yellow text.
		oldformat.crTextColor = RGB(255, 255, 170);
		SendMessage(ConWindow, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&oldformat);

		// Add the error text.
		SendMessageW(ConWindow, EM_REPLACESEL, FALSE, (LPARAM)widetext.c_str());

		// Make sure the error text is not scrolled below the window.
		SendMessage(ConWindow, EM_LINESCROLL, 0, SendMessage(ConWindow, EM_GETLINECOUNT, 0, 0));
		// The above line scrolled everything off the screen, so pretend to move the scroll
		// bar thumb, which clamps to not show any extra lines if it doesn't need to.
		SendMessage(ConWindow, EM_SCROLL, SB_PAGEDOWN, 0);
	}

	BOOL bRet;
	MSG msg;

	while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if (bRet == -1)
		{
			MessageBoxW(Window, widetext.c_str(), WGAMENAME " Fatal Error", MB_OK | MB_ICONSTOP | MB_TASKMODAL);
			return;
		}
		else if (!IsDialogMessage(ErrorPane, &msg))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}
}

void MainWindow::ShowProgressBar(int maxpos)
{
	if (ProgressBar == 0)
		ProgressBar = CreateWindowEx(0, PROGRESS_CLASS, 0, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS, 0, 0, 0, 0, Window, 0, GetModuleHandle(0), 0);
	SendMessage(ProgressBar, PBM_SETRANGE, 0, MAKELPARAM(0, maxpos));
	LayoutMainWindow(Window, 0);
}

void MainWindow::HideProgressBar()
{
	if (ProgressBar != 0)
	{
		DestroyWindow(ProgressBar);
		ProgressBar = 0;
		LayoutMainWindow(Window, 0);
	}
}

void MainWindow::SetProgressPos(int pos)
{
	if (ProgressBar != 0)
		SendMessage(ProgressBar, PBM_SETPOS, pos, 0);
}

// DialogProc for the network startup pane. It just waits for somebody to click a button, and the only button available is the abort one.
static INT_PTR CALLBACK NetStartPaneProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (msg == WM_COMMAND && HIWORD(wParam) == BN_CLICKED && LOWORD(wParam) == IDCANCEL)
	{
		PostMessage(hDlg, WM_COMMAND, 1337, 1337);
		return TRUE;
	}
	return FALSE;
}

void MainWindow::ShowNetStartPane(const char* message, int maxpos)
{
	// Create the dialog child window
	if (NetStartPane == NULL)
	{
		NetStartPane = CreateDialogParam(GetModuleHandle(0), MAKEINTRESOURCE(IDD_NETSTARTPANE), Window, NetStartPaneProc, 0);
		if (ProgressBar != 0)
		{
			DestroyWindow(ProgressBar);
			ProgressBar = 0;
		}
		RECT winrect;
		GetWindowRect(Window, &winrect);
		SetWindowPos(Window, NULL, 0, 0,
			winrect.right - winrect.left, winrect.bottom - winrect.top + LayoutNetStartPane(NetStartPane, 0),
			SWP_NOACTIVATE | SWP_NOMOVE | SWP_NOOWNERZORDER | SWP_NOZORDER);
		LayoutMainWindow(Window, NULL);
		SetFocus(NetStartPane);
	}

	// Set the message text
	std::wstring wmessage = WideString(message);
	SetDlgItemTextW(NetStartPane, IDC_NETSTARTMESSAGE, wmessage.c_str());

	// Set the progress bar range
	NetStartMaxPos = maxpos;
	HWND ctl = GetDlgItem(NetStartPane, IDC_NETSTARTPROGRESS);
	if (maxpos == 0)
	{
		SendMessage(ctl, PBM_SETMARQUEE, TRUE, 100);
		SetWindowLong(ctl, GWL_STYLE, GetWindowLong(ctl, GWL_STYLE) | PBS_MARQUEE);
		SetDlgItemTextW(NetStartPane, IDC_NETSTARTCOUNT, L"");
	}
	else
	{
		SendMessage(ctl, PBM_SETMARQUEE, FALSE, 0);
		SetWindowLong(ctl, GWL_STYLE, GetWindowLong(ctl, GWL_STYLE) & (~PBS_MARQUEE));

		SendMessage(ctl, PBM_SETRANGE, 0, MAKELPARAM(0, maxpos));
		if (maxpos == 1)
		{
			SendMessage(ctl, PBM_SETPOS, 1, 0);
			SetDlgItemTextW(NetStartPane, IDC_NETSTARTCOUNT, L"");
		}
	}
}

void MainWindow::HideNetStartPane()
{
	if (NetStartPane != 0)
	{
		DestroyWindow(NetStartPane);
		NetStartPane = 0;
		LayoutMainWindow(Window, 0);
	}
}

void MainWindow::SetNetStartProgress(int pos)
{
	if (NetStartPane != 0 && NetStartMaxPos > 1)
	{
		char buf[16];
		mysnprintf(buf, sizeof(buf), "%d/%d", pos, NetStartMaxPos);
		SetDlgItemTextA(NetStartPane, IDC_NETSTARTCOUNT, buf);
		SendDlgItemMessage(NetStartPane, IDC_NETSTARTPROGRESS, PBM_SETPOS, min(pos, NetStartMaxPos), 0);
	}
}

bool MainWindow::RunMessageLoop(bool (*timer_callback)(void*), void* userdata)
{
	BOOL bRet;
	MSG msg;

	if (SetTimer(Window, 1337, 500, NULL) == 0)
	{
		I_FatalError("Could not set network synchronization timer.");
	}

	while ((bRet = GetMessage(&msg, NULL, 0, 0)) != 0)
	{
		if (bRet == -1)
		{
			KillTimer(Window, 1337);
			return false;
		}
		else
		{
			// This must be outside the window function so that the exception does not cross DLL boundaries.
			if (msg.message == WM_COMMAND && msg.wParam == 1337 && msg.lParam == 1337)
			{
				throw CExitEvent(0);
			}
			if (msg.message == WM_TIMER && msg.hwnd == Window && msg.wParam == 1337)
			{
				if (timer_callback(userdata))
				{
					KillTimer(Window, 1337);
					return true;
				}
			}
			if (!IsDialogMessage(NetStartPane, &msg))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
		}
	}
	KillTimer(Window, 1337);
	return false;
}

void MainWindow::UpdateLayout()
{
	LayoutMainWindow(Window, 0);
}

// Lays out the main window with the game title and log controls and possibly the error pane and progress bar.
void MainWindow::LayoutMainWindow(HWND hWnd, HWND pane)
{
	RECT rect;
	int errorpaneheight = 0;
	int bannerheight = 0;
	int progressheight = 0;
	int netpaneheight = 0;
	int leftside = 0;
	int w, h;

	GetClientRect(hWnd, &rect);
	w = rect.right;
	h = rect.bottom;

	if (GameStartupInfo.Name.IsNotEmpty() && GameTitleWindow != NULL)
	{
		bannerheight = GameTitleFontHeight + 5;
		MoveWindow(GameTitleWindow, 0, 0, w, bannerheight, TRUE);
		InvalidateRect(GameTitleWindow, NULL, FALSE);
	}
	if (ProgressBar != NULL)
	{
		// Base the height of the progress bar on the height of a scroll bar
		// arrow, just as in the progress bar example.
		progressheight = GetSystemMetrics(SM_CYVSCROLL);
		MoveWindow(ProgressBar, 0, h - progressheight, w, progressheight, TRUE);
	}
	if (NetStartPane != NULL)
	{
		netpaneheight = LayoutNetStartPane(NetStartPane, w);
		SetWindowPos(NetStartPane, HWND_TOP, 0, h - progressheight - netpaneheight, w, netpaneheight, SWP_SHOWWINDOW);
	}
	if (pane != NULL)
	{
		errorpaneheight = LayoutErrorPane(pane, w);
		SetWindowPos(pane, HWND_TOP, 0, h - progressheight - netpaneheight - errorpaneheight, w, errorpaneheight, 0);
	}
	if (ErrorIcon != NULL)
	{
		leftside = GetSystemMetrics(SM_CXICON) + 6;
		MoveWindow(ErrorIcon, 0, bannerheight, leftside, h - bannerheight - errorpaneheight - progressheight - netpaneheight, TRUE);
	}
	// The log window uses whatever space is left.
	MoveWindow(ConWindow, leftside, bannerheight, w - leftside, h - bannerheight - errorpaneheight - progressheight - netpaneheight, TRUE);
}

// Lays out the error pane to the desired width, returning the required height.
int MainWindow::LayoutErrorPane(HWND pane, int w)
{
	HWND ctl, ctl_two;
	RECT rectc, rectc_two;

	// Right-align the Quit button.
	ctl = GetDlgItem(pane, IDOK);
	GetClientRect(ctl, &rectc);	// Find out how big it is.
	MoveWindow(ctl, w - rectc.right - 1, 1, rectc.right, rectc.bottom, TRUE);

	// Second-right-align the Restart button
	ctl_two = GetDlgItem(pane, IDC_BUTTON1);
	GetClientRect(ctl_two, &rectc_two);	// Find out how big it is.
	MoveWindow(ctl_two, w - rectc.right - rectc_two.right - 2, 1, rectc.right, rectc.bottom, TRUE);

	InvalidateRect(ctl, NULL, TRUE);
	InvalidateRect(ctl_two, NULL, TRUE);

	// Return the needed height for this layout
	return rectc.bottom + 2;
}

// Lays out the network startup pane to the specified width, returning its required height.
int MainWindow::LayoutNetStartPane(HWND pane, int w)
{
	HWND ctl;
	RECT margin, rectc;
	int staticheight, barheight;

	// Determine margin sizes.
	SetRect(&margin, 7, 7, 0, 0);
	MapDialogRect(pane, &margin);

	// Stick the message text in the upper left corner.
	ctl = GetDlgItem(pane, IDC_NETSTARTMESSAGE);
	GetClientRect(ctl, &rectc);
	MoveWindow(ctl, margin.left, margin.top, rectc.right, rectc.bottom, TRUE);

	// Stick the count text in the upper right corner.
	ctl = GetDlgItem(pane, IDC_NETSTARTCOUNT);
	GetClientRect(ctl, &rectc);
	MoveWindow(ctl, w - rectc.right - margin.left, margin.top, rectc.right, rectc.bottom, TRUE);
	staticheight = rectc.bottom;

	// Stretch the progress bar to fill the entire width.
	ctl = GetDlgItem(pane, IDC_NETSTARTPROGRESS);
	barheight = GetSystemMetrics(SM_CYVSCROLL);
	MoveWindow(ctl, margin.left, margin.top * 2 + staticheight, w - margin.left * 2, barheight, TRUE);

	// Center the abort button underneath the progress bar.
	ctl = GetDlgItem(pane, IDCANCEL);
	GetClientRect(ctl, &rectc);
	MoveWindow(ctl, (w - rectc.right) / 2, margin.top * 3 + staticheight + barheight, rectc.right, rectc.bottom, TRUE);

	return margin.top * 4 + staticheight + barheight + rectc.bottom;
}

void MainWindow::CheckForRestart()
{
	if (restartrequest)
	{
		HMODULE hModule = GetModuleHandleW(NULL);
		WCHAR path[MAX_PATH];
		GetModuleFileNameW(hModule, path, MAX_PATH);
		ShellExecuteW(NULL, L"open", path, GetCommandLineW(), NULL, SW_SHOWNORMAL);
	}
	restartrequest = false;
}

// The main window's WndProc during startup. During gameplay, the WndProc in i_input.cpp is used instead.
LRESULT MainWindow::LConProc(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return mainwindow.OnMessage(hWnd, msg, wParam, lParam);
}

INT_PTR MainWindow::ErrorPaneProc(HWND hDlg, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_INITDIALOG:
		// Appear in the main window.
		mainwindow.LayoutMainWindow(GetParent(hDlg), hDlg);
		return TRUE;

	case WM_COMMAND:
		if (HIWORD(wParam) == BN_CLICKED)
		{
			if (LOWORD(wParam) == IDC_BUTTON1) // we pressed the restart button, so run GZDoom again
			{
				mainwindow.restartrequest = true;
			}
			PostQuitMessage(0);
			return TRUE;
		}
		break;
	}
	return FALSE;
}

LRESULT MainWindow::OnMessage(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
	case WM_CREATE: return OnCreate(hWnd, msg, wParam, lParam);
	case WM_SIZE: return OnSize(hWnd, msg, wParam, lParam);
	case WM_DRAWITEM: return OnDrawItem(hWnd, msg, wParam, lParam);
	case WM_COMMAND: return OnCommand(hWnd, msg, wParam, lParam);
	case WM_CLOSE: return OnClose(hWnd, msg, wParam, lParam);
	case WM_DESTROY: return OnDestroy(hWnd, msg, wParam, lParam);
	default: return DefWindowProc(hWnd, msg, wParam, lParam);
	}
}

LRESULT MainWindow::OnCreate(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HWND view;
	HDC hdc;
	HGDIOBJ oldfont;
	LOGFONT lf;
	TEXTMETRIC tm;
	CHARFORMAT2W format;

	HINSTANCE inst = (HINSTANCE)(LONG_PTR)GetWindowLongPtr(hWnd, GWLP_HINSTANCE);

	// Create game title static control
	memset(&lf, 0, sizeof(lf));
	hdc = GetDC(hWnd);
	lf.lfHeight = -MulDiv(12, GetDeviceCaps(hdc, LOGPIXELSY), 72);
	lf.lfCharSet = ANSI_CHARSET;
	lf.lfWeight = FW_BOLD;
	lf.lfPitchAndFamily = VARIABLE_PITCH | FF_ROMAN;
	wcscpy(lf.lfFaceName, L"Trebuchet MS");
	GameTitleFont = CreateFontIndirect(&lf);

	oldfont = SelectObject(hdc, GetStockObject(DEFAULT_GUI_FONT));
	GetTextMetrics(hdc, &tm);
	DefaultGUIFontHeight = tm.tmHeight;
	if (GameTitleFont == NULL)
	{
		GameTitleFontHeight = DefaultGUIFontHeight;
	}
	else
	{
		SelectObject(hdc, GameTitleFont);
		GetTextMetrics(hdc, &tm);
		GameTitleFontHeight = tm.tmHeight;
	}
	SelectObject(hdc, oldfont);

	// Create log read-only edit control
	view = CreateWindowExW(WS_EX_NOPARENTNOTIFY, L"RichEdit20W", nullptr,
		WS_CHILD | WS_VISIBLE | WS_VSCROLL |
		ES_LEFT | ES_MULTILINE | WS_CLIPSIBLINGS,
		0, 0, 0, 0,
		hWnd, NULL, inst, NULL);
	HRESULT hr;
	hr = GetLastError();
	if (view == NULL)
	{
		ReleaseDC(hWnd, hdc);
		return -1;
	}
	SendMessage(view, EM_SETREADONLY, TRUE, 0);
	SendMessage(view, EM_EXLIMITTEXT, 0, 0x7FFFFFFE);
	SendMessage(view, EM_SETBKGNDCOLOR, 0, RGB(70, 70, 70));
	// Setup default font for the log.
	//SendMessage (view, WM_SETFONT, (WPARAM)GetStockObject (DEFAULT_GUI_FONT), FALSE);
	format.cbSize = sizeof(format);
	format.dwMask = CFM_BOLD | CFM_COLOR | CFM_FACE | CFM_SIZE | CFM_CHARSET;
	format.dwEffects = 0;
	format.yHeight = 200;
	format.crTextColor = RGB(223, 223, 223);
	format.bCharSet = ANSI_CHARSET;
	format.bPitchAndFamily = FF_SWISS | VARIABLE_PITCH;
	wcscpy(format.szFaceName, L"DejaVu Sans");	// At least I have it. :p
	SendMessageW(view, EM_SETCHARFORMAT, SCF_ALL, (LPARAM)&format);

	ConWindow = view;
	ReleaseDC(hWnd, hdc);

	view = CreateWindowExW(WS_EX_NOPARENTNOTIFY, L"STATIC", NULL, WS_CHILD | WS_VISIBLE | WS_CLIPSIBLINGS | SS_OWNERDRAW, 0, 0, 0, 0, hWnd, nullptr, inst, nullptr);
	if (view == nullptr)
	{
		return -1;
	}
	SetWindowLong(view, GWL_ID, IDC_STATIC_TITLE);
	GameTitleWindow = view;

	return 0;
}

LRESULT MainWindow::OnSize(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (wParam != SIZE_MAXHIDE && wParam != SIZE_MAXSHOW)
	{
		LayoutMainWindow(hWnd, ErrorPane);
	}
	return 0;
}

LRESULT MainWindow::OnDrawItem(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	HGDIOBJ oldfont;
	HBRUSH hbr;
	DRAWITEMSTRUCT* drawitem;
	RECT rect;
	SIZE size;

	// Draw title banner.
	if (wParam == IDC_STATIC_TITLE && GameStartupInfo.Name.IsNotEmpty())
	{
		const PalEntry* c;

		// Draw the game title strip at the top of the window.
		drawitem = (LPDRAWITEMSTRUCT)lParam;

		// Draw the background.
		rect = drawitem->rcItem;
		rect.bottom -= 1;
		c = (const PalEntry*)&GameStartupInfo.BkColor;
		hbr = CreateSolidBrush(RGB(c->r, c->g, c->b));
		FillRect(drawitem->hDC, &drawitem->rcItem, hbr);
		DeleteObject(hbr);

		// Calculate width of the title string.
		SetTextAlign(drawitem->hDC, TA_TOP);
		oldfont = SelectObject(drawitem->hDC, GameTitleFont != NULL ? GameTitleFont : (HFONT)GetStockObject(DEFAULT_GUI_FONT));
		auto widename = GameStartupInfo.Name.WideString();
		GetTextExtentPoint32W(drawitem->hDC, widename.c_str(), (int)widename.length(), &size);

		// Draw the title.
		c = (const PalEntry*)&GameStartupInfo.FgColor;
		SetTextColor(drawitem->hDC, RGB(c->r, c->g, c->b));
		SetBkMode(drawitem->hDC, TRANSPARENT);
		TextOutW(drawitem->hDC, rect.left + (rect.right - rect.left - size.cx) / 2, 2, widename.c_str(), (int)widename.length());
		SelectObject(drawitem->hDC, oldfont);
		return TRUE;
	}
	// Draw stop icon.
	else if (wParam == IDC_ICONPIC)
	{
		HICON icon;
		POINTL char_pos;
		drawitem = (LPDRAWITEMSTRUCT)lParam;

		// This background color should match the edit control's.
		hbr = CreateSolidBrush(RGB(70, 70, 70));
		FillRect(drawitem->hDC, &drawitem->rcItem, hbr);
		DeleteObject(hbr);

		// Draw the icon aligned with the first line of error text.
		SendMessage(ConWindow, EM_POSFROMCHAR, (WPARAM)&char_pos, ErrorIconChar);
		icon = (HICON)LoadImage(0, IDI_ERROR, IMAGE_ICON, 0, 0, LR_DEFAULTSIZE | LR_SHARED);
		DrawIcon(drawitem->hDC, 6, char_pos.y, icon);
		return TRUE;
	}
	return FALSE;
}

LRESULT MainWindow::OnCommand(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (ErrorIcon != NULL && (HWND)lParam == ConWindow && HIWORD(wParam) == EN_UPDATE)
	{
		// Be sure to redraw the error icon if the edit control changes.
		InvalidateRect(ErrorIcon, NULL, TRUE);
		return 0;
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT MainWindow::OnClose(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	PostQuitMessage(0);
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

LRESULT MainWindow::OnDestroy(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	if (GameTitleFont != NULL)
	{
		DeleteObject(GameTitleFont);
	}
	return DefWindowProc(hWnd, msg, wParam, lParam);
}

void MainWindow::PrintStr(const char* cp)
{
	if (ConWindowHidden)
	{
		bufferedConsoleStuff.Push(cp);
	}
	else
	{
		DoPrintStr(cp);
	}
}

void MainWindow::FlushBufferedConsoleStuff()
{
	for (unsigned i = 0; i < bufferedConsoleStuff.Size(); i++)
	{
		DoPrintStr(bufferedConsoleStuff[i].GetChars());
	}
	bufferedConsoleStuff.Clear();
}

void MainWindow::DoPrintStr(const char* cpt)
{
	wchar_t wbuf[256];
	int bpos = 0;
	CHARRANGE selection = {};
	CHARRANGE endselection = {};
	LONG lines_before = 0, lines_after = 0;

	// Store the current selection and set it to the end so we can append text.
	SendMessage(ConWindow, EM_EXGETSEL, 0, (LPARAM)&selection);
	endselection.cpMax = endselection.cpMin = GetWindowTextLength(ConWindow);
	SendMessage(ConWindow, EM_EXSETSEL, 0, (LPARAM)&endselection);

	// GetWindowTextLength and EM_EXSETSEL can disagree on where the end of
	// the text is. Find out what EM_EXSETSEL thought it was and use that later.
	SendMessage(ConWindow, EM_EXGETSEL, 0, (LPARAM)&endselection);

	// Remember how many lines there were before we added text.
	lines_before = (LONG)SendMessage(ConWindow, EM_GETLINECOUNT, 0, 0);

	const uint8_t* cptr = (const uint8_t*)cpt;

	auto outputIt = [&]()
	{
		wbuf[bpos] = 0;
		SendMessageW(ConWindow, EM_REPLACESEL, FALSE, (LPARAM)wbuf);
		bpos = 0;
	};

	while (int chr = GetCharFromString(cptr))
	{
		if ((chr == TEXTCOLOR_ESCAPE && bpos != 0) || bpos == 255)
		{
			outputIt();
		}
		if (chr != TEXTCOLOR_ESCAPE)
		{
			if (chr >= 0x1D && chr <= 0x1F)
			{ // The bar characters, most commonly used to indicate map changes
				chr = 0x2550;	// Box Drawings Double Horizontal
			}
			wbuf[bpos++] = chr;
		}
		else
		{
			EColorRange range = V_ParseFontColor(cptr, CR_UNTRANSLATED, CR_YELLOW);

			if (range != CR_UNDEFINED)
			{
				// Change the color of future text added to the control.
				PalEntry color = V_LogColorFromColorRange(range);

				// Change the color.
				CHARFORMAT format;
				format.cbSize = sizeof(format);
				format.dwMask = CFM_COLOR;
				format.dwEffects = 0;
				format.crTextColor = RGB(color.r, color.g, color.b);
				SendMessage(ConWindow, EM_SETCHARFORMAT, SCF_SELECTION, (LPARAM)&format);
			}
		}
	}
	if (bpos != 0)
	{
		outputIt();
	}

	// If the old selection was at the end of the text, keep it at the end and
	// scroll. Don't scroll if the selection is anywhere else.
	if (selection.cpMin == endselection.cpMin && selection.cpMax == endselection.cpMax)
	{
		selection.cpMax = selection.cpMin = GetWindowTextLength(ConWindow);
		lines_after = (LONG)SendMessage(ConWindow, EM_GETLINECOUNT, 0, 0);
		if (lines_after > lines_before)
		{
			SendMessage(ConWindow, EM_LINESCROLL, 0, lines_after - lines_before);
		}
	}
	// Restore the previous selection.
	SendMessage(ConWindow, EM_EXSETSEL, 0, (LPARAM)&selection);
	// Give the edit control a chance to redraw itself.
	I_GetEvent();
}

static DWORD CALLBACK WriteLogFileStreamer(DWORD_PTR cookie, LPBYTE buffer, LONG cb, LONG* pcb)
{
	uint32_t didwrite;
	LONG p, pp;

	// Replace gray foreground color with black.
	static const char* badfg = "\\red223\\green223\\blue223;";
	//                           4321098 765432109 876543210
	//                               2          1          0
	for (p = pp = 0; p < cb; ++p)
	{
		if (buffer[p] == badfg[pp])
		{
			++pp;
			if (pp == 25)
			{
				buffer[p - 1] = buffer[p - 2] = buffer[p - 3] =
					buffer[p - 9] = buffer[p - 10] = buffer[p - 11] =
					buffer[p - 18] = buffer[p - 19] = buffer[p - 20] = '0';
				break;
			}
		}
		else
		{
			pp = 0;
		}
	}

	auto& writeData = *reinterpret_cast<std::function<bool(const void* data, uint32_t size, uint32_t& written)>*>(cookie);
	if (!writeData((const void*)buffer, cb, didwrite))
	{
		return 1;
	}
	*pcb = didwrite;
	return 0;
}

void MainWindow::GetLog(std::function<bool(const void* data, uint32_t size, uint32_t& written)> writeData)
{
	FlushBufferedConsoleStuff();

	EDITSTREAM streamer = { (DWORD_PTR)&writeData, 0, WriteLogFileStreamer };
	SendMessage(ConWindow, EM_STREAMOUT, SF_RTF, (LPARAM)&streamer);
}

// each platform has its own specific version of this function.
void MainWindow::SetWindowTitle(const char* caption)
{
	std::wstring widecaption;
	if (!caption)
	{
		FStringf default_caption("" GAMENAME " %s " X64 " (%s)", GetVersionString(), GetGitTime());
		widecaption = default_caption.WideString();
	}
	else
	{
		widecaption = WideString(caption);
	}
	SetWindowText(Window, widecaption.c_str());
}
