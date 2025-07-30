#ifndef XMP_H
#define XMP_H

#if defined(EMSCRIPTEN)
# include <emscripten.h>
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define XMP_VERSION "4.6.0"
#define XMP_VERCODE 0x040600
#define XMP_VER_MAJOR 4
#define XMP_VER_MINOR 6
#define XMP_VER_RELEASE 0

#if defined(_WIN32) && !defined(__CYGWIN__)
# if defined(LIBXMP_STATIC)
#  define LIBXMP_EXPORT
# elif defined(BUILDING_DLL)
#  define LIBXMP_EXPORT __declspec(dllexport)
# else
#  define LIBXMP_EXPORT __declspec(dllimport)
# endif
#elif defined(__OS2__) && defined(__WATCOMC__)
# if defined(LIBXMP_STATIC)
#  define LIBXMP_EXPORT
# elif defined(BUILDING_DLL)
#  define LIBXMP_EXPORT __declspec(dllexport)
# else
#  define LIBXMP_EXPORT
# endif
#elif (defined(__GNUC__) || defined(__clang__) || defined(__HP_cc)) && defined(XMP_SYM_VISIBILITY)
# if defined(LIBXMP_STATIC)
#  define LIBXMP_EXPORT
# else
#  define LIBXMP_EXPORT __attribute__((visibility("default")))
# endif
#elif defined(__SUNPRO_C) && defined(XMP_LDSCOPE_GLOBAL)
# if defined(LIBXMP_STATIC)
#  define LIBXMP_EXPORT
# else
#  define LIBXMP_EXPORT __global
# endif
#elif defined(EMSCRIPTEN)
# define LIBXMP_EXPORT EMSCRIPTEN_KEEPALIVE
# define LIBXMP_EXPORT_VAR
#else
# define LIBXMP_EXPORT
#endif

#if !defined(LIBXMP_EXPORT_VAR)
# define LIBXMP_EXPORT_VAR LIBXMP_EXPORT
#endif

#define XMP_NAME_SIZE		64	/* Size of module name and type */

#define XMP_KEY_OFF		0x81	/* Note number for key off event */
#define XMP_KEY_CUT		0x82	/* Note number for key cut event */
#define XMP_KEY_FADE		0x83	/* Note number for fade event */

/* mixer parameter macros */

/* sample format flags */
#define XMP_FORMAT_8BIT		(1 << 0) /* Mix to 8-bit instead of 16 */
#define XMP_FORMAT_UNSIGNED	(1 << 1) /* Mix to unsigned samples */
#define XMP_FORMAT_MONO		(1 << 2) /* Mix to mono instead of stereo */

/* player parameters */
#define XMP_PLAYER_AMP		0	/* Amplification factor */
#define XMP_PLAYER_MIX		1	/* Stereo mixing */
#define XMP_PLAYER_INTERP	2	/* Interpolation type */
#define XMP_PLAYER_DSP		3	/* DSP effect flags */
#define XMP_PLAYER_FLAGS	4	/* Player flags */
#define XMP_PLAYER_CFLAGS	5	/* Player flags for current module */
#define XMP_PLAYER_SMPCTL	6	/* Sample control flags */
#define XMP_PLAYER_VOLUME	7	/* Player module volume */
#define XMP_PLAYER_STATE	8	/* Internal player state (read only) */
#define XMP_PLAYER_SMIX_VOLUME	9	/* SMIX volume */
#define XMP_PLAYER_DEFPAN	10	/* Default pan setting */
#define XMP_PLAYER_MODE 	11	/* Player personality */
#define XMP_PLAYER_MIXER_TYPE	12	/* Current mixer (read only) */
#define XMP_PLAYER_VOICES	13	/* Maximum number of mixer voices */

/* interpolation types */
#define XMP_INTERP_NEAREST	0	/* Nearest neighbor */
#define XMP_INTERP_LINEAR	1	/* Linear (default) */
#define XMP_INTERP_SPLINE	2	/* Cubic spline */

