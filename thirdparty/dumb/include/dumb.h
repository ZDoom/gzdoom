/*  _______         ____    __         ___    ___
 * \    _  \       \    /  \  /       \   \  /   /       '   '  '
 *  |  | \  \       |  |    ||         |   \/   |         .      .
 *  |  |  |  |      |  |    ||         ||\  /|  |
 *  |  |  |  |      |  |    ||         || \/ |  |         '  '  '
 *  |  |  |  |      |  |    ||         ||    |  |         .      .
 *  |  |_/  /        \  \__//          ||    |  |
 * /_______/ynamic    \____/niversal  /__\  /____\usic   /|  .  . ibliotheque
 *                                                      /  \
 *                                                     / .  \
 * dumb.h - The user header file for DUMB.            / / \  \
 *                                                   | <  /   \_
 * Include this file in any of your files in         |  \/ /\   /
 * which you wish to use the DUMB functions           \_  /  > /
 * and variables.                                       | \ / /
 *                                                      |  ' /
 * Allegro users, you will probably want aldumb.h.       \__/
 */

#ifndef DUMB_H
#define DUMB_H


#include <stdlib.h>
#include <stdio.h>

#if defined(_DEBUG) && defined(_MSC_VER)
#ifndef _CRTDBG_MAP_ALLOC
//#define _CRTDBG_MAP_ALLOC
#endif
#include <crtdbg.h>
#endif

#ifdef __cplusplus
	extern "C" {
#endif


#define DUMB_MAJOR_VERSION    1
#define DUMB_MINOR_VERSION    0
#define DUMB_REVISION_VERSION 0

#define DUMB_VERSION (DUMB_MAJOR_VERSION*10000 + DUMB_MINOR_VERSION*100 + DUMB_REVISION_VERSION)

#define DUMB_VERSION_STR "1.0.0"

#define DUMB_NAME "DUMB v" DUMB_VERSION_STR

#define DUMB_YEAR  2015
#define DUMB_MONTH 1
#define DUMB_DAY   17

#define DUMB_YEAR_STR2  "15"
#define DUMB_YEAR_STR4  "2015"
#define DUMB_MONTH_STR1 "1"
#define DUMB_DAY_STR1   "17"

#if DUMB_MONTH < 10
#define DUMB_MONTH_STR2 "0" DUMB_MONTH_STR1
#else
#define DUMB_MONTH_STR2 DUMB_MONTH_STR1
#endif

#if DUMB_DAY < 10
#define DUMB_DAY_STR2 "0" DUMB_DAY_STR1
#else
#define DUMB_DAY_STR2 DUMB_DAY_STR1
#endif


/* WARNING: The month and day were inadvertently swapped in the v0.8 release.
 *          Please do not compare this constant against any date in 2002. In
 *          any case, DUMB_VERSION is probably more useful for this purpose.
 */
#define DUMB_DATE (DUMB_YEAR*10000 + DUMB_MONTH*100 + DUMB_DAY)

#define DUMB_DATE_STR DUMB_DAY_STR1 "." DUMB_MONTH_STR1 "." DUMB_YEAR_STR4


#undef MIN
#undef MAX
#undef MID

#define MIN(x,y)   (((x) < (y)) ? (x) : (y))
#define MAX(x,y)   (((x) > (y)) ? (x) : (y))
#define MID(x,y,z) MAX((x), MIN((y), (z)))

#undef ABS
#define ABS(x) (((x) >= 0) ? (x) : (-(x)))


#ifdef DEBUGMODE

#ifndef ASSERT
#include <assert.h>
#define ASSERT(n) assert(n)
#endif
#ifndef TRACE
// it would be nice if this did actually trace ...
#define TRACE 1 ? (void)0 : (void)printf
#endif

#else

#ifndef ASSERT
#define ASSERT(n)
#endif
#ifndef TRACE
#define TRACE 1 ? (void)0 : (void)printf
#endif

#endif


#define DUMB_ID(a,b,c,d) (((unsigned int)(a) << 24) | \
                          ((unsigned int)(b) << 16) | \
                          ((unsigned int)(c) <<  8) | \
                          ((unsigned int)(d)      ))


#ifdef __DOS__
typedef long int32;
typedef unsigned long uint32;
typedef signed long sint32;
#else
typedef int int32;
typedef unsigned int uint32;
typedef signed int sint32;
#endif

#define CDECL
#ifndef LONG_LONG
#if defined __GNUC__ || defined __INTEL_COMPILER || defined __MWERKS__
#define LONG_LONG long long
#elif defined _MSC_VER || defined __WATCOMC__
#define LONG_LONG __int64
#undef CDECL
#define CDECL __cdecl
#elif defined __sgi
#define LONG_LONG long long
#else
#error 64-bit integer type unknown
#endif
#endif

#if __GNUC__ * 100 + __GNUC_MINOR__ >= 301 /* GCC 3.1+ */
#ifndef DUMB_DECLARE_DEPRECATED
#define DUMB_DECLARE_DEPRECATED
#endif
#define DUMB_DEPRECATED __attribute__((__deprecated__))
#else
#define DUMB_DEPRECATED
#endif

#define DUMBEXPORT CDECL
#define DUMBCALLBACK CDECL

