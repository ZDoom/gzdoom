#ifndef LIBXMP_MED_EXTRAS_H
#define LIBXMP_MED_EXTRAS_H

#define MED_EXTRAS_MAGIC 0x7f20ca5

struct med_instrument_extras {
	uint32 magic;
	int vts;		/* Volume table speed */
	int wts;		/* Waveform table speed */
	int vtlen;		/* Volume table length */
	int wtlen;		/* Waveform table length */
	int hold;
};

struct med_channel_extras {
	uint32 magic;
	int vp;			/* MED synth volume table pointer */
	int vv;			/* MED synth volume slide value */
	int vs;			/* MED synth volume speed */
	int vc;			/* MED synth volume speed counter */
	int vw;			/* MED synth volume wait counter */
	int wp;			/* MED synth waveform table pointer */
	int wv;			/* MED synth waveform slide value */
	int ws;			/* MED synth waveform speed */
	int wc;			/* MED synth waveform speed counter */
	int ww;			/* MED synth waveform wait counter */
	int period;		/* MED synth period for RES */
	int arp;		/* MED synth arpeggio start */
	int aidx;		/* MED synth arpeggio index */
	int vwf;		/* MED synth vibrato waveform */
	int vib_depth;		/* MED synth vibrato depth */
	int vib_speed;		/* MED synth vibrato speed */
	int vib_idx;		/* MED synth vibrato index */
	int vib_wf;		/* MED synth vibrato waveform */
	int volume;		/* MED synth note volume */
	int hold;		/* MED note on hold flag */
	int hold_count;		/* MED note on hold frame counter */
	int env_wav;		/* MED synth volume envelope waveform */
	int env_idx;		/* MED synth volume envelope index */
#define MED_SYNTH_ENV_LOOP (1 << 0)
	int flags;		/* flags */
};

struct med_module_extras {
	uint32 magic;
	uint8 **vol_table;	/* MED volume sequence table */
	uint8 **wav_table;	/* MED waveform sequence table */
};

#define MED_INSTRUMENT_EXTRAS(x) ((struct med_instrument_extras *)(x).extra)
#define HAS_MED_INSTRUMENT_EXTRAS(x) \
	(MED_INSTRUMENT_EXTRAS(x) != NULL && \
	 MED_INSTRUMENT_EXTRAS(x)->magic == MED_EXTRAS_MAGIC)

#define MED_CHANNEL_EXTRAS(x) ((struct med_channel_extras *)(x).extra)
#define HAS_MED_CHANNEL_EXTRAS(x) \
	(MED_CHANNEL_EXTRAS(x) != NULL && \
	 MED_CHANNEL_EXTRAS(x)->magic == MED_EXTRAS_MAGIC)

#define MED_MODULE_EXTRAS(x) ((struct med_module_extras *)(x).extra)
#define HAS_MED_MODULE_EXTRAS(x) \
	(MED_MODULE_EXTRAS(x) != NULL && \
	 MED_MODULE_EXTRAS(x)->magic == MED_EXTRAS_MAGIC)

int  libxmp_med_change_period(struct context_data *, struct channel_data *);
int  libxmp_med_linear_bend(struct context_data *, struct channel_data *);
int  libxmp_med_get_vibrato(struct channel_data *);
void libxmp_med_play_extras(struct context_data *, struct channel_data *, int);
int  libxmp_med_new_instrument_extras(struct xmp_instrument *);
int  libxmp_med_new_channel_extras(struct channel_data *);
void libxmp_med_reset_channel_extras(struct channel_data *);
void libxmp_med_release_channel_extras(struct channel_data *);
int  libxmp_med_new_module_extras(struct module_data *);
void libxmp_med_release_module_extras(struct module_data *);
void libxmp_med_extras_process_fx(struct context_data *, struct channel_data *, int, uint8, uint8, uint8, int);

#endif
