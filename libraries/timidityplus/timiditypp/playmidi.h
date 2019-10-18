/*
    TiMidity++ -- MIDI to WAVE converter and player
    Copyright (C) 1999-2004 Masanao Izumo <iz@onicos.co.jp>
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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA

    playmidi.h
*/

#ifndef ___PLAYMIDI_H_
#define ___PLAYMIDI_H_
#include <stdint.h>

namespace TimidityPlus
{

struct AlternateAssign;

struct MidiEvent 
{
  uint8_t type, channel, a, b;
};

#define REDUCE_CHANNELS		16
#define REVERB_MAX_DELAY_OUT (4 * playback_rate)

#define SYSEX_TAG 0xFF

/* Midi events */
enum midi_event_t
{
	ME_NONE,
	
	/* MIDI events */
	ME_NOTEOFF,
	ME_NOTEON,
	ME_KEYPRESSURE,
	ME_PROGRAM,
	ME_CHANNEL_PRESSURE,
	ME_PITCHWHEEL,
	
	/* Controls */
	ME_TONE_BANK_MSB,
	ME_TONE_BANK_LSB,
	ME_MODULATION_WHEEL,
	ME_BREATH,
	ME_FOOT,
	ME_MAINVOLUME,
	ME_BALANCE,
	ME_PAN,
	ME_EXPRESSION,
	ME_SUSTAIN,
	ME_PORTAMENTO_TIME_MSB,
	ME_PORTAMENTO_TIME_LSB,
	ME_PORTAMENTO,
	ME_PORTAMENTO_CONTROL,
	ME_DATA_ENTRY_MSB,
	ME_DATA_ENTRY_LSB,
	ME_SOSTENUTO,
	ME_SOFT_PEDAL,
	ME_LEGATO_FOOTSWITCH,
	ME_HOLD2,
	ME_HARMONIC_CONTENT,
	ME_RELEASE_TIME,
	ME_ATTACK_TIME,
	ME_BRIGHTNESS,
	ME_REVERB_EFFECT,
	ME_TREMOLO_EFFECT,
	ME_CHORUS_EFFECT,
	ME_CELESTE_EFFECT,
	ME_PHASER_EFFECT,
	ME_RPN_INC,
	ME_RPN_DEC,
	ME_NRPN_LSB,
	ME_NRPN_MSB,
	ME_RPN_LSB,
	ME_RPN_MSB,
	ME_ALL_SOUNDS_OFF,
	ME_RESET_CONTROLLERS,
	ME_ALL_NOTES_OFF,
	ME_MONO,
	ME_POLY,
	
	/* TiMidity Extensionals */
	ME_MASTER_TUNING,		/* Master tuning */
	ME_SCALE_TUNING,		/* Scale tuning */
	ME_BULK_TUNING_DUMP,	/* Bulk tuning dump */
	ME_SINGLE_NOTE_TUNING,	/* Single-note tuning */
	ME_RANDOM_PAN,
	ME_SET_PATCH,			/* Install special instrument */
	ME_DRUMPART,
	ME_KEYSHIFT,
	ME_PATCH_OFFS,			/* Change special instrument sample position
							 * Channel, LSB, MSB
							 */
	
	/* Global channel events */
	ME_TEMPO,
	ME_CHORUS_TEXT,
	ME_LYRIC,
	ME_GSLCD,				/* GS L.C.D. Exclusive message event */
	ME_MARKER,
	ME_INSERT_TEXT,			/* for SC */
	ME_TEXT,
	ME_KARAOKE_LYRIC,		/* for KAR format */
	ME_MASTER_VOLUME,
	ME_RESET,				/* Reset and change system mode */
	ME_NOTE_STEP,
	