/* Basic Sample Type. Normal range is -0x800000 to 0x7FFFFF. */

typedef int sample_t;


/* Library Clean-up Management */

int dumb_atexit(void (*proc)(void));

void dumb_exit(void);


/* File Input Functions */

typedef struct DUMBFILE_SYSTEM
{
	void *(DUMBCALLBACK *open)(const char *filename);
	int (DUMBCALLBACK *skip)(void *f, long n);
	int (DUMBCALLBACK *getc)(void *f);
	int32 (DUMBCALLBACK *getnc)(char *ptr, int32 n, void *f);
	void (DUMBCALLBACK *close)(void *f);
    int (DUMBCALLBACK *seek)(void *f, long n);
    long (DUMBCALLBACK *get_size)(void *f);
}
DUMBFILE_SYSTEM;

typedef struct DUMBFILE DUMBFILE;

void DUMBEXPORT register_dumbfile_system(const DUMBFILE_SYSTEM *dfs);

DUMBFILE *DUMBEXPORT dumbfile_open(const char *filename);
DUMBFILE *DUMBEXPORT dumbfile_open_ex(void *file, const DUMBFILE_SYSTEM *dfs);

int32 DUMBEXPORT dumbfile_pos(DUMBFILE *f);
int DUMBEXPORT dumbfile_skip(DUMBFILE *f, long n);

#define DFS_SEEK_SET 0
#define DFS_SEEK_CUR 1
#define DFS_SEEK_END 2

int DUMBEXPORT dumbfile_seek(DUMBFILE *f, long n, int origin);

int32 DUMBEXPORT dumbfile_get_size(DUMBFILE *f);

int DUMBEXPORT dumbfile_getc(DUMBFILE *f);

int DUMBEXPORT dumbfile_igetw(DUMBFILE *f);
int DUMBEXPORT dumbfile_mgetw(DUMBFILE *f);

int32 DUMBEXPORT dumbfile_igetl(DUMBFILE *f);
int32 DUMBEXPORT dumbfile_mgetl(DUMBFILE *f);

uint32 DUMBEXPORT dumbfile_cgetul(DUMBFILE *f);
sint32 DUMBEXPORT dumbfile_cgetsl(DUMBFILE *f);

int32 DUMBEXPORT dumbfile_getnc(char *ptr, int32 n, DUMBFILE *f);

int DUMBEXPORT dumbfile_error(DUMBFILE *f);
int DUMBEXPORT dumbfile_close(DUMBFILE *f);


/* stdio File Input Module */

void DUMBEXPORT dumb_register_stdfiles(void);

DUMBFILE *DUMBEXPORT dumbfile_open_stdfile(FILE *p);


/* Memory File Input Module */

DUMBFILE *DUMBEXPORT dumbfile_open_memory(const char *data, int32 size);


/* DUH Management */

typedef struct DUH DUH;

#define DUH_SIGNATURE DUMB_ID('D','U','H','!')

void DUMBEXPORT unload_duh(DUH *duh);

DUH *DUMBEXPORT load_duh(const char *filename);
DUH *DUMBEXPORT read_duh(DUMBFILE *f);

int32 DUMBEXPORT duh_get_length(DUH *duh);

const char *DUMBEXPORT duh_get_tag(DUH *duh, const char *key);

/* Signal Rendering Functions */

typedef struct DUH_SIGRENDERER DUH_SIGRENDERER;

DUH_SIGRENDERER *DUMBEXPORT duh_start_sigrenderer(DUH *duh, int sig, int n_channels, int32 pos);

#ifdef DUMB_DECLARE_DEPRECATED
typedef void (*DUH_SIGRENDERER_CALLBACK)(void *data, sample_t **samples, int n_channels, int32 length);
/* This is deprecated, but is not marked as such because GCC tends to
 * complain spuriously when the typedef is used later. See comments below.
 */

void duh_sigrenderer_set_callback(
	DUH_SIGRENDERER *sigrenderer,
	DUH_SIGRENDERER_CALLBACK callback, void *data
) DUMB_DEPRECATED;
/* The 'callback' argument's type has changed for const-correctness. See the
 * DUH_SIGRENDERER_CALLBACK definition just above. Also note that the samples
 * in the buffer are now 256 times as large; the normal range is -0x800000 to
 * 0x7FFFFF. The function has been renamed partly because its functionality
 * has changed slightly and partly so that its name is more meaningful. The
 * new one is duh_sigrenderer_set_analyser_callback(), and the typedef for
 * the function pointer has also changed, from DUH_SIGRENDERER_CALLBACK to
 * DUH_SIGRENDERER_ANALYSER_CALLBACK. (If you wanted to use this callback to
 * apply a DSP effect, don't worry; there is a better way of doing this. It
 * is undocumented, so contact me and I shall try to help. Contact details
 * are in readme.txt.)
 */

typedef void (*DUH_SIGRENDERER_ANALYSER_CALLBACK)(void *data, const sample_t *const *samples, int n_channels, int32 length);
/* This is deprecated, but is not marked as such because GCC tends to
 * complain spuriously when the typedef is used later. See comments below.
 */

