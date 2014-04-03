// Z80 CPU emulator

// Game_Music_Emu 0.6.0
#ifndef KSS_CPU_H
#define KSS_CPU_H

#include "blargg_endian.h"

typedef blargg_long cpu_time_t;

// must be defined by caller
void kss_cpu_out( class Kss_Cpu*, cpu_time_t, unsigned addr, int data );
int  kss_cpu_in( class Kss_Cpu*, cpu_time_t, unsigned addr );
void kss_cpu_write( class Kss_Cpu*, unsigned addr, int data );

class Kss_Cpu {
public:
	typedef BOOST::uint8_t uint8_t;
	
	// Clear registers and map all pages to unmapped
	void reset( void* unmapped_write, void const* unmapped_read );
	
	// Map memory. Start and size must be multiple of page_size.
	enum { page_size = 0x2000 };
	void map_mem( unsigned addr, blargg_ulong size, void* write, void const* read );
	
	// Map address to page
	uint8_t* write( unsigned addr );
	uint8_t const* read( unsigned addr );
	
	// Run until specified time is reached. Returns true if suspicious/unsupported
	// instruction was encountered at any point during run.
	bool run( cpu_time_t end_time );
	
	// Time of beginning of next instruction
	cpu_time_t time() const             { return state->time + state->base; }
	
	// Alter current time. Not supported during run() call.
	void set_time( cpu_time_t t )       { state->time = t - state->base; }
	void adjust_time( int delta )       { state->time += delta; }
	
	typedef BOOST::uint16_t uint16_t;
	
	#if BLARGG_BIG_ENDIAN
		struct regs_t { uint8_t b, c, d, e, h, l, flags, a; };
	#else
		struct regs_t { uint8_t c, b, e, d, l, h, a, flags; };
	#endif
	BOOST_STATIC_ASSERT( sizeof (regs_t) == 8 );
	
	struct pairs_t { uint16_t bc, de, hl, fa; };
	
	// Registers are not updated until run() returns
	struct registers_t {
		uint16_t pc;
		uint16_t sp;
		uint16_t ix;
		uint16_t iy;
		union {
			regs_t b; //  b.b, b.c, b.d, b.e, b.h, b.l, b.flags, b.a
			pairs_t w; // w.bc, w.de, w.hl. w.fa
		};
		union {
			regs_t b;
			pairs_t w;
		} alt;
		uint8_t iff1;
		uint8_t iff2;
		uint8_t r;
		uint8_t i;
		uint8_t im;
	};
	//registers_t r; (below for efficiency)
	
	enum { idle_addr = 0xFFFF };
	
	// can read this far past end of a page
	enum { cpu_padding = 0x100 };
	
public:
	Kss_Cpu();
	enum { page_shift = 13 };
	enum { page_count = 0x10000 >> page_shift };
private:
	uint8_t szpc [0x200];
	cpu_time_t end_time_;
	struct state_t {
		uint8_t const* read  [page_count + 1];
		uint8_t      * write [page_count + 1];
		cpu_time_t base;
		cpu_time_t time;
	};
	state_t* state; // points to state_ or a local copy within run()
	state_t state_;
	void set_end_time( cpu_time_t t );
	void set_page( int i, void* write, void const* read );
public:
	registers_t r;
};

#if BLARGG_NONPORTABLE
	#define KSS_CPU_PAGE_OFFSET( addr ) (addr)
#else
	#define KSS_CPU_PAGE_OFFSET( addr ) ((addr) & (page_size - 1))
#endif

inline BOOST::uint8_t* Kss_Cpu::write( unsigned addr )
{
	return state->write [addr >> page_shift] + KSS_CPU_PAGE_OFFSET( addr );
}

inline BOOST::uint8_t const* Kss_Cpu::read( unsigned addr )
{
	return state->read [addr >> page_shift] + KSS_CPU_PAGE_OFFSET( addr );
}

inline void Kss_Cpu::set_end_time( cpu_time_t t )
{
	cpu_time_t delta = state->base - t;
	state->base = t;
	state->time += delta;
}

#endif