	ME_TIMESIG,				/* Time signature */
	ME_KEYSIG,				/* Key signature */
	ME_TEMPER_KEYSIG,		/* Temperament key signature */
	ME_TEMPER_TYPE,			/* Temperament type */
	ME_MASTER_TEMPER_TYPE,	/* Master temperament type */
	ME_USER_TEMPER_ENTRY,	/* User-defined temperament entry */
	
	ME_SYSEX_LSB,			/* Universal system exclusive message (LSB) */
	ME_SYSEX_MSB,			/* Universal system exclusive message (MSB) */
	ME_SYSEX_GS_LSB,		/* GS system exclusive message (LSB) */
	ME_SYSEX_GS_MSB,		/* GS system exclusive message (MSB) */
	ME_SYSEX_XG_LSB,		/* XG system exclusive message (LSB) */
	ME_SYSEX_XG_MSB,		/* XG system exclusive message (MSB) */
	
	ME_WRD,					/* for MIMPI WRD tracer */
	ME_SHERRY,				/* for Sherry WRD tracer */
	ME_BARMARKER,
	ME_STEP,				/* for Metronome */
	
	ME_LAST = 254,			/* Last sequence of MIDI list.
							 * This event is reserved for realtime player.
							 */
	ME_EOT = 255			/* End of MIDI.  Finish to play */
};

#define GLOBAL_CHANNEL_EVENT_TYPE(type)	\
	((type) == ME_NONE || (type) >= ME_TEMPO)

enum rpn_data_address_t /* NRPN/RPN */
{
    NRPN_ADDR_0108,
    NRPN_ADDR_0109,
    NRPN_ADDR_010A,
    NRPN_ADDR_0120,
    NRPN_ADDR_0121,
	NRPN_ADDR_0130,
	NRPN_ADDR_0131,
	NRPN_ADDR_0134,
	NRPN_ADDR_0135,
    NRPN_ADDR_0163,
    NRPN_ADDR_0164,
    NRPN_ADDR_0166,
    NRPN_ADDR_1400,
    NRPN_ADDR_1500,
    NRPN_ADDR_1600,
    NRPN_ADDR_1700,
    NRPN_ADDR_1800,
    NRPN_ADDR_1900,
    NRPN_ADDR_1A00,
    NRPN_ADDR_1C00,
    NRPN_ADDR_1D00,
    NRPN_ADDR_1E00,
    NRPN_ADDR_1F00,
    NRPN_ADDR_3000,
    NRPN_ADDR_3100,
    NRPN_ADDR_3400,
    NRPN_ADDR_3500,
    RPN_ADDR_0000,
    RPN_ADDR_0001,
    RPN_ADDR_0002,
    RPN_ADDR_0003,
    RPN_ADDR_0004,
	RPN_ADDR_0005,
    RPN_ADDR_7F7F,
    RPN_ADDR_FFFF,
    RPN_MAX_DATA_ADDR
};

#define RX_PITCH_BEND (1<<0)
#define RX_CH_PRESSURE (1<<1)
#define RX_PROGRAM_CHANGE (1<<2)
#define RX_CONTROL_CHANGE (1<<3)
#define RX_POLY_PRESSURE (1<<4)
#define RX_NOTE_MESSAGE (1<<5)
#define RX_RPN (1<<6)
#define RX_NRPN (1<<7)
#define RX_MODULATION (1<<8)
#define RX_VOLUME (1<<9)
#define RX_PANPOT (1<<10)
#define RX_EXPRESSION (1<<11)
#define RX_HOLD1 (1<<12)
#define RX_PORTAMENTO (1<<13)
#define RX_SOSTENUTO (1<<14)
#define RX_SOFT (1<<15)
#define RX_NOTE_ON (1<<16)
#define RX_NOTE_OFF (1<<17)
#define RX_BANK_SELECT (1<<18)
#define RX_BANK_SELECT_LSB (1<<19)

enum {
	EG_ATTACK = 0,
	EG_DECAY = 2,
	EG_DECAY1 = 1,
	EG_DECAY2 = 2,
	EG_RELEASE = 3,
	EG_NULL = 5,
	EG_GUS_ATTACK = 0,
	EG_GUS_DECAY = 1,
	EG_GUS_SUSTAIN = 2,
	EG_GUS_RELEASE1 = 3,
	EG_GUS_RELEASE2 = 4,
	EG_GUS_RELEASE3 = 5,
	EG_SF_ATTACK = 0,
	EG_SF_HOLD = 1,
	EG_SF_DECAY = 2,
	EG_SF_RELEASE = 3,
};

#ifndef PART_EQ_XG
#define PART_EQ_XG
/*! shelving filter */
struct filter_shelving
{
	double freq, gain, q;
	int32_t x1l, x2l, y1l, y2l, x1r, x2r, y1r, y2r;
	int32_t a1, a2, b0, b1, b2;
};

/*! Part EQ (XG) */
struct part_eq_xg {
	int8_t bass, treble, bass_freq, treble_freq;
	filter_shelving basss, trebles;
	int8_t valid;
};
#endif /* PART_EQ_XG */

struct midi_controller 
{
  int16_t val;
  int8_t pitch;	/* in +-semitones [-24, 24] */
  int16_t cutoff;	/* in +-cents [-9600, 9600] */
  float amp;	/* [-1.0, 1.0] */
  /* in GS, LFO1 means LFO for voice 1, LFO2 means LFO for voice2.
     LFO2 is not supported. */
  float lfo1_rate, lfo2_rate;	/* in +-Hz [-10.0, 10.0] */
  int16_t lfo1_pitch_depth, lfo2_pitch_depth;	/* in cents [0, 600] */
  int16_t lfo1_tvf_depth, lfo2_tvf_depth;	/* in cents [0, 2400] */
  float lfo1_tva_depth, lfo2_tva_depth;	/* [0, 1.0] */
  int8_t variation_control_depth, insertion_control_depth;
};

struct DrumPartEffect
{
	int32_t *buf;
	int8_t note, reverb_send, chorus_send, delay_send;
};

struct DrumParts
{
    int8_t drum_panning;
    int32_t drum_envelope_rate[6]; /* drum instrument envelope */
    int8_t pan_random;    /* flag for drum random pan */
	float drum_level;

