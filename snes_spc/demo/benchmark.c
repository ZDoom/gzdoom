/* Measures performance of SPC emulator. Takes about 4 seconds.
NOTE: This assumes that the program is getting all processor time; you might need to
arrange for this or else the performance will be reported lower than it really is.

Usage: benchmark [test.spc]
*/

#include "snes_spc/spc.h"

#include "demo_util.h" /* error(), load_file() */
#include <time.h>

clock_t start_timing( int seconds );

int main( int argc, char** argv )
{
	/* Load SPC */
	long spc_size;
	void* spc = load_file( (argc > 1) ? argv [1] : "test.spc", &spc_size );
	SNES_SPC* snes_spc = spc_new();
	if ( !snes_spc ) error( "Out of memory" );
	spc_load_spc( snes_spc, spc, spc_size );
	free( spc );
	
	{
		/* Repeatedly fill buffer for 4 seconds */
		int const seconds = 4;
		#define BUF_SIZE 4096
		clock_t end = start_timing( seconds );
		int count = 0;
		while ( clock() < end )
		{
			static short buf [BUF_SIZE];
			error( spc_play( snes_spc, BUF_SIZE, buf ) );
			count++;
		}
		
		/* Report performance based on how many buffer fills were possible */
		{
			double rate = (double) count * BUF_SIZE / (spc_sample_rate * 2 * seconds);
			printf( "Performance: %.3fx real-time, or %.0f%% CPU for normal rate\n",
					rate, 100.0 / rate );
		}
	}
	
	return 0;
}

/* Synchronizes with host clock and returns clock() time that is duration seconds from now */
clock_t start_timing( int duration )
{
	clock_t clock_dur = duration * CLOCKS_PER_SEC;
	clock_t time = clock();
	while ( clock() == time ) { }
	if ( clock() - time > clock_dur )
		error( "Insufficient clock() time resolution" );
	return clock() + clock_dur;
}
