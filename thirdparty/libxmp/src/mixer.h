#ifndef LIBXMP_MIXER_H
#define LIBXMP_MIXER_H

#define C4_PERIOD	428.0

#define SMIX_NUMVOC	128	/* default number of softmixer voices */
#define SMIX_SHIFT	16
#define SMIX_MASK	0xffff

#define FILTER_SHIFT	16
#define ANTICLICK_SHIFT	3

#ifdef LIBXMP_PAULA_SIMULATOR
#include "paula.h"
#endif

#define MIXER(f) void libxmp_mix_##f(struct mixer_voice *vi, int *buffer, \
	int count, int vl, int vr, int step, int ramp, int delta_l, int delta_r)

struct mixer_voice {
	int chn;		/* channel number */
	int root;		/* */
	int note;		/* */
#define PAN_SURROUND 0x8000
	int pan;		/* */
	int vol;		/* */
	double period;		/* current period */
	double pos;		/* position in sample */
	int pos0;		/* position in sample before mixing */
	int fidx;		/* mixer function index */
	int ins;		/* instrument number */
	int smp;		/* sample number */
	int start;		/* loop start */
	int end;		/* loop end */
	int act;		/* nna info & status of voice */
	int key;		/* key for DCA note check */
	int old_vl;		/* previous volume, left channel */
	int old_vr;		/* previous volume, right channel */
	int sleft;		/* last left sample output, in 32bit */
	int sright;		/* last right sample output, in 32bit */
#define VOICE_RELEASE	(1 << 0)
#define ANTICLICK	(1 << 1)
#define SAMPLE_LOOP	(1 << 2)
#define VOICE_REVERSE	(1 << 3)
#define VOICE_BIDIR	(1 << 4)
	int flags;		/* flags */
	void *sptr;		/* sample pointer */
#ifdef LIBXMP_PAULA_SIMULATOR
	struct paula_state *paula; /* paula simulation state */
#endif

#ifndef LIBXMP_CORE_DISABLE_IT
	struct {
		int r1;		/* filter variables */
		int r2;
		int l1;
		int l2;
		int a0;
		int b0;
		int b1;
		int cutoff;
		int resonance;
	} filter;
#endif
};

int	libxmp_mixer_on		(struct context_data *, int, int, int);
void	libxmp_mixer_off	(struct context_data *);
void    libxmp_mixer_setvol	(struct context_data *, int, int);
void    libxmp_mixer_seteffect	(struct context_data *, int, int, int);
void    libxmp_mixer_setpan	(struct context_data *, int, int);
int	libxmp_mixer_numvoices	(struct context_data *, int);
void	libxmp_mixer_softmixer	(struct context_data *);
void	libxmp_mixer_reset	(struct context_data *);
void	libxmp_mixer_setpatch	(struct context_data *, int, int, int);
void	libxmp_mixer_voicepos	(struct context_data *, int, double, int);
double	libxmp_mixer_getvoicepos(struct context_data *, int);
void	libxmp_mixer_setnote	(struct context_data *, int, int);
void	libxmp_mixer_setperiod	(struct context_data *, int, double);
void	libxmp_mixer_release	(struct context_data *, int, int);
void	libxmp_mixer_reverse	(struct context_data *, int, int);

#endif /* LIBXMP_MIXER_H */
