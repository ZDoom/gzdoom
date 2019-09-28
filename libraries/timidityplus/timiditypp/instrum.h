/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2002 Masanao Izumo <mo@goice.co.jp>
    Copyright (C) 1995 Tuukka Toivonen <tt@cgs.fi>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA

   instrum.h

   */

#ifndef ___INSTRUM_H_
#define ___INSTRUM_H_

#include <string>
#include "common.h"
#include "sysdep.h"
#include "sffile.h"
#include "sflayer.h"
#include "sfitem.h"
#include "../../music_common/fileio.h"


namespace TimidityPlus
{
	using timidity_file = MusicIO::FileInterface;

enum
{
	READ_CONFIG_SUCCESS = 0,
	READ_CONFIG_ERROR = 1,
	READ_CONFIG_RECURSION = 2, /* Too much recursion */
	READ_CONFIG_FILE_NOT_FOUND = 3, /* Returned only w. allow_missing_file */
};


struct Sample
{
	splen_t
		loop_start, loop_end, data_length;
	int32_t
		sample_rate, low_freq, high_freq, root_freq;
	int8_t panning, note_to_use;
	int32_t
		envelope_rate[6], envelope_offset[6],
		modenv_rate[6], modenv_offset[6];
	double
		volume;
	sample_t
		*data;
	int32_t
		tremolo_sweep_increment, tremolo_phase_increment,
		vibrato_sweep_increment, vibrato_control_ratio;
	int16_t
		tremolo_depth;
	int16_t vibrato_depth;
	uint8_t
		modes, data_alloced,
		low_vel, high_vel;
	int32_t cutoff_freq;	/* in Hz, [1, 20000] */
	int16_t resonance;	/* in centibels, [0, 960] */
	/* in cents, [-12000, 12000] */
	int16_t tremolo_to_pitch, tremolo_to_fc, modenv_to_pitch, modenv_to_fc,
		envelope_keyf[6], envelope_velf[6], modenv_keyf[6], modenv_velf[6],
		vel_to_fc, key_to_fc;
	int16_t vel_to_resonance;	/* in centibels, [-960, 960] */
	int8_t envelope_velf_bpo, modenv_velf_bpo,
		key_to_fc_bpo, vel_to_fc_threshold;	/* in notes */
	int32_t vibrato_delay, tremolo_delay, envelope_delay, modenv_delay;	/* in samples */
	int16_t scale_freq;	/* in notes */
	int16_t scale_factor;	/* in 1024divs/key */
	int8_t inst_type;
	int32_t sf_sample_index, sf_sample_link;	/* for stereo SoundFont */
	uint16_t sample_type;	/* 1 = Mono, 2 = Right, 4 = Left, 8 = Linked, $8000 = ROM */
	double root_freq_detected;	/* root freq from pitch detection */
	int transpose_detected;	/* note offset from detected root */
	int chord;			/* type of chord for detected pitch */
};

/* Bits in modes: */
enum
{
	MODES_16BIT = (1 << 0),
	MODES_UNSIGNED = (1 << 1),
	MODES_LOOPING = (1 << 2),
	MODES_PINGPONG = (1 << 3),
	MODES_REVERSE = (1 << 4),
	MODES_SUSTAIN = (1 << 5),
	MODES_ENVELOPE = (1 << 6),
	MODES_CLAMPED = (1 << 7), /* ?? (for last envelope??) */

	INST_GUS = 0,
	INST_SF2 = 1,
	INST_MOD = 2,
	INST_PCM = 3,	/* %sample */

