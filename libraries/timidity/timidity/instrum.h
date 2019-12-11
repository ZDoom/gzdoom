#pragma once

#include "../../music_common/fileio.h"

namespace Timidity
{
/*
instrum.h
*/

enum
{
	PATCH_16				= (1<<0),
	PATCH_UNSIGNED			= (1<<1),
	PATCH_LOOPEN			= (1<<2),
	PATCH_BIDIR				= (1<<3),
	PATCH_BACKWARD			= (1<<4),
	PATCH_SUSTAIN			= (1<<5),
	PATCH_NO_SRELEASE		= (1<<6),
	PATCH_FAST_REL			= (1<<7),
};

struct Sample
{
	int32_t
		loop_start, loop_end, data_length,
		sample_rate;
	float
		low_freq, high_freq, root_freq;
	union
	{
		struct
		{
			uint8_t rate[6], offset[6];
		} gf1;
		struct
		{
			short delay_vol;
			short attack_vol;
			short hold_vol;
			short decay_vol;
			short sustain_vol;
			short release_vol;
		} sf2;
	} envelope;
	sample_t *data;
	int32_t
		tremolo_sweep_increment, tremolo_phase_increment,
		vibrato_sweep_increment, vibrato_control_ratio;
	uint8_t
		tremolo_depth, vibrato_depth,
		low_vel, high_vel,
		 type;
	uint16_t
		modes;
	int16_t
		panning;
	uint16_t
		scale_factor, key_group;
	int16_t
		scale_note;
	bool
		self_nonexclusive;
	float
		left_offset, right_offset;

	// SF2 stuff
	int16_t tune;
	int8_t velocity;

	float initial_attenuation;
};


/* Magic file words */

#define ID_RIFF		MAKE_ID('R','I','F','F')
#define ID_LIST		MAKE_ID('L','I','S','T')
#define ID_INFO		MAKE_ID('I','N','F','O')
#define ID_sfbk		MAKE_ID('s','f','b','k')
#define ID_sdta		MAKE_ID('s','d','t','a')
#define ID_pdta		MAKE_ID('p','d','t','a')
#define ID_ifil		MAKE_ID('i','f','i','l')
#define ID_iver		MAKE_ID('i','v','e','r')
#define ID_irom		MAKE_ID('i','r','o','m')
#define ID_smpl		MAKE_ID('s','m','p','l')
#define ID_sm24		MAKE_ID('s','m','2','4')
#define ID_phdr		MAKE_ID('p','h','d','r')
#define ID_pbag		MAKE_ID('p','b','a','g')
#define ID_pmod		MAKE_ID('p','m','o','d')
#define ID_pgen		MAKE_ID('p','g','e','n')
#define ID_inst		MAKE_ID('i','n','s','t')
#define ID_ibag		MAKE_ID('i','b','a','g')
#define ID_imod		MAKE_ID('i','m','o','d')
#define ID_igen		MAKE_ID('i','g','e','n')
#define ID_shdr		MAKE_ID('s','h','d','r')

/* Instrument definitions */

enum
{
	INST_GUS,
	INST_DLS,
	INST_SF2
};

struct Instrument
{
	Instrument();
	~Instrument();

	int samples;
	Sample *sample;
};

struct ToneBankElement
{
	ToneBankElement() :
		note(0), pan(0), strip_loop(0), strip_envelope(0), strip_tail(0)
	{}

	std::string name;
	int note, pan, fontbank, fontpreset, fontnote;
	int8_t strip_loop, strip_envelope, strip_tail;
};

/* A hack to delay instrument loading until after reading the entire MIDI file. */
#define MAGIC_LOAD_INSTRUMENT ((Instrument *)(-1))

enum
{
	MAXPROG				= 128,
	MAXBANK				= 128,
	SPECIAL_PROGRAM		= -1
};

struct ToneBank
{
	ToneBank();
	~ToneBank();

	ToneBankElement *tone;
	Instrument *instrument[MAXPROG];
};



/*
instrum_font.cpp
*/

class FontFile
{
public:
	FontFile(const char *filename);
	virtual ~FontFile();

	std::string Filename;
	FontFile *Next;

	virtual Instrument *LoadInstrument(struct Renderer *song, int drum, int bank, int program) = 0;
	virtual Instrument *LoadInstrumentOrder(struct Renderer *song, int order, int drum, int bank, int program) = 0;
	virtual void SetOrder(int order, int drum, int bank, int program) = 0;
	virtual void SetAllOrders(int order) = 0;
};

class Instruments
{
public:
	MusicIO::SoundFontReaderInterface *sfreader;
	ToneBank* tonebank[MAXBANK] = {};
	ToneBank* drumset[MAXBANK] = {};
	FontFile* Fonts = nullptr;
	std::string def_instr_name;

	Instruments(MusicIO::SoundFontReaderInterface* reader);
	~Instruments();

	int LoadConfig() { return read_config_file(nullptr); }
	int read_config_file(const char* name);
	int LoadDMXGUS(int gus_memsize, const char *dmxgusdata, size_t dmxgussize);

	void font_freeall();
	FontFile* font_find(const char* filename);
	void font_add(const char* filename, int load_order);
	void font_remove(const char* filename);
	void font_order(int order, int bank, int preset, int keynote);
	void convert_sample_data(Sample* sample, const void* data);
	void free_instruments();

	FontFile* ReadDLS(const char* filename, timidity_file* f);

};

void convert_sample_data(Sample* sp, const void* data);

}