	int8_t chorus_level, reverb_level, delay_level, coarse, fine,
		play_note, drum_cutoff_freq, drum_resonance;
	int32_t rx;
};

struct Channel
{
  int8_t	bank_msb, bank_lsb, bank, program, volume,
	expression, sustain, panning, mono, portamento,
	key_shift, loop_timeout;

  /* chorus, reverb... Coming soon to a 300-MHz, eight-way superscalar
     processor near you */
  int8_t	chorus_level,	/* Chorus level */
	reverb_level;	/* Reverb level. */
  int	reverb_id;	/* Reverb ID used for reverb optimize implementation
			   >=0 reverb_level
			   -1: DEFAULT_REVERB_SEND_LEVEL
			   */
  int8_t delay_level;	/* Delay Send Level */
  int8_t eq_gs;	/* EQ ON/OFF (GS) */
  int8_t insertion_effect;

  /* Special sample ID. (0 means Normal sample) */
  uint8_t special_sample;

  int pitchbend;

  double
    pitchfactor; /* precomputed pitch bend factor to save some fdiv's */

  /* For portamento */
  uint8_t portamento_time_msb, portamento_time_lsb;
  int porta_control_ratio, porta_dpb;
  int32_t last_note_fine;

  /* For Drum part */
  struct DrumParts *drums[128];

  /* For NRPN Vibrato */
  int32_t vibrato_depth, vibrato_delay;
  float vibrato_ratio;

  /* For RPN */
  uint8_t rpnmap[RPN_MAX_DATA_ADDR]; /* pseudo RPN address map */
  uint8_t rpnmap_lsb[RPN_MAX_DATA_ADDR];
  uint8_t lastlrpn, lastmrpn;
  int8_t  nrpn; /* 0:RPN, 1:NRPN, -1:Undefined */
  int rpn_7f7f_flag;		/* Boolean flag used for RPN 7F/7F */