void duh_sigrenderer_set_analyser_callback(
	DUH_SIGRENDERER *sigrenderer,
	DUH_SIGRENDERER_ANALYSER_CALLBACK callback, void *data
) DUMB_DEPRECATED;
/* This is deprecated because the meaning of the 'samples' parameter in the
 * callback needed to change. For stereo applications, the array used to be
 * indexed with samples[channel][pos]. It is now indexed with
 * samples[0][pos*2+channel]. Mono sample data are still indexed with
 * samples[0][pos]. The array is still 2D because samples will probably only
 * ever be interleaved in twos. In order to fix your code, adapt it to the
 * new sample layout and then call
 * duh_sigrenderer_set_sample_analyser_callback below instead of this
 * function.
 */
#endif

typedef void (*DUH_SIGRENDERER_SAMPLE_ANALYSER_CALLBACK)(void *data, const sample_t *const *samples, int n_channels, int32 length);

void duh_sigrenderer_set_sample_analyser_callback(
	DUH_SIGRENDERER *sigrenderer,
	DUH_SIGRENDERER_SAMPLE_ANALYSER_CALLBACK callback, void *data
);

int DUMBEXPORT duh_sigrenderer_get_n_channels(DUH_SIGRENDERER *sigrenderer);
int32 DUMBEXPORT duh_sigrenderer_get_position(DUH_SIGRENDERER *sigrenderer);

void DUMBEXPORT duh_sigrenderer_set_sigparam(DUH_SIGRENDERER *sigrenderer, unsigned char id, int32 value);

#ifdef DUMB_DECLARE_DEPRECATED
int32 duh_sigrenderer_get_samples(
	DUH_SIGRENDERER *sigrenderer,
	float volume, float delta,
	int32 size, sample_t **samples
) DUMB_DEPRECATED;
/* The sample format has changed, so if you were using this function,
 * you should switch to duh_sigrenderer_generate_samples() and change
 * how you interpret the samples array. See the comments for
 * duh_sigrenderer_set_analyser_callback().
 */
#endif

int32 DUMBEXPORT duh_sigrenderer_generate_samples(
	DUH_SIGRENDERER *sigrenderer,
	double volume, double delta,
	int32 size, sample_t **samples
);

void DUMBEXPORT duh_sigrenderer_get_current_sample(DUH_SIGRENDERER *sigrenderer, float volume, sample_t *samples);

void DUMBEXPORT duh_end_sigrenderer(DUH_SIGRENDERER *sigrenderer);


/* DUH Rendering Functions */

int32 DUMBEXPORT duh_render(
	DUH_SIGRENDERER *sigrenderer,
	int bits, int unsign,
	float volume, float delta,
	int32 size, void *sptr
);

#ifdef DUMB_DECLARE_DEPRECATED

int32 duh_render_signal(
	DUH_SIGRENDERER *sigrenderer,
	float volume, float delta,
	int32 size, sample_t **samples
) DUMB_DEPRECATED;
/* Please use duh_sigrenderer_generate_samples(), and see the
 * comments for the deprecated duh_sigrenderer_get_samples() too.
 */

typedef DUH_SIGRENDERER DUH_RENDERER DUMB_DEPRECATED;
/* Please use DUH_SIGRENDERER instead of DUH_RENDERER. */

DUH_SIGRENDERER *duh_start_renderer(DUH *duh, int n_channels, int32 pos) DUMB_DEPRECATED;
/* Please use duh_start_sigrenderer() instead. Pass 0 for 'sig'. */

int duh_renderer_get_n_channels(DUH_SIGRENDERER *dr) DUMB_DEPRECATED;
int32 duh_renderer_get_position(DUH_SIGRENDERER *dr) DUMB_DEPRECATED;
/* Please use the duh_sigrenderer_*() equivalents of these two functions. */

void duh_end_renderer(DUH_SIGRENDERER *dr) DUMB_DEPRECATED;
/* Please use duh_end_sigrenderer() instead. */

DUH_SIGRENDERER *duh_renderer_encapsulate_sigrenderer(DUH_SIGRENDERER *sigrenderer) DUMB_DEPRECATED;
DUH_SIGRENDERER *duh_renderer_get_sigrenderer(DUH_SIGRENDERER *dr) DUMB_DEPRECATED;
DUH_SIGRENDERER *duh_renderer_decompose_to_sigrenderer(DUH_SIGRENDERER *dr) DUMB_DEPRECATED;
/* These functions have become no-ops that just return the parameter.
 * So, for instance, replace
 *   duh_renderer_encapsulate_sigrenderer(my_sigrenderer)
 * with
 *   my_sigrenderer
 */

#endif


/* Impulse Tracker Support */

extern int dumb_it_max_to_mix;

typedef struct DUMB_IT_SIGDATA DUMB_IT_SIGDATA;
typedef struct DUMB_IT_SIGRENDERER DUMB_IT_SIGRENDERER;

DUMB_IT_SIGDATA *DUMBEXPORT duh_get_it_sigdata(DUH *duh);
DUH_SIGRENDERER *DUMBEXPORT duh_encapsulate_it_sigrenderer(DUMB_IT_SIGRENDERER *it_sigrenderer, int n_channels, int32 pos);
DUMB_IT_SIGRENDERER *DUMBEXPORT duh_get_it_sigrenderer(DUH_SIGRENDERER *sigrenderer);

