
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
	int adl_chips_count = 6;
	int adl_emulator_id = 0;
	int adl_bank = 14;
	int adl_volume_model = 3; // DMX
	bool adl_run_at_pcm_rate = 0;
	bool adl_fullpan = 1;
	std::string adl_custom_bank;
};

extern ADLConfig adlConfig;

struct FluidConfig
{
	std::string fluid_lib;
	std::vector<std::string> fluid_patchset;
	bool fluid_reverb = false;
	bool fluid_chorus = false;
	int fluid_voices = 128;
	int fluid_interp = 1;
	int fluid_samplerate = 0;
	int fluid_threads = 1;
	int fluid_chorus_voices = 3;
	int fluid_chorus_type = 0;
	float fluid_gain = 0.5f;
	float fluid_reverb_roomsize = 0.61f;
	float fluid_reverb_damping = 0.23f;
	float fluid_reverb_width = 0.76f;
	float fluid_reverb_level = 0.57f;
	float fluid_chorus_level = 1.2f;
	float fluid_chorus_speed = 0.3f;
	float fluid_chorus_depth = 8;
};

extern FluidConfig fluidConfig;

struct OPLMidiConfig
{
	int numchips = 2;
	int core = 0;
	bool fullpan = true;
	struct GenMidiInstrument OPLinstruments[GENMIDI_NUM_TOTAL];
};

extern OPLMidiConfig oplMidiConfig;

struct OpnConfig
{
	int opn_chips_count = 8;
	int opn_emulator_id = 0;
	bool opn_run_at_pcm_rate = false;
	bool opn_fullpan = 1;
	std::string opn_custom_bank;
	std::vector<uint8_t> default_bank;
};

extern OpnConfig opnConfig;

namespace Timidity
{
	class Instruments;
	class SoundFontReaderInterface;
}

struct GUSConfig
{
	// This one is a bit more complex because it also implements the instrument cache.
	int midi_voices = 32;
	int gus_memsize = 0;
	void (*errorfunc)(int type, int verbosity_level, const char* fmt, ...) = nullptr;

	Timidity::SoundFontReaderInterface *reader;
	std::string readerName;
	std::vector<uint8_t> dmxgus;				// can contain the contents of a DMXGUS lump that may be used as the instrument set. In this case gus_patchdir must point to the location of the GUS data.
	std::string gus_patchdir;
	
	// These next two fields are for caching the instruments for repeated use. The GUS device will work without them being cached in the config but it'd require reloading the instruments each time.
	// Thus, this config should always be stored globally to avoid this.
	// If the last loaded instrument set is to be reused or the caller wants to manage them itself, both 'reader' and 'dmxgus' fields should be left empty.
	std::string loadedConfig;
	std::shared_ptr<Timidity::Instruments> instruments;	// this is held both by the config and the device
};

extern GUSConfig gusConfig;

namespace TimidityPlus
{
	class Instruments;
	class SoundFontReaderInterface;
}

struct TimidityConfig
{
	void (*errorfunc)(int type, int verbosity_level, const char* fmt, ...) = nullptr;

	TimidityPlus::SoundFontReaderInterface* reader;
	std::string readerName;

	// These next two fields are for caching the instruments for repeated use. The GUS device will work without them being cached in the config but it'd require reloading the instruments each time.
	// Thus, this config should always be stored globally to avoid this.
	// If the last loaded instrument set is to be reused or the caller wants to manage them itself, 'reader' should be left empty.
	std::string loadedConfig;
	std::shared_ptr<TimidityPlus::Instruments> instruments;	// this is held both by the config and the device

};

extern TimidityConfig timidityConfig;

struct WildMidiConfig
{
	bool reverb = false;
	bool enhanced_resampling = true;
	void (*errorfunc)(const char* wmfmt, va_list args) = nullptr;

	WildMidi::SoundFontReaderInterface* reader;
	std::string readerName;

	// These next two fields are for caching the instruments for repeated use. The GUS device will work without them being cached in the config but it'd require reloading the instruments each time.
	// Thus, this config should always be stored globally to avoid this.
	// If the last loaded instrument set is to be reused or the caller wants to manage them itself, 'reader' should be left empty.
	std::string loadedConfig;
	std::shared_ptr<WildMidi::Instruments> instruments;	// this is held both by the config and the device

};

extern WildMidiConfig wildMidiConfig;

struct SoundStreamInfo
{
	// Format is always 32 bit float. If mBufferSize is 0, the song doesn't use streaming but plays through a different interface.
	int mBufferSize;
	int mSampleRate;
	int mNumChannels;
};


class MIDIStreamer;

typedef void(*MidiCallback)(void *);
class MIDIDevice
{
public:
	MIDIDevice() = default;
	virtual ~MIDIDevice();

