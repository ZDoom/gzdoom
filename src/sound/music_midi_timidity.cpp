#include "i_musicinterns.h"
#include "c_cvars.h"
#include "cmdlib.h"
#include "templates.h"
#include "version.h"

#ifndef _WIN32
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <wordexp.h>
#include <signal.h>

int ChildQuit;

void ChildSigHandler (int signum)
{
	ChildQuit = waitpid (-1, NULL, WNOHANG);
}
#endif

#ifdef _WIN32


BOOL SafeTerminateProcess(HANDLE hProcess, UINT uExitCode);

static char TimidityTitle[] = "TiMidity (" GAMENAME " Launched)";
const char TimidityPPMIDIDevice::EventName[] = "TiMidity Killer";

CVAR (String, timidity_exe, "timidity.exe", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
#else
CVAR (String, timidity_exe, "timidity", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
#endif
CVAR (String, timidity_extargs, "", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)	// extra args to pass to Timidity
CVAR (String, timidity_chorus, "0", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (String, timidity_reverb, "0", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, timidity_stereo, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, timidity_8bit, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, timidity_byteswap, false, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)

// added because Timidity's output is rather loud.
CUSTOM_CVAR (Float, timidity_mastervolume, 1.0f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if (self < 0.f)
		self = 0.f;
	else if (self > 4.f)
		self = 4.f;
	if (currSong != NULL)
		currSong->TimidityVolumeChanged();
}

CUSTOM_CVAR (Int, timidity_pipe, 90, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{ // pipe size in ms
	if (timidity_pipe < 0)
	{ // a negative size makes no sense
		timidity_pipe = 0;
	}
}

CUSTOM_CVAR (Int, timidity_frequency, 44100, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{ // Clamp frequency to Timidity's limits
	if (self < 4000)
		self = 4000;
	else if (self > 65000)
		self = 65000;
}

//==========================================================================
//
// TimidityPPMIDIDevice Constructor
//
//==========================================================================

TimidityPPMIDIDevice::TimidityPPMIDIDevice(const char *args)
	: DiskName("zmid"),
#ifdef _WIN32
	  ReadWavePipe(INVALID_HANDLE_VALUE), WriteWavePipe(INVALID_HANDLE_VALUE),
	  ChildProcess(INVALID_HANDLE_VALUE),
	  Validated(false)
#else
	  ChildProcess(-1)
#endif
{
#ifndef _WIN32
	WavePipe[0] = WavePipe[1] = -1;
#endif

	if (args == NULL || *args == 0) args = timidity_exe;

	CommandLine.Format("%s %s -EFchorus=%s -EFreverb=%s -s%d ",
		args, *timidity_extargs,
		*timidity_chorus, *timidity_reverb, *timidity_frequency);

	if (DiskName == NULL)
	{
		Printf(PRINT_BOLD, "Could not create temp music file\n");
		return;
	}
}

//==========================================================================
//
// TimidityPPMIDIDevice Destructor
//
//==========================================================================

TimidityPPMIDIDevice::~TimidityPPMIDIDevice ()
{
#if _WIN32
	if (WriteWavePipe != INVALID_HANDLE_VALUE)
	{
		CloseHandle (WriteWavePipe);
		WriteWavePipe = INVALID_HANDLE_VALUE;
	}
	if (ReadWavePipe != INVALID_HANDLE_VALUE)
	{
		CloseHandle (ReadWavePipe);
		ReadWavePipe = INVALID_HANDLE_VALUE;
	}
#else
	if (WavePipe[1] != -1)
	{
		close (WavePipe[1]);
		WavePipe[1] = -1;
	}
	if (WavePipe[0] != -1)
	{
		close (WavePipe[0]);
		WavePipe[0] = -1;
	}
#endif
}

//==========================================================================
//
// TimidityPPMIDIDevice :: Preprocess
//
//==========================================================================

bool TimidityPPMIDIDevice::Preprocess(MIDIStreamer *song, bool looping)
{
	TArray<BYTE> midi;
	bool success;
	FILE *f;

	if (CommandLine.IsEmpty())
	{
		return false;
	}

	// Tell TiMidity++ whether it should loop or not
	CommandLine.LockBuffer()[LoopPos] = looping ? 'l' : ' ';
	CommandLine.UnlockBuffer();

	// Write MIDI song to temporary file
	song->CreateSMF(midi, looping ? 0 : 1);

	f = fopen(DiskName, "wb");
	if (f == NULL)
	{
		Printf(PRINT_BOLD, "Could not open temp music file\n");
		return false;
	}
	success = (fwrite(&midi[0], 1, midi.Size(), f) == (size_t)midi.Size());
	fclose (f);

	if (!success)
	{
		Printf(PRINT_BOLD, "Could not write temp music file\n");
	}
	return false;
}

//==========================================================================
//
// TimidityPPMIDIDevice :: Open
//
//==========================================================================

int TimidityPPMIDIDevice::Open(void (*callback)(unsigned int, void *, DWORD, DWORD), void *userdata)
{
	int pipeSize;

#ifdef _WIN32
	static SECURITY_ATTRIBUTES inheritable = { sizeof(inheritable), NULL, TRUE };
	
	if (!Validated && !ValidateTimidity ())
	{
		return 101;
	}

	Validated = true;
#endif // WIN32

	pipeSize = (timidity_pipe * timidity_frequency / 1000)
		<< (timidity_stereo + !timidity_8bit);

	{
#ifdef _WIN32
		// Round pipe size up to nearest power of 2 to try and avoid partial
		// buffer reads in FillStream() under NT. This does not seem to be an
		// issue under 9x.
		int bitmask = pipeSize & -pipeSize;
		
		while (bitmask < pipeSize)
			bitmask <<= 1;
		pipeSize = bitmask;

		if (!CreatePipe(&ReadWavePipe, &WriteWavePipe, &inheritable, pipeSize))
#else // WIN32
		if (pipe (WavePipe) == -1)
#endif
		{
			Printf(PRINT_BOLD, "Could not create a data pipe for TiMidity++.\n");
			pipeSize = 0;
		}
		else
		{
			Stream = GSnd->CreateStream(FillStream, pipeSize,
				(timidity_stereo ? 0 : SoundStream::Mono) |
				(timidity_8bit ? SoundStream::Bits8 : 0),
				timidity_frequency, this);
			if (Stream == NULL)
			{
				Printf(PRINT_BOLD, "Could not create music stream.\n");
				pipeSize = 0;
#ifdef _WIN32
				CloseHandle(WriteWavePipe);
				CloseHandle(ReadWavePipe);
				ReadWavePipe = WriteWavePipe = INVALID_HANDLE_VALUE;
#else
				close(WavePipe[1]);
				close(WavePipe[0]);
				WavePipe[0] = WavePipe[1] = -1;
#endif
			}
		}
		
		if (pipeSize == 0)
		{
			Printf(PRINT_BOLD, "If your soundcard cannot play more than one\n"
								"wave at a time, you will hear no music.\n");
		}
		else
		{
			CommandLine += "-o - -Ors";
		}
	}

	if (pipeSize == 0)
	{
		CommandLine += "-Od";
	}

	CommandLine += timidity_stereo ? 'S' : 'M';
	CommandLine += timidity_8bit ? '8' : '1';
	if (timidity_byteswap)
	{
		CommandLine += 'x';
	}

	LoopPos = CommandLine.Len() + 4;

	CommandLine += " -idl ";
	CommandLine += DiskName.GetName();
	return 0;
}

//==========================================================================
//
// TimidityPPMIDIDevice :: ValidateTimidity
//
// Check that this TiMidity++ knows about the TiMidity Killer event.
// If not, then we can't use it, because Win32 provides no other way
// to conveniently signal it to quit. The check is done by simply
// searching for the event's name somewhere in the executable.
//
//==========================================================================

#ifdef _WIN32
bool TimidityPPMIDIDevice::ValidateTimidity()
{
	char foundPath[MAX_PATH];
	char *filePart;
	DWORD pathLen;
	DWORD fileLen;
	HANDLE diskFile;
	HANDLE mapping;
	const BYTE *exeBase;
	const BYTE *exeEnd;
	const BYTE *exe;
	bool good;

	pathLen = SearchPath (NULL, timidity_exe, NULL, MAX_PATH, foundPath, &filePart);
	if (pathLen == 0)
	{
		Printf(PRINT_BOLD, "Please set the timidity_exe cvar to the location of TiMidity++\n");
		return false;
	}
	if (pathLen > MAX_PATH)
	{
		Printf(PRINT_BOLD, "The path to TiMidity++ is too long\n");
		return false;
	}

	diskFile = CreateFile (foundPath, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (diskFile == INVALID_HANDLE_VALUE)
	{
		Printf(PRINT_BOLD, "Could not access %s\n", foundPath);
		return false;
	}
	fileLen = GetFileSize (diskFile, NULL);
	mapping = CreateFileMapping (diskFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if (mapping == NULL)
	{
		Printf(PRINT_BOLD, "Could not create mapping for %s\n", foundPath);
		CloseHandle (diskFile);
		return false;
	}
	exeBase = (const BYTE *)MapViewOfFile (mapping, FILE_MAP_READ, 0, 0, 0);
	if (exeBase == NULL)
	{
		Printf(PRINT_BOLD, "Could not map %s\n", foundPath);
		CloseHandle (mapping);
		CloseHandle (diskFile);
		return false;
	}

	good = false;
	try
	{
		for (exe = exeBase, exeEnd = exeBase+fileLen; exe < exeEnd; )
		{
			const char *tSpot = (const char *)memchr(exe, 'T', exeEnd - exe);
			if (tSpot == NULL)
			{
				break;
			}
			if (memcmp(tSpot+1, EventName+1, sizeof(EventName)-1) == 0)
			{
				good = true;
				break;
			}
			exe = (const BYTE *)tSpot + 1;
		}
	}
	catch (...)
	{
		Printf(PRINT_BOLD, "Error reading %s\n", foundPath);
	}
	if (!good)
	{
		Printf(PRINT_BOLD, GAMENAME " requires a special version of TiMidity++\n");
	}

	UnmapViewOfFile((LPVOID)exeBase);
	CloseHandle(mapping);
	CloseHandle(diskFile);

	return good;
}
#endif // _WIN32

//==========================================================================
//
// TimidityPPMIDIDevice :: LaunchTimidity
//
//==========================================================================

bool TimidityPPMIDIDevice::LaunchTimidity ()
{
	if (CommandLine.IsEmpty())
	{
		return false;
	}

	DPrintf ("cmd: \x1cG%s\n", CommandLine.GetChars());

#ifdef _WIN32
	STARTUPINFO startup = { sizeof(startup), };
	PROCESS_INFORMATION procInfo;

	startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;

	startup.hStdInput = INVALID_HANDLE_VALUE;
	startup.hStdOutput = WriteWavePipe != INVALID_HANDLE_VALUE ?
						  WriteWavePipe : GetStdHandle (STD_OUTPUT_HANDLE);
	startup.hStdError = GetStdHandle (STD_ERROR_HANDLE);

	startup.lpTitle = TimidityTitle;
	startup.wShowWindow = SW_SHOWMINNOACTIVE;

	if (CreateProcess(NULL, CommandLine.LockBuffer(), NULL, NULL, TRUE,
		DETACHED_PROCESS, NULL, NULL, &startup, &procInfo))
	{
		ChildProcess = procInfo.hProcess;
		//SetThreadPriority (procInfo.hThread, THREAD_PRIORITY_HIGHEST);
		CloseHandle(procInfo.hThread);		// Don't care about the created thread
		CommandLine.UnlockBuffer();
		return true;
	}
	CommandLine.UnlockBuffer();

	char hres[9];
	LPTSTR msgBuf;
	HRESULT err = GetLastError();

	if (!FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER |
						FORMAT_MESSAGE_FROM_SYSTEM |
						FORMAT_MESSAGE_IGNORE_INSERTS,
						NULL, err, 0, (LPTSTR)&msgBuf, 0, NULL))
	{
		mysnprintf(hres, countof(hres), "%08lx", err);
		msgBuf = hres;
	}

	Printf(PRINT_BOLD, "Could not run timidity with the command line:\n%s\n"
						"Reason: %s\n", CommandLine.GetChars(), msgBuf);
	if (msgBuf != hres)
	{
		LocalFree (msgBuf);
	}
	return false;
#else
	if (WavePipe[0] != -1 && WavePipe[1] == -1 && Stream != NULL)
	{
		// Timidity was previously launched, so the write end of the pipe
		// is closed, and the read end is still open. Close the pipe
		// completely and reopen it.
		
		close (WavePipe[0]);
		WavePipe[0] = -1;
		delete Stream;
		Stream = NULL;
		Open (NULL, NULL);
	}
	
	int forkres;
	wordexp_t words = {};

	switch (wordexp (CommandLine.GetChars(), &words, 0))
	{
	case 0: // all good
		break;
		
	case WRDE_NOSPACE:
		wordfree (&words);
	default:
		return false;
	}

	forkres = fork ();

	if (forkres == 0)
	{
		close (WavePipe[0]);
		dup2 (WavePipe[1], STDOUT_FILENO);
		freopen ("/dev/null", "r", stdin);
//		freopen ("/dev/null", "w", stderr);
		close (WavePipe[1]);

		execvp (words.we_wordv[0], words.we_wordv);
		fprintf(stderr,"execvp failed\n");
		_exit (0);	// if execvp succeeds, we never get here
	}
	else if (forkres < 0)
	{
		Printf (PRINT_BOLD, "Could not fork when trying to start timidity\n");
	}
	else
	{
//		printf ("child is %d\n", forkres);
		ChildProcess = forkres;
		close (WavePipe[1]);
		WavePipe[1] = -1;
/*		usleep(1000000);
		if (waitpid(ChildProcess, NULL, WNOHANG) == ChildProcess)
		{
		    fprintf(stderr,"Launching timidity failed\n");
		}*/
	}
	
	wordfree (&words);
	return ChildProcess != -1;
#endif // _WIN32
}

//==========================================================================
//
// TimidityPPMIDIDevice :: FillStream
//
//==========================================================================

bool TimidityPPMIDIDevice::FillStream(SoundStream *stream, void *buff, int len, void *userdata)
{
	TimidityPPMIDIDevice *song = (TimidityPPMIDIDevice *)userdata;
	
#ifdef _WIN32
	DWORD avail, got, didget;

	if (!PeekNamedPipe(song->ReadWavePipe, NULL, 0, NULL, &avail, NULL) || avail == 0)
	{ // If nothing is available from the pipe, play silence.
		memset (buff, 0, len);
	}
	else
	{
		didget = 0;
		for (;;)
		{
			ReadFile(song->ReadWavePipe, (BYTE *)buff+didget, len-didget, &got, NULL);
			didget += got;
			if (didget >= (DWORD)len)
				break;

			// Give TiMidity++ a chance to output something more to the pipe
			Sleep (10);
			if (!PeekNamedPipe(song->ReadWavePipe, NULL, 0, NULL, &avail, NULL) || avail == 0)
			{
				memset ((BYTE *)buff+didget, 0, len-didget);
				break;
			}
		} 
	}
#else
	ssize_t got;
	fd_set rfds;
	struct timeval tv;

	if (ChildQuit == song->ChildProcess)
	{
		ChildQuit = 0;
		fprintf(stderr, "child gone\n");
		song->ChildProcess = -1;
		return false;
	}

	FD_ZERO(&rfds);
	FD_SET(song->WavePipe[0], &rfds);
	tv.tv_sec = 0;
	tv.tv_usec = 50;
//	fprintf(stderr,"select\n");
	if (select(1, &rfds, NULL, NULL, &tv) <= 0 && 0)
	{ // Nothing available, so play silence.
//	fprintf(stderr,"nothing\n");
	 //   memset(buff, 0, len);
	    return true;
	}
//	fprintf(stderr,"something\n");

	got = read(song->WavePipe[0], (BYTE *)buff, len);
	if (got < len)
	{
		memset((BYTE *)buff+got, 0, len-got);
	}
#endif
	return true;
}

//==========================================================================
//
// TimidityPPMIDIDevice :: TimidityVolumeChanged
//
//==========================================================================

void TimidityPPMIDIDevice::TimidityVolumeChanged()
{
	if (Stream != NULL)
	{
		Stream->SetVolume(timidity_mastervolume);
	}
}

//==========================================================================
//
// TimidityPPMIDIDevice :: IsOpen
//
//==========================================================================

bool TimidityPPMIDIDevice::IsOpen() const
{
#ifdef _WIN32
	if (ChildProcess != INVALID_HANDLE_VALUE)
	{
		if (WaitForSingleObject(ChildProcess, 0) != WAIT_TIMEOUT)
		{ // Timidity++ has quit
			CloseHandle(ChildProcess);
			const_cast<TimidityPPMIDIDevice *>(this)->ChildProcess = INVALID_HANDLE_VALUE;
#else
	if (ChildProcess != -1)
	{
		if (waitpid (ChildProcess, NULL, WNOHANG) == ChildProcess)
		{
			const_cast<TimidityPPMIDIDevice *>(this)->ChildProcess = -1;
#endif
			return false;
		}
		return true;
	}
	return false;
}

//==========================================================================
//
// TimidityPPMIDIDevice :: Resume
//
//==========================================================================

int TimidityPPMIDIDevice::Resume()
{
	if (!Started)
	{
		if (LaunchTimidity())
		{
			// Assume success if not mixing with the sound system
			if (Stream == NULL || Stream->Play(true, timidity_mastervolume))
			{
				Started = true;
				return 0;
			}
		}
		return 1;
	}
	return 0;
}

//==========================================================================
//
// TimidityPPMIDIDevice :: Stop
//
//==========================================================================

void TimidityPPMIDIDevice::Stop ()
{
	if (Started)
	{
		if (Stream != NULL)
		{
			Stream->Stop();
		}
#ifdef _WIN32
		if (ChildProcess != INVALID_HANDLE_VALUE)
		{
			if (!SafeTerminateProcess(ChildProcess, 666) && GetLastError() != ERROR_PROCESS_ABORTED)
			{
				TerminateProcess(ChildProcess, 666);
			}
			CloseHandle(ChildProcess);
			ChildProcess = INVALID_HANDLE_VALUE;
		}
#else
		if (ChildProcess != -1)
		{
			if (kill(ChildProcess, SIGTERM) != 0)
			{
				kill(ChildProcess, SIGKILL);
			}
			waitpid(ChildProcess, NULL, 0);
			ChildProcess = -1;
		}
#endif
	}
	Started = false;
}

#ifdef _WIN32
/*
	Safely terminate a process by creating a remote thread
	in the process that calls ExitProcess

	Source is a Dr Dobbs article circa 1999.
*/
typedef HANDLE (WINAPI *CreateRemoteThreadProto)(HANDLE,LPSECURITY_ATTRIBUTES,SIZE_T,LPTHREAD_START_ROUTINE,LPVOID,DWORD,LPDWORD);

BOOL SafeTerminateProcess(HANDLE hProcess, UINT uExitCode)
{
	DWORD dwTID, dwCode;
	HRESULT dwErr = 0;
	HANDLE hRT = NULL;
	HINSTANCE hKernel = GetModuleHandle("Kernel32");
	BOOL bSuccess = FALSE;

	// Detect the special case where the process is already dead...
	if ( GetExitCodeProcess(hProcess, &dwCode) && (dwCode == STILL_ACTIVE) )
	{
		FARPROC pfnExitProc;
		CreateRemoteThreadProto pfCreateRemoteThread;

		pfnExitProc = GetProcAddress(hKernel, "ExitProcess");

		// CreateRemoteThread does not exist on 9x systems.
		pfCreateRemoteThread = (CreateRemoteThreadProto)GetProcAddress(hKernel, "CreateRemoteThread");

		if (pfCreateRemoteThread == NULL)
		{
			dwErr = ERROR_INVALID_FUNCTION;
		}
		else
		{
			hRT = pfCreateRemoteThread(hProcess,
										NULL,
										0,
										(LPTHREAD_START_ROUTINE)pfnExitProc,
										(PVOID)(UINT_PTR)uExitCode, 0, &dwTID);

			if ( hRT == NULL )
				dwErr = GetLastError();
		}
	}
	else
	{
		dwErr = ERROR_PROCESS_ABORTED;
	}

	if ( hRT )
	{
		// Must wait for process to terminate to guarantee that it has exited...
		DWORD res = WaitForSingleObject(hProcess, 1000);
		CloseHandle(hRT);
		bSuccess = (res == WAIT_OBJECT_0);
		dwErr = WAIT_TIMEOUT;
	}

	if ( !bSuccess )
		SetLastError(dwErr);

	return bSuccess;
}
#endif