  /* For channel envelope */
  int32_t envelope_rate[6]; /* for Envelope Generator in mix.c
			   * 0: value for attack rate
			   * 2: value for decay rate
			   * 3: value for release rate
			   */

  int mapID;			/* Program map ID */
  AlternateAssign *altassign;	/* Alternate assign patch table */
  int32_t lasttime;     /* Last sample time of computed voice on this channel */

  /* flag for random pan */
  int pan_random;

  /* for Voice LPF / Resonance */
  int8_t param_resonance, param_cutoff_freq;	/* -64 ~ 63 */
  float cutoff_freq_coef, resonance_dB;

  int8_t velocity_sense_depth, velocity_sense_offset;
  
  int8_t scale_tuning[12], prev_scale_tuning;
  int8_t temper_type;

  int8_t soft_pedal;
  int8_t sostenuto;
  int8_t damper_mode;

  int8_t tone_map0_number;
  double pitch_offset_fine;	/* in Hz */
  int8_t assign_mode;

  int8_t legato;	/* legato footswitch */
  int8_t legato_flag;	/* note-on flag for legato */

  midi_controller mod, bend, caf, paf, cc1, cc2;

  ChannelBitMask channel_layer;
  int port_select;

  struct part_eq_xg eq_xg;

  int8_t dry_level;
  int8_t note_limit_high, note_limit_low;	/* Note Limit (Keyboard Range) */
  int8_t vel_limit_high, vel_limit_low;	/* Velocity Limit */
  int32_t rx;	/* Rx. ~ (Rcv ~) */

  int drum_effect_num;
  int8_t drum_effect_flag;
  struct DrumPartEffect *drum_effect;

