#ifndef __WAVE_H__
#define __WAVE_H__

/* wave.h: Definitions used for reading WAVE file
 */

typedef struct { 
    short	FormatTag; 
    short	Channels; 
    int		SamplesPerSec; 
    int		AvgBytesPerSec; 
    short	BlockAlign; 
} fmt_t; 

#define ID_RIFF		(('R')|('I'<<8)|('F'<<16)|('F'<<24))
#define ID_WAVE		(('W')|('A'<<8)|('V'<<16)|('E'<<24))
#define ID_fmt		(('f')|('m'<<8)|('t'<<16)|(' '<<24))
#define ID_data		(('d')|('a'<<8)|('t'<<16)|('a'<<24))

#define WAVE_FMT_PCM	1

#endif //__WAVE_H__