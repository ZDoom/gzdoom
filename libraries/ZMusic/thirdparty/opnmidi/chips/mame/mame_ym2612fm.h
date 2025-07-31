/*
  File: fm.h -- header file for software emulation for FM sound generator

*/

#ifndef FM_HHHHH
#define FM_HHHHH

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#include "mamedef.h"

/* --- select emulation chips --- */
/*
#define BUILD_YM2203  (HAS_YM2203)		// build YM2203(OPN)   emulator
#define BUILD_YM2608  (HAS_YM2608)		// build YM2608(OPNA)  emulator
#define BUILD_YM2610  (HAS_YM2610)		// build YM2610(OPNB)  emulator
#define BUILD_YM2610B (HAS_YM2610B)		// build YM2610B(OPNB?)emulator
#define BUILD_YM2612  (HAS_YM2612)		// build YM2612(OPN2)  emulator
#define BUILD_YM3438  (HAS_YM3438)		// build YM3438(OPN) emulator
*/
#define BUILD_YM2203  0
#define BUILD_YM2608  0
#define BUILD_YM2610  0
#define BUILD_YM2610B 0
#define BUILD_YM2612  1
#define BUILD_YM3438  0

#define FM_BUSY_FLAG_SUPPORT 0

/* select bit size of output : 8 or 16 */
#define FM_SAMPLE_BITS 16

/* select timer system internal or external */
#define FM_INTERNAL_TIMER 1

/* --- speedup optimize --- */
/* busy flag enulation , The definition of FM_GET_TIME_NOW() is necessary. */
/* #define FM_BUSY_FLAG_SUPPORT 1 */

/* --- external SSG(YM2149/AY-3-8910)emulator interface port */
/* used by YM2203,YM2608,and YM2610 */
typedef struct _ssg_callbacks ssg_callbacks;
struct _ssg_callbacks
{
	void (*set_clock)(void *param, int clock);
	void (*write)(void *param, int address, int data);
	int (*read)(void *param);
	void (*reset)(void *param);
};

/* --- external callback funstions for realtime update --- */

#if FM_BUSY_FLAG_SUPPORT
#define TIME_TYPE					attotime
#define UNDEFINED_TIME				attotime_zero
#define FM_GET_TIME_NOW(machine)			timer_get_time(machine)
#define ADD_TIMES(t1, t2)   		attotime_add((t1), (t2))
#define COMPARE_TIMES(t1, t2)		attotime_compare((t1), (t2))
#define MULTIPLY_TIME_BY_INT(t,i)	attotime_mul(t, i)
#endif

/* compiler dependence */
#if 0
#ifndef OSD_CPU_H
#define OSD_CPU_H
typedef unsigned char	UINT8;   /* unsigned  8bit */
typedef unsigned short	UINT16;  /* unsigned 16bit */
typedef unsigned int	UINT32;  /* unsigned 32bit */
typedef signed char		INT8;    /* signed  8bit   */
typedef signed short	INT16;   /* signed 16bit   */
typedef signed int		INT32;   /* signed 32bit   */
#endif /* OSD_CPU_H */
#endif



typedef stream_sample_t FMSAMPLE;
/*
#if (FM_SAMPLE_BITS==16)
typedef INT16 FMSAMPLE;
#endif
#if (FM_SAMPLE_BITS==8)
typedef unsigned char  FMSAMPLE;
#endif
*/

typedef void (*FM_TIMERHANDLER)(void *param,int c,int cnt,int clock);
typedef void (*FM_IRQHANDLER)(void *param,int irq);
/* FM_TIMERHANDLER : Stop or Start timer         */
/* int n          = chip number                  */
/* int c          = Channel 0=TimerA,1=TimerB    */
/* int count      = timer count (0=stop)         */
/* doube stepTime = step time of one count (sec.)*/

/* FM_IRQHHANDLER : IRQ level changing sense     */
/* int n       = chip number                     */
/* int irq     = IRQ level 0=OFF,1=ON            */

#if (BUILD_YM2612||BUILD_YM3438)

/**
 * @brief Initialize chip and return the instance
 * @param param Unused, keep NULL
 * @param baseclock YM2612 clock
 * @param rate Output sample rate
 * @param TimerHandler Keep NULL
 * @param IRQHandler Keep NULL
 * @return Chip instance or NULL on any error
 */
void * ym2612_init(void *param, int baseclock, int rate,
               FM_TIMERHANDLER TimerHandler,FM_IRQHANDLER IRQHandler);
/**
 * @brief Free chip instance
 * @param chip Chip instance
 */
void ym2612_shutdown(void *chip);
/**
 * @brief Reset state of the chip
 * @param chip Chip instance
 */
void ym2612_reset_chip(void *chip);
/**
 * @brief Generate stereo output of specified length
 * @param chip Chip instance
 * @param buffer Output sound buffer
 * @param frames Output buffer size in frames (one frame - two array entries of the buffer)
 * @param mix 0 - override buffer data, 1 - mix output data with a content of the buffer
 */
void ym2612_generate(void *chip, FMSAMPLE *buffer, int frames, int mix);
#define ym2612_update_one(chip, buffer, length) ym2612_generate(chip, buffer, length, 0)

/**
 * @brief Single-Sample generation prepare
 * @param chip Chip instance
 */
void ym2612_pre_generate(void *chip);
/**
 * @brief Generate single stereo PCM frame. Will be used native sample rate of 53267 Hz
 * @param chip Chip instance
 * @param buffer One stereo PCM frame
 */
void ym2612_generate_one_native(void *chip, FMSAMPLE *buffer);

/* void ym2612_post_generate(void *chip, int length); */

int ym2612_write(void *chip, int a, unsigned char v);
void ym2612_write_pan(void *chip, int c, unsigned char v);
unsigned char ym2612_read(void *chip, int a);
int ym2612_timer_over(void *chip, int c );
void ym2612_postload(void *chip);

void ym2612_set_mutemask(void *chip, UINT32 MuteMask);
void ym2612_setoptions(UINT8 Flags);
#endif /* (BUILD_YM2612||BUILD_YM3438) */

#ifdef __cplusplus
}
#endif

#endif /* FM_HHHHH */