  int8_t sysex_gs_msb_addr, sysex_gs_msb_val,
		sysex_xg_msb_addr, sysex_xg_msb_val, sysex_msb_addr, sysex_msb_val;
};

/* Causes the instrument's default panning to be used. */
#define NO_PANNING -1

typedef struct {
	int16_t freq, last_freq, orig_freq;
	double reso_dB, last_reso_dB, orig_reso_dB, reso_lin; 
	int8_t type;	/* filter type. 0: Off, 1: 12dB/oct, 2: 24dB/oct */ 
	int32_t f, q, p;	/* coefficients in fixed-point */
	int32_t b0, b1, b2, b3, b4;
	float gain;
	int8_t start_flag;
} FilterCoefficients;

#define ENABLE_PAN_DELAY
#define PAN_DELAY_BUF_MAX 48	/* 0.5ms in 96kHz */

typedef struct {
  uint8_t
    status, channel, note, velocity;
  int vid, temper_instant;
  Sample *sample;
  int64_t sample_offset;	/* sample_offset must be signed */
  int32_t
    orig_frequency, frequency, sample_increment,
    envelope_volume, envelope_target, envelope_increment,
    tremolo_sweep, tremolo_sweep_position,
    tremolo_phase, tremolo_phase_increment,
    vibrato_sweep, vibrato_sweep_position;

  final_volume_t left_mix, right_mix;
  int32_t old_left_mix, old_right_mix,
     left_mix_offset, right_mix_offset,
     left_mix_inc, right_mix_inc;

  double
    left_amp, right_amp, tremolo_volume;
  int32_t
    vibrato_sample_increment[VIBRATO_SAMPLE_INCREMENTS], vibrato_delay;
  int
	vibrato_phase, orig_vibrato_control_ratio, vibrato_control_ratio,
    vibrato_depth, vibrato_control_counter,
    envelope_stage, control_counter, panning, panned;
  int16_t tremolo_depth;

  /* for portamento */
  int porta_control_ratio, porta_control_counter, porta_dpb;
  int32_t porta_pb;

  int delay; /* Note ON delay samples */
  int32_t timeout;
  struct cache_hash *cache;

  uint8_t chorus_link;	/* Chorus link */
  int8_t proximate_flag;

  FilterCoefficients fc;

  double envelope_scale, last_envelope_volume;
  int32_t inv_envelope_scale;

  int modenv_stage;
  int32_t
    modenv_volume, modenv_target, modenv_increment;
  double last_modenv_volume;
  int32_t tremolo_delay, modenv_delay;

  int32_t delay_counter;

#ifdef ENABLE_PAN_DELAY
  int32_t *pan_delay_buf, pan_delay_rpt, pan_delay_wpt, pan_delay_spt;
#endif	/* ENABLE_PAN_DELAY */
} Voice;

/* Voice status options: */
#define VOICE_FREE	(1<<0)
#define VOICE_ON	(1<<1)
#define VOICE_SUSTAINED	(1<<2)
#define VOICE_OFF	(1<<3)
#define VOICE_DIE	(1<<4)

/* Voice panned options: */
#define PANNED_MYSTERY 0
#define PANNED_LEFT 1
#define PANNED_RIGHT 2
#define PANNED_CENTER 3
/* Anything but PANNED_MYSTERY only uses the left volume */

enum {
	MODULE_TIMIDITY_DEFAULT = 0x0,
	/* GS modules */
	MODULE_SC55 = 0x1,
	MODULE_SC88 = 0x2,
	MODULE_SC88PRO = 0x3,
	MODULE_SC8850 = 0x4,
	/* XG modules */
	MODULE_MU50 = 0x10,
	MODULE_MU80 = 0x11,
	MODULE_MU90 = 0x12,
	MODULE_MU100 = 0x13,
	/* GM modules */
	MODULE_SBLIVE = 0x20,
	MODULE_SBAUDIGY = 0x21,
	/* Special modules */
	MODULE_TIMIDITY_SPECIAL1 = 0x70,
	MODULE_TIMIDITY_DEBUG = 0x7f,
};



struct midi_file_info
{
	int readflag;
	int16_t hdrsiz;
	int16_t format;
	int16_t tracks;
	int32_t divisions;
	int time_sig_n, time_sig_d, time_sig_c, time_sig_b;	/* Time signature */
	int drumchannels_isset;
	ChannelBitMask drumchannels;
	ChannelBitMask drumchannel_mask;
	int32_t samples;
	int max_channel;
	int compressed; /* True if midi_data is compressed */
};


class Recache;
class Mixer;
class Reverb;
class Effect;

class Player
{
public:
	Channel channel[MAX_CHANNELS];
	Voice voice[max_voices];
	ChannelBitMask default_drumchannel_mask;
	ChannelBitMask default_drumchannels;
	ChannelBitMask drumchannel_mask;
	ChannelBitMask drumchannels;
	double *vol_table;

	// make this private later
	Instruments *instruments;
private:
	int last_reverb_setting;
	Recache *recache;
	Mixer *mixer;
	Reverb *reverb;
	Effect *effect;


	MidiEvent *current_event;
	int32_t sample_count;	/* Length of event_list */
	int32_t current_sample;		/* Number of calclated samples */
	double midi_time_ratio;	/* For speed up/down */
	int computed_samples;

	int note_key_offset = 0;		/* For key up/down */
	ChannelBitMask channel_mute;	/* For channel mute */
	double master_volume;
	int32_t master_volume_ratio;

	int play_system_mode;
	int midi_streaming;
	int volatile stream_max_compute; /* compute time limit (in msec) when streaming */
	int8_t current_keysig;
	int8_t current_temper_keysig;
	int temper_adj;
	int32_t current_play_tempo;
	int opt_realtime_playing;
	int check_eot_flag;
	int playmidi_seek_flag;
	int opt_pure_intonation;
	int current_freq_table;
	int current_temper_freq_table;
	int master_tuning;