int DUMBEXPORT dumb_it_trim_silent_patterns(DUH * duh);

typedef int (*dumb_scan_callback)(void *, int, int32);
int DUMBEXPORT dumb_it_scan_for_playable_orders(DUMB_IT_SIGDATA *sigdata, dumb_scan_callback callback, void * callback_data);

DUH_SIGRENDERER *DUMBEXPORT dumb_it_start_at_order(DUH *duh, int n_channels, int startorder);

enum
{
    DUMB_IT_RAMP_NONE = 0,
    DUMB_IT_RAMP_ONOFF_ONLY = 1,
    DUMB_IT_RAMP_FULL = 2
};
        
void DUMBEXPORT dumb_it_set_ramp_style(DUMB_IT_SIGRENDERER * sigrenderer, int ramp_style);
        
void DUMBEXPORT dumb_it_set_loop_callback(DUMB_IT_SIGRENDERER *sigrenderer, int (DUMBCALLBACK *callback)(void *data), void *data);
void DUMBEXPORT dumb_it_set_xm_speed_zero_callback(DUMB_IT_SIGRENDERER *sigrenderer, int (DUMBCALLBACK *callback)(void *data), void *data);
void DUMBEXPORT dumb_it_set_midi_callback(DUMB_IT_SIGRENDERER *sigrenderer, int (DUMBCALLBACK *callback)(void *data, int channel, unsigned char midi_byte), void *data);
void DUMBEXPORT dumb_it_set_global_volume_zero_callback(DUMB_IT_SIGRENDERER *sigrenderer, int (DUMBCALLBACK *callback)(void *data), void *data);

int DUMBCALLBACK dumb_it_callback_terminate(void *data);
int DUMBCALLBACK dumb_it_callback_midi_block(void *data, int channel, unsigned char midi_byte);

/* dumb_*_mod*: restrict_ |= 1-Don't read 15 sample files / 2-Use old pattern counting method */

DUH *DUMBEXPORT dumb_load_it(const char *filename);
DUH *DUMBEXPORT dumb_load_xm(const char *filename);
DUH *DUMBEXPORT dumb_load_s3m(const char *filename);
DUH *DUMBEXPORT dumb_load_stm(const char *filename);
DUH *DUMBEXPORT dumb_load_mod(const char *filename, int restrict_);
DUH *DUMBEXPORT dumb_load_ptm(const char *filename);
DUH *DUMBEXPORT dumb_load_669(const char *filename);
DUH *DUMBEXPORT dumb_load_psm(const char *filename, int subsong);
DUH *DUMBEXPORT dumb_load_old_psm(const char * filename);
DUH *DUMBEXPORT dumb_load_mtm(const char *filename);
DUH *DUMBEXPORT dumb_load_riff(const char *filename);
DUH *DUMBEXPORT dumb_load_asy(const char *filename);
DUH *DUMBEXPORT dumb_load_amf(const char *filename);
DUH *DUMBEXPORT dumb_load_okt(const char *filename);

