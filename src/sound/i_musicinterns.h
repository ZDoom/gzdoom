
#include "oplsynth/opl_mus_player.h"
#include "c_cvars.h"
#include "mus2midi.h"
#include "i_sound.h"
#include "i_music.h"
#include "s_sound.h"
#include "files.h"
#include "wildmidi/wildmidi_lib.h"

void I_InitMusicWin32 ();

extern float relative_volume;

EXTERN_CVAR (Float, timidity_mastervolume)


// A device that provides a WinMM-like MIDI streaming interface -------------

struct MidiHeader
{
	uint8_t *lpData;
	uint32_t dwBufferLength;
	uint32_t dwBytesRecorded;
	MidiHeader *lpNext;
};

// These constants must match the corresponding values of the Windows headers
// to avoid readjustment in the native Windows device's playback functions 
// and should not be changed.
enum
{
	MIDIDEV_MIDIPORT = 1,
	MIDIDEV_SYNTH,
	MIDIDEV_SQSYNTH,
	MIDIDEV_FMSYNTH,
	MIDIDEV_MAPPER,
	MIDIDEV_WAVETABLE,
	MIDIDEV_SWSYNTH
};

enum : uint8_t
{
	MEVENT_TEMPO		= 1,
	MEVENT_NOP			= 2,
	MEVENT_LONGMSG		= 128,
};

#define MEVENT_EVENTTYPE(x)	((uint8_t)((x) >> 24))
#define MEVENT_EVENTPARM(x)   ((x) & 0xffffff)

class MIDIStreamer;

typedef void(*MidiCallback)(void *);
class MIDIDevice
{
public:
	MIDIDevice();
	virtual ~MIDIDevice();

	virtual int Open(MidiCallback, void *userdata) = 0;
	virtual void Close() = 0;
	virtual bool IsOpen() const = 0;
	virtual int GetTechnology() const = 0;
	virtual int SetTempo(int tempo) = 0;
	virtual int SetTimeDiv(int timediv) = 0;
	virtual int StreamOut(MidiHeader *data) = 0;
	virtual int StreamOutSync(MidiHeader *data) = 0;
	virtual int Resume() = 0;
	virtual void Stop() = 0;
	virtual int PrepareHeader(MidiHeader *data);
	virtual int UnprepareHeader(MidiHeader *data);
	virtual bool FakeVolume();
	virtual bool Pause(bool paused) = 0;
	virtual void InitPlayback();
	virtual bool Update();
	virtual void PrecacheInstruments(const uint16_t *instruments, int count);
	virtual void TimidityVolumeChanged();
	virtual void FluidSettingInt(const char *setting, int value);
	virtual void FluidSettingNum(const char *setting, double value);
	virtual void FluidSettingStr(const char *setting, const char *value);
	virtual void WildMidiSetOption(int opt, int set);
	virtual bool Preprocess(MIDIStreamer *song, bool looping);
	virtual FString GetStats();
	virtual int GetDeviceType() const { return MDEV_DEFAULT; }
};


#ifdef _WIN32
MIDIDevice *CreateWinMIDIDevice(int mididevice);
#elif defined __APPLE__
MIDIDevice *CreateAudioToolboxMIDIDevice();
#endif
MIDIDevice *CreateTimidityPPMIDIDevice(const char *args);

// Base class for pseudo-MIDI devices ---------------------------------------

class PseudoMIDIDevice : public MIDIDevice
{
public:
	PseudoMIDIDevice();
	~PseudoMIDIDevice();

	void Close();
	bool IsOpen() const;
	int GetTechnology() const;
	bool Pause(bool paused);
	int Resume();
	void Stop();
	int StreamOut(MidiHeader *data);
	int StreamOutSync(MidiHeader *data);
	int SetTempo(int tempo);
	int SetTimeDiv(int timediv);
	FString GetStats();

protected:
	SoundStream *Stream;
	bool Started;
	bool bLooping;
};

// Sound System pseudo-MIDI device ------------------------------------------

class SndSysMIDIDevice : public PseudoMIDIDevice
{
public:
	int Open(MidiCallback, void *userdata);
	bool Preprocess(MIDIStreamer *song, bool looping);
};

// Base class for software synthesizer MIDI output devices ------------------

class SoftSynthMIDIDevice : public MIDIDevice
{
public:
	SoftSynthMIDIDevice();
	~SoftSynthMIDIDevice();

