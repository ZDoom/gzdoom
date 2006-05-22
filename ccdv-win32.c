/* ccdv.c */

/* Ported to Win32 by Randy Heit
 * Maybe I got a little carried away. This is pure Win32 code without any CRT in sight.
 *
 * To build this, use one of these command lines:
 *
 * [MinGW] gcc -Os -s -nostdlib -o ccdv.exe ccdv-win32.c -lkernel32 -luser32
 * [MSC]   cl -O1 ccdv-win32.c -link -subsystem:console -opt:nowin98 kernel32.lib user32.lib 
 *
 * Rewriting this to not use any global variables can save 512 bytes when compiled with MSC
 * because it allows the .data section to be ommitted, which means the header can occupy
 * 512 bytes rather than 1024. With GCC, it doesn't help the size any, since GCC still has
 * the separate .idata and .rdata sections. Since MSC really doesn't need this tool,
 * I'm not bothering with that size optimization.
 */

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

#define COLOR_SUCCESS	(FOREGROUND_GREEN|FOREGROUND_INTENSITY)                 /* green */
#define COLOR_FAILURE	(FOREGROUND_RED|FOREGROUND_INTENSITY)                   /* red */
#define COLOR_WARNING	(FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_INTENSITY)  /* yellow */
#define COLOR_COMMAND	(FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY) /* cyan */

#define TEXT_BLOCK_SIZE 8192
#define INDENT 2

PROCESS_INFORMATION gCCP;
size_t gNBufUsed, gNBufAllocated;
char *gBuf;
char *gAction;
char *gTarget;
char *gAr;
char *gArLibraryTarget;
BOOL  gDumpCmdArgs;
char *gArgsStr;
int gColumns;
int gExitStatus;
HANDLE gHeap;

HANDLE gStdOut, gStdErr;

#ifdef __GNUC__
#define REGPARM(x) __attribute((regparm(x)))
#else
#define REGPARM(x)
#endif

void REGPARM(1) perror(const char *string)
{
	char *buffer;
	DWORD error = GetLastError();
	DWORD len = FormatMessageA(FORMAT_MESSAGE_ALLOCATE_BUFFER|FORMAT_MESSAGE_FROM_SYSTEM|FORMAT_MESSAGE_IGNORE_INSERTS,
		NULL, error, 0, (LPTSTR)&buffer, 0, NULL);
	DWORD wrote;
	WriteFile (gStdErr, string, lstrlen(string), &wrote, NULL);
	if(len == 0)
	{
		char errcode[9];
		wsprintf(errcode, "%08x", error);
		WriteFile (gStdErr, ": Error ", 8, &wrote, NULL);
		WriteFile (gStdErr, errcode, 8, &wrote, NULL);
	}
	else
	{
		WriteFile (gStdErr, ": ", 2, &wrote, NULL);
		WriteFile (gStdErr, buffer, len, &wrote, NULL);
		LocalFree(buffer);
	}
	WriteFile (gStdErr, "\n", 1, &wrote, NULL);
}

static void DumpFormattedOutput()
{
	CONSOLE_SCREEN_BUFFER_INFO info;
	DWORD out;
	WORD color;
	char *cp;
	char spaces[8 + 1];
	char *saved;
	int curcol;
	int i;

	if(!GetConsoleScreenBufferInfo(gStdOut, &info))
	{
		WriteFile(gStdOut, gBuf, lstrlen(gBuf), &out, NULL);
		WriteFile(gStdOut, "\n", 1, &out, NULL);
		return;
	}

	spaces[0] = ' ';
	spaces[1] = ' ';
	spaces[2] = ' ';
	spaces[3] = ' ';
	spaces[4] = ' ';
	spaces[5] = ' ';
	spaces[6] = ' ';
	spaces[7] = ' ';
	spaces[8] = '\0';

	color = info.wAttributes & ~(FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY);
	curcol = 0;
	saved = NULL;
	if(gDumpCmdArgs)
	{
		SetConsoleTextAttribute(gStdOut, color | COLOR_COMMAND); 
		WriteConsole(gStdOut, gBuf, lstrlen(gArgsStr)+1, &out, NULL);
		SetConsoleTextAttribute(gStdOut, info.wAttributes);
	}
	for(cp = gBuf + lstrlen(gArgsStr) + 1;; cp++)
	{
		if(*cp == '\0')
		{
			if(saved != NULL)
			{
				cp = saved;
				saved = NULL;
			}
			else
				break;
		}
		if(*cp == '\r')
			continue;
		if(*cp == '\t')
		{
			saved = cp + 1;
			cp = spaces + 8 - (8 - ((curcol - INDENT - 1) % 8));
		}
		if(curcol == 0)
		{
			for(i = INDENT; --i >= 0;)
				WriteConsole(gStdOut, " " ,1, &out, NULL);
			curcol = INDENT;
		}
		WriteConsole(gStdOut, cp, 1, &out, NULL);
		if(++curcol == (gColumns - 1))
		{
			WriteConsole(gStdOut, "\n", 1, &out, NULL);
			curcol = 0;
		}
		else if(*cp == '\n')
			curcol = 0;
	}
	HeapFree(gHeap, 0, gBuf);
}	/* DumpFormattedOutput */