DUH *DUMBEXPORT dumb_read_it(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_xm(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_s3m(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_stm(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_mod(DUMBFILE *f, int restrict_);
DUH *DUMBEXPORT dumb_read_ptm(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_669(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_psm(DUMBFILE *f, int subsong);
DUH *DUMBEXPORT dumb_read_old_psm(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_mtm(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_riff(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_asy(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_amf(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_okt(DUMBFILE *f);

DUH *DUMBEXPORT dumb_load_it_quick(const char *filename);
DUH *DUMBEXPORT dumb_load_xm_quick(const char *filename);
DUH *DUMBEXPORT dumb_load_s3m_quick(const char *filename);
DUH *DUMBEXPORT dumb_load_stm_quick(const char *filename);
DUH *DUMBEXPORT dumb_load_mod_quick(const char *filename, int restrict_);
DUH *DUMBEXPORT dumb_load_ptm_quick(const char *filename);
DUH *DUMBEXPORT dumb_load_669_quick(const char *filename);
DUH *DUMBEXPORT dumb_load_psm_quick(const char *filename, int subsong);
DUH *DUMBEXPORT dumb_load_old_psm_quick(const char * filename);
DUH *DUMBEXPORT dumb_load_mtm_quick(const char *filename);
DUH *DUMBEXPORT dumb_load_riff_quick(const char *filename);
DUH *DUMBEXPORT dumb_load_asy_quick(const char *filename);
DUH *DUMBEXPORT dumb_load_amf_quick(const char *filename);
DUH *DUMBEXPORT dumb_load_okt_quick(const char *filename);

DUH *DUMBEXPORT dumb_read_it_quick(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_xm_quick(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_s3m_quick(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_stm_quick(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_mod_quick(DUMBFILE *f, int restrict_);
DUH *DUMBEXPORT dumb_read_ptm_quick(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_669_quick(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_psm_quick(DUMBFILE *f, int subsong);
DUH *DUMBEXPORT dumb_read_old_psm_quick(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_mtm_quick(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_riff_quick(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_asy_quick(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_amf_quick(DUMBFILE *f);
DUH *DUMBEXPORT dumb_read_okt_quick(DUMBFILE *f);

DUH *DUMBEXPORT dumb_read_any_quick(DUMBFILE *f, int restrict_, int subsong);
DUH *DUMBEXPORT dumb_read_any(DUMBFILE *f, int restrict_, int subsong);

DUH *DUMBEXPORT dumb_load_any_quick(const char *filename, int restrict_, int subsong);
DUH *DUMBEXPORT dumb_load_any(const char *filename, int restrict_, int subsong);

int32 DUMBEXPORT dumb_it_build_checkpoints(DUMB_IT_SIGDATA *sigdata, int startorder);
void DUMBEXPORT dumb_it_do_initial_runthrough(DUH *duh);

int DUMBEXPORT dumb_get_psm_subsong_count(DUMBFILE *f);

const unsigned char *DUMBEXPORT dumb_it_sd_get_song_message(DUMB_IT_SIGDATA *sd);

int DUMBEXPORT dumb_it_sd_get_n_orders(DUMB_IT_SIGDATA *sd);
int DUMBEXPORT dumb_it_sd_get_n_samples(DUMB_IT_SIGDATA *sd);
int DUMBEXPORT dumb_it_sd_get_n_instruments(DUMB_IT_SIGDATA *sd);

const unsigned char *DUMBEXPORT dumb_it_sd_get_sample_name(DUMB_IT_SIGDATA *sd, int i);
const unsigned char *DUMBEXPORT dumb_it_sd_get_sample_filename(DUMB_IT_SIGDATA *sd, int i);
const unsigned char *DUMBEXPORT dumb_it_sd_get_instrument_name(DUMB_IT_SIGDATA *sd, int i);
const unsigned char *DUMBEXPORT dumb_it_sd_get_instrument_filename(DUMB_IT_SIGDATA *sd, int i);

int DUMBEXPORT dumb_it_sd_get_initial_global_volume(DUMB_IT_SIGDATA *sd);
void DUMBEXPORT dumb_it_sd_set_initial_global_volume(DUMB_IT_SIGDATA *sd, int gv);

int DUMBEXPORT dumb_it_sd_get_mixing_volume(DUMB_IT_SIGDATA *sd);
void DUMBEXPORT dumb_it_sd_set_mixing_volume(DUMB_IT_SIGDATA *sd, int mv);

int DUMBEXPORT dumb_it_sd_get_initial_speed(DUMB_IT_SIGDATA *sd);
void DUMBEXPORT dumb_it_sd_set_initial_speed(DUMB_IT_SIGDATA *sd, int speed);

int DUMBEXPORT dumb_it_sd_get_initial_tempo(DUMB_IT_SIGDATA *sd);
void DUMBEXPORT dumb_it_sd_set_initial_tempo(DUMB_IT_SIGDATA *sd, int tempo);

int DUMBEXPORT dumb_it_sd_get_initial_channel_volume(DUMB_IT_SIGDATA *sd, int channel);
void DUMBEXPORT dumb_it_sd_set_initial_channel_volume(DUMB_IT_SIGDATA *sd, int channel, int volume);

int DUMBEXPORT dumb_it_sr_get_current_order(DUMB_IT_SIGRENDERER *sr);
int DUMBEXPORT dumb_it_sr_get_current_row(DUMB_IT_SIGRENDERER *sr);

int DUMBEXPORT dumb_it_sr_get_global_volume(DUMB_IT_SIGRENDERER *sr);
void DUMBEXPORT dumb_it_sr_set_global_volume(DUMB_IT_SIGRENDERER *sr, int gv);

int DUMBEXPORT dumb_it_sr_get_tempo(DUMB_IT_SIGRENDERER *sr);
void DUMBEXPORT dumb_it_sr_set_tempo(DUMB_IT_SIGRENDERER *sr, int tempo);

int DUMBEXPORT dumb_it_sr_get_speed(DUMB_IT_SIGRENDERER *sr);
void DUMBEXPORT dumb_it_sr_set_speed(DUMB_IT_SIGRENDERER *sr, int speed);

#define DUMB_IT_N_CHANNELS 64
#define DUMB_IT_N_NNA_CHANNELS 192
#define DUMB_IT_TOTAL_CHANNELS (DUMB_IT_N_CHANNELS + DUMB_IT_N_NNA_CHANNELS)

/* Channels passed to any of these functions are 0-based */
int DUMBEXPORT dumb_it_sr_get_channel_volume(DUMB_IT_SIGRENDERER *sr, int channel);
void DUMBEXPORT dumb_it_sr_set_channel_volume(DUMB_IT_SIGRENDERER *sr, int channel, int volume);

int DUMBEXPORT dumb_it_sr_get_channel_muted(DUMB_IT_SIGRENDERER *sr, int channel);
void DUMBEXPORT dumb_it_sr_set_channel_muted(DUMB_IT_SIGRENDERER *sr, int channel, int muted);

typedef struct DUMB_IT_CHANNEL_STATE DUMB_IT_CHANNEL_STATE;

struct DUMB_IT_CHANNEL_STATE
{
	int channel; /* 0-based; meaningful for NNA channels */
	int sample; /* 1-based; 0 if nothing playing, then other fields undef */
	int freq; /* in Hz */
	float volume; /* 1.0 maximum; affected by ALL factors, inc. mixing vol */
	unsigned char pan; /* 0-64, 100 for surround */
	signed char subpan; /* use (pan + subpan/256.0f) or ((pan<<8)+subpan) */
	unsigned char filter_cutoff;    /* 0-127    cutoff=127 AND resonance=0 */
	unsigned char filter_subcutoff; /* 0-255      -> no filters (subcutoff */
	unsigned char filter_resonance; /* 0-127        always 0 in this case) */
	/* subcutoff only changes from zero if filter envelopes are in use. The
	 * calculation (filter_cutoff + filter_subcutoff/256.0f) gives a more
	 * accurate filter cutoff measurement as a float. It would often be more
	 * useful to use a scaled int such as ((cutoff<<8) + subcutoff).
	 */
};

/* Values of 64 or more will access NNA channels here. */
void DUMBEXPORT dumb_it_sr_get_channel_state(DUMB_IT_SIGRENDERER *sr, int channel, DUMB_IT_CHANNEL_STATE *state);


/* Signal Design Helper Values */

/* Use pow(DUMB_SEMITONE_BASE, n) to get the 'delta' value to transpose up by
 * n semitones. To transpose down, use negative n.
 */
#define DUMB_SEMITONE_BASE 1.059463094359295309843105314939748495817

/* Use pow(DUMB_QUARTERTONE_BASE, n) to get the 'delta' value to transpose up
 * by n quartertones. To transpose down, use negative n.
 */
#define DUMB_QUARTERTONE_BASE 1.029302236643492074463779317738953977823

/* Use pow(DUMB_PITCH_BASE, n) to get the 'delta' value to transpose up by n
 * units. In this case, 256 units represent one semitone; 3072 units
 * represent one octave. These units are used by the sequence signal (SEQU).
 */
#define DUMB_PITCH_BASE 1.000225659305069791926712241547647863626


/* Signal Design Function Types */

typedef void sigdata_t;
typedef void sigrenderer_t;

typedef sigdata_t *(*DUH_LOAD_SIGDATA)(DUH *duh, DUMBFILE *file);

typedef sigrenderer_t *(*DUH_START_SIGRENDERER)(
	DUH *duh,
	sigdata_t *sigdata,
	int n_channels,
	int32 pos
);

typedef void (*DUH_SIGRENDERER_SET_SIGPARAM)(
	sigrenderer_t *sigrenderer,
	unsigned char id, int32 value
);

typedef int32 (*DUH_SIGRENDERER_GENERATE_SAMPLES)(
	sigrenderer_t *sigrenderer,
	double volume, double delta,
	int32 size, sample_t **samples
);

typedef void (*DUH_SIGRENDERER_GET_CURRENT_SAMPLE)(
	sigrenderer_t *sigrenderer,
	double volume,
	sample_t *samples
);

typedef void (*DUH_END_SIGRENDERER)(sigrenderer_t *sigrenderer);

typedef void (*DUH_UNLOAD_SIGDATA)(sigdata_t *sigdata);


/* Signal Design Function Registration */

typedef struct DUH_SIGTYPE_DESC
{
	int32 type;
	DUH_LOAD_SIGDATA                   load_sigdata;
	DUH_START_SIGRENDERER              start_sigrenderer;
	DUH_SIGRENDERER_SET_SIGPARAM       sigrenderer_set_sigparam;
	DUH_SIGRENDERER_GENERATE_SAMPLES   sigrenderer_generate_samples;
	DUH_SIGRENDERER_GET_CURRENT_SAMPLE sigrenderer_get_current_sample;
	DUH_END_SIGRENDERER                end_sigrenderer;
	DUH_UNLOAD_SIGDATA                 unload_sigdata;
}
DUH_SIGTYPE_DESC;

void DUMBEXPORT dumb_register_sigtype(DUH_SIGTYPE_DESC *desc);


// Decide where to put these functions; new heading?

sigdata_t *DUMBEXPORT duh_get_raw_sigdata(DUH *duh, int sig, int32 type);

DUH_SIGRENDERER *DUMBEXPORT duh_encapsulate_raw_sigrenderer(sigrenderer_t *vsigrenderer, DUH_SIGTYPE_DESC *desc, int n_channels, int32 pos);
sigrenderer_t *DUMBEXPORT duh_get_raw_sigrenderer(DUH_SIGRENDERER *sigrenderer, int32 type);

int DUMBEXPORT duh_add_signal(DUH *duh, DUH_SIGTYPE_DESC *desc, sigdata_t *sigdata);


/* Standard Signal Types */

//void dumb_register_sigtype_sample(void);


/* Sample Buffer Allocation Helpers */

#ifdef DUMB_DECLARE_DEPRECATED
sample_t **create_sample_buffer(int n_channels, int32 length) DUMB_DEPRECATED;
/* DUMB has been changed to interleave stereo samples. Use
 * allocate_sample_buffer() instead, and see the comments for
 * duh_sigrenderer_set_analyser_callback().
 */
#endif
sample_t **DUMBEXPORT allocate_sample_buffer(int n_channels, int32 length);
void DUMBEXPORT destroy_sample_buffer(sample_t **samples);


/* Silencing Helper */

void DUMBEXPORT dumb_silence(sample_t *samples, int32 length);


/* Click Removal Helpers */

typedef struct DUMB_CLICK_REMOVER DUMB_CLICK_REMOVER;

DUMB_CLICK_REMOVER *DUMBEXPORT dumb_create_click_remover(void);
void DUMBEXPORT dumb_record_click(DUMB_CLICK_REMOVER *cr, int32 pos, sample_t step);
void DUMBEXPORT dumb_remove_clicks(DUMB_CLICK_REMOVER *cr, sample_t *samples, int32 length, int step, double halflife);
sample_t DUMBEXPORT dumb_click_remover_get_offset(DUMB_CLICK_REMOVER *cr);
void DUMBEXPORT dumb_destroy_click_remover(DUMB_CLICK_REMOVER *cr);

DUMB_CLICK_REMOVER **DUMBEXPORT dumb_create_click_remover_array(int n);
void DUMBEXPORT dumb_record_click_array(int n, DUMB_CLICK_REMOVER **cr, int32 pos, sample_t *step);
void DUMBEXPORT dumb_record_click_negative_array(int n, DUMB_CLICK_REMOVER **cr, int32 pos, sample_t *step);
void DUMBEXPORT dumb_remove_clicks_array(int n, DUMB_CLICK_REMOVER **cr, sample_t **samples, int32 length, double halflife);
void DUMBEXPORT dumb_click_remover_get_offset_array(int n, DUMB_CLICK_REMOVER **cr, sample_t *offset);
void DUMBEXPORT dumb_destroy_click_remover_array(int n, DUMB_CLICK_REMOVER **cr);


/* Resampling Helpers */

#define DUMB_RQ_ALIASING 0
#define DUMB_LQ_LINEAR   1
#define DUMB_LQ_CUBIC    2

#define DUMB_RQ_BLEP     3
#define DUMB_RQ_LINEAR   4
#define DUMB_RQ_BLAM     5
#define DUMB_RQ_CUBIC    6
#define DUMB_RQ_FIR      7
#define DUMB_RQ_N_LEVELS 8

/* Subtract quality above by this to convert to resampler.c's quality */
#define DUMB_RESAMPLER_BASE	2

extern int dumb_resampling_quality; /* This specifies the default */
void DUMBEXPORT dumb_it_set_resampling_quality(DUMB_IT_SIGRENDERER * sigrenderer, int quality); /* This overrides it */

typedef struct DUMB_RESAMPLER DUMB_RESAMPLER;

typedef struct DUMB_VOLUME_RAMP_INFO DUMB_VOLUME_RAMP_INFO;

typedef void (*DUMB_RESAMPLE_PICKUP)(DUMB_RESAMPLER *resampler, void *data);

struct DUMB_RESAMPLER
{
	void *src;
	int32 pos;
	int subpos;
	int32 start, end;
	int dir;
	DUMB_RESAMPLE_PICKUP pickup;
	void *pickup_data;
	int quality;
	/* Everything below this point is internal: do not use. */
	union {
		sample_t x24[3*2];
		short x16[3*2];
		signed char x8[3*2];
	} x;
	int overshot;
    double fir_resampler_ratio;
    void* fir_resampler[2];
};

struct DUMB_VOLUME_RAMP_INFO
{
	float volume;
	float delta;
	float target;
	float mix;
    unsigned char declick_stage;
};

void dumb_reset_resampler(DUMB_RESAMPLER *resampler, sample_t *src, int src_channels, int32 pos, int32 start, int32 end, int quality);
DUMB_RESAMPLER *dumb_start_resampler(sample_t *src, int src_channels, int32 pos, int32 start, int32 end, int quality);
//int32 dumb_resample_1_1(DUMB_RESAMPLER *resampler, sample_t *dst, int32 dst_size, DUMB_VOLUME_RAMP_INFO * volume, double delta);
int32 dumb_resample_1_2(DUMB_RESAMPLER *resampler, sample_t *dst, int32 dst_size, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, double delta);
//int32 dumb_resample_2_1(DUMB_RESAMPLER *resampler, sample_t *dst, int32 dst_size, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, double delta);
int32 dumb_resample_2_2(DUMB_RESAMPLER *resampler, sample_t *dst, int32 dst_size, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, double delta);
//void dumb_resample_get_current_sample_1_1(DUMB_RESAMPLER *resampler, DUMB_VOLUME_RAMP_INFO * volume, sample_t *dst);
void dumb_resample_get_current_sample_1_2(DUMB_RESAMPLER *resampler, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, sample_t *dst);
//void dumb_resample_get_current_sample_2_1(DUMB_RESAMPLER *resampler, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, sample_t *dst);
void dumb_resample_get_current_sample_2_2(DUMB_RESAMPLER *resampler, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, sample_t *dst);
void dumb_end_resampler(DUMB_RESAMPLER *resampler);

void dumb_reset_resampler_16(DUMB_RESAMPLER *resampler, short *src, int src_channels, int32 pos, int32 start, int32 end, int quality);
DUMB_RESAMPLER *dumb_start_resampler_16(short *src, int src_channels, int32 pos, int32 start, int32 end, int quality);
//int32 dumb_resample_16_1_1(DUMB_RESAMPLER *resampler, sample_t *dst, int32 dst_size, DUMB_VOLUME_RAMP_INFO * volume, double delta);
int32 dumb_resample_16_1_2(DUMB_RESAMPLER *resampler, sample_t *dst, int32 dst_size, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, double delta);
//int32 dumb_resample_16_2_1(DUMB_RESAMPLER *resampler, sample_t *dst, int32 dst_size, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, double delta);
int32 dumb_resample_16_2_2(DUMB_RESAMPLER *resampler, sample_t *dst, int32 dst_size, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, double delta);
//void dumb_resample_get_current_sample_16_1_1(DUMB_RESAMPLER *resampler, DUMB_VOLUME_RAMP_INFO * volume, sample_t *dst);
void dumb_resample_get_current_sample_16_1_2(DUMB_RESAMPLER *resampler, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, sample_t *dst);
//void dumb_resample_get_current_sample_16_2_1(DUMB_RESAMPLER *resampler, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, sample_t *dst);
void dumb_resample_get_current_sample_16_2_2(DUMB_RESAMPLER *resampler, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, sample_t *dst);
void dumb_end_resampler_16(DUMB_RESAMPLER *resampler);

void dumb_reset_resampler_8(DUMB_RESAMPLER *resampler, signed char *src, int src_channels, int32 pos, int32 start, int32 end, int quality);
DUMB_RESAMPLER *dumb_start_resampler_8(signed char *src, int src_channels, int32 pos, int32 start, int32 end, int quality);
//int32 dumb_resample_8_1_1(DUMB_RESAMPLER *resampler, sample_t *dst, int32 dst_size, DUMB_VOLUME_RAMP_INFO * volume, double delta);
int32 dumb_resample_8_1_2(DUMB_RESAMPLER *resampler, sample_t *dst, int32 dst_size, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, double delta);
//int32 dumb_resample_8_2_1(DUMB_RESAMPLER *resampler, sample_t *dst, int32 dst_size, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, double delta);
int32 dumb_resample_8_2_2(DUMB_RESAMPLER *resampler, sample_t *dst, int32 dst_size, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, double delta);
//void dumb_resample_get_current_sample_8_1_1(DUMB_RESAMPLER *resampler, DUMB_VOLUME_RAMP_INFO * volume, sample_t *dst);
void dumb_resample_get_current_sample_8_1_2(DUMB_RESAMPLER *resampler, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, sample_t *dst);
//void dumb_resample_get_current_sample_8_2_1(DUMB_RESAMPLER *resampler, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, sample_t *dst);
void dumb_resample_get_current_sample_8_2_2(DUMB_RESAMPLER *resampler, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, sample_t *dst);
void dumb_end_resampler_8(DUMB_RESAMPLER *resampler);

void dumb_reset_resampler_n(int n, DUMB_RESAMPLER *resampler, void *src, int src_channels, int32 pos, int32 start, int32 end, int quality);
DUMB_RESAMPLER *dumb_start_resampler_n(int n, void *src, int src_channels, int32 pos, int32 start, int32 end, int quality);
//int32 dumb_resample_n_1_1(int n, DUMB_RESAMPLER *resampler, sample_t *dst, int32 dst_size, DUMB_VOLUME_RAMP_INFO * volume, double delta);
int32 dumb_resample_n_1_2(int n, DUMB_RESAMPLER *resampler, sample_t *dst, int32 dst_size, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, double delta);
//int32 dumb_resample_n_2_1(int n, DUMB_RESAMPLER *resampler, sample_t *dst, int32 dst_size, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, double delta);
int32 dumb_resample_n_2_2(int n, DUMB_RESAMPLER *resampler, sample_t *dst, int32 dst_size, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, double delta);
//void dumb_resample_get_current_sample_n_1_1(int n, DUMB_RESAMPLER *resampler, DUMB_VOLUME_RAMP_INFO * volume, sample_t *dst);
void dumb_resample_get_current_sample_n_1_2(int n, DUMB_RESAMPLER *resampler, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, sample_t *dst);
//void dumb_resample_get_current_sample_n_2_1(int n, DUMB_RESAMPLER *resampler, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, sample_t *dst);
void dumb_resample_get_current_sample_n_2_2(int n, DUMB_RESAMPLER *resampler, DUMB_VOLUME_RAMP_INFO * volume_left, DUMB_VOLUME_RAMP_INFO * volume_right, sample_t *dst);
void dumb_end_resampler_n(int n, DUMB_RESAMPLER *resampler);

/* This sets the default panning separation for hard panned formats,
   or for formats with default panning information. This must be set
   before using any readers or loaders, and is not really thread safe. */

extern int dumb_it_default_panning_separation; /* in percent, default 25 */

/* DUH Construction */

DUH *make_duh(
	int32 length,
	int n_tags,
	const char *const tag[][2],
	int n_signals,
	DUH_SIGTYPE_DESC *desc[],
	sigdata_t *sigdata[]
);

void DUMBEXPORT duh_set_length(DUH *duh, int32 length);

#ifdef __cplusplus
	}
#endif


#endif /* DUMB_H */