	void Close();
	bool IsOpen() const;
	int GetTechnology() const;
	int SetTempo(int tempo);
	int SetTimeDiv(int timediv);
	int StreamOut(MidiHeader *data);
	int StreamOutSync(MidiHeader *data);
	int Resume();
	void Stop();
	bool Pause(bool paused);

protected:
	FCriticalSection CritSec;
	SoundStream *Stream;
	double Tempo;
	double Division;
	double SamplesPerTick;
	double NextTickIn;
	MidiHeader *Events;
	bool Started;
	uint32_t Position;
	int SampleRate;

	MidiCallback Callback;
	void *CallbackData;

	virtual void CalcTickRate();
	int PlayTick();
	int OpenStream(int chunks, int flags, MidiCallback, void *userdata);
	static bool FillStream(SoundStream *stream, void *buff, int len, void *userdata);
	virtual bool ServiceStream (void *buff, int numbytes);

	virtual void HandleEvent(int status, int parm1, int parm2) = 0;
	virtual void HandleLongEvent(const uint8_t *data, int len) = 0;
	virtual void ComputeOutput(float *buffer, int len) = 0;
};

// OPL implementation of a MIDI output device -------------------------------

class OPLMIDIDevice : public SoftSynthMIDIDevice, protected OPLmusicBlock
{
public:
	OPLMIDIDevice(const char *args);
	int Open(MidiCallback, void *userdata);
	void Close();
	int GetTechnology() const;
	FString GetStats();

protected:
	void CalcTickRate();
	int PlayTick();
	void HandleEvent(int status, int parm1, int parm2);
	void HandleLongEvent(const uint8_t *data, int len);
	void ComputeOutput(float *buffer, int len);
	bool ServiceStream(void *buff, int numbytes);
	int GetDeviceType() const override { return MDEV_OPL; }
};

// OPL dumper implementation of a MIDI output device ------------------------

class OPLDumperMIDIDevice : public OPLMIDIDevice
{
public:
	OPLDumperMIDIDevice(const char *filename);
	~OPLDumperMIDIDevice();
	int Resume();
	void Stop();
};

// Internal TiMidity MIDI device --------------------------------------------

namespace Timidity { struct Renderer; }

class TimidityMIDIDevice : public SoftSynthMIDIDevice
{
public:
	TimidityMIDIDevice(const char *args);
	~TimidityMIDIDevice();

	int Open(MidiCallback, void *userdata);
	void PrecacheInstruments(const uint16_t *instruments, int count);
	FString GetStats();
	int GetDeviceType() const override { return MDEV_GUS; }

protected:
	Timidity::Renderer *Renderer;

	void HandleEvent(int status, int parm1, int parm2);
	void HandleLongEvent(const uint8_t *data, int len);
	void ComputeOutput(float *buffer, int len);
};

// Internal TiMidity disk writing version of a MIDI device ------------------

class TimidityWaveWriterMIDIDevice : public TimidityMIDIDevice
{
public:
	TimidityWaveWriterMIDIDevice(const char *filename, int rate);
	~TimidityWaveWriterMIDIDevice();
	int Resume();
	void Stop();

protected:
	FILE *File;
};

// WildMidi implementation of a MIDI device ---------------------------------

class WildMIDIDevice : public SoftSynthMIDIDevice
{
public:
	WildMIDIDevice(const char *args);
	~WildMIDIDevice();

	int Open(MidiCallback, void *userdata);
	void PrecacheInstruments(const uint16_t *instruments, int count);
	FString GetStats();
	int GetDeviceType() const override { return MDEV_WILDMIDI; }

protected:
	WildMidi_Renderer *Renderer;

	void HandleEvent(int status, int parm1, int parm2);
	void HandleLongEvent(const uint8_t *data, int len);
	void ComputeOutput(float *buffer, int len);
	void WildMidiSetOption(int opt, int set);
};

// FluidSynth implementation of a MIDI device -------------------------------

#ifdef HAVE_FLUIDSYNTH
#ifndef DYN_FLUIDSYNTH
#include <fluidsynth.h>
#else
#include "i_module.h"
extern FModule FluidSynthModule;

struct fluid_settings_t;
struct fluid_synth_t;
#endif

class FluidSynthMIDIDevice : public SoftSynthMIDIDevice
{
public:
	FluidSynthMIDIDevice(const char *args);
	~FluidSynthMIDIDevice();

	int Open(MidiCallback, void *userdata);
	FString GetStats();
	void FluidSettingInt(const char *setting, int value);
	void FluidSettingNum(const char *setting, double value);
	void FluidSettingStr(const char *setting, const char *value);
	int GetDeviceType() const override { return MDEV_FLUIDSYNTH; }

protected:
	void HandleEvent(int status, int parm1, int parm2);
	void HandleLongEvent(const uint8_t *data, int len);
	void ComputeOutput(float *buffer, int len);
	int LoadPatchSets(const char *patches);

