#ifndef LIBXMP_LFO_H
#define LIBXMP_LFO_H

#include "common.h"

struct lfo {
	int type;
	int rate;
	int depth;
	int phase;
};

int  libxmp_lfo_get(struct context_data *, struct lfo *, int);
void libxmp_lfo_update(struct lfo *);
void libxmp_lfo_set_phase(struct lfo *, int);
void libxmp_lfo_set_depth(struct lfo *, int);
void libxmp_lfo_set_rate(struct lfo *, int);
void libxmp_lfo_set_waveform(struct lfo *, int);

#endif
