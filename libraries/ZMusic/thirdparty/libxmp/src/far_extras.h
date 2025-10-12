#ifndef XMP_FAR_EXTRAS_H
#define XMP_FAR_EXTRAS_H

#include "common.h"

#define FAR_EXTRAS_MAGIC 0x7b12a83f

/*
struct far_instrument_extras {
	uint32 magic;
};
*/
struct far_channel_extras {
	uint32 magic;
	int vib_sustain;	/* Is vibrato persistent? */
	int vib_rate;		/* Vibrato rate. */
};

struct far_module_extras {
	uint32 magic;
	int coarse_tempo;
	int fine_tempo;
	int tempo_mode;
	int vib_depth;		/* Vibrato depth for all channels. */
};

/*
#define FAR_INSTRUMENT_EXTRAS(x) ((struct far_instrument_extras *)(x).extra)
#define HAS_FAR_INSTRUMENT_EXTRAS(x) \
	(FAR_INSTRUMENT_EXTRAS(x) != NULL && \
	 FAR_INSTRUMENT_EXTRAS(x)->magic == FAR_EXTRAS_MAGIC)
*/
#define FAR_CHANNEL_EXTRAS(x) ((struct far_channel_extras *)(x).extra)
#define HAS_FAR_CHANNEL_EXTRAS(x) \
	(FAR_CHANNEL_EXTRAS(x) != NULL && \
	 FAR_CHANNEL_EXTRAS(x)->magic == FAR_EXTRAS_MAGIC)

#define FAR_MODULE_EXTRAS(x) ((struct far_module_extras *)(x).extra)
#define HAS_FAR_MODULE_EXTRAS(x) \
	(FAR_MODULE_EXTRAS(x) != NULL && \
	 FAR_MODULE_EXTRAS(x)->magic == FAR_EXTRAS_MAGIC)

int libxmp_far_translate_tempo(int, int, int, int *, int *, int *);

void libxmp_far_play_extras(struct context_data *, struct channel_data *, int);
int  libxmp_far_linear_bend(struct context_data *, struct channel_data *);
int  libxmp_far_new_channel_extras(struct channel_data *);
void libxmp_far_reset_channel_extras(struct channel_data *);
void libxmp_far_release_channel_extras(struct channel_data *);
int  libxmp_far_new_module_extras(struct module_data *);
void libxmp_far_release_module_extras(struct module_data *);
void libxmp_far_extras_process_fx(struct context_data *, struct channel_data *, int, uint8, uint8, uint8, int);

#endif

