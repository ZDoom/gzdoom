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
*/

/*
 * REVERB EFFECT FOR TIMIDITY++-1.X (Version 0.06e  1999/1/28)
 *
 * Copyright (C) 1997,1998,1999  Masaki Kiryu <mkiryu@usa.net>
 *                           (http://w3mb.kcom.ne.jp/~mkiryu/)
 *
 * reverb.h
 *
 */
#ifndef ___REVERB_H_
#define ___REVERB_H_

#include <stdint.h>

namespace TimidityPlus
{


#define DEFAULT_REVERB_SEND_LEVEL 40


/*                    */
/*  Effect Utitities  */
/*                    */
/*! simple delay */
typedef struct {
	int32_t *buf, size, index;
} simple_delay;

/*! Pink Noise Generator */
typedef struct {
	float b0, b1, b2, b3, b4, b5, b6;
} pink_noise;


#ifndef SINE_CYCLE_LENGTH
#define SINE_CYCLE_LENGTH 1024
#endif

/*! LFO */
struct lfo {
	int32_t buf[SINE_CYCLE_LENGTH];
	int32_t count, cycle;	/* in samples */
	int32_t icycle;	/* proportional to (SINE_CYCLE_LENGTH / cycle) */
	int type;	/* current content of its buffer */
	double freq;	/* in Hz */
};

enum {
	LFO_NONE = 0,
	LFO_SINE,
	LFO_TRIANGULAR,
};

/*! modulated delay with allpass interpolation */
typedef struct {
	int32_t *buf, size, rindex, windex, hist;
	int32_t ndelay, depth;	/* in samples */
} mod_delay;

/*! modulated allpass filter with allpass interpolation */
typedef struct {
	int32_t *buf, size, rindex, windex, hist;
	int32_t ndelay, depth;	/* in samples */
	double feedback;
	int32_t feedbacki;
} mod_allpass;

/*! Moog VCF (resonant IIR state variable filter) */
typedef struct {
	int16_t freq, last_freq;	/* in Hz */
	double res_dB, last_res_dB; /* in dB */
	int32_t f, q, p;	/* coefficients in fixed-point */
	int32_t b0, b1, b2, b3, b4;
} filter_moog;

/*! Moog VCF (resonant IIR state variable filter with distortion) */
typedef struct {
	int16_t freq, last_freq;	/* in Hz */
	double res_dB, last_res_dB; /* in dB */
	double dist, last_dist, f, q, p, d, b0, b1, b2, b3, b4;
} filter_moog_dist;

/*! LPF18 (resonant IIR lowpass filter with waveshaping) */
typedef struct {
	int16_t freq, last_freq;	/* in Hz */
	double dist, res, last_dist, last_res; /* in linear */
	double ay1, ay2, aout, lastin, kres, value, kp, kp1h;
} filter_lpf18;

/*! 1st order lowpass filter */
typedef struct {
	double a;
	int32_t ai, iai;	/* coefficients in fixed-point */
	int32_t x1l, x1r;
} filter_lowpass1;

/*! lowpass / highpass filter */
typedef struct {
	double freq, q, last_freq, last_q;
	int32_t x1l, x2l, y1l, y2l, x1r, x2r, y1r, y2r;
	int32_t a1, a2, b1, b02;
} filter_biquad;

#ifndef PART_EQ_XG
#define PART_EQ_XG
/*! shelving filter */
typedef struct {
	double freq, gain, q;
	int32_t x1l, x2l, y1l, y2l, x1r, x2r, y1r, y2r;
	int32_t a1, a2, b0, b1, b2;
} filter_shelving;

struct part_eq_xg {
	int8_t bass, treble, bass_freq, treble_freq;
	filter_shelving basss, trebles;
	int8_t valid;
};
#endif /* PART_EQ_XG */


/*! peaking filter */
typedef struct {
	double freq, gain, q;
	int32_t x1l, x2l, y1l, y2l, x1r, x2r, y1r, y2r;
	int32_t ba1, a2, b0, b2;
} filter_peaking;


/*! allpass filter */
typedef struct _allpass {
	int32_t *buf, size, index;
	double feedback;
	int32_t feedbacki;
} allpass;

/*! comb filter */
typedef struct _comb {
	int32_t *buf, filterstore, size, index;
	double feedback, damp1, damp2;
	int32_t feedbacki, damp1i, damp2i;
} comb;

/*                                  */
/*  Insertion and Variation Effect  */
/*                                  */
struct effect_xg_t {
	int8_t use_msb, type_msb, type_lsb, param_lsb[16], param_msb[10],
		ret, pan, send_reverb, send_chorus, connection, part,
		mw_depth, bend_depth, cat_depth, ac1_depth, ac2_depth, cbc1_depth,
		cbc2_depth;
	struct _EffectList *ef;
};


enum {
	EFFECT_NONE,
	EFFECT_EQ2,
	EFFECT_EQ3,
	EFFECT_STEREO_EQ,
	EFFECT_OVERDRIVE1,
	EFFECT_DISTORTION1,
	EFFECT_OD1OD2,
	EFFECT_CHORUS,	
	EFFECT_FLANGER,
	EFFECT_SYMPHONIC,
	EFFECT_CHORUS_EQ3,
	EFFECT_STEREO_OVERDRIVE,
	EFFECT_STEREO_DISTORTION,
	EFFECT_STEREO_AMP_SIMULATOR,
	EFFECT_OD_EQ3,
	EFFECT_HEXA_CHORUS,
	EFFECT_DELAY_LCR,
	EFFECT_DELAY_LR,
	EFFECT_ECHO,
	EFFECT_CROSS_DELAY,
	EFFECT_DELAY_EQ2,
	EFFECT_LOFI,
	EFFECT_LOFI1,
	EFFECT_LOFI2,
	EFFECT_XG_AUTO_WAH,
	EFFECT_XG_AUTO_WAH_EQ2,
	EFFECT_XG_AUTO_WAH_OD,
	EFFECT_XG_AUTO_WAH_OD_EQ3,
};

#define MAGIC_INIT_EFFECT_INFO -1
#define MAGIC_FREE_EFFECT_INFO -2

class Reverb;

struct insertion_effect_gs_t {
	int32_t type;
	int8_t type_lsb, type_msb, parameter[20], send_reverb,
		send_chorus, send_delay, control_source1, control_depth1,
		control_source2, control_depth2, send_eq_switch;
	struct _EffectList *ef;
};

enum {
	XG_CONN_INSERTION = 0,
	XG_CONN_SYSTEM = 1,
	XG_CONN_SYSTEM_CHORUS,
	XG_CONN_SYSTEM_REVERB,
};

#define XG_INSERTION_EFFECT_NUM 2
#define XG_VARIATION_EFFECT_NUM 1

typedef struct _EffectList {
	int type;
	void *info;
	const struct _EffectEngine *engine;
	struct _EffectList *next_ef;
} EffectList;

struct _EffectEngine {
	int type;
	const char *name;
	void (Reverb::*do_effect)(int32_t *, int32_t, struct _EffectList *);
	void (Reverb::*conv_gs)(struct insertion_effect_gs_t *, struct _EffectList *);
	void (Reverb::*conv_xg)(struct effect_xg_t *, struct _EffectList *);
	int info_size;
};


struct effect_parameter_gs_t {
	int8_t type_msb, type_lsb;
	const char *name;
	int8_t param[20];
	int8_t control1, control2;
};


struct effect_parameter_xg_t {
	int8_t type_msb, type_lsb;
	const char *name;
	int8_t param_msb[10], param_lsb[16];
	int8_t control;
};


/*! 2-Band EQ */
typedef struct {
    int16_t low_freq, high_freq;		/* in Hz */
	int16_t low_gain, high_gain;		/* in dB */
	filter_shelving hsf, lsf;
} InfoEQ2;

/*! 3-Band EQ */
typedef struct {
    int16_t low_freq, high_freq, mid_freq;		/* in Hz */
	int16_t low_gain, high_gain, mid_gain;		/* in dB */
	double mid_width;
	filter_shelving hsf, lsf;
	filter_peaking peak;
} InfoEQ3;

/*! Stereo EQ */
typedef struct {
    int16_t low_freq, high_freq, m1_freq, m2_freq;		/* in Hz */
	int16_t low_gain, high_gain, m1_gain, m2_gain;		/* in dB */
	double m1_q, m2_q, level;
	int32_t leveli;
	filter_shelving hsf, lsf;
	filter_peaking m1, m2;
} InfoStereoEQ;

/*! Overdrive 1 / Distortion 1 */
typedef struct {
	double level;
	int32_t leveli, di;	/* in fixed-point */
	int8_t drive, pan, amp_sw, amp_type;
	filter_moog svf;
	filter_biquad lpf1;
	void (Reverb::*amp_sim)(int32_t *, int32_t);
} InfoOverdrive1;

/*! OD1 / OD2 */
typedef struct {
	double level, levell, levelr;
	int32_t levelli, levelri, dli, dri;	/* in fixed-point */
	int8_t drivel, driver, panl, panr, typel, typer, amp_swl, amp_swr, amp_typel, amp_typer;
	filter_moog svfl, svfr;
	filter_biquad lpf1;
	void (Reverb::*amp_siml)(int32_t *, int32_t), (Reverb::*amp_simr)(int32_t *, int32_t);
	void (Reverb::*odl)(int32_t *, int32_t), (Reverb::*odr)(int32_t *, int32_t);
} InfoOD1OD2;

/*! HEXA-CHORUS */
typedef struct {
	simple_delay buf0;
	lfo lfo0;
	double dry, wet, level;
	int32_t pdelay, depth;	/* in samples */
	int8_t pdelay_dev, depth_dev, pan_dev;
	int32_t dryi, weti;	/* in fixed-point */
	int32_t pan0, pan1, pan2, pan3, pan4, pan5;
	int32_t depth0, depth1, depth2, depth3, depth4, depth5,
		pdelay0, pdelay1, pdelay2, pdelay3, pdelay4, pdelay5;
	int32_t spt0, spt1, spt2, spt3, spt4, spt5,
		hist0, hist1, hist2, hist3, hist4, hist5;
} InfoHexaChorus;

/*! Plate Reverb */
typedef struct {
	simple_delay pd, od1l, od2l, od3l, od4l, od5l, od6l, od7l,
		od1r, od2r, od3r, od4r, od5r, od6r, od7r,
		td1, td2, td1d, td2d;
	lfo lfo1, lfo1d;
	allpass ap1, ap2, ap3, ap4, ap6, ap6d;
	mod_allpass ap5, ap5d;
	filter_lowpass1 lpf1, lpf2;
	int32_t t1, t1d;
	double decay, ddif1, ddif2, idif1, idif2, dry, wet;
	int32_t decayi, ddif1i, ddif2i, idif1i, idif2i, dryi, weti;
} InfoPlateReverb;

/*! Standard Reverb */
typedef struct {
	int32_t spt0, spt1, spt2, spt3, rpt0, rpt1, rpt2, rpt3;
	int32_t ta, tb, HPFL, HPFR, LPFL, LPFR, EPFL, EPFR;
	simple_delay buf0_L, buf0_R, buf1_L, buf1_R, buf2_L, buf2_R, buf3_L, buf3_R;
	double fbklev, nmixlev, cmixlev, monolev, hpflev, lpflev, lpfinp, epflev, epfinp, width, wet;
	int32_t fbklevi, nmixlevi, cmixlevi, monolevi, hpflevi, lpflevi, lpfinpi, epflevi, epfinpi, widthi, weti;
} InfoStandardReverb;

/*! Freeverb */
#define numcombs 8
#define numallpasses 4

typedef struct {
	simple_delay pdelay;
	double roomsize, roomsize1, damp, damp1, wet, wet1, wet2, width;
	comb combL[numcombs], combR[numcombs];
	allpass allpassL[numallpasses], allpassR[numallpasses];
	int32_t wet1i, wet2i;
	int8_t alloc_flag;
} InfoFreeverb;

/*! 3-Tap Stereo Delay Effect */
typedef struct {
	simple_delay delayL, delayR;
	int32_t size[3], index[3];
	double level[3], feedback, send_reverb;
	int32_t leveli[3], feedbacki, send_reverbi;
} InfoDelay3;

/*! Stereo Chorus Effect */
typedef struct {
	simple_delay delayL, delayR;
	lfo lfoL, lfoR;
	int32_t wpt0, spt0, spt1, hist0, hist1;
	int32_t rpt0, depth, pdelay;
	double level, feedback, send_reverb, send_delay;
	int32_t leveli, feedbacki, send_reverbi, send_delayi;
} InfoStereoChorus;

/*! Chorus */
typedef struct {
	simple_delay delayL, delayR;
	lfo lfoL, lfoR;
	int32_t wpt0, spt0, spt1, hist0, hist1;
	int32_t rpt0, depth, pdelay;
	double dry, wet, feedback, pdelay_ms, depth_ms, rate, phase_diff;
	int32_t dryi, weti, feedbacki;
} InfoChorus;


/*! Stereo Overdrive / Distortion */
typedef struct {
	double level, dry, wet, drive, cutoff;
	int32_t dryi, weti, di;
	filter_moog svfl, svfr;
	filter_biquad lpf1;
	void (Reverb::*od)(int32_t *, int32_t);
} InfoStereoOD;

/*! Delay L,C,R */
typedef struct {
	simple_delay delayL, delayR;
	int32_t index[3], size[3];	/* L,C,R */
	double rdelay, ldelay, cdelay, fdelay;	/* in ms */
	double dry, wet, feedback, clevel, high_damp;
	int32_t dryi, weti, feedbacki, cleveli;
	filter_lowpass1 lpf;
} InfoDelayLCR;

/*! Delay L,R */
typedef struct {
	simple_delay delayL, delayR;
	int32_t index[2], size[2];	/* L,R */
	double rdelay, ldelay, fdelay1, fdelay2;	/* in ms */
	double dry, wet, feedback, high_damp;
	int32_t dryi, weti, feedbacki;
	filter_lowpass1 lpf;
} InfoDelayLR;

/*! Echo */
typedef struct {
	simple_delay delayL, delayR;
	int32_t index[2], size[2];	/* L1,R1 */
	double rdelay1, ldelay1, rdelay2, ldelay2;	/* in ms */
	double dry, wet, lfeedback, rfeedback, high_damp, level;
	int32_t dryi, weti, lfeedbacki, rfeedbacki, leveli;
	filter_lowpass1 lpf;
} InfoEcho;

/*! Cross Delay */
typedef struct {
	simple_delay delayL, delayR;
	double lrdelay, rldelay;	/* in ms */
	double dry, wet, feedback, high_damp;
	int32_t dryi, weti, feedbacki, input_select;
	filter_lowpass1 lpf;
} InfoCrossDelay;

/*! Lo-Fi 1 */
typedef struct {
	int8_t lofi_type, pan, pre_filter, post_filter;
	double level, dry, wet;
	int32_t bit_mask, level_shift, dryi, weti;
	filter_biquad pre_fil, post_fil;
} InfoLoFi1;

/*! Lo-Fi 2 */
typedef struct {
	int8_t wp_sel, disc_type, hum_type, ms, pan, rdetune, lofi_type, fil_type;
	double wp_level, rnz_lev, discnz_lev, hum_level, dry, wet, level;
	int32_t bit_mask, level_shift, wp_leveli, rnz_levi, discnz_levi, hum_keveki, dryi, weti;
	filter_biquad fil, wp_lpf, hum_lpf, disc_lpf;
} InfoLoFi2;

/*! LO-FI */
typedef struct {
	int8_t output_gain, word_length, filter_type, bit_assign, emphasis;
	double dry, wet;
	int32_t bit_mask, level_shift, dryi, weti;
	filter_biquad lpf, srf;
} InfoLoFi;

/*! XG: Auto Wah */
typedef struct {
	int8_t lfo_depth, drive;
	double resonance, lfo_freq, offset_freq, dry, wet;
	int32_t dryi, weti, fil_count, fil_cycle;
	struct lfo lfo;
	filter_moog_dist fil0, fil1;
} InfoXGAutoWah;

typedef struct {
	double level;
	int32_t leveli;
	filter_biquad lpf;
} InfoXGAutoWahOD;


/* GS parameters of reverb effect */
struct reverb_status_gs_t
{
	/* GS parameters */
	int8_t character, pre_lpf, level, time, delay_feedback, pre_delay_time;