/* dsp effect types */
#define XMP_DSP_LOWPASS		(1 << 0) /* Lowpass filter effect */
#define XMP_DSP_ALL		(XMP_DSP_LOWPASS)

/* player state */
#define XMP_STATE_UNLOADED	0	/* Context created */
#define XMP_STATE_LOADED	1	/* Module loaded */
#define XMP_STATE_PLAYING	2	/* Module playing */

/* player flags */
#define XMP_FLAGS_VBLANK	(1 << 0) /* Use vblank timing */
#define XMP_FLAGS_FX9BUG	(1 << 1) /* Emulate FX9 bug */
#define XMP_FLAGS_FIXLOOP	(1 << 2) /* Emulate sample loop bug */
#define XMP_FLAGS_A500		(1 << 3) /* Use Paula mixer in Amiga modules */

/* player modes */
#define XMP_MODE_AUTO		0	/* Autodetect mode (default) */
#define XMP_MODE_MOD		1	/* Play as a generic MOD player */
#define XMP_MODE_NOISETRACKER	2	/* Play using Noisetracker quirks */
#define XMP_MODE_PROTRACKER	3	/* Play using Protracker quirks */
#define XMP_MODE_S3M		4	/* Play as a generic S3M player */
#define XMP_MODE_ST3		5	/* Play using ST3 bug emulation */
#define XMP_MODE_ST3GUS		6	/* Play using ST3+GUS quirks */
#define XMP_MODE_XM		7	/* Play as a generic XM player */
#define XMP_MODE_FT2		8	/* Play using FT2 bug emulation */
#define XMP_MODE_IT		9	/* Play using IT quirks */
#define XMP_MODE_ITSMP		10	/* Play using IT sample mode quirks */

/* mixer types */
#define XMP_MIXER_STANDARD	0	/* Standard mixer */
#define XMP_MIXER_A500		1	/* Amiga 500 */
#define XMP_MIXER_A500F		2	/* Amiga 500 with led filter */

/* sample flags */
#define XMP_SMPCTL_SKIP		(1 << 0) /* Don't load samples */

/* limits */
#define XMP_MAX_KEYS		121	/* Number of valid keys */
#define XMP_MAX_ENV_POINTS	32	/* Max number of envelope points */
#define XMP_MAX_MOD_LENGTH	256	/* Max number of patterns in module */
#define XMP_MAX_CHANNELS	64	/* Max number of channels in module */
#define XMP_MAX_SRATE		49170	/* max sampling rate (Hz) */
#define XMP_MIN_SRATE		4000	/* min sampling rate (Hz) */
#define XMP_MIN_BPM		20	/* min BPM */
/* frame rate = (50 * bpm / 125) Hz */
/* frame size = (sampling rate * channels * size) / frame rate */
#define XMP_MAX_FRAMESIZE	(5 * XMP_MAX_SRATE * 2 / XMP_MIN_BPM)

/* error codes */
#define XMP_END			1
#define XMP_ERROR_INTERNAL	2	/* Internal error */
#define XMP_ERROR_FORMAT	3	/* Unsupported module format */
#define XMP_ERROR_LOAD		4	/* Error loading file */
#define XMP_ERROR_DEPACK	5	/* Error depacking file */
#define XMP_ERROR_SYSTEM	6	/* System error */
#define XMP_ERROR_INVALID	7	/* Invalid parameter */
#define XMP_ERROR_STATE		8	/* Invalid player state */

struct xmp_channel {
	int pan;			/* Channel pan (0x80 is center) */
	int vol;			/* Channel volume */
#define XMP_CHANNEL_SYNTH	(1 << 0)  /* Channel is synthesized */
#define XMP_CHANNEL_MUTE  	(1 << 1)  /* Channel is muted */
#define XMP_CHANNEL_SPLIT	(1 << 2)  /* Split Amiga channel in bits 5-4 */
#define XMP_CHANNEL_SURROUND	(1 << 4)  /* Surround channel */
	int flg;			/* Channel flags */
};

struct xmp_pattern {
	int rows;			/* Number of rows */
	int index[1];			/* Track index */
};