static void Wait(void)
{
	DWORD exitcode;
	WaitForSingleObject(gCCP.hProcess, INFINITE);
	GetExitCodeProcess(gCCP.hProcess, &exitcode);
	gExitStatus = (int)exitcode;
}	/* Wait */

static DWORD WINAPI SlurpThread(LPVOID foo)
{
	HANDLE fd = (HANDLE)foo;
	char *newbuf;
	DWORD ntoread;
	DWORD nread;

	for(;;)
	{
		if(gNBufUsed == (gNBufAllocated - 1))
		{
			if((newbuf =
				(char *) HeapReAlloc(gHeap, 0, gBuf,
								 gNBufAllocated + TEXT_BLOCK_SIZE)) == NULL)
			{
				return 1;
			}
			gNBufAllocated += TEXT_BLOCK_SIZE;
			gBuf = newbuf;
		}
		ntoread = (gNBufAllocated - gNBufUsed - 1);
		if(!ReadFile(fd, gBuf + gNBufUsed, ntoread, &nread, NULL))
		{
			return 2;
		}
		else if(nread > 0)
		{
			gNBufUsed += nread;
			gBuf[gNBufUsed] = '\0';
		}
	}
	return 0;
}

static void REGPARM(2) WriteAction(HANDLE hStdOut, const char *suffix)
{
	DWORD out;

	WriteFile(hStdOut, gAction, lstrlen(gAction), &out, NULL);
	if(gTarget != NULL)
	{
		WriteFile(hStdOut, " ", 1, &out, NULL);
		WriteFile(hStdOut, gTarget, lstrlen(gTarget), &out, NULL);
	}
	WriteFile(hStdOut, suffix, 3, &out, NULL);
}