	InfoStandardReverb info_standard_reverb;
	InfoPlateReverb info_plate_reverb;
	InfoFreeverb info_freeverb;
	InfoDelay3 info_reverb_delay;
	filter_lowpass1 lpf;
};

struct chorus_text_gs_t
{
    int status;
    uint8_t voice_reserve[18], macro[3], pre_lpf[3], level[3], feed_back[3],
		delay[3], rate[3], depth[3], send_level[3];
};

/* GS parameters of chorus effect */
struct chorus_status_gs_t
{
	/* GS parameters */
	int8_t macro, pre_lpf, level, feedback, delay, rate, depth, send_reverb, send_delay;

	//struct chorus_text_gs_t text;

	InfoStereoChorus info_stereo_chorus;
	filter_lowpass1 lpf;
};

/* GS parameters of delay effect */
struct delay_status_gs_t
{
	/* GS parameters */
	int8_t type, level, level_center, level_left, level_right,
		feedback, pre_lpf, send_reverb, time_c, time_l, time_r;
    double time_center;			/* in ms */
    double time_ratio_left, time_ratio_right;		/* in pct */

	/* for pre-calculation */
	int32_t sample[3];	/* center, left, right */
	double level_ratio[3];	/* center, left, right */
	double feedback_ratio, send_reverb_ratio;