	fluid_settings_t *FluidSettings;
	fluid_synth_t *FluidSynth;

#ifdef DYN_FLUIDSYNTH
	enum { FLUID_FAILED = -1, FLUID_OK = 0 };
	static TReqProc<FluidSynthModule, fluid_settings_t *(*)()> new_fluid_settings;
	static TReqProc<FluidSynthModule, fluid_synth_t *(*)(fluid_settings_t *)> new_fluid_synth;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *)> delete_fluid_synth;
	static TReqProc<FluidSynthModule, void (*)(fluid_settings_t *)> delete_fluid_settings;
	static TReqProc<FluidSynthModule, int (*)(fluid_settings_t *, const char *, double)> fluid_settings_setnum;
	static TReqProc<FluidSynthModule, int (*)(fluid_settings_t *, const char *, const char *)> fluid_settings_setstr;
	static TReqProc<FluidSynthModule, int (*)(fluid_settings_t *, const char *, int)> fluid_settings_setint;
	static TReqProc<FluidSynthModule, int (*)(fluid_settings_t *, const char *, char **)> fluid_settings_getstr;
	static TReqProc<FluidSynthModule, int (*)(fluid_settings_t *, const char *, int *)> fluid_settings_getint;
	static TReqProc<FluidSynthModule, void (*)(fluid_synth_t *, int)> fluid_synth_set_reverb_on;
	static TReqProc<FluidSynthModule, void (*)(fluid_synth_t *, int)> fluid_synth_set_chorus_on;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, int, int)> fluid_synth_set_interp_method;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, int)> fluid_synth_set_polyphony;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *)> fluid_synth_get_polyphony;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *)> fluid_synth_get_active_voice_count;
	static TReqProc<FluidSynthModule, double (*)(fluid_synth_t *)> fluid_synth_get_cpu_load;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *)> fluid_synth_system_reset;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, int, int, int)> fluid_synth_noteon;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, int, int)> fluid_synth_noteoff;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, int, int, int)> fluid_synth_cc;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, int, int)> fluid_synth_program_change;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, int, int)> fluid_synth_channel_pressure;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, int, int)> fluid_synth_pitch_bend;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, int, void *, int, int, void *, int, int)> fluid_synth_write_float;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, const char *, int)> fluid_synth_sfload;
	static TReqProc<FluidSynthModule, void (*)(fluid_synth_t *, double, double, double, double)> fluid_synth_set_reverb;
	static TReqProc<FluidSynthModule, void (*)(fluid_synth_t *, int, double, double, double, int)> fluid_synth_set_chorus;
	static TReqProc<FluidSynthModule, int (*)(fluid_synth_t *, const char *, int, char *, int *, int *, int)> fluid_synth_sysex;

	bool LoadFluidSynth();
	void UnloadFluidSynth();
#endif
};
#endif

// Base class for streaming MUS and MIDI files ------------------------------

class MIDIStreamer : public MusInfo
{
public:
	MIDIStreamer(EMidiDevice type, const char *args);
	~MIDIStreamer();

	void MusicVolumeChanged();
	void TimidityVolumeChanged();
	void Play(bool looping, int subsong);
	void Pause();
	void Resume();
	void Stop();
	bool IsPlaying();
	bool IsMIDI() const;
	bool IsValid() const;
	bool SetSubsong(int subsong);
	void Update();
	FString GetStats();
	void FluidSettingInt(const char *setting, int value);
	void FluidSettingNum(const char *setting, double value);
	void FluidSettingStr(const char *setting, const char *value);
	void WildMidiSetOption(int opt, int set);
	void CreateSMF(TArray<uint8_t> &file, int looplimit=0);
	int ServiceEvent();
	int GetDeviceType() const override
	{
		return nullptr == MIDI
			? MusInfo::GetDeviceType()
			: MIDI->GetDeviceType();
	}

protected:
	MIDIStreamer(const char *dumpname, EMidiDevice type);

	void OutputVolume (uint32_t volume);
	int FillBuffer(int buffer_num, int max_events, uint32_t max_time);
	int FillStopBuffer(int buffer_num);
	uint32_t *WriteStopNotes(uint32_t *events);
	int VolumeControllerChange(int channel, int volume);
	int ClampLoopCount(int loopcount);
	void SetTempo(int new_tempo);
	static EMidiDevice SelectMIDIDevice(EMidiDevice devtype);
	MIDIDevice *CreateMIDIDevice(EMidiDevice devtype);

	static void Callback(void *userdata);

