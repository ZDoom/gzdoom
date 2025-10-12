// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/

// NES MMC5 sound chip emulator

#ifndef NES_MMC5_APU_H
#define NES_MMC5_APU_H

#include "blargg_common.h"
#include "Nes_Apu.h"

class Nes_Mmc5_Apu : public Nes_Apu {
public:
	enum { regs_addr = 0x5000 };
	enum { regs_size = 0x16 };

	enum { osc_count  = 3 };
	void write_register( blip_time_t, unsigned addr, int data );
	void osc_output( int i, Blip_Buffer* );

	enum { exram_size = 1024 };
	unsigned char exram [exram_size];
};

inline void Nes_Mmc5_Apu::osc_output( int i, Blip_Buffer* b )
{
	// in: square 1, square 2, PCM
	// out: square 1, square 2, skipped, skipped, PCM
	assert( (unsigned) i < osc_count );
	if ( i > 1 )
		i += 2;
	Nes_Apu::osc_output( i, b );
}

inline void Nes_Mmc5_Apu::write_register( blip_time_t time, unsigned addr, int data )
{
	switch ( addr )
	{
	case 0x5015: // channel enables
		data &= 0x03; // enable the square waves only
		// fall through
	case 0x5000: // Square 1
	case 0x5002:
	case 0x5003:
	case 0x5004: // Square 2
	case 0x5006:
	case 0x5007:
	case 0x5011: // DAC
		Nes_Apu::write_register( time, addr - 0x1000, data );
		break;

	case 0x5010: // some things write to this for some reason
		break;

#ifdef BLARGG_DEBUG_H
	default:
			debug_printf( "Unmapped MMC5 APU write: $%04X <- $%02X\n", addr, data );
#endif
	}
}

#endif
