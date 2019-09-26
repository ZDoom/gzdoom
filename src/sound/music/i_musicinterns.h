
#include <mutex>
#include <string>
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

struct ADLConfig
{
	const char * (*adl_full_path)(const char* filename);
	int adl_chips_count;
	int adl_emulator_id;
	int adl_bank;
	int adl_volume_model;
	bool adl_run_at_pcm_rate;
	bool adl_fullpan;
	bool adl_use_custom_bank;
	const char *adl_custom_bank;
};

extern ADLConfig adlConfig;


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



void Timidity_Shutdown();
void TimidityPP_Shutdown();
void WildMidi_Shutdown ();


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
	std::mutex CritSec;
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

// Base class for streaming MUS and MIDI files ------------------------------

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

protected:

	static bool FillStream (SoundStream *stream, void *buff, int len, void *userdata);

	OPLmusicFile *Music;
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
MusInfo* XA_OpenSong(FileReader& reader);

// --------------------------------------------------------------------------

extern MusInfo *currSong;
void MIDIDeviceChanged(int newdev, bool force = false);

EXTERN_CVAR (Float, snd_mastervolume)
EXTERN_CVAR (Float, snd_musicvolume)
