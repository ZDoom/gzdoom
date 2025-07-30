#ifndef LIBXMP_PERIOD_H
#define LIBXMP_PERIOD_H

#define PERIOD_BASE	13696.0		/* C0 period */

/* Macros for period conversion */
#define NOTE_B0		11
#define NOTE_Bb0	(NOTE_B0 + 1)
#define MAX_NOTE	(NOTE_B0 * 8)
#define MAX_PERIOD	0x1c56
#define MIN_PERIOD_A	0x0071
#define MAX_PERIOD_A	0x0358
#define MIN_PERIOD_L	0x0000
#define MAX_PERIOD_L	0x1e00
#define MIN_NOTE_MOD	48
#define MAX_NOTE_MOD	83

double	libxmp_note_to_period	(struct context_data *, int, int, double);
double	libxmp_note_to_period_mix (int, int);
int	libxmp_period_to_note	(int);
int	libxmp_period_to_bend	(struct context_data *, double, int, double);
void	libxmp_c2spd_to_note	(int, int *, int *);
#ifndef LIBXMP_CORE_PLAYER
double	libxmp_gus_frequency_steps (int, int);
#endif

#endif /* LIBXMP_PERIOD_H */
