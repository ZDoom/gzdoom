//
//  midisources.h
//  GZDoom
//
//  Created by Christoph Oelckers on 23.02.18.
//

#ifndef midisources_h
#define midisources_h

#include <stdint.h>
#include <functional>

extern char MIDI_EventLengths[7];
extern char MIDI_CommonLengths[15];

// base class for the different MIDI sources --------------------------------------

class MIDISource
{
	int Volume = 0xffff;
	int LoopLimit = 0;
	std::function<bool(int)> TempoCallback = [](int t) { return false; };
	
protected:

	bool isLooping = false;
	bool skipSysex = false;
	int Division = 0;
	int Tempo = 500000;
	int InitialTempo = 500000;
	uint8_t ChannelVolumes[16];
	
	int VolumeControllerChange(int channel, int volume);
	void SetTempo(int new_tempo);
	int ClampLoopCount(int loopcount);

	
public:
	bool Exporting = false;

	// Virtuals for subclasses to override
	virtual ~MIDISource() {}
	virtual void CheckCaps(int tech);
	virtual void DoInitialSetup() = 0;
	virtual void DoRestart() = 0;
	virtual bool CheckDone() = 0;
	virtual TArray<uint16_t> PrecacheData();
	virtual bool SetMIDISubsong(int subsong);
	virtual uint32_t *MakeEvents(uint32_t *events, uint32_t *max_event_p, uint32_t max_time) = 0;
	
	void StartPlayback(bool looped = true, int looplimit = 0)
	{
		Tempo = InitialTempo;
		LoopLimit = looplimit;
		isLooping = looped;
	}

	void SkipSysex() { skipSysex = true; }
	
	bool isValid() const { return Division > 0; }
	int getDivision() const { return Division; }
	int getInitialTempo() const { return InitialTempo; }
	int getTempo() const { return Tempo; }
	int getChannelVolume(int ch) const { return ChannelVolumes[ch]; }
	void setVolume(int vol) { Volume = vol; }
	void setLoopLimit(int lim) { LoopLimit = lim; }
	void setTempoCallback(std::function<bool(int)> cb)
	{
		TempoCallback = cb;
	}
	
	void CreateSMF(TArray<uint8_t> &file, int looplimit);

};

// MUS file played with a MIDI stream ---------------------------------------

class MUSSong2 : public MIDISource
{
public:
	MUSSong2(FileReader &reader);
	~MUSSong2();
	
protected:
	void DoInitialSetup() override;
	void DoRestart() override;
	bool CheckDone() override;
	TArray<uint16_t> PrecacheData() override;
	uint32_t *MakeEvents(uint32_t *events, uint32_t *max_events_p, uint32_t max_time) override;
	
private:
	MUSHeader *MusHeader;
	uint8_t *MusBuffer;
	uint8_t LastVelocity[16];
	size_t MusP, MaxMusP;
};


// MIDI file played with a MIDI stream --------------------------------------

class MIDISong2 : public MIDISource
{
public:
	MIDISong2(FileReader &reader);
	~MIDISong2();
	
protected:
	void CheckCaps(int tech) override;
	void DoInitialSetup() override;
	void DoRestart() override;
	bool CheckDone() override;
	uint32_t *MakeEvents(uint32_t *events, uint32_t *max_events_p, uint32_t max_time) override;
	
private:
	void AdvanceTracks(uint32_t time);
	
	struct TrackInfo;
	
	void ProcessInitialMetaEvents ();
	uint32_t *SendCommand (uint32_t *event, TrackInfo *track, uint32_t delay, ptrdiff_t room, bool &sysex_noroom);
	TrackInfo *FindNextDue ();
	
	TArray<uint8_t> MusHeader;
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

class HMISong : public MIDISource
{
public:
	HMISong(FileReader &reader);
	~HMISong();
	
protected:
	
	void DoInitialSetup() override;
	void DoRestart() override;
	bool CheckDone() override;
	void CheckCaps(int tech) override;
	uint32_t *MakeEvents(uint32_t *events, uint32_t *max_events_p, uint32_t max_time) override;
	
private:
	void SetupForHMI(int len);
	void SetupForHMP(int len);
	void AdvanceTracks(uint32_t time);
	
	struct TrackInfo;
	
	void ProcessInitialMetaEvents ();
	uint32_t *SendCommand (uint32_t *event, TrackInfo *track, uint32_t delay, ptrdiff_t room, bool &sysex_noroom);
	TrackInfo *FindNextDue ();
	
	static uint32_t ReadVarLenHMI(TrackInfo *);
	static uint32_t ReadVarLenHMP(TrackInfo *);
	
	TArray<uint8_t> MusHeader;
	int NumTracks;
	TrackInfo *Tracks;
	TrackInfo *TrackDue;
	TrackInfo *FakeTrack;
	uint32_t (*ReadVarLen)(TrackInfo *);
	NoteOffQueue NoteOffs;
};

// XMI file played with a MIDI stream ---------------------------------------

class XMISong : public MIDISource
{
public:
	XMISong(FileReader &reader);
	~XMISong();
	
protected:
	bool SetMIDISubsong(int subsong) override;
	void DoInitialSetup() override;
	void DoRestart() override;
	bool CheckDone() override;
	uint32_t *MakeEvents(uint32_t *events, uint32_t *max_events_p, uint32_t max_time) override;
	
private:
	struct TrackInfo;
	enum EventSource { EVENT_None, EVENT_Real, EVENT_Fake };
	
	int FindXMIDforms(const uint8_t *chunk, int len, TrackInfo *songs) const;
	void FoundXMID(const uint8_t *chunk, int len, TrackInfo *song) const;
	void AdvanceSong(uint32_t time);
	
	void ProcessInitialMetaEvents();
	uint32_t *SendCommand (uint32_t *event, EventSource track, uint32_t delay, ptrdiff_t room, bool &sysex_noroom);
	EventSource FindNextDue();
	
	TArray<uint8_t> MusHeader;
	int NumSongs;
	TrackInfo *Songs;
	TrackInfo *CurrSong;
	NoteOffQueue NoteOffs;
	EventSource EventDue;
};



#endif /* midisources_h */