	/* sfSampleType */
	SF_SAMPLETYPE_MONO = 1,
	SF_SAMPLETYPE_RIGHT = 2,
	SF_SAMPLETYPE_LEFT = 4,
	SF_SAMPLETYPE_LINKED = 8,
	SF_SAMPLETYPE_ROM = 0x8000,
};

struct Instrument
{
	int type;
	int samples;
	Sample *sample;
	char *instname;
};

struct ToneBankElement
{
	char *name;
	char *comment;
	Instrument *instrument;
	int8_t note, pan, strip_loop, strip_envelope, strip_tail, loop_timeout,
		font_preset, font_keynote, legato, tva_level, play_note, damper_mode;
	uint8_t font_bank;
	uint8_t instype; /* 0: Normal
				1: %font
				2: %sample
				3-255: reserved
				*/
	int16_t amp;
	int16_t rnddelay;
	int tunenum;
	float *tune;
	int sclnotenum;
	int16_t *sclnote;
	int scltunenum;
	int16_t *scltune;
	int fcnum;
	int16_t *fc;
	int resonum;
	int16_t *reso;
	int trempitchnum, tremfcnum, modpitchnum, modfcnum;
	int16_t *trempitch, *tremfc, *modpitch, *modfc;
	int envratenum, envofsnum;
	int **envrate, **envofs;
	int modenvratenum, modenvofsnum;
	int **modenvrate, **modenvofs;
	int envvelfnum, envkeyfnum;
	int **envvelf, **envkeyf;
	int modenvvelfnum, modenvkeyfnum;
	int **modenvvelf, **modenvkeyf;
	int tremnum, vibnum;
	struct Quantity_ **trem, **vib;
	int16_t vel_to_fc, key_to_fc, vel_to_resonance;
	int8_t reverb_send, chorus_send, delay_send;
};

/* A hack to delay instrument loading until after reading the
	entire MIDI file. */
#define MAGIC_LOAD_INSTRUMENT ((Instrument *)(-1))
#define MAGIC_ERROR_INSTRUMENT ((Instrument *)(-2))
#define IS_MAGIC_INSTRUMENT(ip) ((ip) == MAGIC_LOAD_INSTRUMENT || (ip) == MAGIC_ERROR_INSTRUMENT)

#define DYNAMIC_INSTRUMENT_NAME ""

struct AlternateAssign
{
	/* 128 bit vector:
		* bits[(note >> 5) & 0x3] & (1 << (note & 0x1F))
		*/
	uint32_t bits[4];
	AlternateAssign* next;
};

struct ToneBank
{
	ToneBankElement tone[128];
	AlternateAssign *alt;
};

struct SpecialPatch /* To be used MIDI Module play mode */
{
	int type;
	int samples;
	Sample *sample;
	char *name;
	int32_t sample_offset;
};

enum instrument_mapID
{
	INST_NO_MAP = 0,
	SC_55_TONE_MAP,
	SC_55_DRUM_MAP,
	SC_88_TONE_MAP,
	SC_88_DRUM_MAP,
	SC_88PRO_TONE_MAP,
	SC_88PRO_DRUM_MAP,
	SC_8850_TONE_MAP,
	SC_8850_DRUM_MAP,
	XG_NORMAL_MAP,
	XG_SFX64_MAP,
	XG_SFX126_MAP,
	XG_DRUM_MAP,
	GM2_TONE_MAP,
	GM2_DRUM_MAP,
	NUM_INST_MAP
};

enum
{
	MAP_BANK_COUNT = 256,
	NSPECIAL_PATCH = 256,
	SPECIAL_PROGRAM = -1,
	MAX_MREL = 5000,
	DEFAULT_MREL = 800,
};

struct SFInsts;
struct InstList;
struct SampleList;
struct AIFFCommonChunk;
struct AIFFSoundDataChunk;
struct  SampleImporter;

class Instruments
{
	std::string configFileName;
    MusicIO::SoundFontReaderInterface *sfreader;

	ToneBank standard_tonebank, standard_drumset;

	enum
	{
		INSTRUMENT_HASH_SIZE = 128,
	};

	struct InstrumentCache
	{
		char *name;
		int panning, amp, note_to_use, strip_loop, strip_envelope, strip_tail;
		Instrument *ip;
		InstrumentCache *next;
	};

	InstrumentCache *instrument_cache[INSTRUMENT_HASH_SIZE] = { nullptr };

	/* bank mapping (mapped bank) */
	struct bank_map_elem
	{
		int16_t used = 0, mapid = 0;
		int bankno = 0;
	};
	bank_map_elem map_bank[MAP_BANK_COUNT], map_drumset[MAP_BANK_COUNT];
	int map_bank_counter = 0;

	struct inst_map_elem
	{
		int set, elem, mapped;
	};

	inst_map_elem *inst_map_table[NUM_INST_MAP][128] = { { nullptr} };