	filter_lowpass1 lpf;
	InfoDelay3 info_delay;
};

/* GS parameters of channel EQ */
struct eq_status_gs_t
{
	/* GS parameters */
    int8_t low_freq, high_freq, low_gain, high_gain;

	filter_shelving hsf, lsf;
};

/* XG parameters of Multi EQ */
struct multi_eq_xg_t
{
	/* XG parameters */
	int8_t type, gain1, gain2, gain3, gain4, gain5,
		freq1, freq2, freq3, freq4, freq5,
		q1, q2, q3, q4, q5, shape1, shape5;

	int8_t valid, valid1, valid2, valid3, valid4, valid5;
	filter_shelving eq1s, eq5s;
	filter_peaking eq1p, eq2p, eq3p, eq4p, eq5p;
};



class Reverb
{
	double REV_INP_LEV;

	int32_t direct_buffer[AUDIO_BUFFER_SIZE * 2];
	int32_t direct_bufsize;

	int32_t  reverb_effect_buffer[AUDIO_BUFFER_SIZE * 2];
	int32_t  reverb_effect_bufsize;

	int32_t delay_effect_buffer[AUDIO_BUFFER_SIZE * 2];
	int32_t chorus_effect_buffer[AUDIO_BUFFER_SIZE * 2];
	int32_t eq_buffer[AUDIO_BUFFER_SIZE * 2];


