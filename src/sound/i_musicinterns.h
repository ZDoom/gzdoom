#define WIN32_LEAN_AND_MEAN
#if defined(_WIN32_WINNT) && _WIN32_WINNT < 0x0400
#undef _WIN32_WINNT
#endif
#ifndef _WIN32_WINNT
#define _WIN32_WINNT 0x0400
#endif
#include <windows.h>
#include <mmsystem.h>
#include <fmod.h>
#include "tempfiles.h"
#include "oplsynth/opl_mus_player.h"
#include "c_cvars.h"
#include "mus2midi.h"
#include "files.h"

void I_InitMusicWin32 ();
void I_ShutdownMusicWin32 ();

// The base music class. Everything is derived from this --------------------

class MusInfo
{
public:
	MusInfo () : m_Status(STATE_Stopped) {}
	virtual ~MusInfo ();
	virtual void SetVolume (float volume) = 0;
	virtual void Play (bool looping) = 0;
	virtual void Pause () = 0;
	virtual void Resume () = 0;
	virtual void Stop () = 0;
	virtual bool IsPlaying () = 0;
	virtual bool IsMIDI () const = 0;
	virtual bool IsValid () const = 0;
	virtual bool SetPosition (int order);

	enum EState
	{
		STATE_Stopped,
		STATE_Playing,
		STATE_Paused
	} m_Status;
	bool m_Looping;
};

// MUS file played with MIDI output messages --------------------------------

class MUSSong2 : public MusInfo
{
public:
	MUSSong2 (FileReader *file);
	~MUSSong2 ();

	void SetVolume (float volume);
	void Play (bool looping);
	void Pause ();
	void Resume ();
	void Stop ();
	bool IsPlaying ();
	bool IsMIDI () const;
	bool IsValid () const;

protected:
	static DWORD WINAPI PlayerProc (LPVOID lpParameter);
	void OutputVolume (DWORD volume);
	int SendCommand ();

	enum
	{
		SEND_DONE,
		SEND_WAIT
	};

	HMIDIOUT MidiOut;
	HANDLE PlayerThread;
	HANDLE PauseEvent;
	HANDLE ExitEvent;
	HANDLE VolumeChangeEvent;
	DWORD SavedVolume;
	bool VolumeWorks;

	BYTE *MusBuffer;
	MUSHeader *MusHeader;
	BYTE LastVelocity[16];
	BYTE ChannelVolumes[16];
	size_t MusP, MaxMusP;
};

// MIDI file played with MIDI output messages -------------------------------

class MIDISong2 : public MusInfo
{
public:
	MIDISong2 (FileReader *file);
	~MIDISong2 ();

	void SetVolume (float volume);
	void Play (bool looping);
	void Pause ();
	void Resume ();
	void Stop ();
	bool IsPlaying ();
	bool IsMIDI () const;
	bool IsValid () const;

protected:
	struct TrackInfo;

	static DWORD WINAPI PlayerProc (LPVOID lpParameter);
	void OutputVolume (DWORD volume);
	void ProcessInitialMetaEvents ();
	DWORD SendCommands ();
	void SendCommand (TrackInfo *track);
	TrackInfo *FindNextDue ();

	HMIDIOUT MidiOut;
	HANDLE PlayerThread;
	HANDLE PauseEvent;
	HANDLE ExitEvent;
	HANDLE VolumeChangeEvent;
	DWORD SavedVolume;
	bool VolumeWorks;

	BYTE *MusHeader;
	BYTE ChannelVolumes[16];
	TrackInfo *Tracks;
	TrackInfo *TrackDue;
	int NumTracks;
	int Format;
	int Division;
	int Tempo;
	WORD DesignationMask;
};

// MOD file played with FMOD ------------------------------------------------

class MODSong : public MusInfo
{
public:
	MODSong (FileReader *file);
	~MODSong ();
	void SetVolume (float volume);
	void Play (bool looping);
	void Pause ();
	void Resume ();
	void Stop ();
	bool IsPlaying ();
	bool IsMIDI () const { return false; }
	bool IsValid () const { return m_Module != NULL; }
	bool SetPosition (int order);

protected:
	MODSong () {}

	FMUSIC_MODULE *m_Module;
};

// OGG/MP3/WAV/other format streamed through FMOD ---------------------------

class StreamSong : public MusInfo
{
public:
	StreamSong (FileReader *file);
	~StreamSong ();
	void SetVolume (float volume);
	void Play (bool looping);
	void Pause ();
	void Resume ();
	void Stop ();
	bool IsPlaying ();
	bool IsMIDI () const { return false; }
	bool IsValid () const { return m_Stream != NULL; }

protected:
	StreamSong () : m_Stream(NULL), m_Channel(-1), m_File(NULL) {}

