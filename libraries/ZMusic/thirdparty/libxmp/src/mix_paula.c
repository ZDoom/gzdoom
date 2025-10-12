#include "common.h"

#ifdef LIBXMP_PAULA_SIMULATOR
/*
 * Based on Antti S. Lankila's reference code, modified for libxmp
 * by Claudio Matsuoka.
 */
#include "virtual.h"
#include "mixer.h"
#include "paula.h"
#include "precomp_blep.h"

void libxmp_paula_init(struct context_data *ctx, struct paula_state *paula)
{
	struct mixer_data *s = &ctx->s;

	paula->global_output_level = 0;
	paula->active_bleps = 0;
	paula->fdiv = (double)PAULA_HZ / s->freq;
	paula->remainder = paula->fdiv;
}

/* return output simulated as series of bleps */
static int16 output_sample(struct paula_state *paula, int tabnum)
{
	int i;
	int32 output;

	output = paula->global_output_level << BLEP_SCALE;
	for (i = 0; i < paula->active_bleps; i++) {
		int age = paula->blepstate[i].age;
		int level = paula->blepstate[i].level;
		output -= winsinc_integral[tabnum][age] * level;
	}
	output >>= BLEP_SCALE;

	if (output < -32768)
		output = -32768;
	else if (output > 32767)
		output = 32767;

	return output;
}

static void input_sample(struct paula_state *paula, int16 sample)
{
	if (sample != paula->global_output_level) {
		/* Start a new blep: level is the difference, age (or phase) is 0 clocks. */
		if (paula->active_bleps > MAX_BLEPS - 1) {
			D_(D_WARN "active blep list truncated!");
			paula->active_bleps = MAX_BLEPS - 1;
		}

		/* Make room for new blep */
		memmove(&paula->blepstate[1], &paula->blepstate[0],
			sizeof(struct blep_state) * paula->active_bleps);

		/* Update state to account for the new blep */
		paula->active_bleps++;
		paula->blepstate[0].age = 0;
		paula->blepstate[0].level = sample - paula->global_output_level;
		paula->global_output_level = sample;
	}
}

static void do_clock(struct paula_state *paula, int cycles)
{
	int i;

	if (cycles <= 0) {
		return;
	}

	for (i = 0; i < paula->active_bleps; i++) {
		paula->blepstate[i].age += cycles;
		if (paula->blepstate[i].age >= BLEP_SIZE) {
			paula->active_bleps = i;
			break;
		}
	}
}

#define LOOP for (; count; count--)

#define UPDATE_POS(x) do { \
	frac += (x); \
	pos += frac >> SMIX_SHIFT; \
	frac &= SMIX_MASK; \
} while (0)

#define PAULA_SIMULATION(x) do { \
	int num_in = vi->paula->remainder / MINIMUM_INTERVAL; \
	int ministep = step / num_in; \
	int i; \
	\
	/* input is always sampled at a higher rate than output */ \
	for (i = 0; i < num_in - 1; i++) { \
		input_sample(vi->paula, sptr[pos]); \
		do_clock(vi->paula, MINIMUM_INTERVAL); \
		UPDATE_POS(ministep); \
	} \
	input_sample(vi->paula, sptr[pos]); \
	vi->paula->remainder -= num_in * MINIMUM_INTERVAL; \
	\
	do_clock(vi->paula, (int)vi->paula->remainder); \
	smp_in = output_sample(vi->paula, (x)); \
	do_clock(vi->paula, MINIMUM_INTERVAL - (int)vi->paula->remainder); \
	UPDATE_POS(step - (num_in - 1) * ministep); \
	\
	vi->paula->remainder += vi->paula->fdiv; \
} while (0)

#define MIX_MONO() do { \
	*(buffer++) += smp_in * vl; \
} while (0)

#define MIX_STEREO() do { \
	*(buffer++) += smp_in * vl; \
	*(buffer++) += smp_in * vr; \
} while (0)

#define VAR_NORM(x) \
    int smp_in; \
    x *sptr = (x *)vi->sptr; \
    unsigned int pos = vi->pos; \
    int frac = (1 << SMIX_SHIFT) * (vi->pos - (int)vi->pos)

#define VAR_PAULA_MONO(x) \
    VAR_NORM(x); \
    vl <<= 8

#define VAR_PAULA(x) \
    VAR_NORM(x); \
    vl <<= 8; \
    vr <<= 8

MIXER(monoout_mono_a500)
{
	VAR_PAULA_MONO(int8);

	LOOP { PAULA_SIMULATION(0); MIX_MONO(); }
}

MIXER(monoout_mono_a500_filter)
{
	VAR_PAULA_MONO(int8);

	LOOP { PAULA_SIMULATION(1); MIX_MONO(); }
}

MIXER(stereoout_mono_a500)
{
	VAR_PAULA(int8);

	LOOP { PAULA_SIMULATION(0); MIX_STEREO(); }
}

MIXER(stereoout_mono_a500_filter)
{
	VAR_PAULA(int8);

	LOOP { PAULA_SIMULATION(1); MIX_STEREO(); }
}

#endif /* LIBXMP_PAULA_SIMULATOR */