static int REGPARM(2) Slurp(HANDLE fd, HANDLE hStdOut)
{
	HANDLE handles[2];
	DWORD waitstate;
	DWORD exitcode;
	DWORD out;
	const char *trail = "/-\\|", *trailcp;
	CONSOLE_SCREEN_BUFFER_INFO info;
	CONSOLE_CURSOR_INFO cursorInfo;
	WORD color, colors[5];
	int ncells, i;

	trailcp = trail;
	if(hStdOut != NULL)
	{
		WriteAction(hStdOut, "...");
	}

	handles[0] = gCCP.hProcess;
	handles[1] = CreateThread(NULL, 0, SlurpThread, (LPVOID)fd, 0, NULL);

	if(handles[1] == 0)
	{
		perror("ccdv: CreateThread");
		return -1;
	}

	if(hStdOut != NULL)
	{
		GetConsoleScreenBufferInfo(hStdOut, &info);
		info.dwCursorPosition.X = info.dwSize.X - 9;
		if(GetConsoleCursorInfo(hStdOut, &cursorInfo))
		{
			cursorInfo.bVisible = FALSE;
			SetConsoleCursorInfo(hStdOut, &cursorInfo);
		}
		else
		{
			cursorInfo.bVisible = TRUE;
		}
	}

	gExitStatus = 0xabadcafe;
	while(gExitStatus == 0xabadcafe)
	{
		waitstate = WaitForMultipleObjects(2, handles, FALSE, 1000);
		switch(waitstate)
		{
		case WAIT_TIMEOUT:
			if(hStdOut != NULL)
			{
				SetConsoleCursorPosition(hStdOut, info.dwCursorPosition);
				WriteConsoleA(hStdOut, trailcp, 1, &out, NULL);
				if(*++trailcp == '\0')
					trailcp = trail;
			}
			break;

		case WAIT_FAILED:
			perror("ccdv: WaitForMultipleObjects");
			CloseHandle(handles[1]);
			return -1;

		case WAIT_OBJECT_0:
			GetExitCodeProcess(gCCP.hProcess, &exitcode);
			CloseHandle(handles[1]);
			gExitStatus = (int)exitcode;
			break;

		case WAIT_OBJECT_0+1:
			GetExitCodeThread(handles[1], &exitcode);
			CloseHandle(handles[1]);
			if(exitcode == 1)
			{
				perror("ccdv: HeapReAlloc");
				return -1;
			}
			else if(exitcode == 2)
			{
				perror("ccdv: ReadFile");
				return -1;
			}
			Wait();
			break;
		}
	}
	if(hStdOut != NULL)
	{
		info.dwCursorPosition.X = 0;
		SetConsoleCursorPosition(hStdOut, info.dwCursorPosition);
		WriteAction(hStdOut, ":  ");
		info.dwCursorPosition.X = info.dwSize.X - 10;
		if(gExitStatus == 0)
		{
			SetConsoleCursorPosition(hStdOut, info.dwCursorPosition);
			WriteConsoleA(hStdOut, "[OK]     ", 9, &out, NULL);
			color = ((gNBufUsed - lstrlen(gArgsStr)) < 4) ? COLOR_SUCCESS : COLOR_WARNING;
			ncells = 2;
		}
		else
		{
			SetConsoleCursorPosition(hStdOut, info.dwCursorPosition);
			WriteConsoleA(hStdOut, "[ERROR]  ", 9, &out, NULL);
			color = COLOR_FAILURE;
			ncells = 5;
			gDumpCmdArgs = 1;	/* print cmd when there are errors */
		}
		color |= info.wAttributes & ~(FOREGROUND_RED|FOREGROUND_GREEN|FOREGROUND_BLUE|FOREGROUND_INTENSITY);
		for(i = 0; i < ncells; ++i)
		{
			colors[i] = color;
		}
		info.dwCursorPosition.X++;
		WriteConsoleOutputAttribute(hStdOut, colors, ncells, info.dwCursorPosition, &out);
		if(!cursorInfo.bVisible)
		{
			cursorInfo.bVisible = TRUE;
			SetConsoleCursorInfo(hStdOut, &cursorInfo);
		}
		WriteConsole(hStdOut, "\n", 1, &out, NULL);
	}
	else
	{
		gDumpCmdArgs = (gExitStatus != 0);	/* print cmd when there are errors */
	}
	return (0);
}	/* SlurpProgress */

static const char *REGPARM(2) Basename(const char *path, int len)
{
	while(len > 0)
	{
		len--;
		if(path[len] == '/' || path[len] == '\\')
		{
			return path + len + 1;
		}
	}
	return path;
}	/* Basename */

static const char *REGPARM(2) Extension(const char *path, int len)
{
	while(len > 0)
	{
		len--;
		if(path[len] == '.')
		{
			return path + len;
		}
	}
	return "";
}	/* Extension */

static void Usage(void)
{
	static const char sUsage[] = "\
Usage: ccdv /path/to/cc CFLAGS...\n\
\n\
I wrote this to reduce the deluge Make output to make finding actual problems\n\
easier.  It is intended to be invoked from Makefiles, like this.  Instead of:\n\
\n\
	.c.o:\n\
		$(CC) $(CFLAGS) $(DEFS) $(CPPFLAGS) $< -c\n\
\n\
Rewrite your rule so it looks like:\n\
\n\
	.c.o:\n\
		@ccdv $(CC) $(CFLAGS) $(DEFS) $(CPPFLAGS) $< -c\n\
	.cpp.o:\n\
		@ccdv $(CXX) $(CFLAGS) $(DEFS) $(CPPFLAGS) $< -c\n\
\n\
ccdv 1.1.0 is Free under the GNU Public License.  Enjoy!\n\
  -- Mike Gleason, NcFTP Software <http://www.ncftp.com>\n\
  -- John F Meinel Jr, <http://ccdv.sourceforge.net>\n\
  ";
	DWORD out;
	WriteFile (gStdErr, sUsage, sizeof(sUsage)-1, &out, NULL);
	ExitProcess(96);
}	/* Usage */

