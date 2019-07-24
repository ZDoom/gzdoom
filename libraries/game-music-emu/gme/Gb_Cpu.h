// Nintendo Game Boy CPU emulator
// Treats every instruction as taking 4 cycles

// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/
#ifndef GB_CPU_H
#define GB_CPU_H

#include "blargg_common.h"
#include "blargg_endian.h"

typedef unsigned gb_addr_t; // 16-bit CPU address

class Gb_Cpu {
	enum { clocks_per_instr = 4 };
public:
	// Clear registers and map all pages to unmapped
	void reset( void* unmapped = 0 );
	
	// Map code memory (memory accessed via the program counter). Start and size
	// must be multiple of page_size.
	enum { page_size = 0x2000 };
	void map_code( gb_addr_t start, unsigned size, void* code );
	
	uint8_t* get_code( gb_addr_t );
	
	// Push a byte on the stack
	void push_byte( int );
	
	// Game Boy Z80 registers. *Not* kept updated during a call to run().
	struct core_regs_t {
	#if BLARGG_BIG_ENDIAN
		uint8_t b, c, d, e, h, l, flags, a;
	#else
		uint8_t c, b, e, d, l, h, a, flags;
	#endif
	};
	
	struct registers_t : core_regs_t {
		long pc; // more than 16 bits to allow overflow detection
		uint16_t sp;
	};
	registers_t r;
	
	// Interrupt enable flag set by EI and cleared by DI
	//bool interrupts_enabled; // unused
	
	// Base address for RST vectors (normally 0)
	gb_addr_t rst_base;
	
	// If CPU executes opcode 0xFF at this address, it treats as illegal instruction
	enum { idle_addr = 0xF00D };
	
	// Run CPU for at least 'count' cycles and return false, or return true if
	// illegal instruction is encountered.
	bool run( blargg_long count );
	
	// Number of clock cycles remaining for most recent run() call
	blargg_long remain() const { return state->remain * clocks_per_instr; }
	
	// Can read this many bytes past end of a page
	enum { cpu_padding = 8 };
	
public:
	Gb_Cpu() : rst_base( 0 ) { state = &state_; }
	enum { page_shift = 13 };
	enum { page_count = 0x10000 >> page_shift };
private:
	// noncopyable
	Gb_Cpu( const Gb_Cpu& );
	Gb_Cpu& operator = ( const Gb_Cpu& );
	
	struct state_t {
		uint8_t* code_map [page_count + 1];
		blargg_long remain;
	};
	state_t* state; // points to state_ or a local copy within run()
	state_t state_;
	
	void set_code_page( int, uint8_t* );
};

inline uint8_t* Gb_Cpu::get_code( gb_addr_t addr )
{
	return state->code_map [addr >> page_shift] + addr
	#if !BLARGG_NONPORTABLE
		% (unsigned) page_size
	#endif
	;
}

#endif