struct xmp_event {
	unsigned char note;		/* Note number (0 means no note) */
	unsigned char ins;		/* Patch number */
	unsigned char vol;		/* Volume (0 to basevol) */
	unsigned char fxt;		/* Effect type */
	unsigned char fxp;		/* Effect parameter */
	unsigned char f2t;		/* Secondary effect type */
	unsigned char f2p;		/* Secondary effect parameter */
	unsigned char _flag;		/* Internal (reserved) flags */
};

struct xmp_track {
	int rows;			/* Number of rows */
	struct xmp_event event[1];	/* Event data */
};

struct xmp_envelope {
#define XMP_ENVELOPE_ON		(1 << 0)  /* Envelope is enabled */
#define XMP_ENVELOPE_SUS	(1 << 1)  /* Envelope has sustain point */
#define XMP_ENVELOPE_LOOP	(1 << 2)  /* Envelope has loop */
#define XMP_ENVELOPE_FLT	(1 << 3)  /* Envelope is used for filter */
#define XMP_ENVELOPE_SLOOP	(1 << 4)  /* Envelope has sustain loop */
#define XMP_ENVELOPE_CARRY	(1 << 5)  /* Don't reset envelope position */
	int flg;			/* Flags */
	int npt;			/* Number of envelope points */
	int scl;			/* Envelope scaling */
	int sus;			/* Sustain start point */
	int sue;			/* Sustain end point */
	int lps;			/* Loop start point */
	int lpe;			/* Loop end point */
	short data[XMP_MAX_ENV_POINTS * 2];
};

struct xmp_subinstrument {
	int vol;			/* Default volume */
	int gvl;			/* Global volume */
	int pan;			/* Pan */
	int xpo;			/* Transpose */
	int fin;			/* Finetune */
	int vwf;			/* Vibrato waveform */
	int vde;			/* Vibrato depth */
	int vra;			/* Vibrato rate */
	int vsw;			/* Vibrato sweep */
	int rvv;			/* Random volume/pan variation (IT) */
	int sid;			/* Sample number */
#define XMP_INST_NNA_CUT	0x00
#define XMP_INST_NNA_CONT	0x01
#define XMP_INST_NNA_OFF	0x02
#define XMP_INST_NNA_FADE	0x03
	int nna;			/* New note action */
#define XMP_INST_DCT_OFF	0x00
#define XMP_INST_DCT_NOTE	0x01
#define XMP_INST_DCT_SMP	0x02
#define XMP_INST_DCT_INST	0x03
	int dct;			/* Duplicate check type */
#define XMP_INST_DCA_CUT	XMP_INST_NNA_CUT
#define XMP_INST_DCA_OFF	XMP_INST_NNA_OFF
#define XMP_INST_DCA_FADE	XMP_INST_NNA_FADE
	int dca;			/* Duplicate check action */
	int ifc;			/* Initial filter cutoff */
	int ifr;			/* Initial filter resonance */
};

struct xmp_instrument {
	char name[32];			/* Instrument name */
	int vol;			/* Instrument volume */
	int nsm;			/* Number of samples */
	int rls;			/* Release (fadeout) */
	struct xmp_envelope aei;	/* Amplitude envelope info */
	struct xmp_envelope pei;	/* Pan envelope info */
	struct xmp_envelope fei;	/* Frequency envelope info */

	struct {
		unsigned char ins;	/* Instrument number for each key */
		signed char xpo;	/* Instrument transpose for each key */
	} map[XMP_MAX_KEYS];

	struct xmp_subinstrument	*sub;

	void *extra;			/* Extra fields */
};

