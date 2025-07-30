#ifndef LIBXMP_EXTRAS_H
#define LIBXMP_EXTRAS_H

void libxmp_release_module_extras(struct context_data *);
int  libxmp_new_channel_extras(struct context_data *, struct channel_data *);
void libxmp_release_channel_extras(struct context_data *, struct channel_data *);
void libxmp_reset_channel_extras(struct context_data *, struct channel_data *);
void libxmp_play_extras(struct context_data *, struct channel_data *, int);
int  libxmp_extras_get_volume(struct context_data *, struct channel_data *);
int  libxmp_extras_get_period(struct context_data *, struct channel_data *);
int  libxmp_extras_get_linear_bend(struct context_data *, struct channel_data *);
void libxmp_extras_process_fx(struct context_data *, struct channel_data *, int, uint8, uint8, uint8, int);


/* FIXME */
void libxmp_med_hold_hack(struct context_data *ctx, int, int, int);

#endif
