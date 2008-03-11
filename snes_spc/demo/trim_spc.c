/* Trims silence off beginning of SPC file.
Requires the accurate DSP; won't compile with the fast DSP.
Please note that the algorithm could be improved; this is just
a simple example showing the idea.

Usage: trim_spc [test.spc [trimmed.spc]]
*/

#include "snes_spc/spc.h"

#include "demo_util.h" /* error(), load_file() */

/* Change to 1 to have it trim to next key on event rather than trim silence */
enum { use_kon_check = 0 };

/* True if all count samples from in are silent (or very close to it) */
int is_silent( short const* in, int count );

int main( int argc, char** argv )
{
	/* Load file into memory */
	long spc_size;
	void* spc = load_file( (argc > 1) ? argv [1] : "test.spc", &spc_size );
	
	/* Load into emulator */
	SNES_SPC* snes_spc = spc_new();
	if ( !snes_spc ) error( "Out of memory" );
	error( spc_load_spc( snes_spc, spc, spc_size ) );
	
	/* Expand SPC data so there's enough room for emulator to save to.
	We simply overwrite the emulator data in the old SPC file rather
	than creating new SPC data. This preserves the text tags from
	the old file. */
	if ( spc_size < spc_file_size )
	{
		spc_size = spc_file_size;
		spc = realloc( spc, spc_size ); /* leaks memory if allocation fails */
		if ( !spc ) error( "Out of memory" );
	}
	
	/* Keep saving SPC, then playing a little more. Once SPC becomes non-silent,
	write the SPC data saved just before this. */
	{
		long samples_trimmed = 0;
		while ( 1 )
		{
			#define BUF_SIZE 1024
			short buf [BUF_SIZE];
			
			if ( samples_trimmed > 10 * spc_sample_rate * 2 )
				error( "Excess silence found" );
			
			spc_clear_echo( snes_spc );
			spc_save_spc( snes_spc, spc );
			
			/* Fill buffer */
			error( spc_play( snes_spc, BUF_SIZE, buf ) );
			samples_trimmed += BUF_SIZE;
			
			/* See if SPC became non-silent */
			if ( use_kon_check ? spc_check_kon( snes_spc ) : !is_silent( buf, BUF_SIZE ) )
				break;
		}
		
		printf( "Trimmed %.1f seconds\n", (double) samples_trimmed / spc_sample_rate / 2 );
	}
	
	spc_delete( snes_spc );
	write_file( (argc > 2) ? argv [2] : "trimmed.spc", spc, spc_size );
	
	return 0;
}

int is_silent( short const* in, int count )
{
	unsigned const threshold = 0x10;
	while ( count-- )
	{
		if ( (unsigned) (*in++ + threshold / 2) > threshold )
			return 0;
	}
	return 1;
}
