#ifndef LIBXMP_EFFECTS_H
#define LIBXMP_EFFECTS_H

/* Protracker effects */
#define FX_ARPEGGIO	0x00
#define FX_PORTA_UP	0x01
#define FX_PORTA_DN	0x02
#define FX_TONEPORTA	0x03
#define FX_VIBRATO	0x04
#define FX_TONE_VSLIDE  0x05
#define FX_VIBRA_VSLIDE	0x06
#define FX_TREMOLO	0x07
#define FX_OFFSET	0x09
#define FX_VOLSLIDE	0x0a
#define FX_JUMP		0x0b
#define FX_VOLSET	0x0c
#define FX_BREAK	0x0d
#define FX_EXTENDED	0x0e
#define FX_SPEED	0x0f

/* Fast tracker effects */
#define FX_SETPAN	0x08

/* Fast Tracker II effects */
#define FX_GLOBALVOL	0x10
#define FX_GVOL_SLIDE	0x11
#define FX_KEYOFF	0x14
#define FX_ENVPOS	0x15
#define FX_PANSLIDE	0x19
#define FX_MULTI_RETRIG	0x1b
#define FX_TREMOR	0x1d
#define FX_XF_PORTA	0x21

/* Protracker extended effects */
#define EX_FILTER	0x00
#define EX_F_PORTA_UP	0x01
#define EX_F_PORTA_DN	0x02
#define EX_GLISS	0x03
#define EX_VIBRATO_WF	0x04
#define EX_FINETUNE	0x05
#define EX_PATTERN_LOOP	0x06
#define EX_TREMOLO_WF	0x07
#define EX_SETPAN	0x08
#define EX_RETRIG	0x09
#define EX_F_VSLIDE_UP	0x0a
#define EX_F_VSLIDE_DN	0x0b
#define EX_CUT		0x0c
#define EX_DELAY	0x0d
#define EX_PATT_DELAY	0x0e
#define EX_INVLOOP	0x0f

#ifndef LIBXMP_CORE_PLAYER
/* Oktalyzer effects */
#define FX_OKT_ARP3	0x70
#define FX_OKT_ARP4	0x71
#define FX_OKT_ARP5	0x72
#define FX_NSLIDE2_DN	0x73
#define FX_NSLIDE2_UP	0x74
#define FX_F_NSLIDE_DN	0x75
#define FX_F_NSLIDE_UP	0x76

/* Persistent effects -- for FNK */
#define FX_PER_PORTA_DN	0x78
#define FX_PER_PORTA_UP	0x79
#define FX_PER_TPORTA	0x7a
#define FX_PER_VIBRATO	0x7b
#define FX_PER_VSLD_UP	0x7c
#define FX_PER_VSLD_DN	0x7d
#define FX_SPEED_CP	0x7e
#define FX_PER_CANCEL	0x7f

/* 669 frequency based effects */
#define FX_669_PORTA_UP	0x60
#define FX_669_PORTA_DN	0x61
#define FX_669_TPORTA	0x62
#define FX_669_FINETUNE	0x63
#define FX_669_VIBRATO	0x64

/* FAR effects */
#define FX_FAR_PORTA_UP	0x65	/* FAR pitch offset up */
#define FX_FAR_PORTA_DN	0x66	/* FAR pitch offset down */
#define FX_FAR_TPORTA	0x67	/* FAR persistent tone portamento */
#define FX_FAR_TEMPO	0x68	/* FAR coarse tempo and tempo mode */
#define FX_FAR_F_TEMPO	0x69	/* FAR fine tempo slide up/down */
#define FX_FAR_VIBDEPTH	0x6a	/* FAR set vibrato depth */
#define FX_FAR_VIBRATO	0x6b	/* FAR persistent vibrato */
#define FX_FAR_SLIDEVOL	0x6c	/* FAR persistent slide-to-volume */
#define FX_FAR_RETRIG	0x6d	/* FAR retrigger */
#define FX_FAR_DELAY	0x6e	/* FAR note offset */