	int make_rvid_flag; /* For reverb optimization */

	int32_t amplification;
	int voices, upper_voices;

	struct midi_file_info midifileinfo, *current_file_info;
	MBlockList playmidi_pool;
	int32_t freq_table_user[4][48][128];
	char *reverb_buffer; /* MAX_CHANNELS*AUDIO_BUFFER_SIZE*8 */

	int32_t lost_notes, cut_notes;
	int32_t common_buffer[AUDIO_BUFFER_SIZE * 2], *buffer_pointer; /* stereo samples */
	int16_t wav_buffer[AUDIO_BUFFER_SIZE * 2];

	int32_t insertion_effect_buffer[AUDIO_BUFFER_SIZE * 2];


	/* Ring voice id for each notes.  This ID enables duplicated note. */
	uint8_t vidq_head[128 * MAX_CHANNELS], vidq_tail[128 * MAX_CHANNELS];

	int MIDI_EVENT_NOTE(MidiEvent *ep)
	{
		return (ISDRUMCHANNEL((ep)->channel) ? (ep)->a : (((int)(ep)->a + note_key_offset + channel[ep->channel].key_shift) & 0x7f));
	}

	int16_t conv_lfo_pitch_depth(float val)
	{
		return (int16_t)(0.0318f * val * val + 0.6858f * val + 0.5f);
	}

	int16_t conv_lfo_filter_depth(float val)
	{
		return (int16_t)((0.0318f * val * val + 0.6858f * val) * 4.0f + 0.5f);
	}

	bool IS_SYSEX_EVENT_TYPE(MidiEvent *event);
	double cnv_Hz_to_vib_ratio(double freq);
	int new_vidq(int ch, int note);
	int last_vidq(int ch, int note);
	void reset_voices(void);
	void kill_note(int i);
	void kill_all_voices(void);
	void reset_drum_controllers(struct DrumParts *d[], int note);
	void reset_nrpn_controllers(int c);
	void reset_controllers(int c);
	int32_t calc_velocity(int32_t ch, int32_t vel);
	void recompute_voice_tremolo(int v);
	void recompute_amp(int v);
	void reset_midi(int playing);
	void recompute_channel_filter(int ch, int note);
	void init_voice_filter(int i);
	int reduce_voice(void);
	int find_free_voice(void);
	int get_panning(int ch, int note, int v);
	void init_voice_vibrato(int v);
	void init_voice_pan_delay(int v);
	void init_voice_portamento(int v);
	void init_voice_tremolo(int v);
	void start_note(MidiEvent *e, int i, int vid, int cnt);
	void set_envelope_time(int ch, int val, int stage);
	void new_chorus_voice_alternate(int v1, int level);
	void note_on(MidiEvent *e);
	void update_sostenuto_controls(int ch);
	void update_redamper_controls(int ch);
	void note_off(MidiEvent *e);
	void all_notes_off(int c);
	void all_sounds_off(int c);
	void adjust_pressure(MidiEvent *e);
	void adjust_channel_pressure(MidiEvent *e);
	void adjust_panning(int c);
	void adjust_drum_panning(int ch, int note);
	void drop_sustain(int c);
	void adjust_all_pitch(void);
	void adjust_pitch(int c);
	void adjust_volume(int c);
	void set_reverb_level(int ch, int level);
	void make_drum_effect(int ch);
	void adjust_master_volume(void);
	void add_channel_layer(int to_ch, int from_ch);
	void remove_channel_layer(int ch);
	void process_sysex_event(int ev, int ch, int val, int b);
	double gs_cnv_vib_rate(int rate);
	int32_t gs_cnv_vib_depth(int depth);
	int32_t gs_cnv_vib_delay(int delay);
	int last_rpn_addr(int ch);
	void voice_increment(int n);
	void voice_decrement(int n);
	void voice_decrement_conservative(int n);
	void mix_signal(int32_t *dest, int32_t *src, int32_t count);
	int is_insertion_effect_xg(int ch);
	void do_compute_data(int32_t count);
	int check_midi_play_end(MidiEvent *e, int len);
	int midi_play_end(void);
	void update_modulation_wheel(int ch);
	void drop_portamento(int ch);
	void update_portamento_time(int ch);
	void update_legato_controls(int ch);
	void set_master_tuning(int tune);
	struct midi_file_info *new_midi_file_info();