	static const struct _EffectEngine effect_engine[];

	void free_delay(simple_delay *delay);
	void set_delay(simple_delay *delay, int32_t size);
	void do_delay(int32_t *stream, int32_t *buf, int32_t size, int32_t *index);
	void init_lfo(lfo *lfo, double freq, int type, double phase);
	int32_t do_lfo(lfo *lfo);
	void do_mod_delay(int32_t *stream, int32_t *buf, int32_t size, int32_t *rindex, int32_t *windex, int32_t ndelay, int32_t depth, int32_t lfoval, int32_t *hist);
	void free_mod_allpass(mod_allpass *delay);
	void set_mod_allpass(mod_allpass *delay, int32_t ndelay, int32_t depth, double feedback);
	void do_mod_allpass(int32_t *stream, int32_t *buf, int32_t size, int32_t *rindex, int32_t *windex, int32_t ndelay, int32_t depth, int32_t lfoval, int32_t *hist, int32_t feedback);
	void free_allpass(allpass *allpass);
	void set_allpass(allpass *allpass, int32_t size, double feedback);
	void do_allpass(int32_t *stream, int32_t *buf, int32_t size, int32_t *index, int32_t feedback);
	void init_filter_moog(filter_moog *svf);
	void calc_filter_moog(filter_moog *svf);
	void do_filter_moog(int32_t *stream, int32_t *high, int32_t f, int32_t p, int32_t q, int32_t *b0, int32_t *b1, int32_t *b2, int32_t *b3, int32_t *b4);
	void init_filter_moog_dist(filter_moog_dist *svf);
	void calc_filter_moog_dist(filter_moog_dist *svf);
	void do_filter_moog_dist(double *stream, double *high, double *band, double f, double p, double q, double d, double *b0, double *b1, double *b2, double *b3, double *b4);
	void do_filter_moog_dist_band(double *stream, double f, double p, double q, double d, double *b0, double *b1, double *b2, double *b3, double *b4);
	void init_filter_lpf18(filter_lpf18 *p);
	void calc_filter_lpf18(filter_lpf18 *p);
	void do_filter_lpf18(double *stream, double *ay1, double *ay2, double *aout, double *lastin, double kres, double value, double kp, double kp1h);
	void do_dummy_clipping(int32_t *stream, int32_t d) {}
	void do_hard_clipping(int32_t *stream, int32_t d);
	void do_soft_clipping1(int32_t *stream, int32_t d);
	void do_soft_clipping2(int32_t *stream, int32_t d);
	void do_filter_lowpass1(int32_t *stream, int32_t *x1, int32_t a, int32_t ia);
	void do_filter_lowpass1_stereo(int32_t *buf, int32_t count, filter_lowpass1 *p);
	void init_filter_biquad(filter_biquad *p);
	void calc_filter_biquad_low(filter_biquad *p);
	void calc_filter_biquad_high(filter_biquad *p);
	void do_filter_biquad(int32_t *stream, int32_t a1, int32_t a2, int32_t b1, int32_t b02, int32_t *x1, int32_t *x2, int32_t *y1, int32_t *y2);
	void init_filter_shelving(filter_shelving *p);
	void do_shelving_filter_stereo(int32_t* buf, int32_t count, filter_shelving *p);
	void do_peaking_filter_stereo(int32_t* buf, int32_t count, filter_peaking *p);
	double gs_revchar_to_roomsize(int character);
	double gs_revchar_to_level(int character);
	double gs_revchar_to_rt(int character);
	void init_filter_peaking(filter_peaking *p);
	void init_standard_reverb(InfoStandardReverb *info);
	void free_standard_reverb(InfoStandardReverb *info);
	void do_ch_standard_reverb(int32_t *buf, int32_t count, InfoStandardReverb *info);
	void do_ch_standard_reverb_mono(int32_t *buf, int32_t count, InfoStandardReverb *info);
	void set_freeverb_allpass(allpass *allpass, int32_t size);
	void init_freeverb_allpass(allpass *allpass);
	void set_freeverb_comb(comb *comb, int32_t size);
	void init_freeverb_comb(comb *comb);
	void realloc_freeverb_buf(InfoFreeverb *rev);
	void update_freeverb(InfoFreeverb *rev);
	void init_freeverb(InfoFreeverb *rev);
	void alloc_freeverb_buf(InfoFreeverb *rev);
	void free_freeverb_buf(InfoFreeverb *rev);
	void do_freeverb_allpass(int32_t *stream, int32_t *buf, int32_t size, int32_t *index, int32_t feedback);
	void do_freeverb_comb(int32_t input, int32_t *stream, int32_t *buf, int32_t size, int32_t *index, int32_t damp1, int32_t damp2, int32_t *fs, int32_t feedback);
	void do_ch_freeverb(int32_t *buf, int32_t count, InfoFreeverb *rev);
	void init_ch_reverb_delay(InfoDelay3 *info);
	void free_ch_reverb_delay(InfoDelay3 *info);
	void do_ch_reverb_panning_delay(int32_t *buf, int32_t count, InfoDelay3 *info);
	void do_ch_reverb_normal_delay(int32_t *buf, int32_t count, InfoDelay3 *info);
	int32_t get_plate_delay(double delay, double t);
	void do_ch_plate_reverb(int32_t *buf, int32_t count, InfoPlateReverb *info);
	void init_ch_3tap_delay(InfoDelay3 *info);
	void free_ch_3tap_delay(InfoDelay3 *info);
	void do_ch_3tap_delay(int32_t *buf, int32_t count, InfoDelay3 *info);
	void do_ch_cross_delay(int32_t *buf, int32_t count, InfoDelay3 *info);
	void do_ch_normal_delay(int32_t *buf, int32_t count, InfoDelay3 *info);
	void do_ch_stereo_chorus(int32_t *buf, int32_t count, InfoStereoChorus *info);
	void alloc_effect(EffectList *ef);
	void do_eq2(int32_t *buf, int32_t count, EffectList *ef);
	int32_t do_left_panning(int32_t sample, int32_t pan);
	int32_t do_right_panning(int32_t sample, int32_t pan);
	double calc_gs_drive(int val);
	void do_overdrive1(int32_t *buf, int32_t count, EffectList *ef);
	void do_distortion1(int32_t *buf, int32_t count, EffectList *ef);
	void do_dual_od(int32_t *buf, int32_t count, EffectList *ef);
	void do_hexa_chorus(int32_t *buf, int32_t count, EffectList *ef);
	void free_effect_xg(struct effect_xg_t *st);
	int clip_int(int val, int min, int max);
	void conv_gs_eq2(struct insertion_effect_gs_t *ieffect, EffectList *ef);
	void conv_gs_overdrive1(struct insertion_effect_gs_t *ieffect, EffectList *ef);
	void conv_gs_dual_od(struct insertion_effect_gs_t *ieffect, EffectList *ef);
	double calc_dry_gs(int val);
	double calc_wet_gs(int val);
	void conv_gs_hexa_chorus(struct insertion_effect_gs_t *ieffect, EffectList *ef);
	double calc_dry_xg(int val, struct effect_xg_t *st);
	double calc_wet_xg(int val, struct effect_xg_t *st);
	void do_eq3(int32_t *buf, int32_t count, EffectList *ef);
	void do_stereo_eq(int32_t *buf, int32_t count, EffectList *ef);
	void conv_xg_eq2(struct effect_xg_t *st, EffectList *ef);
	void conv_xg_eq3(struct effect_xg_t *st, EffectList *ef);
	void conv_gs_stereo_eq(struct insertion_effect_gs_t *st, EffectList *ef);
	void conv_xg_chorus_eq3(struct effect_xg_t *st, EffectList *ef);
	void conv_xg_chorus(struct effect_xg_t *st, EffectList *ef);
	void conv_xg_flanger(struct effect_xg_t *st, EffectList *ef);
	void conv_xg_symphonic(struct effect_xg_t *st, EffectList *ef);
	void do_chorus(int32_t *buf, int32_t count, EffectList *ef);
	void conv_xg_od_eq3(struct effect_xg_t *st, EffectList *ef);
	void conv_xg_overdrive(struct effect_xg_t *st, EffectList *ef);
	void conv_xg_distortion(struct effect_xg_t *st, EffectList *ef);
	void conv_xg_amp_simulator(struct effect_xg_t *st, EffectList *ef);
	void do_stereo_od(int32_t *buf, int32_t count, EffectList *ef);
	void do_delay_lcr(int32_t *buf, int32_t count, EffectList *ef);
	void conv_xg_delay_eq2(struct effect_xg_t *st, EffectList *ef);
	void conv_xg_delay_lcr(struct effect_xg_t *st, EffectList *ef);
	void conv_xg_delay_lr(struct effect_xg_t *st, EffectList *ef);
	void do_delay_lr(int32_t *buf, int32_t count, EffectList *ef);
	void conv_xg_echo(struct effect_xg_t *st, EffectList *ef);
	void do_echo(int32_t *buf, int32_t count, EffectList *ef);
	void conv_xg_cross_delay(struct effect_xg_t *st, EffectList *ef);
	void do_cross_delay(int32_t *buf, int32_t count, EffectList *ef);
	void conv_gs_lofi1(struct insertion_effect_gs_t *st, EffectList *ef);
	inline int32_t apply_lofi(int32_t input, int32_t bit_mask, int32_t level_shift);
	void do_lofi1(int32_t *buf, int32_t count, EffectList *ef);
	void conv_gs_lofi2(struct insertion_effect_gs_t *st, EffectList *ef);
	void do_lofi2(int32_t *buf, int32_t count, EffectList *ef);
	void conv_xg_lofi(struct effect_xg_t *st, EffectList *ef);
	void do_lofi(int32_t *buf, int32_t count, EffectList *ef);
	void conv_xg_auto_wah_od(struct effect_xg_t *st, EffectList *ef);
	void conv_xg_auto_wah_od_eq3(struct effect_xg_t *st, EffectList *ef);
	void conv_xg_auto_wah_eq2(struct effect_xg_t *st, EffectList *ef);
	void conv_xg_auto_wah(struct effect_xg_t *st, EffectList *ef);
	double calc_xg_auto_wah_freq(int32_t lfo_val, double offset_freq, int8_t depth);
	void do_xg_auto_wah(int32_t *buf, int32_t count, EffectList *ef);
	void do_xg_auto_wah_od(int32_t *buf, int32_t count, EffectList *ef);

public:
	Reverb()
	{
		// Make sure that this starts out with all zeros.
		memset(this, 0, sizeof(*this));
		REV_INP_LEV = 1.0;
		direct_bufsize = sizeof(direct_buffer);
		reverb_effect_bufsize = sizeof(reverb_effect_buffer);

	}

