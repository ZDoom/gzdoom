
#include "Nsf_Emu.h"

#if !NSF_EMU_APU_ONLY
	#include "Nes_Namco_Apu.h"
#endif

#include "blargg_source.h"

int Nsf_Emu::cpu_read( nes_addr_t addr )
{
	int result;
	
	result = cpu::low_mem [addr & 0x7FF];
	if ( !(addr & 0xE000) )
		goto exit;
	
	result = *cpu::get_code( addr );
	if ( addr > 0x7FFF )
		goto exit;
	
	result = sram [addr & (sizeof sram - 1)];
	if ( addr > 0x5FFF )
		goto exit;
	
	if ( addr == Nes_Apu::status_addr )
		return apu.read_status( cpu::time() );
	
	#if !NSF_EMU_APU_ONLY
		if ( addr == Nes_Namco_Apu::data_reg_addr && namco )
			return namco->read_data();
	#endif
	
	result = addr >> 8; // simulate open bus
	
	if ( addr != 0x2002 )
		dprintf( "Read unmapped $%.4X\n", (unsigned) addr );
	
exit:
	return result;
}

void Nsf_Emu::cpu_write( nes_addr_t addr, int data )
{
	{
		nes_addr_t offset = addr ^ sram_addr;
		if ( offset < sizeof sram )
		{
			sram [offset] = data;
			return;
		}
	}
	{
		int temp = addr & 0x7FF;
		if ( !(addr & 0xE000) )
		{
			cpu::low_mem [temp] = data;
			return;
		}
	}
	
	if ( unsigned (addr - Nes_Apu::start_addr) <= Nes_Apu::end_addr - Nes_Apu::start_addr )
	{
		GME_APU_HOOK( this, addr - Nes_Apu::start_addr, data );
		apu.write_register( cpu::time(), addr, data );
		return;
	}
	
	unsigned bank = addr - bank_select_addr;
	if ( bank < bank_count )
	{
		blargg_long offset = rom.mask_addr( data * (blargg_long) bank_size );
		if ( offset >= rom.size() )
			set_warning( "Invalid bank" );
		cpu::map_code( (bank + 8) * bank_size, bank_size, rom.at_addr( offset ) );
		return;
	}
	
	cpu_write_misc( addr, data );
}

#define CPU_READ( cpu, addr, time )         STATIC_CAST(Nsf_Emu&,*cpu).cpu_read( addr )
#define CPU_WRITE( cpu, addr, data, time )  STATIC_CAST(Nsf_Emu&,*cpu).cpu_write( addr, data )
