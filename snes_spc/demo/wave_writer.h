/* WAVE sound file writer for recording 16-bit output during program development */

#ifndef WAVE_WRITER_H
#define WAVE_WRITER_H

#ifdef __cplusplus
	extern "C" {
#endif

void wave_open( long sample_rate, const char* filename );
void wave_enable_stereo( void );
void wave_write( short const* in, long count );
long wave_sample_count( void );
void wave_close( void );

#ifdef __cplusplus
	}
#endif

#endif
