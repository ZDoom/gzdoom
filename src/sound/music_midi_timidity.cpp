#include "i_musicinterns.h"
#include "c_cvars.h"
#include "cmdlib.h"
#include "templates.h"

#if !defined(_WIN32) && 0
// Under Linux, buffer output from Timidity to try to avoid "bubbles"
// in the sound output.
class FPipeBuffer
{
public:
	FPipeBuffer::FPipeBuffer (int fragSize, int nFrags, int pipe);
	~FPipeBuffer ();

	int ReadFrag (BYTE *Buf);

private:
	int PipeHandle;
	int FragSize;
	int BuffSize;
	int WritePos, ReadPos;
	BYTE *Buffer;
	bool GotFull;
	SDL_mutex *BufferMutex;
	SDL_thread *Reader;

	static int ThreadProc (void *data);
};
#endif

#ifndef _WIN32
#include <unistd.h>

#include <sys/types.h>
#include <sys/wait.h>
#include <wordexp.h>

int ChildQuit;

void ChildSigHandler (int signum)
{
	ChildQuit = waitpid (-1, NULL, WNOHANG);
}
#endif

#ifdef _WIN32
const char TimiditySong::EventName[] = "TiMidity Killer";
static char TimidityTitle[] = "TiMidity (ZDoom Launched)";

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
	else if (currSong != NULL && !currSong->IsMIDI ())
		currSong->SetVolume (clamp<float> (snd_musicvolume, 0.f, 1.f));
}