	struct UserInstrument
	{
		int8_t bank;
		int8_t prog;
		int8_t source_map;
		int8_t source_bank;
		int8_t source_prog;
		int8_t vibrato_rate;
		int8_t vibrato_depth;
		int8_t cutoff_freq;
		int8_t resonance;
		int8_t env_attack;
		int8_t env_decay;
		int8_t env_release;
		int8_t vibrato_delay;
		UserInstrument *next;
	};

	UserInstrument *userinst_first = (UserInstrument *)NULL;
	UserInstrument *userinst_last = (UserInstrument *)NULL;

	struct UserDrumset {
		int8_t bank;
		int8_t prog;
		int8_t play_note;
		int8_t level;
		int8_t assign_group;
		int8_t pan;
		int8_t reverb_send_level;
		int8_t chorus_send_level;
		int8_t rx_note_off;
		int8_t rx_note_on;
		int8_t delay_send_level;
		int8_t source_map;
		int8_t source_prog;
		int8_t source_note;
		UserDrumset *next;
	};

	struct SFBags
	{
		int nbags;
		uint16_t *bag;
		int ngens;
		SFGenRec *gen;
	};

	SFBags prbags, inbags;

	UserDrumset *userdrum_first = (UserDrumset *)NULL;
	UserDrumset *userdrum_last = (UserDrumset *)NULL;

	AlternateAssign alt[2];

	/* Some functions get aggravated if not even the standard banks are available. */
	ToneBank
		*tonebank[128 + MAP_BANK_COUNT] = { &standard_tonebank },
		*drumset[128 + MAP_BANK_COUNT] = { &standard_drumset };

	Instrument *default_instrument = 0;
	SpecialPatch *special_patch[NSPECIAL_PATCH] = { nullptr };
	int default_program[MAX_CHANNELS] = { 0 };	/* This is only used for tracks that don't specify a program */

	char *default_instrument_name = nullptr;
	int progbase = 0;
	int32_t modify_release = 0;
	bool opt_sf_close_each_file = true;
	char def_instr_name[256] = { '\0' };
	SFInsts *sfrecs = nullptr;
	SFInsts *current_sfrec = nullptr;

	int last_sample_type = 0;
	int last_sample_instrument = 0;
	int last_sample_keyrange = 0;
	SampleList *last_sample_list = nullptr;

	LayerItem layer_items[SF_EOF];

	/* convert from 8bit value to fractional offset (15.15) */
	int32_t to_offset_22(int offset)
	{
		return (int32_t)offset << (7 + 15);
	}

	int32_t calc_rate_i(int diff, double msec);
	int32_t convert_envelope_rate(uint8_t rate);
	int32_t convert_envelope_offset(uint8_t offset);
	int32_t convert_tremolo_sweep(uint8_t sweep);
	int32_t convert_vibrato_sweep(uint8_t sweep, int32_t vib_control_ratio);
	int32_t convert_tremolo_rate(uint8_t rate);
	int32_t convert_vibrato_rate(uint8_t rate);
	void  reverse_data(int16_t *sp, int32_t ls, int32_t le);
	int name_hash(char *name);
	Instrument *search_instrument_cache(char *name, int panning, int amp, int note_to_use, int strip_loop, int strip_envelope, int strip_tail);
	void store_instrument_cache(Instrument *ip, char *name, int panning, int amp, int note_to_use, int strip_loop, int strip_envelope, int strip_tail);
	int32_t to_rate(int rate);
	void apply_bank_parameter(Instrument *ip, ToneBankElement *tone);
	Instrument *load_gus_instrument(char *name, ToneBank *bank, int dr, int prog);
	int fill_bank(int dr, int b, int *rc);
	void free_tone_bank_list(ToneBank *tb[]);
	void free_tone_bank(void);
	void free_instrument_map(void);
	int set_default_instrument(char *name);
	void *safe_memdup(void *s, size_t size);
	void MarkInstrument(int banknum, int percussion, int instr);

	//smplfile.c
	Instrument *extract_sample_file(char *);
	int32_t convert_envelope_rate_s(uint8_t rate);
	void initialize_sample(Instrument *inst, int frames, int sample_bits, int sample_rate);
	int get_importers(const char *sample_file, int limit, SampleImporter **importers);
	int get_next_importer(char *sample_file, int start, int count, SampleImporter **importers);