	void SetCallback(MidiCallback callback, void* userdata)
	{
		Callback = callback;
		CallbackData = userdata;
	}
	virtual int Open() = 0;
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
	virtual void ChangeSettingInt(const char *setting, int value);
	virtual void ChangeSettingNum(const char *setting, double value);
	virtual void ChangeSettingString(const char *setting, const char *value);
	virtual bool Preprocess(MIDIStreamer *song, bool looping);
	virtual FString GetStats();
	virtual int GetDeviceType() const { return MDEV_DEFAULT; }
	virtual bool CanHandleSysex() const { return true; }
	virtual SoundStreamInfo GetStreamInfo() const;

protected:
	MidiCallback Callback;
	void* CallbackData;
};



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

	virtual int Open();
	virtual bool ServiceStream(void* buff, int numbytes);
	int GetSampleRate() const { return SampleRate; }
	SoundStreamInfo GetStreamInfo() const override;

protected:
	std::mutex CritSec;
	double Tempo;
	double Division;
	double SamplesPerTick;
	double NextTickIn;
	MidiHeader *Events;
	bool Started;
	bool isMono = false; // only relevant for OPL.
	bool isOpen = false;
	uint32_t Position;
	int SampleRate;
	int StreamBlockSize = 2;

	virtual void CalcTickRate();
	int PlayTick();

	virtual int OpenRenderer() = 0;
	virtual void HandleEvent(int status, int parm1, int parm2) = 0;
	virtual void HandleLongEvent(const uint8_t *data, int len) = 0;
	virtual void ComputeOutput(float *buffer, int len) = 0;
};


// Internal disk writing version of a MIDI device ------------------

class MIDIWaveWriter : public SoftSynthMIDIDevice
{
public:
	MIDIWaveWriter(const char *filename, SoftSynthMIDIDevice *devtouse);
	~MIDIWaveWriter();
	int Resume() override;
	int Open() override
	{
		return playDevice->Open();
	}
	int OpenRenderer() override { return playDevice->OpenRenderer();  }
	void Stop() override;
	void HandleEvent(int status, int parm1, int parm2) override { playDevice->HandleEvent(status, parm1, parm2);  }
	void HandleLongEvent(const uint8_t *data, int len) override { playDevice->HandleLongEvent(data, len);  }
	void ComputeOutput(float *buffer, int len) override { playDevice->ComputeOutput(buffer, len);  }
	int StreamOutSync(MidiHeader *data) override { return playDevice->StreamOutSync(data); }
	int StreamOut(MidiHeader *data) override { return playDevice->StreamOut(data); }
	int GetDeviceType() const override { return playDevice->GetDeviceType(); }
	bool ServiceStream (void *buff, int numbytes) override { return playDevice->ServiceStream(buff, numbytes); }
	int GetTechnology() const override { return playDevice->GetTechnology(); }
	int SetTempo(int tempo) override { return playDevice->SetTempo(tempo); }
	int SetTimeDiv(int timediv) override { return playDevice->SetTimeDiv(timediv); }
	bool IsOpen() const override { return playDevice->IsOpen(); }
	void CalcTickRate() override { playDevice->CalcTickRate(); }

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
	void ChangeSettingInt(const char *setting, int value) override;
	void ChangeSettingNum(const char *setting, double value) override;
	void ChangeSettingString(const char *setting, const char *value) override;
	int ServiceEvent();
	void SetMIDISource(MIDISource *_source);

	int GetDeviceType() const override
	{
		return nullptr == MIDI
			? MusInfo::GetDeviceType()
			: MIDI->GetDeviceType();
	}

	bool DumpWave(const char *filename, int subsong, int samplerate);
	static bool FillStream(SoundStream* stream, void* buff, int len, void* userdata);


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

	MIDIDevice *MIDI = nullptr;
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
	MIDISource *source = nullptr;
	SoundStream* Stream = nullptr;
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
	void ChangeSettingInt (const char *, int) override;

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

// MIDI devices

MIDIDevice *CreateFluidSynthMIDIDevice(int samplerate, const FluidConfig* config, int (*printfunc)(const char*, ...));
MIDIDevice *CreateADLMIDIDevice(const ADLConfig* config);
MIDIDevice *CreateOPNMIDIDevice(const OpnConfig *args);
MIDIDevice *CreateOplMIDIDevice(const OPLMidiConfig* config);
MIDIDevice *CreateTimidityMIDIDevice(GUSConfig *config, int samplerate);
MIDIDevice *CreateTimidityPPMIDIDevice(TimidityConfig *config, int samplerate);
MIDIDevice *CreateWildMIDIDevice(WildMidiConfig *config, int samplerate);

#ifdef _WIN32
MIDIDevice* CreateWinMIDIDevice(int mididevice);
#endif

// Data interface

void Fluid_SetupConfig(FluidConfig *config, const char* patches, bool systemfallback);
void ADL_SetupConfig(ADLConfig *config, const char *Args);
void OPL_SetupConfig(OPLMidiConfig *config, const char *args);
void OPN_SetupConfig(OpnConfig *config, const char *Args);
bool GUS_SetupConfig(GUSConfig *config, const char *args);
bool Timidity_SetupConfig(TimidityConfig* config, const char* args);
bool WildMidi_SetupConfig(WildMidiConfig* config, const char* args);

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