	FSOUND_STREAM *m_Stream;
	int m_Channel;
	int m_LastPos;

	FileReader *m_File;
};

// SPC file, rendered with SNESAPU.DLL and streamed through FMOD ------------

typedef void (__stdcall *SNESAPUInfo_TYPE) (DWORD*, DWORD*, DWORD*);
typedef void (__stdcall *GetAPUData_TYPE) (void**, BYTE**, BYTE**, DWORD**, void**, void**, DWORD**, DWORD**);
typedef void (__stdcall *ResetAPU_TYPE) (DWORD);
typedef void (__stdcall *SetDSPAmp_TYPE) (DWORD);
typedef void (__stdcall *FixAPU_TYPE) (WORD, BYTE, BYTE, BYTE, BYTE, BYTE);
typedef void (__stdcall *SetAPUOpt_TYPE) (DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
typedef void *(__stdcall *EmuAPU_TYPE) (void *, DWORD, BYTE);

class SPCSong : public StreamSong
{
public:
	SPCSong (FileReader *file);
	~SPCSong ();
	void Play (bool looping);
	bool IsPlaying ();
	bool IsValid () const;

protected:
	bool LoadEmu ();
	void CloseEmu ();

	static signed char F_CALLBACKAPI FillStream (FSOUND_STREAM *stream, void *buff, int len, int param);

	HINSTANCE HandleAPU;
	int APUVersion;

	SNESAPUInfo_TYPE SNESAPUInfo;
	GetAPUData_TYPE GetAPUData;
	ResetAPU_TYPE ResetAPU;
	SetDSPAmp_TYPE SetDSPAmp;
	FixAPU_TYPE FixAPU;
	SetAPUOpt_TYPE SetAPUOpt;
	EmuAPU_TYPE EmuAPU;
};

// MIDI file played with Timidity and possibly streamed through FMOD --------

class TimiditySong : public StreamSong
{
public:
	TimiditySong (FileReader *file);
	~TimiditySong ();
	void Play (bool looping);
	void Stop ();
	bool IsPlaying ();
	bool IsValid () const { return CommandLine != NULL; }

protected:
	void PrepTimidity ();
	bool LaunchTimidity ();

	FTempFileName DiskName;
#ifdef _WIN32
	HANDLE ReadWavePipe;
	HANDLE WriteWavePipe;
	HANDLE KillerEvent;
	HANDLE ChildProcess;
	bool Validated;
	bool ValidateTimidity ();
#else // _WIN32
	int WavePipe[2];
	pid_t ChildProcess;
#endif
	char *CommandLine;
	int LoopPos;

	static signed char F_CALLBACKAPI FillStream (FSOUND_STREAM *stream, void *buff, int len, int param);
#ifdef _WIN32
	static const char EventName[];
#endif
};

// MUS file played by a software OPL2 synth and streamed through FMOD -------

class OPLMUSSong : public StreamSong
{
public:
	OPLMUSSong (FileReader *file);
	~OPLMUSSong ();
	void Play (bool looping);
	bool IsPlaying ();
	bool IsValid () const;
	void ResetChips ();

protected:
	static signed char F_CALLBACKAPI FillStream (FSOUND_STREAM *stream, void *buff, int len, int param);

	OPLmusicBlock *Music;
};

// FLAC file streamed through FMOD (You should probably use Vorbis instead) -

class FLACSong : public StreamSong
{
public:
	FLACSong (FileReader *file);
	~FLACSong ();
	void Play (bool looping);
	bool IsPlaying ();
	bool IsValid () const;

protected:
	static signed char F_CALLBACKAPI FillStream (FSOUND_STREAM *stream, void *buff, int len, int param);

	class FLACStreamer;

	FLACStreamer *State;
};

// CD track/disk played through the multimedia system -----------------------

class CDSong : public MusInfo
{
public:
	CDSong (int track, int id);
	~CDSong ();
	void SetVolume (float volume) {}
	void Play (bool looping);
	void Pause ();
	void Resume ();
	void Stop ();
	bool IsPlaying ();
	bool IsMIDI () const { return false; }
	bool IsValid () const { return m_Inited; }

protected:
	CDSong () : m_Inited(false) {}

	int m_Track;
	bool m_Inited;
};

// CD track on a specific disk played through the multimedia system ---------

class CDDAFile : public CDSong
{
public:
	CDDAFile (FileReader *file);
};

// --------------------------------------------------------------------------

extern MusInfo *currSong;
extern int		nomusic;

EXTERN_CVAR (Float, snd_musicvolume)
EXTERN_CVAR (Bool, opl_enable)
