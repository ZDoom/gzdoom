
#include "Gbs_Emu.h"

#include "blargg_source.h"

int Gbs_Emu::cpu_read( gb_addr_t addr )
{
	int result = *cpu::get_code( addr );
	if ( unsigned (addr - Gb_Apu::start_addr) < Gb_Apu::register_count )
		result = apu.read_register( clock(), addr );
#ifndef NDEBUG
	else if ( unsigned (addr - 0x8000) < 0x2000 || unsigned (addr - 0xE000) < 0x1F00 )
		debug_printf( "Read from unmapped memory $%.4x\n", (unsigned) addr );
	else if ( unsigned (addr - 0xFF01) < 0xFF80 - 0xFF01 )
		debug_printf( "Unhandled I/O read 0x%4X\n", (unsigned) addr );
#endif
	return result;
}

void Gbs_Emu::cpu_write( gb_addr_t addr, int data )
{
	unsigned offset = addr - ram_addr;
	if ( offset <= 0xFFFF - ram_addr )
	{
		ram [offset] = data;
		if ( (addr ^ 0xE000) <= 0x1F80 - 1 )
		{
			if ( unsigned (addr - Gb_Apu::start_addr) < Gb_Apu::register_count )
			{
				GME_APU_HOOK( this, addr - Gb_Apu::start_addr, data );
				apu.write_register( clock(), addr, data );
			}
			else if ( (addr ^ 0xFF06) < 2 )
				update_timer();
			else if ( addr == joypad_addr )
				ram [offset] = 0; // keep joypad return value 0
			else
				ram [offset] = 0xFF;

			//if ( addr == 0xFFFF )
			//  debug_printf( "Wrote interrupt mask\n" );
		}
	}
	else if ( (addr ^ 0x2000) <= 0x2000 - 1 )
	{
		set_bank( data );
	}
#ifndef NDEBUG
	else if ( unsigned (addr - 0x8000) < 0x2000 || unsigned (addr - 0xE000) < 0x1F00 )
	{
		debug_printf( "Wrote to unmapped memory $%.4x\n", (unsigned) addr );
	}
#endif
}

#define CPU_READ_FAST( cpu, addr, time, out ) \
	CPU_READ_FAST_( STATIC_CAST(Gbs_Emu*,cpu), addr, time, out )

#define CPU_READ_FAST_( emu, addr, time, out ) \
{\
	out = READ_PROG( addr );\
	if ( unsigned (addr - Gb_Apu::start_addr) < Gb_Apu::register_count )\
		out = emu->apu.read_register( emu->cpu_time - time * clocks_per_instr, addr );\
	else\
		check( out == emu->cpu_read( addr ) );\
}

#define CPU_READ( cpu, addr, time ) \
	STATIC_CAST(Gbs_Emu*,cpu)->cpu_read( addr )

#define CPU_WRITE( cpu, addr, data, time ) \
	STATIC_CAST(Gbs_Emu*,cpu)->cpu_write( addr, data )