CUSTOM_CVAR (Int, timidity_pipe, 60, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{ // pipe size in ms
	if (timidity_pipe < 0)
	{ // a negative size makes no sense
		timidity_pipe = 0;
	}
}

CUSTOM_CVAR (Int, timidity_frequency, 22050, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{ // Clamp frequency to Timidity's limits
	if (self < 4000)
		self = 4000;
	else if (self > 65000)
		self = 65000;
}

void TimiditySong::Play (bool looping)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;

	if (LaunchTimidity ())
	{
		if (m_Stream != NULL)
		{
			if (m_Stream->Play (true, timidity_mastervolume * snd_musicvolume))
			{
				m_Status = STATE_Playing;
			}
		}
		else
		{ // Assume success if not mixing with FMOD
			m_Status = STATE_Playing;
		}
	}
}

void TimiditySong::Stop ()
{
	if (m_Status != STATE_Stopped)
	{
		if (m_Stream != NULL)
		{
			m_Stream->Stop ();
		}
#ifdef _WIN32
		if (ChildProcess != INVALID_HANDLE_VALUE)
		{
			SetEvent (KillerEvent);
			if (WaitForSingleObject (ChildProcess, 500) != WAIT_OBJECT_0)
			{
				TerminateProcess (ChildProcess, 666);
			}
			CloseHandle (ChildProcess);
			ChildProcess = INVALID_HANDLE_VALUE;
		}
#else
		if (ChildProcess != -1)
		{
			if (kill (ChildProcess, SIGTERM) != 0)
			{
				kill (ChildProcess, SIGKILL);
			}
			waitpid (ChildProcess, NULL, 0);
			ChildProcess = -1;
		}
#endif
	}
	m_Status = STATE_Stopped;
}

TimiditySong::~TimiditySong ()
{
	Stop ();
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
	if (KillerEvent != INVALID_HANDLE_VALUE)
	{
		CloseHandle (KillerEvent);
		KillerEvent = INVALID_HANDLE_VALUE;
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

TimiditySong::TimiditySong (FILE *file, char * musiccache, int len)
	: DiskName ("zmid"),
#ifdef _WIN32
	  ReadWavePipe (INVALID_HANDLE_VALUE), WriteWavePipe (INVALID_HANDLE_VALUE),
	  KillerEvent (INVALID_HANDLE_VALUE),
	  ChildProcess (INVALID_HANDLE_VALUE),
	  Validated (false)
#else
	  ChildProcess (-1)
#endif
{
	bool success;
	FILE *f;

#ifndef _WIN32
	WavePipe[0] = WavePipe[1] = -1;
#endif
	
	if (DiskName == NULL)
	{
		Printf (PRINT_BOLD, "Could not create temp music file\n");
		return;
	}

	f = fopen (DiskName, "wb");
	if (f == NULL)
	{
		Printf (PRINT_BOLD, "Could not open temp music file\n");
		return;
	}

	BYTE *buf;
	
	if (file!=NULL) 
	{
		buf = new BYTE[len];
		fread (buf, 1, len, file);
	}
	else buf = (BYTE*)musiccache;


	// The file type has already been checked before this class instance was
	// created, so we only need to check one character to determine if this
	// is a MUS or MIDI file and write it to disk as appropriate.
	if (buf[1] == 'T')
	{
		success = (fwrite (buf, 1, len, f) == (size_t)len);
	}
	else
	{
		success = ProduceMIDI (buf, f);
	}
	fclose (f);
	if (file!=NULL) 
	{
		delete[] buf;
	}

	if (success)
	{
		PrepTimidity ();
	}
	else
	{
		Printf (PRINT_BOLD, "Could not write temp music file\n");
	}
}

void TimiditySong::PrepTimidity ()
{
	int pipeSize;
#ifdef _WIN32
	static SECURITY_ATTRIBUTES inheritable = { sizeof(inheritable), NULL, TRUE };
	
	if (!Validated && !ValidateTimidity ())
	{
		return;
	}

	Validated = true;
	KillerEvent = CreateEvent (NULL, FALSE, FALSE, EventName);
	if (KillerEvent == INVALID_HANDLE_VALUE)
	{
		Printf (PRINT_BOLD, "Could not create TiMidity++ kill event.\n");
		return;
	}
#endif // WIN32

	CommandLine.Format ("%s %s -EFchorus=%s -EFreverb=%s -s%d ",
		*timidity_exe, *timidity_extargs,
		*timidity_chorus, *timidity_reverb, *timidity_frequency);

	pipeSize = (timidity_pipe * timidity_frequency / 1000)
		<< (timidity_stereo + !timidity_8bit);

	if (pipeSize != 0)
	{
#ifdef _WIN32
		// Round pipe size up to nearest power of 2 to try and avoid partial
		// buffer reads in FillStream() under NT. This does not seem to be an
		// issue under 9x.
		int bitmask = pipeSize & -pipeSize;
		
		while (bitmask < pipeSize)
			bitmask <<= 1;
		pipeSize = bitmask;

		if (!CreatePipe (&ReadWavePipe, &WriteWavePipe, &inheritable, pipeSize))
#else // WIN32
		if (pipe (WavePipe) == -1)
#endif
		{
			Printf (PRINT_BOLD, "Could not create a data pipe for TiMidity++.\n");
			pipeSize = 0;
		}
		else
		{
			m_Stream = GSnd->CreateStream (FillStream, pipeSize,
				(timidity_stereo ? 0 : SoundStream::Mono) |
				(timidity_8bit ? SoundStream::Bits8 : 0),
				timidity_frequency, this);
			if (m_Stream == NULL)
			{
				Printf (PRINT_BOLD, "Could not create music stream.\n");
				pipeSize = 0;
#ifdef _WIN32
				CloseHandle (WriteWavePipe);
				CloseHandle (ReadWavePipe);
				ReadWavePipe = WriteWavePipe = INVALID_HANDLE_VALUE;
#else
				close (WavePipe[1]);
				close (WavePipe[0]);
				WavePipe[0] = WavePipe[1] = -1;
#endif
			}
		}
		
		if (pipeSize == 0)
		{
			Printf (PRINT_BOLD, "If your soundcard cannot play more than one\n"
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
}

#ifdef _WIN32
// Check that this TiMidity++ knows about the TiMidity Killer event.
// If not, then we can't use it, because Win32 provides no other way
// to conveniently signal it to quit. The check is done by simply
// searching for the event's name somewhere in the executable.
bool TimiditySong::ValidateTimidity ()
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
		Printf (PRINT_BOLD, "Please set the timidity_exe cvar to the location of TiMidity++\n");
		return false;
	}
	if (pathLen > MAX_PATH)
	{
		Printf (PRINT_BOLD, "The path to TiMidity++ is too long\n");
		return false;
	}

	diskFile = CreateFile (foundPath, GENERIC_READ, FILE_SHARE_READ, NULL,
		OPEN_EXISTING, FILE_FLAG_SEQUENTIAL_SCAN, NULL);
	if (diskFile == INVALID_HANDLE_VALUE)
	{
		Printf (PRINT_BOLD, "Could not access %s\n", foundPath);
		return false;
	}
	fileLen = GetFileSize (diskFile, NULL);
	mapping = CreateFileMapping (diskFile, NULL, PAGE_READONLY, 0, 0, NULL);
	if (mapping == NULL)
	{
		Printf (PRINT_BOLD, "Could not create mapping for %s\n", foundPath);
		CloseHandle (diskFile);
		return false;
	}
	exeBase = (const BYTE *)MapViewOfFile (mapping, FILE_MAP_READ, 0, 0, 0);
	if (exeBase == NULL)
	{
		Printf (PRINT_BOLD, "Could not map %s\n", foundPath);
		CloseHandle (mapping);
		CloseHandle (diskFile);
		return false;
	}

	good = false;
	try
	{
		for (exe = exeBase, exeEnd = exeBase+fileLen; exe < exeEnd; )
		{
			const char *tSpot = (const char *)memchr (exe, 'T', exeEnd - exe);
			if (tSpot == NULL)
			{
				break;
			}
			if (memcmp (tSpot+1, EventName+1, sizeof(EventName)-1) == 0)
			{
				good = true;
				break;
			}
			exe = (const BYTE *)tSpot + 1;
		}
	}
	catch (...)
	{
		Printf (PRINT_BOLD, "Error reading %s\n", foundPath);
	}
	if (!good)
	{
		Printf (PRINT_BOLD, "ZDoom requires a special version of TiMidity++\n");
	}

	UnmapViewOfFile ((LPVOID)exeBase);
	CloseHandle (mapping);
	CloseHandle (diskFile);

	return good;
}
#endif // _WIN32

bool TimiditySong::LaunchTimidity ()
{
	if (CommandLine.IsEmpty())
	{
		return false;
	}

	// Tell Timidity whether it should loop or not
	char *cmdline = CommandLine.LockBuffer();
	cmdline[LoopPos] = m_Looping ? 'l' : ' ';
	DPrintf ("cmd: \x1cG%s\n", cmdline);

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

	if (CreateProcess (NULL, cmdline, NULL, NULL, TRUE,
		/*HIGH_PRIORITY_CLASS|*/DETACHED_PROCESS, NULL, NULL, &startup, &procInfo))
	{
		ChildProcess = procInfo.hProcess;
		//SetThreadPriority (procInfo.hThread, THREAD_PRIORITY_HIGHEST);
		CloseHandle (procInfo.hThread);		// Don't care about the created thread
		CommandLine.UnlockBuffer();
		return true;
	}
	CommandLine.UnlockBuffer();

	char hres[9];
	LPTSTR msgBuf;
	HRESULT err = GetLastError ();

	if (!FormatMessage (FORMAT_MESSAGE_ALLOCATE_BUFFER |
						FORMAT_MESSAGE_FROM_SYSTEM |
						FORMAT_MESSAGE_IGNORE_INSERTS,
						NULL, err, 0, (LPTSTR)&msgBuf, 0, NULL))
	{
		sprintf (hres, "%08lx", err);
		msgBuf = hres;
	}

	Printf (PRINT_BOLD, "Could not run timidity with the command line:\n%s\n"
						"Reason: %s\n", CommandLine.GetChars(), msgBuf);
	if (msgBuf != hres)
	{
		LocalFree (msgBuf);
	}
	return false;
#else
	if (WavePipe[0] != -1 && WavePipe[1] == -1 && m_Stream != NULL)
	{
		// Timidity was previously launched, so the write end of the pipe
		// is closed, and the read end is still open. Close the pipe
		// completely and reopen it.
		
		close (WavePipe[0]);
		WavePipe[0] = -1;
		delete m_Stream;
		m_Stream = NULL;
		PrepTimidity ();
	}
	
	int forkres;
	wordexp_t words;

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
		freopen ("/dev/null", "w", stderr);
		close (WavePipe[1]);

		printf ("exec %s\n", words.we_wordv[0]);
		execvp (words.we_wordv[0], words.we_wordv);
		exit (0);	// if execvp succeeds, we never get here
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
	}
	
	wordfree (&words);
	return ChildProcess != -1;
#endif // _WIN32
}

bool TimiditySong::FillStream (SoundStream *stream, void *buff, int len, void *userdata)
{
	TimiditySong *song = (TimiditySong *)userdata;
	
#ifdef _WIN32
	DWORD avail, got, didget;

	if (!PeekNamedPipe (song->ReadWavePipe, NULL, 0, NULL, &avail, NULL) || avail == 0)
	{ // If nothing is available from the pipe, play silence.
		memset (buff, 0, len);
	}
	else
	{
		didget = 0;
		for (;;)
		{
			ReadFile (song->ReadWavePipe, (BYTE *)buff+didget, len-didget, &got, NULL);
			didget += got;
			if (didget >= (DWORD)len)
				break;

			// Give TiMidity a chance to output something more to the pipe
			Sleep (10);
			if (!PeekNamedPipe (song->ReadWavePipe, NULL, 0, NULL, &avail, NULL) || avail == 0)
			{
				memset ((BYTE *)buff+didget, 0, len-didget);
				break;
			}
		} 
	}
#else
	ssize_t got;

	got = read (song->WavePipe[0], (BYTE *)buff, len);
	if (got < len)
	{
		memset ((BYTE *)buff+got, 0, len-got);
	}

	if (ChildQuit == song->ChildProcess)
	{
		ChildQuit = 0;
//		printf ("child gone\n");
		song->ChildProcess = -1;
		return false;
	}
#endif
	return true;
}

void TimiditySong::SetVolume (float volume)
{
	if (m_Stream!=NULL) m_Stream->SetVolume (volume*timidity_mastervolume);
}

bool TimiditySong::IsPlaying ()
{
#ifdef _WIN32
	if (ChildProcess != INVALID_HANDLE_VALUE)
	{
		if (WaitForSingleObject (ChildProcess, 0) != WAIT_TIMEOUT)
		{ // Timidity has quit
			CloseHandle (ChildProcess);
			ChildProcess = INVALID_HANDLE_VALUE;
#else
	if (ChildProcess != -1)
	{
		if (waitpid (ChildProcess, NULL, WNOHANG) == ChildProcess)
		{
			ChildProcess = -1;
#endif
			if (m_Looping)
			{
				if (!LaunchTimidity ())
				{
					Stop ();
					return false;
				}
				return true;
			}
			return false;
		}
		return true;
	}
	return false;
}

#if !defined(_WIN32) && 0
FPipeBuffer::FPipeBuffer (int fragSize, int nFrags, int pipe)
	: PipeHandle (pipe),
	  FragSize (fragSize),
	  BuffSize (fragSize * nFrags),
	  WritePos (0), ReadPos (0), GotFull (false),
	  Reader (0), PleaseExit (0)
{
	Buffer = new BYTE[BuffSize];
	if (Buffer != NULL)
	{
		BufferMutex = SDL_CreateMutex ();
		if (BufferMutex == NULL)
		{
			Reader = SDL_CreateThread (ThreadProc, (void *)this);
		}
	}
}

FPipeBuffer::~FPipeBuffer ()
{
	if (Reader != NULL)
	{ // I like the Win32 IPC facilities better than SDL's
	  // Fortunately, this is a simple thread, so I can cheat
	  // like this.
		SDL_KillThread (ThreadProc);
	}
	if (BufferMutex != NULL)
	{
		SDL_DestroyMutex (BufferMutex);
	}
	if (Buffer != NULL)
	{
		delete[] Buffer;
	}
}

int FPipeBuffer::ReadFrag (BYTE *buf)
{
	int startavvail;
	int avail;
	int pos;
	int opos;
	
	if (SDL_mutexP (BufferMutex) == -1)
		return 0;
	
	if (WritePos > ReadPos)
	{
		avail = WritePos - ReadPos;
	}
	else
	{
		avail = BuffSize - ReadPos + WritePos;
	}
	if (avail > FragSize)
		avail = FragSize;
	
	startavail = avali;
	pos = ReadPos;
	opos = 0;
	
	while (avail != 0)
	{
		int thistime;
		
		thistime = (pos + avail > BuffSize) ? BuffSize - pos : avail;
		memcpy (buf + opos, Buffer + pos, thistime);
		if (thistime != avail)
		{
			pos = 0;
			avail -= thistime;
		}
		opos += thistime;
	}

	ReadPos = pos;
	
	SDL_mutexV (BufferMutex);
	
	return startavail;
}
#endif	// !_WIN32