	int import_wave_discriminant(char *sample_file);
	int import_wave_load(char *sample_file, Instrument *inst);
	int import_aiff_discriminant(char *sample_file);
	int import_aiff_load(char *sample_file, Instrument *inst);
	int read_AIFFCommonChunk(timidity_file *tf, AIFFCommonChunk *comm, int csize, int compressed);
	int read_AIFFSoundData(timidity_file *tf, Instrument *inst, AIFFCommonChunk *common);
	int read_AIFFSoundDataChunk(timidity_file *tf, AIFFSoundDataChunk *sound, int csize, int mode);

	// sndfont.cpp

	SFInsts *find_soundfont(char *sf_file);
	SFInsts *new_soundfont(char *sf_file);
	void init_sf(SFInsts *rec);
	void end_soundfont(SFInsts *rec);
	Instrument *try_load_soundfont(SFInsts *rec, int order, int bank, int preset, int keynote);
	Instrument *load_from_file(SFInsts *rec, InstList *ip);
	int is_excluded(SFInsts *rec, int bank, int preset, int keynote);
	int is_ordered(SFInsts *rec, int bank, int preset, int keynote);
	int load_font(SFInfo *sf, int pridx);
	int parse_layer(SFInfo *sf, int pridx, LayerTable *tbl, int level);
	int is_global(SFGenLayer *layer);
	void clear_table(LayerTable *tbl);
	void set_to_table(SFInfo *sf, LayerTable *tbl, SFGenLayer *lay, int level);
	void add_item_to_table(LayerTable *tbl, int oper, int amount, int level);
	void merge_table(SFInfo *sf, LayerTable *dst, LayerTable *src);
	void init_and_merge_table(SFInfo *sf, LayerTable *dst, LayerTable *src);
	int sanity_range(LayerTable *tbl);
	int make_patch(SFInfo *sf, int pridx, LayerTable *tbl);
	void make_info(SFInfo *sf, SampleList *vp, LayerTable *tbl);
	double calc_volume(LayerTable *tbl);
	void set_sample_info(SFInfo *sf, SampleList *vp, LayerTable *tbl);
	void set_init_info(SFInfo *sf, SampleList *vp, LayerTable *tbl);
	void reset_last_sample_info(void);
	int abscent_to_Hz(int abscents);
	void set_rootkey(SFInfo *sf, SampleList *vp, LayerTable *tbl);
	void set_rootfreq(SampleList *vp);
	int32_t to_offset(int32_t offset);
	int32_t to_rate(int32_t diff, int timecent);
	int32_t calc_rate(int32_t diff, double msec);
	double to_msec(int timecent);
	int32_t calc_sustain(int sust_cB);
	void convert_volume_envelope(SampleList *vp, LayerTable *tbl);
	void convert_tremolo(SampleList *vp, LayerTable *tbl);
	void convert_vibrato(SampleList *vp, LayerTable *tbl);
	void set_envelope_parameters(SampleList *vp);

	// configfile

	int set_patchconf(const char *name, int line, ToneBank *bank, char *w[], int dr, int mapid, int bankmapfrom, int bankno);
	int strip_trailing_comment(char *string, int next_token_index);
	char *expand_variables(char *string, MBlockList *varbuf, const char *basedir);
	int set_gus_patchconf(const char *name, int line, ToneBankElement *tone, char *pat, char **opts);
	void reinit_tone_bank_element(ToneBankElement *tone);
	int set_gus_patchconf_opts(const char *name, int line, char *opts, ToneBankElement *tone);
	int copymap(int mapto, int mapfrom, int isdrum);
	void copybank(ToneBank *to, ToneBank *from, int mapid, int bankmapfrom, int bankno);

	// sffile.cpp

