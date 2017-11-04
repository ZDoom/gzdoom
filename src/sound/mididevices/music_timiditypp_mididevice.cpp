/*
** music_timiditypp_mididevice.cpp
** Provides access to timidity.exe
**
**---------------------------------------------------------------------------
** Copyright 2001-2017 Randy Heit
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

#include "i_midi_win32.h"

#include <string>
#include <vector>

#include "i_musicinterns.h"
#include "c_cvars.h"
#include "cmdlib.h"
#include "templates.h"
#include "version.h"
#include "tmpfileplus.h"

#ifndef _WIN32
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>
#ifndef __OpenBSD__
#include <wordexp.h>
#endif
#include <glob.h>
#include <signal.h>

int ChildQuit;

void ChildSigHandler (int signum)
{
	ChildQuit = waitpid (-1, NULL, WNOHANG);
}
#endif

class TimidityPPMIDIDevice : public PseudoMIDIDevice
{
public:
	TimidityPPMIDIDevice(const char *args);
	~TimidityPPMIDIDevice();

	int Open(MidiCallback, void *userdata);
	bool Preprocess(MIDIStreamer *song, bool looping);
	bool IsOpen() const;
	int Resume();

	void Stop();
	bool IsOpen();
	void TimidityVolumeChanged();
	int GetDeviceType() const override { return MDEV_TIMIDITY; }

protected:
	bool LaunchTimidity();

	char* DiskName;
#ifdef _WIN32
	HANDLE ReadWavePipe;
	HANDLE WriteWavePipe;
	HANDLE ChildProcess;
	FString CommandLine;
	bool Validated;
	bool ValidateTimidity();
#else // _WIN32
	int WavePipe[2];
	pid_t ChildProcess;
#endif
	FString ExeName;
	bool Looping;

	static bool FillStream(SoundStream *stream, void *buff, int len, void *userdata);
#ifdef _WIN32
	static const char EventName[];
#endif
};



#ifdef _WIN32


BOOL SafeTerminateProcess(HANDLE hProcess, UINT uExitCode);

static char TimidityTitle[] = "TiMidity (" GAMENAME " Launched)";
const char TimidityPPMIDIDevice::EventName[] = "TiMidity Killer";

CUSTOM_CVAR (String, timidity_exe, "timidity.exe", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
#else
CUSTOM_CVAR(String, timidity_exe, "timidity", CVAR_ARCHIVE | CVAR_GLOBALCONFIG)
#endif
{
	if (currSong != nullptr && currSong->GetDeviceType() == MDEV_TIMIDITY)
	{
		MIDIDeviceChanged(-1, true);
	}
}

CVAR (String, timidity_extargs, "", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)	// extra args to pass to Timidity
CVAR (String, timidity_config, "", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
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
	if (self < 20)
	{ // Don't allow pipes less than 20ms
		self = 20;
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
	: DiskName(nullptr),
#ifdef _WIN32
	  ReadWavePipe(INVALID_HANDLE_VALUE), WriteWavePipe(INVALID_HANDLE_VALUE),
	  ChildProcess(INVALID_HANDLE_VALUE),
	  Validated(false),
#else
	  ChildProcess(-1),
#endif
	  Looping(false)
{
#ifndef _WIN32
	WavePipe[0] = WavePipe[1] = -1;
#endif

	if (args == NULL || *args == 0) args = timidity_exe;
	ExeName = args;

#ifdef _WIN32
	CommandLine.Format("%s %s -EFchorus=%s -EFreverb=%s -s%d ",
		args, *timidity_extargs,
		*timidity_chorus, *timidity_reverb, *timidity_frequency);
	if (**timidity_config != '\0')
	{
		CommandLine += "-c \"";
		CommandLine += timidity_config;
		CommandLine += "\" ";
	}
#endif
}

//==========================================================================
//
// TimidityPPMIDIDevice Destructor
//
//==========================================================================

TimidityPPMIDIDevice::~TimidityPPMIDIDevice ()
{
	if (nullptr != DiskName)
	{
		remove(DiskName);
		free(DiskName);
	}

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
	TArray<uint8_t> midi;
	bool success;
	FILE *f;

	if (ExeName.IsEmpty())
	{
		return false;
	}

	Looping = looping;

	// Write MIDI song to temporary file
	song->CreateSMF(midi, looping ? 0 : 1);

	f = tmpfileplus(nullptr, "zmid", &DiskName, 1);
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

#ifdef _WIN32
	CommandLine.AppendFormat("-o - -Ors%c%c%c -id%c %s",
		timidity_stereo ? 'S' : 'M',
		timidity_8bit ? '8' : '1',
		timidity_byteswap ? 'x' : ' ',
		looping ? 'l' : ' ',
		DiskName);
#endif

	return false;
}

//==========================================================================
//
// TimidityPPMIDIDevice :: Open
//
//==========================================================================

int TimidityPPMIDIDevice::Open(MidiCallback callback, void *userdata)
{
	int pipeSize;

#ifdef _WIN32
	static SECURITY_ATTRIBUTES inheritable = { sizeof(inheritable), NULL, true };

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
			return 1;
		}

		Stream = GSnd->CreateStream(FillStream, pipeSize,
			(timidity_stereo ? 0 : SoundStream::Mono) |
			(timidity_8bit ? SoundStream::Bits8 : 0),
			timidity_frequency, this);
		if (Stream == NULL)
		{
			Printf(PRINT_BOLD, "Could not create music stream.\n");
#ifdef _WIN32
			CloseHandle(WriteWavePipe);
			CloseHandle(ReadWavePipe);
			ReadWavePipe = WriteWavePipe = INVALID_HANDLE_VALUE;
#else
			close(WavePipe[1]);
			close(WavePipe[0]);
			WavePipe[0] = WavePipe[1] = -1;
#endif
			return 1;
		}
	}

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
	const uint8_t *exeBase;
	const uint8_t *exeEnd;
	const uint8_t *exe;
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
	exeBase = (const uint8_t *)MapViewOfFile (mapping, FILE_MAP_READ, 0, 0, 0);
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
			exe = (const uint8_t *)tSpot + 1;
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
	if (ExeName.IsEmpty() || nullptr == DiskName)
	{
		return false;
	}

#ifdef _WIN32
	DPrintf (DMSG_NOTIFY, "cmd: \x1cG%s\n", CommandLine.GetChars());

	STARTUPINFO startup = { sizeof(startup), };
	PROCESS_INFORMATION procInfo;

	startup.dwFlags = STARTF_USESTDHANDLES | STARTF_USESHOWWINDOW;

	startup.hStdInput = INVALID_HANDLE_VALUE;
	startup.hStdOutput = WriteWavePipe != INVALID_HANDLE_VALUE ?
						  WriteWavePipe : GetStdHandle (STD_OUTPUT_HANDLE);
	startup.hStdError = GetStdHandle (STD_ERROR_HANDLE);

	startup.lpTitle = TimidityTitle;
	startup.wShowWindow = SW_SHOWMINNOACTIVE;

	if (CreateProcess(NULL, CommandLine.LockBuffer(), NULL, NULL, true,
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
#ifndef __OpenBSD__
	wordexp_t words;
#endif
	glob_t glb;

	// Get timidity executable path
	const char *exename = "timidity"; // Fallback default
	glob(ExeName.GetChars(), 0, NULL, &glb);
	if(glb.gl_pathc != 0)
		exename = glb.gl_pathv[0];
#ifndef __OpenBSD__
	// Get user-defined extra args
	wordexp(timidity_extargs, &words, WRDE_NOCMD);
#endif

	std::string chorusarg = std::string("-EFchorus=") + *timidity_chorus;
	std::string reverbarg = std::string("-EFreverb=") + *timidity_reverb;
	std::string sratearg = std::string("-s") + std::to_string(*timidity_frequency);
	std::string outfilearg = "-o"; // An extra "-" is added later
	std::string outmodearg = "-Or";
	outmodearg += timidity_8bit ? "u8" : "s1";
	outmodearg += timidity_stereo ? "S" : "M";
	if(timidity_byteswap) outmodearg += "x";
	std::string ifacearg = "-id";
	if(Looping) ifacearg += "l";

	std::vector<const char*> arglist;
	arglist.push_back(exename);
#ifndef __OpenBSD__
	for(size_t i = 0;i < words.we_wordc;i++)
		arglist.push_back(words.we_wordv[i]);
#endif
	if(**timidity_config != '\0')
	{
		arglist.push_back("-c");
		arglist.push_back(timidity_config);
	}
	arglist.push_back(chorusarg.c_str());
	arglist.push_back(reverbarg.c_str());
	arglist.push_back(sratearg.c_str());
	arglist.push_back(outfilearg.c_str());
	arglist.push_back("-");
	arglist.push_back(outmodearg.c_str());
	arglist.push_back(ifacearg.c_str());
	arglist.push_back(DiskName);

	DPrintf(DMSG_NOTIFY, "Timidity EXE: \x1cG%s\n", exename);
	int i = 1;
	std::for_each(arglist.begin()+1, arglist.end(),
		[&i](const char *arg)
		{ DPrintf(DMSG_NOTIFY, "arg %d: \x1cG%s\n", i++, arg); }
	);
	arglist.push_back(nullptr);

	forkres = fork ();
	if (forkres == 0)
	{
		close (WavePipe[0]);
		dup2 (WavePipe[1], STDOUT_FILENO);
		freopen ("/dev/null", "r", stdin);
//		freopen ("/dev/null", "w", stderr);
		close (WavePipe[1]);

		execvp (exename, const_cast<char*const*>(arglist.data()));
		fprintf(stderr,"execvp failed: %s\n", strerror(errno));
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

#ifndef __OpenBSD__
	wordfree(&words);
#endif
	globfree (&glb);
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
			ReadFile(song->ReadWavePipe, (uint8_t *)buff+didget, len-didget, &got, NULL);
			didget += got;
			if (didget >= (DWORD)len)
				break;

			// Give TiMidity++ a chance to output something more to the pipe
			Sleep (10);
			if (!PeekNamedPipe(song->ReadWavePipe, NULL, 0, NULL, &avail, NULL) || avail == 0)
			{
				memset ((uint8_t *)buff+didget, 0, len-didget);
				break;
			}
		}
	}
#else
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

	ssize_t got = 0;
	do {
		ssize_t r = read(song->WavePipe[0], (uint8_t*)buff+got, len-got);
		if(r < 0)
		{
			if(errno == EWOULDBLOCK || errno == EAGAIN)
			{
				FD_ZERO(&rfds);
				FD_SET(song->WavePipe[0], &rfds);
				tv.tv_sec = 0;
				tv.tv_usec = 50;
				select(1, &rfds, NULL, NULL, &tv);
				continue;
			}
			break;
		}
		got += r;
	} while(got < len);
	if(got < len)
		memset((uint8_t*)buff+got, 0, len-got);
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
			if (Stream != NULL && Stream->Play(true, timidity_mastervolume))
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


MIDIDevice *CreateTimidityPPMIDIDevice(const char *args)
{
	return new TimidityPPMIDIDevice(args);
}