	~Reverb()
	{
		free_effect_buffers();
	}

	void set_dry_signal(int32_t *, int32_t);
	void set_dry_signal_xg(int32_t *, int32_t, int32_t);
	void mix_dry_signal(int32_t *, int32_t);
	void free_effect_buffers(void);
	void init_pink_noise(pink_noise *);
	float get_pink_noise(pink_noise *);
	float get_pink_noise_light(pink_noise *);
	void calc_filter_shelving_high(filter_shelving *);
	void calc_filter_shelving_low(filter_shelving *);
	void calc_filter_peaking(filter_peaking *);
	void do_insertion_effect_gs(int32_t*, int32_t);
	void do_insertion_effect_xg(int32_t*, int32_t, struct effect_xg_t *);
	void do_variation_effect1_xg(int32_t*, int32_t);
	void init_ch_effect_xg(void);
	EffectList *push_effect(EffectList *, int);
	void do_effect_list(int32_t *, int32_t, EffectList *);
	void free_effect_list(EffectList *);
	void init_filter_lowpass1(filter_lowpass1 *p);

	/*                             */
	/*        System Effect        */
	/*                             */
	/* Reverb Effect */
	void do_ch_reverb(int32_t *, int32_t);
	void set_ch_reverb(int32_t *, int32_t, int32_t);
	void init_reverb(void);
	void do_ch_reverb_xg(int32_t *, int32_t);

