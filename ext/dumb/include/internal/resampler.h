#ifndef _RESAMPLER_H_
#define _RESAMPLER_H_

// Ugglay
#ifdef RESAMPLER_DECORATE
#define PASTE(a,b) a ## b
#define EVALUATE(a,b) PASTE(a,b)
#define resampler_init EVALUATE(RESAMPLER_DECORATE,_resampler_init)
#define resampler_create EVALUATE(RESAMPLER_DECORATE,_resampler_create)
#define resampler_delete EVALUATE(RESAMPLER_DECORATE,_resampler_delete)
#define resampler_dup EVALUATE(RESAMPLER_DECORATE,_resampler_dup)
#define resampler_dup_inplace EVALUATE(RESAMPLER_DECORATE,_resampler_dup_inplace)
#define resampler_set_quality EVALUATE(RESAMPLER_DECORATE,_resampler_set_quality)
#define resampler_get_free_count EVALUATE(RESAMPLER_DECORATE,_resampler_get_free_count)
#define resampler_write_sample EVALUATE(RESAMPLER_DECORATE,_resampler_write_sample)
#define resampler_write_sample_fixed EVALUATE(RESAMPLER_DECORATE,_resampler_write_sample_fixed)
#define resampler_set_rate EVALUATE(RESAMPLER_DECORATE,_resampler_set_rate)
#define resampler_ready EVALUATE(RESAMPLER_DECORATE,_resampler_ready)
#define resampler_clear EVALUATE(RESAMPLER_DECORATE,_resampler_clear)
#define resampler_get_sample_count EVALUATE(RESAMPLER_DECORATE,_resampler_get_sample_count)
#define resampler_get_sample EVALUATE(RESAMPLER_DECORATE,_resampler_get_sample)
#define resampler_get_sample_float EVALUATE(RESAMPLER_DECORATE,_resampler_get_sample_float)
#define resampler_remove_sample EVALUATE(RESAMPLER_DECORATE,_resampler_remove_sample)
#endif

void resampler_init(void);

void * resampler_create(void);
void resampler_delete(void *);
void * resampler_dup(const void *);
void resampler_dup_inplace(void *, const void *);

enum
{
    RESAMPLER_QUALITY_MIN = 0,
    RESAMPLER_QUALITY_ZOH = 0,
    RESAMPLER_QUALITY_BLEP = 1,
    RESAMPLER_QUALITY_LINEAR = 2,
    RESAMPLER_QUALITY_BLAM = 3,
    RESAMPLER_QUALITY_CUBIC = 4,
    RESAMPLER_QUALITY_SINC = 5,
    RESAMPLER_QUALITY_MAX = 5
};

void resampler_set_quality(void *, int quality);

int resampler_get_free_count(void *);
void resampler_write_sample(void *, short sample);
void resampler_write_sample_fixed(void *, int sample, unsigned char depth);
void resampler_set_rate( void *, double new_factor );
int resampler_ready(void *);
void resampler_clear(void *);
int resampler_get_sample_count(void *);
int resampler_get_sample(void *);
float resampler_get_sample_float(void *);
void resampler_remove_sample(void *, int decay);

#endif