	// Virtuals for subclasses to override
	virtual void StartPlayback();
	virtual void CheckCaps(int tech);
	virtual void DoInitialSetup() = 0;
	virtual void DoRestart() = 0;
	virtual bool CheckDone() = 0;
	virtual void Precache();
	virtual bool SetMIDISubsong(int subsong);
	virtual uint32_t *MakeEvents(uint32_t *events, uint32_t *max_event_p, uint32_t max_time) = 0;

	enum
	{
		MAX_EVENTS = 128
	};

	enum
	{
		SONG_MORE,
		SONG_DONE,
		SONG_ERROR
	};

	MIDIDevice *MIDI;
	uint32_t Events[2][MAX_EVENTS*3];
	MidiHeader Buffer[2];
	int BufferNum;
	int EndQueued;
	bool VolumeChanged;
	bool Restarting;
	bool InitialPlayback;
	uint32_t NewVolume;
	int Division;
	int Tempo;
	int InitialTempo;
	uint8_t ChannelVolumes[16];
	uint32_t Volume;
	EMidiDevice DeviceType;
	bool CallbackIsThreaded;
	int LoopLimit;
	FString DumpFilename;
	FString Args;
};

// MUS file played with a MIDI stream ---------------------------------------

class MUSSong2 : public MIDIStreamer
{
public:
	MUSSong2(FileReader &reader, EMidiDevice type, const char *args);
	~MUSSong2();

	MusInfo *GetOPLDumper(const char *filename);
	MusInfo *GetWaveDumper(const char *filename, int rate);

protected:
	MUSSong2(const MUSSong2 *original, const char *filename, EMidiDevice type);	// file dump constructor

	void DoInitialSetup();
	void DoRestart();
	bool CheckDone();
	void Precache();
	uint32_t *MakeEvents(uint32_t *events, uint32_t *max_events_p, uint32_t max_time);

	MUSHeader *MusHeader;
	uint8_t *MusBuffer;
	uint8_t LastVelocity[16];
	size_t MusP, MaxMusP;
};

// MIDI file played with a MIDI stream --------------------------------------

class MIDISong2 : public MIDIStreamer
{
public:
	MIDISong2(FileReader &reader, EMidiDevice type, const char *args);
	~MIDISong2();

	MusInfo *GetOPLDumper(const char *filename);
	MusInfo *GetWaveDumper(const char *filename, int rate);

protected:
	MIDISong2(const MIDISong2 *original, const char *filename, EMidiDevice type);	// file dump constructor

	void CheckCaps(int tech);
	void DoInitialSetup();
	void DoRestart();
	bool CheckDone();
	uint32_t *MakeEvents(uint32_t *events, uint32_t *max_events_p, uint32_t max_time);
	void AdvanceTracks(uint32_t time);

	struct TrackInfo;

	void ProcessInitialMetaEvents ();
	uint32_t *SendCommand (uint32_t *event, TrackInfo *track, uint32_t delay, ptrdiff_t room, bool &sysex_noroom);
	TrackInfo *FindNextDue ();

	uint8_t *MusHeader;
	int SongLen;
	TrackInfo *Tracks;
	TrackInfo *TrackDue;
	int NumTracks;
	int Format;
	uint16_t DesignationMask;
};

// HMI file played with a MIDI stream ---------------------------------------

struct AutoNoteOff
{
	uint32_t Delay;
	uint8_t Channel, Key;
};
// Sorry, std::priority_queue, but I want to be able to modify the contents of the heap.
class NoteOffQueue : public TArray<AutoNoteOff>
{
public:
	void AddNoteOff(uint32_t delay, uint8_t channel, uint8_t key);
	void AdvanceTime(uint32_t time);
	bool Pop(AutoNoteOff &item);

protected:
	void Heapify();

	unsigned int Parent(unsigned int i) const { return (i + 1u) / 2u - 1u; }
	unsigned int Left(unsigned int i) const { return (i + 1u) * 2u - 1u; }
	unsigned int Right(unsigned int i) const { return (i + 1u) * 2u; }
};

class HMISong : public MIDIStreamer
{
public:
	HMISong(FileReader &reader, EMidiDevice type, const char *args);
	~HMISong();

	MusInfo *GetOPLDumper(const char *filename);
	MusInfo *GetWaveDumper(const char *filename, int rate);

protected:
	HMISong(const HMISong *original, const char *filename, EMidiDevice type);	// file dump constructor

	void SetupForHMI(int len);
	void SetupForHMP(int len);
	void CheckCaps(int tech);

