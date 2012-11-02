#ifndef __FMOPL_H_
#define __FMOPL_H_

#include "zstring.h"

// Multiplying OPL_SAMPLE_RATE by ADLIB_CLOCK_MUL gives the number
// Adlib clocks per second, as used by the RAWADATA file format.

/* compiler dependence */
#ifndef OSD_CPU_H
#define OSD_CPU_H
typedef unsigned char	UINT8;   /* unsigned  8bit */
typedef unsigned short	UINT16;  /* unsigned 16bit */
typedef unsigned int	UINT32;  /* unsigned 32bit */
typedef signed char		INT8;    /* signed  8bit   */
typedef signed short	INT16;   /* signed 16bit   */
typedef signed int		INT32;   /* signed 32bit   */
#endif


typedef void (*OPL_TIMERHANDLER)(int channel,double interval_Sec);
typedef void (*OPL_IRQHANDLER)(int param,int irq);
typedef void (*OPL_UPDATEHANDLER)(int param,int min_interval_us);
typedef void (*OPL_PORTHANDLER_W)(int param,unsigned char data);
typedef unsigned char (*OPL_PORTHANDLER_R)(int param);


void *YM3812Init(int clock, int rate);
void YM3812Shutdown(void *chip);
void YM3812ResetChip(void *chip);
int  YM3812Write(void *chip, int a, int v);
unsigned char YM3812Read(void *chip, int a);
int  YM3812TimerOver(void *chip, int c);
void YM3812UpdateOne(void *chip, float *buffer, int length);
void YM3812SetStereo(void *chip, bool stereo);
void YM3812SetPanning(void *chip, int c, int pan);

void YM3812SetTimerHandler(void *chip, OPL_TIMERHANDLER TimerHandler, int channelOffset);
void YM3812SetIRQHandler(void *chip, OPL_IRQHANDLER IRQHandler, int param);
void YM3812SetUpdateHandler(void *chip, OPL_UPDATEHANDLER UpdateHandler, int param);

FString YM3812GetVoiceString(void *chip);

#endif