struct xmp_sample {
	char name[32];			/* Sample name */
	int len;			/* Sample length */
	int lps;			/* Loop start */
	int lpe;			/* Loop end */
#define XMP_SAMPLE_16BIT	(1 << 0)  /* 16bit sample */
#define XMP_SAMPLE_LOOP		(1 << 1)  /* Sample is looped */
#define XMP_SAMPLE_LOOP_BIDIR	(1 << 2)  /* Bidirectional sample loop */
#define XMP_SAMPLE_LOOP_REVERSE	(1 << 3)  /* Backwards sample loop */
#define XMP_SAMPLE_LOOP_FULL	(1 << 4)  /* Play full sample before looping */
#define XMP_SAMPLE_SLOOP	(1 << 5)  /* Sample has sustain loop */
#define XMP_SAMPLE_SLOOP_BIDIR	(1 << 6)  /* Bidirectional sustain loop */
#define XMP_SAMPLE_STEREO	(1 << 7)  /* Interlaced stereo sample */
#define XMP_SAMPLE_SYNTH	(1 << 15) /* Data contains synth patch */
	int flg;			/* Flags */
	unsigned char *data;		/* Sample data */
};

struct xmp_sequence {
	int entry_point;
	int duration;
};

struct xmp_module {
	char name[XMP_NAME_SIZE];	/* Module title */
	char type[XMP_NAME_SIZE];	/* Module format */
	int pat;			/* Number of patterns */
	int trk;			/* Number of tracks */
	int chn;			/* Tracks per pattern */
	int ins;			/* Number of instruments */
	int smp;			/* Number of samples */
	int spd;			/* Initial speed */
	int bpm;			/* Initial BPM */
	int len;			/* Module length in patterns */
	int rst;			/* Restart position */
	int gvl;			/* Global volume */

	struct xmp_pattern **xxp;	/* Patterns */
	struct xmp_track **xxt;		/* Tracks */
	struct xmp_instrument *xxi;	/* Instruments */
	struct xmp_sample *xxs;		/* Samples */
	struct xmp_channel xxc[XMP_MAX_CHANNELS]; /* Channel info */
	unsigned char xxo[XMP_MAX_MOD_LENGTH];	/* Orders */
};

struct xmp_test_info {
	char name[XMP_NAME_SIZE];	/* Module title */
	char type[XMP_NAME_SIZE];	/* Module format */
};

struct xmp_module_info {
	unsigned char md5[16];		/* MD5 message digest */
	int vol_base;			/* Volume scale */
	struct xmp_module *mod;		/* Pointer to module data */
	char *comment;			/* Comment text, if any */
	int num_sequences;		/* Number of valid sequences */
	struct xmp_sequence *seq_data;	/* Pointer to sequence data */
};

struct xmp_channel_info {
	unsigned int period;		/* Sample period (* 4096) */
	unsigned int position;		/* Sample position */
	short pitchbend;		/* Linear bend from base note*/
	unsigned char note;		/* Current base note number */
	unsigned char instrument;	/* Current instrument number */
	unsigned char sample;		/* Current sample number */
	unsigned char volume;		/* Current volume */
	unsigned char pan;		/* Current stereo pan */
	unsigned char reserved;		/* Reserved */
	struct xmp_event event;		/* Current track event */
};

struct xmp_frame_info {			/* Current frame information */
	int pos;			/* Current position */
	int pattern;			/* Current pattern */
	int row;			/* Current row in pattern */
	int num_rows;			/* Number of rows in current pattern */
	int frame;			/* Current frame */
	int speed;			/* Current replay speed */
	int bpm;			/* Current bpm */
	int time;			/* Current module time in ms */
	int total_time;			/* Estimated replay time in ms*/
	int frame_time;			/* Frame replay time in us */
	void *buffer;			/* Pointer to sound buffer */
	int buffer_size;		/* Used buffer size */
	int total_size;			/* Total buffer size */
	int volume;			/* Current master volume */
	int loop_count;			/* Loop counter */
	int virt_channels;		/* Number of virtual channels */
	int virt_used;			/* Used virtual channels */
	int sequence;			/* Current sequence */

	struct xmp_channel_info channel_info[XMP_MAX_CHANNELS];		/* Current channel information */
};

struct xmp_callbacks {
	unsigned long	(*read_func)(void *dest, unsigned long len,
				     unsigned long nmemb, void *priv);
	int		(*seek_func)(void *priv, long offset, int whence);
	long		(*tell_func)(void *priv);
	int		(*close_func)(void *priv);
};