	/* Chorus Effect */
	void do_ch_chorus(int32_t *, int32_t);
	void set_ch_chorus(int32_t *, int32_t, int32_t);
	void init_ch_chorus(void);
	void do_ch_chorus_xg(int32_t *, int32_t);

	/* Delay (Celeste) Effect */
	void do_ch_delay(int32_t *, int32_t);
	void set_ch_delay(int32_t *, int32_t, int32_t);
	void init_ch_delay(void);

	/* EQ */
	void init_eq_gs(void);
	void set_ch_eq_gs(int32_t *, int32_t);
	void do_ch_eq_gs(int32_t *, int32_t);
	void do_ch_eq_xg(int32_t *, int32_t, struct part_eq_xg *);
	void do_multi_eq_xg(int32_t *, int32_t);

	// These get accessed directly by the player.
	struct multi_eq_xg_t multi_eq_xg;
	pink_noise global_pink_noise_light;
	struct insertion_effect_gs_t insertion_effect_gs;
	struct effect_xg_t insertion_effect_xg[XG_INSERTION_EFFECT_NUM],
		variation_effect_xg[XG_VARIATION_EFFECT_NUM], reverb_status_xg, chorus_status_xg;

	static const struct effect_parameter_gs_t effect_parameter_gs[];
	static const struct effect_parameter_xg_t effect_parameter_xg[];