static BOOL REGPARM(3) StepCmdLine(char **cmdline, char **arg, int *arglen)
{
	char *gArgsStr = *cmdline;

	// Skip whitespace
	while(*gArgsStr == ' ' || *gArgsStr == '\t' || *gArgsStr == '\n' || *gArgsStr == '\r')
	{
		gArgsStr++;
	}
	if(*gArgsStr == 0)
	{
		*cmdline = gArgsStr;
		return FALSE;
	}
	if(*gArgsStr == '\"')
	{ // It's a quoted string
		*arg = ++gArgsStr;
		while(*gArgsStr && *gArgsStr != '\"')
		{
			gArgsStr++;
		}
		*arglen = gArgsStr - *arg;
		if(*gArgsStr)
		{ // Ends with a quote
			gArgsStr++;
		}
		*cmdline = gArgsStr;
		return TRUE;
	}
	// It's a whitespace-separated string
	*arg = gArgsStr;
	while(*gArgsStr && *gArgsStr != ' ' && *gArgsStr != '\t' && *gArgsStr != '\n' && *gArgsStr != '\r')
	{
		gArgsStr++;
	}
	*arglen = gArgsStr - *arg;
	*cmdline = gArgsStr;
	return TRUE;
}

static void REGPARM(3) SetTarget(char **target, const char *arg, int arglen)
{
	const char *base;

	if (*target) HeapFree(gHeap, 0, *target);
	base = Basename(arg, arglen);
	arglen = arglen - (base - arg) + 1;
	*target = HeapAlloc(gHeap, 0, arglen);
	lstrcpyn(*target, base, arglen);
}

