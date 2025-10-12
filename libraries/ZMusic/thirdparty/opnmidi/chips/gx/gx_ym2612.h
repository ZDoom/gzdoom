/*
**
** software implementation of Yamaha FM sound generator (YM2612/YM3438)
**
** Original code (MAME fm.c)
**
** Copyright (C) 2001, 2002, 2003 Jarek Burczynski (bujar at mame dot net)
** Copyright (C) 1998 Tatsuyuki Satoh , MultiArcadeMachineEmulator development
**
** Version 1.4 (final beta)
**
** Additional code & fixes by Eke-Eke for Genesis Plus GX
** Adaptations by Jean Pierre Cimalando for use in libOPNMIDI.
** (based on fee2bc8 dated Jan 7th, 2018)
**
*/

#ifndef _H_YM2612_
#define _H_YM2612_

#if defined(__cplusplus)
extern "C" {
#endif

enum {
  YM2612_DISCRETE = 0,
  YM2612_INTEGRATED,
  YM2612_ENHANCED
};

struct YM2612GX;
typedef struct YM2612GX YM2612GX;

/* typedef signed int FMSAMPLE; */
typedef signed short FMSAMPLE;

extern YM2612GX *YM2612GXAlloc();
extern void YM2612GXFree(YM2612GX *ym2612);
extern void YM2612GXInit(YM2612GX *ym2612);
extern void YM2612GXConfig(YM2612GX *ym2612, int type);
extern void YM2612GXResetChip(YM2612GX *ym2612);
extern void YM2612GXPreGenerate(YM2612GX *ym2612);
extern void YM2612GXPostGenerate(YM2612GX *ym2612, unsigned int count);
extern void YM2612GXGenerateOneNative(YM2612GX *ym2612, FMSAMPLE *frame);
extern void YM2612GXWrite(YM2612GX *ym2612, unsigned int a, unsigned int v);
extern void YM2612GXWritePan(YM2612GX *chip, int c, unsigned char v);
extern unsigned int YM2612GXRead(YM2612GX *ym2612);

#if defined(__cplusplus)
}  /* extern "C" */
#endif

#endif /* _YM2612_ */
