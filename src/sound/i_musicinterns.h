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
#include "mid2strm.h"
#include "tempfiles.h"
#include "oplsynth/opl_mus_player.h"
#include "c_cvars.h"

void I_InitMusicWin32 ();
void I_ShutdownMusicWin32 ();

// The base music class. Everything is derived from this --------------------

class MusInfo
{
public:
	MusInfo () : m_Status(STATE_Stopped), m_LumpMem(0) {}
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
	const void *m_LumpMem;
};

// MIDI file played with a MIDI output stream -------------------------------

class MIDISong : public MusInfo
{
public:
	MIDISong (const void *mem, int len);
	~MIDISong ();
	void SetVolume (float volume);
	void Play (bool looping);
	void Pause ();
	void Resume ();
	void Stop ();
	bool IsPlaying ();
	bool IsMIDI () const;
	bool IsValid () const;
	void MidiProc (UINT uMsg);

protected:
	MIDISong ();
	void MCIError (MMRESULT res, const char *descr);
	void UnprepareHeaders ();
	bool PrepareHeaders ();
	void SubmitBuffer ();
	void AllChannelsOff ();
	void SetStreamVolume ();
	void UnsetStreamVolume ();

	bool m_IsMUS;

	enum
	{
		cb_play,
		cb_die,
		cb_dead
	} m_CallbackStatus;
	HMIDISTRM m_MidiStream;
	PSTREAMBUF m_Buffers;
	PSTREAMBUF m_CurrBuffer;
	bool m_bVolGood, m_bOldVolGood;

	DWORD m_OldVolume, m_LastSetVol;
};

// MUS file played with a MIDI output stream --------------------------------

class MUSSong : public MIDISong
{
public:
	MUSSong (const void *mem, int len);
};

// MUS file played with MIDI output messages --------------------------------

class MUSSong2 : public MusInfo
{
public:
	MUSSong2 (const void *mem, int len);
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

	const BYTE *MusBuffer;
	const MUSHeader *MusHeader;
	BYTE LastVelocity[16];
	BYTE ChannelVolumes[16];
	size_t MusP, MaxMusP;
};

// MIDI file played with MIDI output messages -------------------------------

class MIDISong2 : public MusInfo
{
public:
	MIDISong2 (const void *mem, int len);
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

	const BYTE *MusHeader;
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
	MODSong (const void *mem, int len);
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

// MIDI song played with DirectMusic using FMOD -----------------------------

class DMusSong : public MODSong
{
public:
	DMusSong (const void *mem, int len);

protected:
	FTempFileName DiskName;
};

// OGG/MP3/WAV/other format streamed through FMOD ---------------------------

class StreamSong : public MusInfo
{
public:
	StreamSong (const void *mem, int len);
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
	StreamSong () : m_Stream (NULL), m_Channel (-1) {}

	FSOUND_STREAM *m_Stream;
	int m_Channel;
	int m_LastPos;
};

// SPC file, rendered with SNESAPU.DLL and streamed through FMOD ------------

typedef void (__stdcall *SNESAPUInfo_TYPE) (DWORD&, DWORD&, DWORD&);
typedef void (__stdcall *GetAPUData_TYPE) (void**, BYTE**, BYTE**, DWORD**, void**, void**, DWORD**, DWORD**);
typedef void (__stdcall *ResetAPU_TYPE) (DWORD);
typedef void (__stdcall *FixAPU_TYPE) (WORD, BYTE, BYTE, BYTE, BYTE, BYTE);
typedef void (__stdcall *SetAPUOpt_TYPE) (DWORD, DWORD, DWORD, DWORD, DWORD, DWORD);
typedef void *(__stdcall *EmuAPU_TYPE) (void *, DWORD, BYTE);

class SPCSong : public StreamSong
{
public:
	SPCSong (const void *mem, int len);
	~SPCSong ();
	void Play (bool looping);
	bool IsPlaying ();
	bool IsValid () const;

protected:
	bool LoadEmu ();
	void CloseEmu ();

	static signed char STACK_ARGS FillStream (FSOUND_STREAM *stream, void *buff, int len, int param);

	HINSTANCE HandleAPU;

	SNESAPUInfo_TYPE SNESAPUInfo;
	GetAPUData_TYPE GetAPUData;
	ResetAPU_TYPE ResetAPU;
	FixAPU_TYPE FixAPU;
	SetAPUOpt_TYPE SetAPUOpt;
	EmuAPU_TYPE EmuAPU;
};

// MIDI file played with Timidity and possible streamed through FMOD --------

class TimiditySong : public StreamSong
{
public:
	TimiditySong (const void *mem, int len);
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

	static signed char STACK_ARGS FillStream (FSOUND_STREAM *stream, void *buff, int len, int param);
#ifdef _WIN32
	static const char EventName[];
#endif
};

// MUS file played by a software OPL2 synth and streamed through FMOD -------

class OPLMUSSong : public StreamSong
{
public:
	OPLMUSSong (const void *mem, int len);
	~OPLMUSSong ();
	void Play (bool looping);
	bool IsPlaying ();
	bool IsValid () const;
	void ResetChips ();

protected:
	static signed char STACK_ARGS FillStream (FSOUND_STREAM *stream, void *buff, int len, int param);

	OPLmusicBlock *Music;
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
	CDDAFile (const void *mem, int len);
};

// --------------------------------------------------------------------------

extern MusInfo *currSong;
extern int		nomusic;

EXTERN_CVAR (Float, snd_musicvolume)
EXTERN_CVAR (Bool, opl_enable)