void mainCRTStartup(void)
{
	const char *ext;
	char *cmdline, *arg;
	int arglen, extlen;
	SECURITY_ATTRIBUTES security;
	STARTUPINFO siStartInfo;
	HANDLE pipeRd, pipeWr, pipeRdDup;
	char emerg[256];
	DWORD nread;
	int cc = 0;
	int yy = 0;
	CONSOLE_SCREEN_BUFFER_INFO bufferInfo;

	gExitStatus = 95;
	gHeap = GetProcessHeap();
	gArgsStr = cmdline = GetCommandLine();
	gStdOut = GetStdHandle(STD_OUTPUT_HANDLE);
	gStdErr = GetStdHandle(STD_ERROR_HANDLE);

	if (!StepCmdLine(&cmdline, &arg, &arglen) ||	// Skip ccdv.exe
		!StepCmdLine(&cmdline, &arg, &arglen))		// Read name of process to run
	{
		Usage();
	}

	// "Running *argv[1]*"
	ext = Basename(arg, arglen);
	extlen = arglen - (ext - arg);
	gAction = HeapAlloc(gHeap, 0, 64+extlen);
	lstrcpy(gAction, "Running ");
	lstrcpyn(gAction+8, ext, extlen+1);

	if (extlen == 2 && lstrcmpi(gAction+8, "ar") == CSTR_EQUAL)
	{
		SetTarget(&gAr, arg, arglen);
	}

	while(StepCmdLine(&cmdline, &arg, &arglen))
	{
		if(arglen == 2 && arg[0] == '-' && arg[1] == 'o')
		{
			if(StepCmdLine(&cmdline, &arg, &arglen))
			{
				ext = Extension(arg, arglen);
				if(ext[0] != '.' || (ext[1] != 'o' && ext[1] != 'O'))
				{
					lstrcpy(gAction, "Linking");
					SetTarget(&gTarget, arg, arglen);
				}
			}
			continue;
		}
		else if(arg[0] == '-' || arg[0] == '+' || arg[0] == '/')
		{
			continue;
		}
		ext = Extension(arg, arglen);
		if(ext[0] == '.' && (ext[1] == 'C' || ext[1] == 'c'))
		{
			cc++;
			SetTarget(&gTarget, arg, arglen);
		}
		else if(ext[0] == '.' && ext[1] == 'y')
		{
			yy++;
		}
		else if(CompareString(LOCALE_USER_DEFAULT, NORM_IGNORECASE, ".nas", 4, ext, -1) == CSTR_EQUAL)
		{
			lstrcpy(gAction, "Assembling");
			SetTarget(&gTarget, arg, arglen);
		}
		else if(gArLibraryTarget == NULL && ext[0] == '.' && ext[1] == 'a')
		{
			SetTarget(&gArLibraryTarget, arg, arglen);
		}
	}
	if((gAr != NULL) && (gArLibraryTarget != NULL))
	{
		lstrcpy(gAction, "Creating library");
		if(gTarget != NULL) HeapFree(gHeap, 0, gTarget);
		gTarget = gArLibraryTarget;
		gArLibraryTarget = NULL;
	}
	else if(cc > 0)
	{
		lstrcpy(gAction, yy == 0 ? "Compiling" : "Generating");
	}

	/* Initialize security attributes for the pipe */
	security.nLength = sizeof security;
	security.lpSecurityDescriptor = NULL;
	security.bInheritHandle = TRUE;

	if(!CreatePipe(&pipeRd, &pipeWr, &security, 0))
	{
		perror("ccdv: pipe");
		ExitProcess(97);
	}

	if (!DuplicateHandle(GetCurrentProcess(), pipeRd,
			GetCurrentProcess(), &pipeRdDup , 0,
			FALSE,
			DUPLICATE_SAME_ACCESS))
	{
		perror("ccdv: DuplicateHandle");
	}
    CloseHandle(pipeRd);

	/* Initialize startup info for the child process */
	siStartInfo.cb = sizeof siStartInfo;
	siStartInfo.lpReserved = NULL;
	siStartInfo.lpDesktop = NULL;
	siStartInfo.lpTitle = NULL;
	siStartInfo.dwX = 0;
	siStartInfo.dwY = 0;
	siStartInfo.dwXSize = 0;
	siStartInfo.dwYSize = 0;
	siStartInfo.dwXCountChars = 0;
	siStartInfo.dwYCountChars = 0;
	siStartInfo.dwFillAttribute = 0;
	siStartInfo.dwFlags = STARTF_USESTDHANDLES;
	siStartInfo.wShowWindow = 0;
	siStartInfo.cbReserved2 = 0;
	siStartInfo.lpReserved2 = 0;
	siStartInfo.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
	siStartInfo.hStdOutput = pipeWr;
	siStartInfo.hStdError = pipeWr;

	StepCmdLine(&gArgsStr, &arg, &arglen);	// Skip "ccdv"
	while(*gArgsStr == ' ' || *gArgsStr == '\t' || *gArgsStr == '\n' || *gArgsStr == '\r')
		gArgsStr++;

	extlen = lstrlen(gArgsStr);
	gNBufAllocated = extlen + TEXT_BLOCK_SIZE;
	gBuf = (char *) HeapAlloc(gHeap, 0, gNBufAllocated);
	if(gBuf == NULL)
		goto panic;
	gNBufUsed = extlen + 1;
	lstrcpy(gBuf, gArgsStr);
	gBuf[extlen] = '\n';
	gBuf[extlen+1] = 0;

	if(!CreateProcessA(NULL, 
		gArgsStr,      /* command gArgsStr */
		NULL,          /* process security attributes */
		NULL,          /* primary thread security attributes */
		TRUE,          /* handles are inherited */
		0,             /* creation flags */
		NULL,          /* use parent's environment */
		NULL,          /* use parent's current directory */
		&siStartInfo,  /* STARTUPINFO pointer */
		&gCCP))        /* receives PROCESS_INFORMATION */
	{
		CloseHandle(pipeRdDup);
		CloseHandle(pipeWr);
		perror("ccdv: CreateProcess");
		gExitStatus = 98;
		goto panic;
	}
	CloseHandle(gCCP.hThread);

	if(GetConsoleScreenBufferInfo(gStdOut, &bufferInfo))
	{
		gColumns = bufferInfo.dwSize.X;
		if(Slurp(pipeRdDup, gStdOut) < 0)
			goto panic;
	}
	else
	{
		if(Slurp(pipeRdDup, NULL) < 0)
			goto panic;
	}
	DumpFormattedOutput();
	ExitProcess(gExitStatus);

  panic:
	gDumpCmdArgs = 1;	/* print cmd when there are errors */
	DumpFormattedOutput();
	while(ReadFile(pipeRdDup, emerg, sizeof emerg, &nread, NULL) && nread > 0)
		WriteFile(gStdErr, emerg, nread, &nread, NULL);
	Wait();
	ExitProcess(gExitStatus);
}	/* main */

/* eof ccdv.c */
