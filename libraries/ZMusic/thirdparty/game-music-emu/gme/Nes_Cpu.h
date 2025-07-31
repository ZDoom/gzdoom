// NES 6502 CPU emulator

// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/
#ifndef NES_CPU_H
#define NES_CPU_H

#include "blargg_common.h"

typedef blargg_long nes_time_t; // clock cycle count
typedef unsigned nes_addr_t; // 16-bit address
enum { future_nes_time = INT_MAX / 2 + 1 };

class Nes_Cpu {
public:
	// Clear registers, map low memory and its three mirrors to address 0,
	// and mirror unmapped_page in remaining memory
	void reset( void const* unmapped_page = 0 );
	
	// Map code memory (memory accessed via the program counter). Start and size
	// must be multiple of page_size. If mirror is true, repeats code page
	// throughout address range.
	enum { page_size = 0x800 };
	void map_code( nes_addr_t start, unsigned size, void const* code, bool mirror = false );
	
	// Access emulated memory as CPU does
	uint8_t const* get_code( nes_addr_t );
	
	// 2KB of RAM at address 0
	uint8_t low_mem [0x800];
	
	// NES 6502 registers. Not kept updated during a call to run().
	struct registers_t {
		uint16_t pc;
		uint8_t a;
		uint8_t x;
		uint8_t y;
		uint8_t status;
		uint8_t sp;
	};
	registers_t r;
	
	// Set end_time and run CPU from current time. Returns true if execution
	// stopped due to encountering bad_opcode.
	bool run( nes_time_t end_time );
	
	// Time of beginning of next instruction to be executed
	nes_time_t time() const             { return state->time + state->base; }
	void set_time( nes_time_t t )       { state->time = t - state->base; }
	void adjust_time( int delta )       { state->time += delta; }
	
	nes_time_t irq_time() const         { return irq_time_; }
	void set_irq_time( nes_time_t );
	
	nes_time_t end_time() const         { return end_time_; }
	void set_end_time( nes_time_t );
	
	// Number of undefined instructions encountered and skipped
	void clear_error_count()            { error_count_ = 0; }
	unsigned long error_count() const   { return error_count_; }
	
	// CPU invokes bad opcode handler if it encounters this
	enum { bad_opcode = 0xF2 };
	
public:
	Nes_Cpu() { state = &state_; }
	enum { page_bits = 11 };
	enum { page_count = 0x10000 >> page_bits };
	enum { irq_inhibit = 0x04 };
private:
	struct state_t {
		uint8_t const* code_map [page_count + 1];
		nes_time_t base;
		int time;
	};
	state_t* state; // points to state_ or a local copy within run()
	state_t state_;
	nes_time_t irq_time_;
	nes_time_t end_time_;
	unsigned long error_count_;
	
	void set_code_page( int, void const* );
	inline int update_end_time( nes_time_t end, nes_time_t irq );
};

inline uint8_t const* Nes_Cpu::get_code( nes_addr_t addr )
{
	return state->code_map [addr >> page_bits] + addr
	#if !BLARGG_NONPORTABLE
		% (unsigned) page_size
	#endif
	;
}

inline int Nes_Cpu::update_end_time( nes_time_t t, nes_time_t irq )
{
	if ( irq < t && !(r.status & irq_inhibit) ) t = irq;
	int delta = state->base - t;
	state->base = t;
	return delta;
}

inline void Nes_Cpu::set_irq_time( nes_time_t t )
{
	state->time += update_end_time( end_time_, (irq_time_ = t) );
}

inline void Nes_Cpu::set_end_time( nes_time_t t )
{
	state->time += update_end_time( (end_time_ = t), irq_time_ );
}

#endif