	void DoInitialSetup();
	void DoRestart();
	bool CheckDone();
	uint32_t *MakeEvents(uint32_t *events, uint32_t *max_events_p, uint32_t max_time);
	void AdvanceTracks(uint32_t time);

	struct TrackInfo;

	void ProcessInitialMetaEvents ();
	uint32_t *SendCommand (uint32_t *event, TrackInfo *track, uint32_t delay, ptrdiff_t room, bool &sysex_noroom);
	TrackInfo *FindNextDue ();

	static uint32_t ReadVarLenHMI(TrackInfo *);
	static uint32_t ReadVarLenHMP(TrackInfo *);

	uint8_t *MusHeader;
	int SongLen;
	int NumTracks;
	TrackInfo *Tracks;
	TrackInfo *TrackDue;
	TrackInfo *FakeTrack;
	uint32_t (*ReadVarLen)(TrackInfo *);
	NoteOffQueue NoteOffs;
};

// XMI file played with a MIDI stream ---------------------------------------

class XMISong : public MIDIStreamer
{
public:
	XMISong(FileReader &reader, EMidiDevice type, const char *args);
	~XMISong();

	MusInfo *GetOPLDumper(const char *filename);
	MusInfo *GetWaveDumper(const char *filename, int rate);

protected:
	struct TrackInfo;
	enum EventSource { EVENT_None, EVENT_Real, EVENT_Fake };

	XMISong(const XMISong *original, const char *filename, EMidiDevice type);	// file dump constructor

	int FindXMIDforms(const uint8_t *chunk, int len, TrackInfo *songs) const;
	void FoundXMID(const uint8_t *chunk, int len, TrackInfo *song) const;
	bool SetMIDISubsong(int subsong);
	void DoInitialSetup();
	void DoRestart();
	bool CheckDone();
	uint32_t *MakeEvents(uint32_t *events, uint32_t *max_events_p, uint32_t max_time);
	void AdvanceSong(uint32_t time);

	void ProcessInitialMetaEvents();
	uint32_t *SendCommand (uint32_t *event, EventSource track, uint32_t delay, ptrdiff_t room, bool &sysex_noroom);
	EventSource FindNextDue();

	uint8_t *MusHeader;
	int SongLen;		// length of the entire file
	int NumSongs;
	TrackInfo *Songs;
	TrackInfo *CurrSong;
	NoteOffQueue NoteOffs;
	EventSource EventDue;
};

// Anything supported by the sound system out of the box --------------------

class StreamSong : public MusInfo
{
public:
    StreamSong (FileReader *reader);
	~StreamSong ();
	void Play (bool looping, int subsong);
	void Pause ();
	void Resume ();
	void Stop ();
	bool IsPlaying ();
	bool IsValid () const { return m_Stream != NULL; }
	bool SetPosition (unsigned int pos);
	bool SetSubsong (int subsong);
	FString GetStats();

protected:
	StreamSong () : m_Stream(NULL) {}

	SoundStream *m_Stream;
};

// MUS file played by a software OPL2 synth and streamed through the sound system

class OPLMUSSong : public StreamSong
{
public:
	OPLMUSSong (FileReader &reader, const char *args);
	~OPLMUSSong ();
	void Play (bool looping, int subsong);
	bool IsPlaying ();
	bool IsValid () const;
	void ResetChips ();
	MusInfo *GetOPLDumper(const char *filename);

protected:
	OPLMUSSong(const OPLMUSSong *original, const char *filename);	// OPL dump constructor

	static bool FillStream (SoundStream *stream, void *buff, int len, void *userdata);

	OPLmusicFile *Music;
};

class OPLMUSDumper : public OPLMUSSong
{
public:
	OPLMUSDumper(const OPLMUSSong *original, const char *filename);
	void Play(bool looping, int);
};

// CD track/disk played through the multimedia system -----------------------

class CDSong : public MusInfo
{
public:
	CDSong (int track, int id);
	~CDSong ();
	void Play (bool looping, int subsong);
	void Pause ();
	void Resume ();
	void Stop ();
	bool IsPlaying ();
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
	CDDAFile (FileReader &reader);
};

// Module played via foo_dumb -----------------------------------------------

MusInfo *MOD_OpenSong(FileReader &reader);

// Music played via Game Music Emu ------------------------------------------

const char *GME_CheckFormat(uint32_t header);
MusInfo *GME_OpenSong(FileReader &reader, const char *fmt);
MusInfo *SndFile_OpenSong(FileReader &fr);

// --------------------------------------------------------------------------

extern MusInfo *currSong;
void MIDIDeviceChanged(int newdev, bool force = false);

EXTERN_CVAR (Float, snd_musicvolume)