	int chunkid(char *id);
	int process_list(int size, SFInfo *sf, timidity_file *fd);
	int process_info(int size, SFInfo *sf, timidity_file *fd);
	int process_sdta(int size, SFInfo *sf, timidity_file *fd);
	int process_pdta(int size, SFInfo *sf, timidity_file *fd);
	void load_sample_names(int size, SFInfo *sf, timidity_file *fd);
	void load_preset_header(int size, SFInfo *sf, timidity_file *fd);
	void load_inst_header(int size, SFInfo *sf, timidity_file *fd);
	void load_bag(int size, SFBags *bagp, timidity_file *fd);
	void load_gen(int size, SFBags *bagp, timidity_file *fd);
	void load_sample_info(int size, SFInfo *sf, timidity_file *fd);
	void convert_layers(SFInfo *sf);
	void generate_layers(SFHeader *hdr, SFHeader *next, SFBags *bags);
	void free_layer(SFHeader *hdr);
	int load_soundfont(SFInfo *sf, timidity_file *fd);
	void free_soundfont(SFInfo *sf);
	void correct_samples(SFInfo *sf);


public:

	Instruments();
	bool load(MusicIO::SoundFontReaderInterface *);
	~Instruments();

	const ToneBank *toneBank(int i) const
	{
		return tonebank[i];
	}

	int defaultProgram(int i) const
	{
		return default_program[i];
	}

	const ToneBank *drumSet(int i) const
	{
		return drumset[i];
	}

	const SpecialPatch *specialPatch(int i) const
	{
		return special_patch[i];
	}

	void setSpecialPatchOffset(int i, int32_t ofs)
	{
		special_patch[i]->sample_offset = ofs;
	}
	Instrument *defaultInstrument() const
	{
		return default_instrument;
	}

	/* instrum.c */
	int load_missing_instruments(int *rc);
	void free_instruments(int reload_default_inst);
	void free_special_patch(int id);
	void clear_magic_instruments(void);
	Instrument *load_instrument(int dr, int b, int prog);
	int find_instrument_map_bank(int dr, int map, int bk);
	int alloc_instrument_map_bank(int dr, int map, int bk);
	void alloc_instrument_bank(int dr, int bankset);
	int instrument_map(int mapID, int *set_in_out, int *elem_in_out) const;
	void set_instrument_map(int mapID, int set_from, int elem_from, int set_to, int elem_to);
	AlternateAssign *add_altassign_string(AlternateAssign *old, char **params, int n);
	AlternateAssign *find_altassign(AlternateAssign *altassign, int note);
	void copy_tone_bank_element(ToneBankElement *elm, const ToneBankElement *src);
	void free_tone_bank_element(ToneBankElement *elm);
	void free_instrument(Instrument *ip);
	void squash_sample_16to8(Sample *sp);
	Instrument *play_midi_load_instrument(int dr, int bk, int prog, bool *pLoad_success);
	void recompute_userinst(int bank, int prog);
	Instrument *recompute_userdrum(int bank, int prog);
	UserInstrument *get_userinst(int bank, int prog);
	UserDrumset *get_userdrum(int bank, int prog);
	void recompute_userdrum_altassign(int bank, int group);
	/*! initialize GS user drumset. */
	void init_userdrum();
	void free_userdrum();
	void init_userinst() { free_userinst(); }
	void free_userinst();

	void mark_instrument(int newbank, int newprog)
	{
		if (!(tonebank[newbank]->tone[newprog].instrument))
			tonebank[newbank]->tone[newprog].instrument =
			MAGIC_LOAD_INSTRUMENT;
	}

	void mark_drumset(int newbank, int newprog)
	{
		if (!(drumset[newbank]->tone[newprog].instrument))
			drumset[newbank]->tone[newprog].instrument =
			MAGIC_LOAD_INSTRUMENT;
	}

	/* sndfont.c */
	void add_soundfont(char *sf_file, int sf_order, int cutoff_allowed, int resonance_allowed, int amp);
	void remove_soundfont(char *sf_file);
	void init_load_soundfont(void);
	Instrument *load_soundfont_inst(int order, int bank, int preset, int keynote);
	Instrument *extract_soundfont(char *sf_file, int bank, int preset, int keynote);
	int exclude_soundfont(int bank, int preset, int keynote);
	int order_soundfont(int bank, int preset, int keynote, int order);
	char *soundfont_preset_name(int bank, int preset, int keynote, char **sndfile);
	void free_soundfonts(void);
	void PrecacheInstruments(const uint16_t *instruments, int count);


	int read_config_file(const char *name, int self, int allow_missing_file);

	void set_default_instrument()
	{
		set_default_instrument(def_instr_name);
	}
};


}
#endif /* ___INSTRUM_H_ */
