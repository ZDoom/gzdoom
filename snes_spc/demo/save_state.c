/* Loads "test.spc", skips 5 seconds, saves exact emulator state to "state.bin",
hen records 5 seconds to "first.wav". When run again, loads this state back into
emulator and records 5 seconds to "second.wav". These two wave files should
be identical.

Usage: save_state [test.spc]
*/

#include "snes_spc/spc.h"

#include "wave_writer.h"
#include "demo_util.h" /* error(), load_file() */

static SNES_SPC* snes_spc;

void record_wav( const char* path, int secs )
{
	/* Start writing wave file */
	wave_open( spc_sample_rate, path );
	wave_enable_stereo();
	while ( wave_sample_count() < secs * spc_sample_rate * 2 )
	{
		/* Play into buffer */
		#define BUF_SIZE 2048
		short buf [BUF_SIZE];
		error( spc_play( snes_spc, BUF_SIZE, buf ) );
		
		wave_write( buf, BUF_SIZE );
	}
	wave_close();
}

void state_save( unsigned char** out, void* in, size_t size )
{
	memcpy( *out, in, size );
	*out += size;
}

void make_save_state( const char* path )
{
	/* Load SPC */
	long spc_size;
	void* spc = load_file( path, &spc_size );
	error( spc_load_spc( snes_spc, spc, spc_size ) );
	free( spc );
	spc_clear_echo( snes_spc );
	
	/* Skip several seconds */
	error( spc_play( snes_spc, 5 * spc_sample_rate * 2, 0 ) );
	
	/* Save state to file */
	{
		static unsigned char state [spc_state_size]; /* buffer large enough for data */
		unsigned char* out = state;
		spc_copy_state( snes_spc, &out, state_save );
		write_file( "state.bin", state, out - state );
	}
	
	record_wav( "first.wav", 5 );
}

void state_load( unsigned char** in, void* out, size_t size )
{
	memcpy( out, *in, size );
	*in += size;
}

void use_save_state()
{
	/* Load state into memory */
	long state_size;
	unsigned char* state = load_file( "state.bin", &state_size );
	
	/* Load into emulator */
	unsigned char* in = state;
	spc_copy_state( snes_spc, &in, state_load );
	assert( in  - state <= state_size ); /* be sure it didn't read past end */
	
	record_wav( "second.wav", 5 );
}

int file_exists( const char* path )
{
	FILE* file = fopen( path, "rb" );
	if ( !file )
		return 0;
	
	fclose( file );
	return 1;
}

int main( int argc, char** argv )
{
	snes_spc = spc_new();
	if ( !snes_spc ) error( "Out of memory" );
	
	/* Make new state if it doesn't exist, otherwise load it and
	record to wave file */
	if ( !file_exists( "state.bin" ) )
		make_save_state( (argc > 1) ? argv [1] : "test.spc" );
	else
		use_save_state();
	
	spc_delete( snes_spc );
	
	return 0;
}
