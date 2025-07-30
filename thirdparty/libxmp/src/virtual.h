#ifndef LIBXMP_VIRTUAL_H
#define LIBXMP_VIRTUAL_H

#include "common.h"

#define VIRT_ACTION_CUT		XMP_INST_NNA_CUT
#define VIRT_ACTION_CONT	XMP_INST_NNA_CONT
#define VIRT_ACTION_OFF		XMP_INST_NNA_OFF
#define VIRT_ACTION_FADE	XMP_INST_NNA_FADE

#define VIRT_ACTIVE		0x100
#define VIRT_INVALID		-1

int	libxmp_virt_on		(struct context_data *, int);
void	libxmp_virt_off		(struct context_data *);
int	libxmp_virt_mute	(struct context_data *, int, int);
int	libxmp_virt_setpatch	(struct context_data *, int, int, int, int,
				 int, int, int, int);
int	libxmp_virt_cvt8bit	(void);
void	libxmp_virt_setnote	(struct context_data *, int, int);
void	libxmp_virt_setsmp	(struct context_data *, int, int);
void	libxmp_virt_setnna	(struct context_data *, int, int);
void	libxmp_virt_pastnote	(struct context_data *, int, int);
void	libxmp_virt_setvol	(struct context_data *, int, int);
void	libxmp_virt_voicepos	(struct context_data *, int, double);
double	libxmp_virt_getvoicepos	(struct context_data *, int);
void	libxmp_virt_setperiod	(struct context_data *, int, double);
void	libxmp_virt_setpan	(struct context_data *, int, int);
void	libxmp_virt_seteffect	(struct context_data *, int, int, int);
int	libxmp_virt_cstat	(struct context_data *, int);
int	libxmp_virt_mapchannel	(struct context_data *, int);
void	libxmp_virt_resetchannel(struct context_data *, int);
void	libxmp_virt_resetvoice	(struct context_data *, int, int);
void	libxmp_virt_reset	(struct context_data *);
void	libxmp_virt_release	(struct context_data *, int, int);
void	libxmp_virt_reverse	(struct context_data *, int, int);
int	libxmp_virt_getroot	(struct context_data *, int);

#endif /* LIBXMP_VIRTUAL_H */
