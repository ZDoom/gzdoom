#pragma once

#include <mutex>
#include "zmusic/midiconfig.h"
#include "zmusic/mididefs.h"

typedef void(*MidiCallback)(void *);

// A device that provides a WinMM-like MIDI streaming interface -------------


struct MidiHeader
{
	uint8_t *lpData;
	uint32_t dwBufferLength;
	uint32_t dwBytesRecorded;
	MidiHeader *lpNext;
};

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
	virtual std::string GetStats();
	virtual int GetDeviceType() const { return MDEV_DEFAULT; }
	virtual bool CanHandleSysex() const { return true; }
	virtual SoundStreamInfo GetStreamInfo() const;

protected:
	MidiCallback Callback;
	void* CallbackData;
};

// Base class for software synthesizer MIDI output devices ------------------

class SoftSynthMIDIDevice : public MIDIDevice
{
	friend class MIDIWaveWriter;
public:
	SoftSynthMIDIDevice(int samplerate, int minrate = 1, int maxrate = 1000000 /* something higher than any valid value */);
	~SoftSynthMIDIDevice();

	void Close() override;
	bool IsOpen() const override;
	int GetTechnology() const override;
	int SetTempo(int tempo) override;
	int SetTimeDiv(int timediv) override;
	int StreamOut(MidiHeader *data) override;
	int StreamOutSync(MidiHeader *data) override;
	int Resume() override;
	void Stop() override;
	bool Pause(bool paused) override;

	virtual int Open() override;
	virtual bool ServiceStream(void* buff, int numbytes);
	int GetSampleRate() const { return SampleRate; }
	SoundStreamInfo GetStreamInfo() const override;

protected:
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
	//~MIDIWaveWriter();
	bool CloseFile();
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
	FILE *File;
	SoftSynthMIDIDevice *playDevice;
};


// MIDI devices

MIDIDevice *CreateFluidSynthMIDIDevice(int samplerate, const char *Args);
MIDIDevice *CreateADLMIDIDevice(const char* args);
MIDIDevice *CreateOPNMIDIDevice(const char *args);
MIDIDevice *CreateOplMIDIDevice(const char* Args);
MIDIDevice *CreateTimidityMIDIDevice(const char* Args, int samplerate);
MIDIDevice *CreateTimidityPPMIDIDevice(const char *Args, int samplerate);
MIDIDevice *CreateWildMIDIDevice(const char *Args, int samplerate);

#ifdef _WIN32
MIDIDevice* CreateWinMIDIDevice(int mididevice);
#endif

#ifdef __linux__
MIDIDevice* CreateAlsaMIDIDevice(int mididevice);
#endif
