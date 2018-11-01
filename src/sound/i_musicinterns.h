
#include "oplsynth/opl_mus_player.h"
#include "c_cvars.h"
#include "mus2midi.h"
#include "i_sound.h"
#include "i_music.h"
#include "s_sound.h"
#include "files.h"
#include "wildmidi/wildmidi_lib.h"
#include "midisources/midisource.h"

void I_InitMusicWin32 ();

extern float relative_volume;
class MIDISource;


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
	virtual void FluidSettingInt(const char *setting, int value);
	virtual void FluidSettingNum(const char *setting, double value);
	virtual void FluidSettingStr(const char *setting, const char *value);
	virtual void WildMidiSetOption(int opt, int set);
	virtual bool Preprocess(MIDIStreamer *song, bool looping);
	virtual FString GetStats();
	virtual int GetDeviceType() const { return MDEV_DEFAULT; }
	virtual bool CanHandleSysex() const { return true; }
};


#ifdef _WIN32
MIDIDevice *CreateWinMIDIDevice(int mididevice);
#endif
MIDIDevice *CreateTimidityPPMIDIDevice(const char *args, int samplerate);
void TimidityPP_Shutdown();

// Base class for software synthesizer MIDI output devices ------------------

class SoftSynthMIDIDevice : public MIDIDevice
{
	friend class MIDIWaveWriter;
public:
	SoftSynthMIDIDevice(int samplerate, int minrate = 1, int maxrate = 1000000 /* something higher than any valid value */);
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
	int GetSampleRate() const { return SampleRate; }

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
	TimidityMIDIDevice(const char *args, int samplerate);
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

// Internal disk writing version of a MIDI device ------------------

class MIDIWaveWriter : public SoftSynthMIDIDevice
{
public:
	MIDIWaveWriter(const char *filename, SoftSynthMIDIDevice *devtouse);
	~MIDIWaveWriter();
	int Resume();
	int Open(MidiCallback cb, void *userdata)
	{
		return playDevice->Open(cb, userdata);
	}
	void Stop();
	void HandleEvent(int status, int parm1, int parm2) { playDevice->HandleEvent(status, parm1, parm2);  }
	void HandleLongEvent(const uint8_t *data, int len) { playDevice->HandleLongEvent(data, len);  }
	void ComputeOutput(float *buffer, int len) { playDevice->ComputeOutput(buffer, len);  }
	int StreamOutSync(MidiHeader *data) { return playDevice->StreamOutSync(data); }
	int StreamOut(MidiHeader *data) { return playDevice->StreamOut(data); }
	int GetDeviceType() const override { return playDevice->GetDeviceType(); }
	bool ServiceStream (void *buff, int numbytes) { return playDevice->ServiceStream(buff, numbytes); }
	int GetTechnology() const { return playDevice->GetTechnology(); }
	int SetTempo(int tempo) { return playDevice->SetTempo(tempo); }
	int SetTimeDiv(int timediv) { return playDevice->SetTimeDiv(timediv); }
	bool IsOpen() const { return playDevice->IsOpen(); }
	void CalcTickRate() { playDevice->CalcTickRate(); }

protected:
	FileWriter *File;
	SoftSynthMIDIDevice *playDevice;
};

// WildMidi implementation of a MIDI device ---------------------------------

class WildMIDIDevice : public SoftSynthMIDIDevice
{
public:
	WildMIDIDevice(const char *args, int samplerate);
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
	FluidSynthMIDIDevice(const char *args, int samplerate);
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


class ADLMIDIDevice : public SoftSynthMIDIDevice
{
	struct ADL_MIDIPlayer *Renderer;
public:
	ADLMIDIDevice(const char *args);
	~ADLMIDIDevice();

	int Open(MidiCallback, void *userdata);
	int GetDeviceType() const override { return MDEV_ADL; }

protected:

	void HandleEvent(int status, int parm1, int parm2);
	void HandleLongEvent(const uint8_t *data, int len);
	void ComputeOutput(float *buffer, int len);

private:
	int LoadCustomBank(const char *bankfile);
};


class OPNMIDIDevice : public SoftSynthMIDIDevice
{
	struct OPN2_MIDIPlayer *Renderer;
public:
	OPNMIDIDevice(const char *args);
	~OPNMIDIDevice();


	int Open(MidiCallback, void *userdata);
	int GetDeviceType() const override { return MDEV_OPN; }

protected:
	void HandleEvent(int status, int parm1, int parm2);
	void HandleLongEvent(const uint8_t *data, int len);
	void ComputeOutput(float *buffer, int len);

private:
	int LoadCustomBank(const char *bankfile);
};


// Base class for streaming MUS and MIDI files ------------------------------

enum
{
	MAX_MIDI_EVENTS = 128
};

class MIDIStreamer : public MusInfo
{
public:
	MIDIStreamer(EMidiDevice type, const char *args);
	~MIDIStreamer();

	void MusicVolumeChanged() override;
	void Play(bool looping, int subsong) override;
	void Pause() override;
	void Resume() override;
	void Stop() override;
	bool IsPlaying() override;
	bool IsMIDI() const override;
	bool IsValid() const override;
	bool SetSubsong(int subsong) override;
	void Update() override;
	FString GetStats() override;
	void FluidSettingInt(const char *setting, int value) override;
	void FluidSettingNum(const char *setting, double value) override;
	void FluidSettingStr(const char *setting, const char *value) override;
	void WildMidiSetOption(int opt, int set) override;
	int ServiceEvent();
	void SetMIDISource(MIDISource *_source);

	int GetDeviceType() const override
	{
		return nullptr == MIDI
			? MusInfo::GetDeviceType()
			: MIDI->GetDeviceType();
	}

	bool DumpWave(const char *filename, int subsong, int samplerate);
	bool DumpOPL(const char *filename, int subsong);


protected:
	MIDIStreamer(const char *dumpname, EMidiDevice type);

	void OutputVolume (uint32_t volume);
	int FillBuffer(int buffer_num, int max_events, uint32_t max_time);
	int FillStopBuffer(int buffer_num);
	uint32_t *WriteStopNotes(uint32_t *events);
	int VolumeControllerChange(int channel, int volume);
	void SetTempo(int new_tempo);
	void Precache();
	void StartPlayback();
	bool InitPlayback();

	//void SetMidiSynth(MIDIDevice *synth);
	
	
	static EMidiDevice SelectMIDIDevice(EMidiDevice devtype);
	MIDIDevice *CreateMIDIDevice(EMidiDevice devtype, int samplerate);

	static void Callback(void *userdata);

	enum
	{
		SONG_MORE,
		SONG_DONE,
		SONG_ERROR
	};

	MIDIDevice *MIDI;
	uint32_t Events[2][MAX_MIDI_EVENTS*3];
	MidiHeader Buffer[2];
	int BufferNum;
	int EndQueued;
	bool VolumeChanged;
	bool Restarting;
	bool InitialPlayback;
	uint32_t NewVolume;
	uint32_t Volume;
	EMidiDevice DeviceType;
	bool CallbackIsThreaded;
	int LoopLimit;
	FString Args;
	MIDISource *source;

};

// Anything supported by the sound system out of the box --------------------

class StreamSong : public MusInfo
{
public:
    StreamSong (FileReader &reader);
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