/* Other frequency based effects (ULT, etc) */
#define FX_ULT_TPORTA   0x6f
#endif

#ifndef LIBXMP_CORE_DISABLE_IT
/* IT effects */
#define FX_TRK_VOL      0x80
#define FX_TRK_VSLIDE   0x81
#define FX_TRK_FVSLIDE  0x82
#define FX_IT_INSTFUNC	0x83
#define FX_FLT_CUTOFF	0x84
#define FX_FLT_RESN	0x85
#define FX_IT_BPM	0x87
#define FX_IT_ROWDELAY	0x88
#define FX_IT_PANSLIDE	0x89
#define FX_PANBRELLO	0x8a
#define FX_PANBRELLO_WF	0x8b
#define FX_HIOFFSET	0x8c
#define FX_IT_BREAK	0x8e	/* like FX_BREAK with hex parameter */
#define FX_MACRO_SET	0xbd	/* Set active IT parametered MIDI macro */
#define FX_MACRO	0xbe	/* Execute IT MIDI macro */
#define FX_MACROSMOOTH	0xbf	/* Execute IT MIDI macro slide */
#endif

#ifndef LIBXMP_CORE_PLAYER
/* MED effects */
#define FX_HOLD_DECAY	0x90
#define FX_SETPITCH	0x91
#define FX_VIBRATO2	0x92

/* PTM effects */
#define FX_NSLIDE_DN	0x9c	/* IMF/PTM note slide down */
#define FX_NSLIDE_UP	0x9d	/* IMF/PTM note slide up */
#define FX_NSLIDE_R_UP	0x9e	/* PTM note slide down with retrigger */
#define FX_NSLIDE_R_DN	0x9f	/* PTM note slide up with retrigger */

/* Extra effects */
#define FX_VOLSLIDE_UP	0xa0	/* SFX, MDL */
#define FX_VOLSLIDE_DN	0xa1
#define FX_F_VSLIDE	0xa5	/* IMF/MDL */
#define FX_CHORUS	0xa9	/* IMF */
#define FX_ICE_SPEED	0xa2
#define FX_REVERB	0xaa	/* IMF */
#define FX_MED_HOLD	0xb1	/* MMD hold/decay */
#define FX_MEGAARP	0xb2	/* Smaksak effect 7: MegaArp */
#define FX_VOL_ADD	0xb6	/* SFX change volume up */
#define FX_VOL_SUB	0xb7	/* SFX change volume down */
#define FX_PITCH_ADD	0xb8	/* SFX add steps to current note */
#define FX_PITCH_SUB	0xb9	/* SFX add steps to current note */
#define FX_LINE_JUMP	0xba	/* Archimedes jump to line in current order */
#endif

#define FX_SURROUND	0x8d	/* S3M/IT */
#define FX_REVERSE	0x8f	/* XM/IT/others: play forward/reverse */
#define FX_S3M_SPEED	0xa3	/* S3M */
#define FX_VOLSLIDE_2	0xa4
#define FX_FINETUNE	0xa6
#define FX_S3M_BPM	0xab	/* S3M */
#define FX_FINE_VIBRATO	0xac	/* S3M/PTM/IMF/LIQ */
#define FX_F_VSLIDE_UP	0xad	/* MMD */
#define FX_F_VSLIDE_DN	0xae	/* MMD */
#define FX_F_PORTA_UP	0xaf	/* MMD */
#define FX_F_PORTA_DN	0xb0	/* MMD */
#define FX_PATT_DELAY	0xb3	/* MMD */
#define FX_S3M_ARPEGGIO	0xb4
#define FX_PANSL_NOMEM	0xb5	/* XM volume column */

#define FX_VSLIDE_UP_2	0xc0	/* IT volume column volume slide */
#define FX_VSLIDE_DN_2	0xc1
#define FX_F_VSLIDE_UP_2 0xc2
#define FX_F_VSLIDE_DN_2 0xc3

#endif /* LIBXMP_EFFECTS_H */
