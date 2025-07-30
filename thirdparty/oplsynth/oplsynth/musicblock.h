#pragma once
#include <stdint.h>
#include "genmidi.h"
#include "oplio.h"


struct OPLVoice 
{
	unsigned int index;			// Index of this voice, or -1 if not in use.
	unsigned int key;			// The midi key that this voice is playing.
	unsigned int note;			// The note being played.  This is normally the same as the key, but if the instrument is a fixed pitch instrument, it is different.
	unsigned int note_volume;	// The volume of the note being played on this channel.
	GenMidiInstrument *current_instr;	// Currently-loaded instrument data
	GenMidiVoice *current_instr_voice;// The voice number in the instrument to use. This is normally set to the instrument's first voice; if this is a double voice instrument, it may be the second one
	bool sustained;
	int8_t	fine_tuning;
	int	pitch;
	uint32_t timestamp;
};

struct musicBlock {
	musicBlock();
	~musicBlock();

	OPLChannel oplchannels[NUM_CHANNELS];
	OPLio *io;
	uint32_t timeCounter;

	struct GenMidiInstrument OPLinstruments[GENMIDI_NUM_TOTAL];

	void changeModulation(uint32_t id, int value);
	void changeSustain(uint32_t id, int value);
	void changeVolume(uint32_t id, int value, bool expression);
	void changePanning(uint32_t id, int value);
	void notesOff(uint32_t id, int value);
	void allNotesOff(uint32_t id, int value);
	void changeExtended(uint32_t channel, uint8_t controller, int value);
	void resetControllers(uint32_t channel, int vol);
	void programChange(uint32_t channel, int value);
	void resetAllControllers(int vol);
	void changePitch(uint32_t channel, int val1, int val2);

	void noteOn(uint32_t channel, uint8_t note, int volume);
	void noteOff(uint32_t channel, uint8_t note);
	void stopAllVoices();

protected:
	OPLVoice voices[NUM_VOICES];

	int findFreeVoice();
	int replaceExistingVoice();
	void voiceKeyOn(uint32_t slot, uint32_t channo, GenMidiInstrument *instrument, uint32_t instrument_voice, uint32_t, uint32_t volume);
	int releaseVoice(uint32_t slot, uint32_t killed);

	friend class Stat_opl;

};

enum ExtCtrl {
	ctrlRPNHi,
	ctrlRPNLo,
	ctrlNRPNHi,
	ctrlNRPNLo,
	ctrlDataEntryHi,
	ctrlDataEntryLo,
};