	void init_for_effect()
	{
		init_pink_noise(&global_pink_noise_light);
		init_reverb();
		init_ch_delay();
		init_ch_chorus();
		init_eq_gs();
	}


	struct reverb_status_gs_t reverb_status_gs;
	//struct chorus_text_gs_t chorus_text_gs;
	struct chorus_status_gs_t chorus_status_gs;
	struct delay_status_gs_t delay_status_gs;
	struct eq_status_gs_t eq_status_gs;


	////////////////////////////////// from readmidi

	void init_delay_status_gs(void);
	void recompute_delay_status_gs(void);
	void set_delay_macro_gs(int macro);
	void init_reverb_status_gs(void);
	void recompute_reverb_status_gs(void);
	void set_reverb_macro_gm2(int macro);
	void set_reverb_macro_gs(int macro);
	void init_chorus_status_gs(void);
	void recompute_chorus_status_gs();
	void set_chorus_macro_gs(int macro);
	void init_eq_status_gs(void);
	void recompute_eq_status_gs(void);
	void init_multi_eq_xg(void);
	void set_multi_eq_type_xg(int type);
	void recompute_multi_eq_xg(void);
	void set_effect_param_xg(struct effect_xg_t *st, int type_msb, int type_lsb);
	void recompute_effect_xg(struct effect_xg_t *st);
	void realloc_effect_xg(struct effect_xg_t *st);
	void init_effect_xg(struct effect_xg_t *st);
	void init_all_effect_xg(void);
	void init_insertion_effect_gs(void);
	void set_effect_param_gs(struct insertion_effect_gs_t *st, int msb, int lsb);
	void recompute_insertion_effect_gs(void);
	void realloc_insertion_effect_gs(void);
	void init_effect_status(int play_system_mode);

};

}
#endif /* ___REVERB_H_ */
