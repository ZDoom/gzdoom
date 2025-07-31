#ifndef LIBXMP_PLAYER_H
#define LIBXMP_PLAYER_H

#include "lfo.h"

/* Quirk control */
#define HAS_QUIRK(x)	(m->quirk & (x))

/* Channel flag control */
#define SET(f)		SET_FLAG(xc->flags,(f))
#define RESET(f) 	RESET_FLAG(xc->flags,(f))
#define TEST(f)		TEST_FLAG(xc->flags,(f))

/* Persistent effect flag control */
#define SET_PER(f)	SET_FLAG(xc->per_flags,(f))
#define RESET_PER(f)	RESET_FLAG(xc->per_flags,(f))
#define TEST_PER(f)	TEST_FLAG(xc->per_flags,(f))

/* Note flag control */
#define SET_NOTE(f)	SET_FLAG(xc->note_flags,(f))
#define RESET_NOTE(f)	RESET_FLAG(xc->note_flags,(f))
#define TEST_NOTE(f)	TEST_FLAG(xc->note_flags,(f))

struct retrig_control {
	int s;
	int m;
	int d;
};

/* The following macros are used to set the flags for each channel */
#define VOL_SLIDE	(1 << 0)
#define PAN_SLIDE	(1 << 1)
#define TONEPORTA	(1 << 2)
#define PITCHBEND	(1 << 3)
#define VIBRATO		(1 << 4)
#define TREMOLO		(1 << 5)
#define FINE_VOLS	(1 << 6)
#define FINE_BEND	(1 << 7)
#define OFFSET		(1 << 8)
#define TRK_VSLIDE	(1 << 9)
#define TRK_FVSLIDE	(1 << 10)
#define NEW_INS		(1 << 11)
#define NEW_VOL		(1 << 12)
#define VOL_SLIDE_2	(1 << 13)
#define NOTE_SLIDE	(1 << 14)
#define FINE_NSLIDE	(1 << 15)
#define NEW_NOTE	(1 << 16)
#define FINE_TPORTA	(1 << 17)
#define RETRIG		(1 << 18)
#define PANBRELLO	(1 << 19)
#define GVOL_SLIDE	(1 << 20)
#define TEMPO_SLIDE	(1 << 21)
#define VENV_PAUSE	(1 << 22)
#define PENV_PAUSE	(1 << 23)
#define FENV_PAUSE	(1 << 24)
#define FINE_VOLS_2	(1 << 25)
#define KEY_OFF		(1 << 26)	/* for IT release on envloop end */
#define TREMOR		(1 << 27)	/* for XM tremor */
#define MIDI_MACRO	(1 << 28)	/* IT midi macro */

#define NOTE_FADEOUT	(1 << 0)
#define NOTE_ENV_RELEASE (1 << 1)	/* envelope sustain loop release */
#define NOTE_END	(1 << 2)
#define NOTE_CUT	(1 << 3)
#define NOTE_ENV_END	(1 << 4)
#define NOTE_SAMPLE_END	(1 << 5)
#define NOTE_SET	(1 << 6)	/* for IT portamento after keyoff */
#define NOTE_SUSEXIT	(1 << 7)	/* for delayed envelope release */
#define NOTE_KEY_CUT	(1 << 8)	/* note cut with XMP_KEY_CUT event */
#define NOTE_GLISSANDO	(1 << 9)
#define NOTE_SAMPLE_RELEASE (1 << 10)	/* sample sustain loop release */

/* Most of the time, these should be set/reset together. */
#define NOTE_RELEASE	(NOTE_ENV_RELEASE | NOTE_SAMPLE_RELEASE)

/* Note: checking the data pointer for samples should be good enough to filter
 * broken samples, since libxmp_load_sample will always allocate it for valid
 * samples of >0 length and bound the loop values for these samples. */
#define IS_VALID_INSTRUMENT(x) ((uint32)(x) < mod->ins && mod->xxi[(x)].nsm > 0)
#define IS_VALID_INSTRUMENT_OR_SFX(x) (((uint32)(x) < mod->ins && mod->xxi[(x)].nsm > 0) || (smix->ins > 0 && (uint32)(x) < mod->ins + smix->ins))
#define IS_VALID_SAMPLE(x) ((uint32)(x) < mod->smp && mod->xxs[(x)].data != NULL)
#define IS_VALID_NOTE(x) ((uint32)(x) < XMP_MAX_KEYS)

struct instrument_vibrato {
	int phase;
	int sweep;
};

struct channel_data {
	int flags;		/* Channel flags */
	int per_flags;		/* Persistent effect channel flags */
	int note_flags;		/* Note release, fadeout or end */
	int note;		/* Note number */
	int key;		/* Key number */
	double period;		/* Amiga or linear period */
	double per_adj;		/* MED period/pitch adjustment factor hack */
	int finetune;		/* Guess what */
	int ins;		/* Instrument number */
	int old_ins;		/* Last instruemnt */
	int smp;		/* Sample number */
	int mastervol;		/* Master vol -- for IT track vol effect */
	int delay;		/* Note delay in frames */
	int keyoff;		/* Key off counter */
	int fadeout;		/* Current fadeout (release) value */
	int ins_fade;		/* Instrument fadeout value */
	int volume;		/* Current volume */
	int gvl;		/* Global volume for instrument for IT */