typedef char *xmp_context;

LIBXMP_EXPORT_VAR extern const char *xmp_version;
LIBXMP_EXPORT_VAR extern const unsigned int xmp_vercode;

LIBXMP_EXPORT int         xmp_syserrno        (void);

LIBXMP_EXPORT xmp_context xmp_create_context  (void);
LIBXMP_EXPORT void        xmp_free_context    (xmp_context);

LIBXMP_EXPORT int         xmp_load_module     (xmp_context, const char *);
LIBXMP_EXPORT int         xmp_load_module_from_memory (xmp_context, const void *, long);
LIBXMP_EXPORT int         xmp_load_module_from_file (xmp_context, void *, long);
LIBXMP_EXPORT int         xmp_load_module_from_callbacks (xmp_context, void *, struct xmp_callbacks);

LIBXMP_EXPORT int         xmp_test_module     (const char *, struct xmp_test_info *);
LIBXMP_EXPORT int         xmp_test_module_from_memory (const void *, long, struct xmp_test_info *);
LIBXMP_EXPORT int         xmp_test_module_from_file (void *, struct xmp_test_info *);
LIBXMP_EXPORT int         xmp_test_module_from_callbacks (void *, struct xmp_callbacks, struct xmp_test_info *);

LIBXMP_EXPORT void        xmp_scan_module     (xmp_context);
LIBXMP_EXPORT void        xmp_release_module  (xmp_context);

LIBXMP_EXPORT int         xmp_start_player    (xmp_context, int, int);
LIBXMP_EXPORT int         xmp_play_frame      (xmp_context);
LIBXMP_EXPORT int         xmp_play_buffer     (xmp_context, void *, int, int);
LIBXMP_EXPORT void        xmp_get_frame_info  (xmp_context, struct xmp_frame_info *);
LIBXMP_EXPORT void        xmp_end_player      (xmp_context);
LIBXMP_EXPORT void        xmp_inject_event    (xmp_context, int, struct xmp_event *);
LIBXMP_EXPORT void        xmp_get_module_info (xmp_context, struct xmp_module_info *);
LIBXMP_EXPORT const char *const *xmp_get_format_list (void);
LIBXMP_EXPORT int         xmp_next_position   (xmp_context);
LIBXMP_EXPORT int         xmp_prev_position   (xmp_context);
LIBXMP_EXPORT int         xmp_set_position    (xmp_context, int);
LIBXMP_EXPORT int         xmp_set_row         (xmp_context, int);
LIBXMP_EXPORT int         xmp_set_tempo_factor(xmp_context, double);
LIBXMP_EXPORT void        xmp_stop_module     (xmp_context);
LIBXMP_EXPORT void        xmp_restart_module  (xmp_context);
LIBXMP_EXPORT int         xmp_seek_time       (xmp_context, int);
LIBXMP_EXPORT int         xmp_channel_mute    (xmp_context, int, int);
LIBXMP_EXPORT int         xmp_channel_vol     (xmp_context, int, int);
LIBXMP_EXPORT int         xmp_set_player      (xmp_context, int, int);
LIBXMP_EXPORT int         xmp_get_player      (xmp_context, int);
LIBXMP_EXPORT int         xmp_set_instrument_path (xmp_context, const char *);

/* External sample mixer API */
LIBXMP_EXPORT int         xmp_start_smix       (xmp_context, int, int);
LIBXMP_EXPORT void        xmp_end_smix         (xmp_context);
LIBXMP_EXPORT int         xmp_smix_play_instrument(xmp_context, int, int, int, int);
LIBXMP_EXPORT int         xmp_smix_play_sample (xmp_context, int, int, int, int);
LIBXMP_EXPORT int         xmp_smix_channel_pan (xmp_context, int, int);
LIBXMP_EXPORT int         xmp_smix_load_sample (xmp_context, int, const char *);
LIBXMP_EXPORT int         xmp_smix_release_sample (xmp_context, int);

#ifdef __cplusplus
}
#endif

#endif	/* XMP_H */