	void adjust_amplification(void);
	void init_freq_table_user(void);
	int find_samples(MidiEvent *, int *);
	int select_play_sample(Sample *, int, int *, int *, MidiEvent *);
	double get_play_note_ratio(int, int);
	int find_voice(MidiEvent *);
	void finish_note(int i);
	void update_portamento_controls(int ch);
	void update_rpn_map(int ch, int addr, int update_now);
	void set_single_note_tuning(int, int, int, int);
	void set_user_temper_entry(int, int, int);
	void recompute_bank_parameter(int, int);
	float calc_drum_tva_level(int ch, int note, int level);
	int32_t calc_random_delay(int ch, int note);



	/* XG Part EQ */
	void init_part_eq_xg(struct part_eq_xg *);
	void recompute_part_eq_xg(struct part_eq_xg *);
	/* MIDI controllers (MW, Bend, CAf, PAf,...) */
	void init_midi_controller(midi_controller *);
	float get_midi_controller_amp(midi_controller *);
	float get_midi_controller_filter_cutoff(midi_controller *);
	float get_midi_controller_filter_depth(midi_controller *);
	int32_t get_midi_controller_pitch(midi_controller *);
	int16_t get_midi_controller_pitch_depth(midi_controller *);
	int16_t get_midi_controller_amp_depth(midi_controller *);
	/* Rx. ~ (Rcv ~) */
	void init_rx(int);
	void set_rx(int, int32_t, int);
	void init_rx_drum(struct DrumParts *);
	void set_rx_drum(struct DrumParts *, int32_t, int);
	int32_t get_rx_drum(struct DrumParts *, int32_t);
	int convert_midi_control_change(int chn, int type, int val, MidiEvent *ev_ret);


public:
	Player(Instruments *);
	~Player();

	bool ISDRUMCHANNEL(int c)
	{
		return !!IS_SET_CHANNELMASK(drumchannels, c);
	}

	int midi_drumpart_change(int ch, int isdrum);
	int get_reverb_level(int ch);
	int get_chorus_level(int ch);
	Instrument *play_midi_load_instrument(int dr, int bk, int prog);
	void midi_program_change(int ch, int prog);
	void free_voice(int v);
	void play_midi_setup_drums(int ch, int note);

	/* For stream player */
	void playmidi_stream_init(void);
	void playmidi_tmr_reset(void);
	int play_event(MidiEvent *ev);

	void recompute_voice_filter(int);

	void free_drum_effect(int);
	void change_system_mode(int mode);
	void recompute_freq(int v);
	int get_default_mapID(int ch);
	void init_channel_layer(int ch);
	int compute_data(float *buffer, int32_t count);
	int send_event(int status, int parm1, int parm2);
	void send_long_event(const uint8_t *sysexbuffer, int exlen);
};

class SysexConvert
{
	const int midi_port_number = 0;
	uint8_t rhythm_part[2] = { 0,0 };	/* for GS */
	uint8_t drum_setup_xg[16] = { 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9, 9 };	/* for XG */

public:
	int parse_sysex_event_multi(const uint8_t *val, int32_t len, MidiEvent *evm, Instruments *instruments);
	int parse_sysex_event(const uint8_t *val, int32_t len, MidiEvent *ev, Instruments *instruments);
};

void free_gauss_table(void);
void set_playback_rate(int freq);


}

#endif /* ___PLAYMIDI_H_ */
