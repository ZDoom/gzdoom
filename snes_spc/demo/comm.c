/* Communicates with SPC the way the SNES would.

Note: You'll need an "spc_rom.h" file that contains the 64-byte IPL ROM contents */

#include "snes_spc/spc.h"

#include "demo_util.h"
#include <string.h>
#include <stdio.h>

static SNES_SPC* snes_spc;

/* Make port access more convenient. Fakes time by simply incrementing it each call. */
static spc_time_t stime;
static int  pread ( int port )           { return spc_read_port( snes_spc, stime++, port ); }
static void pwrite( int port, int data ) { spc_write_port( snes_spc, stime++, port, data ); }

static unsigned char const spc_rom [spc_rom_size] = {
	/* ROM data not provided with emulator */
	#include "spc_rom.h"
};

int main()
{
	int i;
	
	/* Data to upload */
	static unsigned char const data [4] = "\xFA\xDE\xD1";
	unsigned const data_addr = 0xF5; /* second I/O port */
	
	snes_spc = spc_new();
	if ( !snes_spc ) error( "Out of memory" );
	spc_init_rom( snes_spc, spc_rom );
	spc_reset( snes_spc );
	
	/* Simulate reads and writes that SNES code would do */
	
	/* Wait for SPC to be ready */
	while ( pread( 0 ) != 0xAA || pread( 1 ) != 0xBB ) { }
	
	/* Start address */
	pwrite( 2, data_addr & 0xFF );
	pwrite( 3, data_addr >> 8 );
	
	/* Tell SPC to start transfer and wait for acknowledgement */
	pwrite( 0, 0xCC );
	pwrite( 1, 0x01 );
	while ( pread( 0 ) != 0xCC ) { }
	
	/* Send each byte and wait for acknowledgement */
	for ( i = 0; i < 4; i++ )
	{
		printf( "%02X ", data [i] );
		pwrite( 1, data [i] );
		pwrite( 0, i );
		while ( pread( 0 ) != i ) { }
	}
	printf( "\n" );
	
	/* Verify that data was transferred properly */
	for ( i = 0; i < 3; i++ )
		printf( "%02X ", pread( i + 1 ) );
	printf( "\n" );
	
	printf( "Cycles: %ld\n", (long) stime );
	
	spc_delete( snes_spc );
	
	return 0;
}