	int rvv;		/* Random volume variation */
	int rpv;		/* Random pan variation */

	uint8 split;		/* Split channel */
	uint8 pair;		/* Split channel pair */

	int v_idx;		/* Volume envelope index */
	int p_idx;		/* Pan envelope index */
	int f_idx;		/* Freq envelope index */

	int key_porta;		/* Key number for portamento target
				 * -- needed to handle IT portamento xpo */
	struct {
		struct lfo lfo;
		int memory;
	} vibrato;

	struct {
		struct lfo lfo;
		int memory;
	} tremolo;

#ifndef LIBXMP_CORE_DISABLE_IT
	struct {
		struct lfo lfo;
		int memory;
	} panbrello;
#endif

	struct {
        	int8 val[16];	/* 16 for Smaksak MegaArps */
		int size;
		int count;
		int memory;
	} arpeggio;

	struct {
		struct lfo lfo;
		int sweep;
	} insvib;

	struct {
		int val;
		int val2;	/* For fx9 bug emulation */
		int memory;
	} offset;

	struct {
		int val;	/* Retrig value */
		int count;	/* Retrig counter */
		int type;	/* Retrig type */
		int limit;	/* Number of retrigs */
	} retrig;

	struct {
		uint8 up,down;	/* Tremor value */
		uint8 count;	/* Tremor counter */
		uint8 memory;	/* Tremor memory */
	} tremor;

	struct {
		int slide;	/* Volume slide value */
		int fslide;	/* Fine volume slide value */
		int slide2;	/* Volume slide value */
		int memory;	/* Volume slide effect memory */
#ifndef LIBXMP_CORE_DISABLE_IT
		int fslide2;
		int memory2;	/* Volume slide effect memory */
#endif
#ifndef LIBXMP_CORE_PLAYER
		int target;	/* Target for persistent volslide */
#endif
	} vol;

	struct {
		int up_memory;	/* Fine volume slide up memory (XM) */
		int down_memory;/* Fine volume slide up memory (XM) */
	} fine_vol;

	struct {
		int slide;	/* Global volume slide value */
		int fslide;	/* Fine global volume slide value */
		int memory;	/* Global volume memory is saved per channel */
	} gvol;

	struct {
		int slide;	/* Track volume slide value */
		int fslide;	/* Track fine volume slide value */
		int memory;	/* Track volume slide effect memory */
	} trackvol;

	struct {
		int slide;	/* Frequency slide value */
		double fslide;	/* Fine frequency slide value */
		int memory;	/* Portamento effect memory */
	} freq;

	struct {
		double target;	/* Target period for tone portamento */
		int dir;	/* Tone portamento up/down directionh */
		int slide;	/* Delta for tone portamento */
		int memory;	/* Tone portamento effect memory */
		int note_memory;/* Tone portamento note memory (ULT) */
	} porta;

	struct {
		int up_memory;	/* FT2 has separate memories for these */
		int down_memory;/* cases (see Porta-LinkMem.xm) */
	} fine_porta;

	struct {
		int val;	/* Current pan value */
		int slide;	/* Pan slide value */
		int fslide;	/* Pan fine slide value */
		int memory;	/* Pan slide effect memory */
		int surround;	/* Surround channel flag */
	} pan;

	struct {
		int speed;
		int count;
		int pos;
	} invloop;

#ifndef LIBXMP_CORE_DISABLE_IT
	struct {
		int slide;	/* IT tempo slide */
	} tempo;

	struct {
		int cutoff;	/* IT filter cutoff frequency */
		int resonance;	/* IT filter resonance */
		int envelope;	/* IT filter envelope */
		int can_disable;/* IT hack: allow disabling for cutoff 127 */
	} filter;

	struct {
		float val;	/* Current macro effect (use float for slides) */
		float target;	/* Current macro target (smooth macro) */
		float slide;	/* Current macro slide (smooth macro) */
		int active;	/* Current active parameterized macro */
		int finalvol;	/* Previous tick calculated volume (0-0x400) */
		int notepan;	/* Previous tick note panning (0x80 center) */
	} macro;
#endif

#ifndef LIBXMP_CORE_PLAYER
	struct {
		int slide;	/* PTM note slide amount */
		int fslide;	/* OKT fine note slide amount */
		int speed;	/* PTM note slide speed */
		int count;	/* PTM note slide counter */
	} noteslide;

	void *extra;
#endif

	struct xmp_event delayed_event;
	int delayed_ins;	/* IT save instrument emulation */

	int info_period;	/* Period */
	int info_pitchbend;	/* Linear pitchbend */
	int info_position;	/* Position before mixing */
	int info_finalvol;	/* Final volume including envelopes */
	int info_finalpan;	/* Final pan including envelopes */
};


void	libxmp_process_fx	(struct context_data *, struct channel_data *,
				 int, struct xmp_event *, int);
void	libxmp_filter_setup	(int, int, int, int*, int*, int *);
int	libxmp_read_event	(struct context_data *, struct xmp_event *, int);

#endif /* LIBXMP_PLAYER_H */
