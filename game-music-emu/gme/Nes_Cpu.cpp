// Game_Music_Emu 0.5.2. http://www.slack.net/~ant/

#include "Nes_Cpu.h"

#include "blargg_endian.h"
#include <limits.h>

#define BLARGG_CPU_X86 1

/* Copyright (C) 2003-2006 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

#ifdef BLARGG_ENABLE_OPTIMIZER
	#include BLARGG_ENABLE_OPTIMIZER
#endif

#define FLUSH_TIME()    (void) (s.time = s_time)
#define CACHE_TIME()    (void) (s_time = s.time)

#include "nes_cpu_io.h"

#include "blargg_source.h"

#ifndef CPU_DONE
	#define CPU_DONE( cpu, time, result_out )   { result_out = -1; }
#endif

#ifndef CPU_READ_PPU
	#define CPU_READ_PPU( cpu, addr, out, time )\
	{\
		FLUSH_TIME();\
		out = CPU_READ( cpu, addr, time );\
		CACHE_TIME();\
	}
#endif

#if BLARGG_NONPORTABLE
	#define PAGE_OFFSET( addr ) (addr)
#else
	#define PAGE_OFFSET( addr ) ((addr) & (page_size - 1))
#endif

inline void Nes_Cpu::set_code_page( int i, void const* p )
{
	state->code_map [i] = (uint8_t const*) p - PAGE_OFFSET( i * page_size );
}

int const st_n = 0x80;
int const st_v = 0x40;
int const st_r = 0x20;
int const st_b = 0x10;
int const st_d = 0x08;
int const st_i = 0x04;
int const st_z = 0x02;
int const st_c = 0x01;

void Nes_Cpu::reset( void const* unmapped_page )
{
	check( state == &state_ );
	state = &state_;
	r.status = st_i;
	r.sp = 0xFF;
	r.pc = 0;
	r.a  = 0;
	r.x  = 0;
	r.y  = 0;
	state_.time = 0;
	state_.base = 0;
	irq_time_ = future_nes_time;
	end_time_ = future_nes_time;
	error_count_ = 0;
	
	assert( page_size == 0x800 ); // assumes this
	set_code_page( page_count, unmapped_page );
	map_code( 0x2000, 0xE000, unmapped_page, true );
	map_code( 0x0000, 0x2000, low_mem, true );
	
	blargg_verify_byte_order();
}

void Nes_Cpu::map_code( nes_addr_t start, unsigned size, void const* data, bool mirror )
{
	// address range must begin and end on page boundaries
	require( start % page_size == 0 );
	require( size % page_size == 0 );
	require( start + size <= 0x10000 );
	
	unsigned page = start / page_size;
	for ( unsigned n = size / page_size; n; --n )
	{
		set_code_page( page++, data );
		if ( !mirror )
			data = (char const*) data + page_size;
	}
}

#define TIME    (s_time + s.base)
#define READ_LIKELY_PPU( addr, out )    {CPU_READ_PPU( this, (addr), out, TIME );}
#define READ( addr )                    CPU_READ( this, (addr), TIME )
#define WRITE( addr, data )             {CPU_WRITE( this, (addr), (data), TIME );}
#define READ_LOW( addr )        (low_mem [int (addr)])
#define WRITE_LOW( addr, data ) (void) (READ_LOW( addr ) = (data))
#define READ_PROG( addr )       (s.code_map [(addr) >> page_bits] [PAGE_OFFSET( addr )])

#define SET_SP( v )     (sp = ((v) + 1) | 0x100)
#define GET_SP()        ((sp - 1) & 0xFF)
#define PUSH( v )       ((sp = (sp - 1) | 0x100), WRITE_LOW( sp, v ))

// even on x86, using short and unsigned char was slower
typedef int         fint16;
typedef unsigned    fuint16;
typedef unsigned    fuint8;

bool Nes_Cpu::run( nes_time_t end_time )
{
	set_end_time( end_time );
	state_t s = this->state_;
	this->state = &s;
	// even on x86, using s.time in place of s_time was slower
	fint16 s_time = s.time;
	
	// registers
	fuint16 pc = r.pc;
	fuint8 a = r.a;
	fuint8 x = r.x;
	fuint8 y = r.y;
	fuint16 sp;
	SET_SP( r.sp );
	
	// status flags
	#define IS_NEG (nz & 0x8080)
	
	#define CALC_STATUS( out ) do {\
		out = status & (st_v | st_d | st_i);\
		out |= ((nz >> 8) | nz) & st_n;\
		out |= c >> 8 & st_c;\
		if ( !(nz & 0xFF) ) out |= st_z;\
	} while ( 0 )

	#define SET_STATUS( in ) do {\
		status = in & (st_v | st_d | st_i);\
		nz = in << 8;\
		c = nz;\
		nz |= ~in & st_z;\
	} while ( 0 )
	
	fuint8 status;
	fuint16 c;  // carry set if (c & 0x100) != 0
	fuint16 nz; // Z set if (nz & 0xFF) == 0, N set if (nz & 0x8080) != 0
	{
		fuint8 temp = r.status;
		SET_STATUS( temp );
	}
	
	goto loop;
dec_clock_loop:
	s_time--;
loop:
	
	check( (unsigned) GET_SP() < 0x100 );
	check( (unsigned) pc < 0x10000 );
	check( (unsigned) a < 0x100 );
	check( (unsigned) x < 0x100 );
	check( (unsigned) y < 0x100 );
	check( -32768 <= s_time && s_time < 32767 );
	
	uint8_t const* instr = s.code_map [pc >> page_bits];
	fuint8 opcode;
	
	// TODO: eliminate this special case
	#if BLARGG_NONPORTABLE
		opcode = instr [pc];
		pc++;
		instr += pc;
	#else
		instr += PAGE_OFFSET( pc );
		opcode = *instr++;
		pc++;
	#endif
	
	static uint8_t const clock_table [256] =
	{// 0 1 2 3 4 5 6 7 8 9 A B C D E F
		0,6,2,8,3,3,5,5,3,2,2,2,4,4,6,6,// 0
		3,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,// 1
		6,6,2,8,3,3,5,5,4,2,2,2,4,4,6,6,// 2
		3,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,// 3
		6,6,2,8,3,3,5,5,3,2,2,2,3,4,6,6,// 4
		3,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,// 5
		6,6,2,8,3,3,5,5,4,2,2,2,5,4,6,6,// 6
		3,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,// 7
		2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4,// 8
		3,6,2,6,4,4,4,4,2,5,2,5,5,5,5,5,// 9
		2,6,2,6,3,3,3,3,2,2,2,2,4,4,4,4,// A
		3,5,2,5,4,4,4,4,2,4,2,4,4,4,4,4,// B
		2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6,// C
		3,5,2,8,4,4,6,6,2,4,2,7,4,4,7,7,// D
		2,6,2,8,3,3,5,5,2,2,2,2,4,4,6,6,// E
		3,5,0,8,4,4,6,6,2,4,2,7,4,4,7,7 // F
	}; // 0x00 was 7 and 0xF2 was 2
	
	fuint16 data;
	
#if !BLARGG_CPU_X86
	if ( s_time >= 0 )
		goto out_of_time;
	s_time += clock_table [opcode];
	
	data = *instr;
	
	switch ( opcode )
	{
#else

	data = clock_table [opcode];
	if ( (s_time += data) >= 0 )
		goto possibly_out_of_time;
almost_out_of_time:
	
	data = *instr;
	
	switch ( opcode )
	{
possibly_out_of_time:
		if ( s_time < (int) data )
			goto almost_out_of_time;
		s_time -= data;
		goto out_of_time;
#endif

// Macros

#define GET_MSB()   (instr [1])
#define ADD_PAGE()  (pc++, data += 0x100 * GET_MSB())
#define GET_ADDR()  GET_LE16( instr )

#define NO_PAGE_CROSSING( lsb )
#define HANDLE_PAGE_CROSSING( lsb ) s_time += (lsb) >> 8;

#define INC_DEC_XY( reg, n ) reg = uint8_t (nz = reg + n); goto loop;

#define IND_Y( cross, out ) {\
		fuint16 temp = READ_LOW( data ) + y;\
		out = temp + 0x100 * READ_LOW( uint8_t (data + 1) );\
		cross( temp );\
	}
	
#define IND_X( out ) {\
		fuint16 temp = data + x;\
		out = 0x100 * READ_LOW( uint8_t (temp + 1) ) + READ_LOW( uint8_t (temp) );\
	}
	
#define ARITH_ADDR_MODES( op )\
case op - 0x04: /* (ind,x) */\
	IND_X( data )\
	goto ptr##op;\
case op + 0x0C: /* (ind),y */\
	IND_Y( HANDLE_PAGE_CROSSING, data )\
	goto ptr##op;\
case op + 0x10: /* zp,X */\
	data = uint8_t (data + x);\
case op + 0x00: /* zp */\
	data = READ_LOW( data );\
	goto imm##op;\
case op + 0x14: /* abs,Y */\
	data += y;\
	goto ind##op;\
case op + 0x18: /* abs,X */\
	data += x;\
ind##op:\
	HANDLE_PAGE_CROSSING( data );\
case op + 0x08: /* abs */\
	ADD_PAGE();\
ptr##op:\
	FLUSH_TIME();\
	data = READ( data );\
	CACHE_TIME();\
case op + 0x04: /* imm */\
imm##op:

// TODO: more efficient way to handle negative branch that wraps PC around
#define BRANCH( cond )\
{\
	fint16 offset = (BOOST::int8_t) data;\
	fuint16 extra_clock = (++pc & 0xFF) + offset;\
	if ( !(cond) ) goto dec_clock_loop;\
	pc = BOOST::uint16_t (pc + offset);\
	s_time += extra_clock >> 8 & 1;\
	goto loop;\
}

// Often-Used

	case 0xB5: // LDA zp,x
		a = nz = READ_LOW( uint8_t (data + x) );
		pc++;
		goto loop;
	
	case 0xA5: // LDA zp
		a = nz = READ_LOW( data );
		pc++;
		goto loop;
	
	case 0xD0: // BNE
		BRANCH( (uint8_t) nz );
	
	case 0x20: { // JSR
		fuint16 temp = pc + 1;
		pc = GET_ADDR();
		WRITE_LOW( 0x100 | (sp - 1), temp >> 8 );
		sp = (sp - 2) | 0x100;
		WRITE_LOW( sp, temp );
		goto loop;
	}
	
	case 0x4C: // JMP abs
		pc = GET_ADDR();
		goto loop;
	
	case 0xE8: // INX
		INC_DEC_XY( x, 1 )
	
	case 0x10: // BPL
		BRANCH( !IS_NEG )
	
	ARITH_ADDR_MODES( 0xC5 ) // CMP
		nz = a - data;
		pc++;
		c = ~nz;
		nz &= 0xFF;
		goto loop;
	
	case 0x30: // BMI
		BRANCH( IS_NEG )
	
	case 0xF0: // BEQ
		BRANCH( !(uint8_t) nz );
	
	case 0x95: // STA zp,x
		data = uint8_t (data + x);
	case 0x85: // STA zp
		pc++;
		WRITE_LOW( data, a );
		goto loop;
	
	case 0xC8: // INY
		INC_DEC_XY( y, 1 )

	case 0xA8: // TAY
		y  = a;
		nz = a;
		goto loop;
	
	case 0x98: // TYA
		a  = y;
		nz = y;
		goto loop;
	
	case 0xAD:{// LDA abs
		unsigned addr = GET_ADDR();
		pc += 2;
		READ_LIKELY_PPU( addr, nz );
		a = nz;
		goto loop;
	}
	
	case 0x60: // RTS
		pc = 1 + READ_LOW( sp );
		pc += 0x100 * READ_LOW( 0x100 | (sp - 0xFF) );
		sp = (sp - 0xFE) | 0x100;
		goto loop;
	
	{
		fuint16 addr;
		
	case 0x99: // STA abs,Y
		addr = y + GET_ADDR();
		pc += 2;
		if ( addr <= 0x7FF )
		{
			WRITE_LOW( addr, a );
			goto loop;
		}
		goto sta_ptr;
	
	case 0x8D: // STA abs
		addr = GET_ADDR();
		pc += 2;
		if ( addr <= 0x7FF )
		{
			WRITE_LOW( addr, a );
			goto loop;
		}
		goto sta_ptr;
	
	case 0x9D: // STA abs,X (slightly more common than STA abs)
		addr = x + GET_ADDR();
		pc += 2;
		if ( addr <= 0x7FF )
		{
			WRITE_LOW( addr, a );
			goto loop;
		}
	sta_ptr:
		FLUSH_TIME();
		WRITE( addr, a );
		CACHE_TIME();
		goto loop;
		
	case 0x91: // STA (ind),Y
		IND_Y( NO_PAGE_CROSSING, addr )
		pc++;
		goto sta_ptr;
	
	case 0x81: // STA (ind,X)
		IND_X( addr )
		pc++;
		goto sta_ptr;
	
	}
	
	case 0xA9: // LDA #imm
		pc++;
		a  = data;
		nz = data;
		goto loop;

	// common read instructions
	{
		fuint16 addr;
		
	case 0xA1: // LDA (ind,X)
		IND_X( addr )
		pc++;
		goto a_nz_read_addr;
	
	case 0xB1:// LDA (ind),Y
		addr = READ_LOW( data ) + y;
		HANDLE_PAGE_CROSSING( addr );
		addr += 0x100 * READ_LOW( (uint8_t) (data + 1) );
		pc++;
		a = nz = READ_PROG( addr );
		if ( (addr ^ 0x8000) <= 0x9FFF )
			goto loop;
		goto a_nz_read_addr;
	
	case 0xB9: // LDA abs,Y
		HANDLE_PAGE_CROSSING( data + y );
		addr = GET_ADDR() + y;
		pc += 2;
		a = nz = READ_PROG( addr );
		if ( (addr ^ 0x8000) <= 0x9FFF )
			goto loop;
		goto a_nz_read_addr;
	
	case 0xBD: // LDA abs,X
		HANDLE_PAGE_CROSSING( data + x );
		addr = GET_ADDR() + x;
		pc += 2;
		a = nz = READ_PROG( addr );
		if ( (addr ^ 0x8000) <= 0x9FFF )
			goto loop;
	a_nz_read_addr:
		FLUSH_TIME();
		a = nz = READ( addr );
		CACHE_TIME();
		goto loop;
	
	}

// Branch

	case 0x50: // BVC
		BRANCH( !(status & st_v) )
	
	case 0x70: // BVS
		BRANCH( status & st_v )
	
	case 0xB0: // BCS
		BRANCH( c & 0x100 )
	
	case 0x90: // BCC
		BRANCH( !(c & 0x100) )
	
// Load/store
	
	case 0x94: // STY zp,x
		data = uint8_t (data + x);
	case 0x84: // STY zp
		pc++;
		WRITE_LOW( data, y );
		goto loop;
	
	case 0x96: // STX zp,y
		data = uint8_t (data + y);
	case 0x86: // STX zp
		pc++;
		WRITE_LOW( data, x );
		goto loop;
	
	case 0xB6: // LDX zp,y
		data = uint8_t (data + y);
	case 0xA6: // LDX zp
		data = READ_LOW( data );
	case 0xA2: // LDX #imm
		pc++;
		x = data;
		nz = data;
		goto loop;
	
	case 0xB4: // LDY zp,x
		data = uint8_t (data + x);
	case 0xA4: // LDY zp
		data = READ_LOW( data );
	case 0xA0: // LDY #imm
		pc++;
		y = data;
		nz = data;
		goto loop;
	
	case 0xBC: // LDY abs,X
		data += x;
		HANDLE_PAGE_CROSSING( data );
	case 0xAC:{// LDY abs
		unsigned addr = data + 0x100 * GET_MSB();
		pc += 2;
		FLUSH_TIME();
		y = nz = READ( addr );
		CACHE_TIME();
		goto loop;
	}
	
	case 0xBE: // LDX abs,y
		data += y;
		HANDLE_PAGE_CROSSING( data );
	case 0xAE:{// LDX abs
		unsigned addr = data + 0x100 * GET_MSB();
		pc += 2;
		FLUSH_TIME();
		x = nz = READ( addr );
		CACHE_TIME();
		goto loop;
	}
	
	{
		fuint8 temp;
	case 0x8C: // STY abs
		temp = y;
		goto store_abs;
	
	case 0x8E: // STX abs
		temp = x;
	store_abs:
		unsigned addr = GET_ADDR();
		pc += 2;
		if ( addr <= 0x7FF )
		{
			WRITE_LOW( addr, temp );
			goto loop;
		}
		FLUSH_TIME();
		WRITE( addr, temp );
		CACHE_TIME();
		goto loop;
	}

// Compare

	case 0xEC:{// CPX abs
		unsigned addr = GET_ADDR();
		pc++;
		FLUSH_TIME();
		data = READ( addr );
		CACHE_TIME();
		goto cpx_data;
	}
	
	case 0xE4: // CPX zp
		data = READ_LOW( data );
	case 0xE0: // CPX #imm
	cpx_data:
		nz = x - data;
		pc++;
		c = ~nz;
		nz &= 0xFF;
		goto loop;
	
	case 0xCC:{// CPY abs
		unsigned addr = GET_ADDR();
		pc++;
		FLUSH_TIME();
		data = READ( addr );
		CACHE_TIME();
		goto cpy_data;
	}
	
	case 0xC4: // CPY zp
		data = READ_LOW( data );
	case 0xC0: // CPY #imm
	cpy_data:
		nz = y - data;
		pc++;
		c = ~nz;
		nz &= 0xFF;
		goto loop;
	
// Logical

	ARITH_ADDR_MODES( 0x25 ) // AND
		nz = (a &= data);
		pc++;
		goto loop;
	
	ARITH_ADDR_MODES( 0x45 ) // EOR
		nz = (a ^= data);
		pc++;
		goto loop;
	
	ARITH_ADDR_MODES( 0x05 ) // ORA
		nz = (a |= data);
		pc++;
		goto loop;
	
	case 0x2C:{// BIT abs
		unsigned addr = GET_ADDR();
		pc += 2;
		status &= ~st_v;
		READ_LIKELY_PPU( addr, nz );
		status |= nz & st_v;
		if ( a & nz )
			goto loop;
		nz <<= 8; // result must be zero, even if N bit is set
		goto loop;
	}
	
	case 0x24: // BIT zp
		nz = READ_LOW( data );
		pc++;
		status &= ~st_v;
		status |= nz & st_v;
		if ( a & nz )
			goto loop;
		nz <<= 8; // result must be zero, even if N bit is set
		goto loop;
		
// Add/subtract

	ARITH_ADDR_MODES( 0xE5 ) // SBC
	case 0xEB: // unofficial equivalent
		data ^= 0xFF;
		goto adc_imm;
	
	ARITH_ADDR_MODES( 0x65 ) // ADC
	adc_imm: {
		fint16 carry = c >> 8 & 1;
		fint16 ov = (a ^ 0x80) + carry + (BOOST::int8_t) data; // sign-extend
		status &= ~st_v;
		status |= ov >> 2 & 0x40;
		c = nz = a + data + carry;
		pc++;
		a = (uint8_t) nz;
		goto loop;
	}
	
// Shift/rotate

	case 0x4A: // LSR A
		c = 0;
	case 0x6A: // ROR A
		nz = c >> 1 & 0x80;
		c = a << 8;
		nz |= a >> 1;
		a = nz;
		goto loop;

	case 0x0A: // ASL A
		nz = a << 1;
		c = nz;
		a = (uint8_t) nz;
		goto loop;

	case 0x2A: { // ROL A
		nz = a << 1;
		fint16 temp = c >> 8 & 1;
		c = nz;
		nz |= temp;
		a = (uint8_t) nz;
		goto loop;
	}
	
	case 0x5E: // LSR abs,X
		data += x;
	case 0x4E: // LSR abs
		c = 0;
	case 0x6E: // ROR abs
	ror_abs: {
		ADD_PAGE();
		FLUSH_TIME();
		int temp = READ( data );
		nz = (c >> 1 & 0x80) | (temp >> 1);
		c = temp << 8;
		goto rotate_common;
	}
	
	case 0x3E: // ROL abs,X
		data += x;
		goto rol_abs;
	
	case 0x1E: // ASL abs,X
		data += x;
	case 0x0E: // ASL abs
		c = 0;
	case 0x2E: // ROL abs
	rol_abs:
		ADD_PAGE();
		nz = c >> 8 & 1;
		FLUSH_TIME();
		nz |= (c = READ( data ) << 1);
	rotate_common:
		pc++;
		WRITE( data, (uint8_t) nz );
		CACHE_TIME();
		goto loop;
	
	case 0x7E: // ROR abs,X
		data += x;
		goto ror_abs;
	
	case 0x76: // ROR zp,x
		data = uint8_t (data + x);
		goto ror_zp;
	
	case 0x56: // LSR zp,x
		data = uint8_t (data + x);
	case 0x46: // LSR zp
		c = 0;
	case 0x66: // ROR zp
	ror_zp: {
		int temp = READ_LOW( data );
		nz = (c >> 1 & 0x80) | (temp >> 1);
		c = temp << 8;
		goto write_nz_zp;
	}
	
	case 0x36: // ROL zp,x
		data = uint8_t (data + x);
		goto rol_zp;
	
	case 0x16: // ASL zp,x
		data = uint8_t (data + x);
	case 0x06: // ASL zp
		c = 0;
	case 0x26: // ROL zp
	rol_zp:
		nz = c >> 8 & 1;
		nz |= (c = READ_LOW( data ) << 1);
		goto write_nz_zp;
	
// Increment/decrement

	case 0xCA: // DEX
		INC_DEC_XY( x, -1 )
	
	case 0x88: // DEY
		INC_DEC_XY( y, -1 )
	
	case 0xF6: // INC zp,x
		data = uint8_t (data + x);
	case 0xE6: // INC zp
		nz = 1;
		goto add_nz_zp;
	
	case 0xD6: // DEC zp,x
		data = uint8_t (data + x);
	case 0xC6: // DEC zp
		nz = (unsigned) -1;
	add_nz_zp:
		nz += READ_LOW( data );
	write_nz_zp:
		pc++;
		WRITE_LOW( data, nz );
		goto loop;
	
	case 0xFE: // INC abs,x
		data = x + GET_ADDR();
		goto inc_ptr;
	
	case 0xEE: // INC abs
		data = GET_ADDR();
	inc_ptr:
		nz = 1;
		goto inc_common;
	
	case 0xDE: // DEC abs,x
		data = x + GET_ADDR();
		goto dec_ptr;
	
	case 0xCE: // DEC abs
		data = GET_ADDR();
	dec_ptr:
		nz = (unsigned) -1;
	inc_common:
		FLUSH_TIME();
		nz += READ( data );
		pc += 2;
		WRITE( data, (uint8_t) nz );
		CACHE_TIME();
		goto loop;
		
// Transfer

	case 0xAA: // TAX
		x  = a;
		nz = a;
		goto loop;
		
	case 0x8A: // TXA
		a  = x;
		nz = x;
		goto loop;

	case 0x9A: // TXS
		SET_SP( x ); // verified (no flag change)
		goto loop;
	
	case 0xBA: // TSX
		x = nz = GET_SP();
		goto loop;
	
// Stack
	
	case 0x48: // PHA
		PUSH( a ); // verified
		goto loop;
		
	case 0x68: // PLA
		a = nz = READ_LOW( sp );
		sp = (sp - 0xFF) | 0x100;
		goto loop;
		
	case 0x40:{// RTI
		fuint8 temp = READ_LOW( sp );
		pc  = READ_LOW( 0x100 | (sp - 0xFF) );
		pc |= READ_LOW( 0x100 | (sp - 0xFE) ) * 0x100;
		sp = (sp - 0xFD) | 0x100;
		data = status;
		SET_STATUS( temp );
		if ( !((data ^ status) & st_i) ) goto loop; // I flag didn't change
		this->r.status = status; // update externally-visible I flag
		blargg_long delta = s.base - irq_time_;
		if ( delta <= 0 ) goto loop;
		if ( status & st_i ) goto loop;
		s_time += delta;
		s.base = irq_time_;
		goto loop;
	}
	
	case 0x28:{// PLP
		fuint8 temp = READ_LOW( sp );
		sp = (sp - 0xFF) | 0x100;
		fuint8 changed = status ^ temp;
		SET_STATUS( temp );
		if ( !(changed & st_i) )
			goto loop; // I flag didn't change
		if ( status & st_i )
			goto handle_sei;
		goto handle_cli;
	}
	
	case 0x08: { // PHP
		fuint8 temp;
		CALC_STATUS( temp );
		PUSH( temp | (st_b | st_r) );
		goto loop;
	}
	
	case 0x6C:{// JMP (ind)
		data = GET_ADDR();
		check( unsigned (data - 0x2000) >= 0x4000 ); // ensure it's outside I/O space
		uint8_t const* page = s.code_map [data >> page_bits];
		pc = page [PAGE_OFFSET( data )];
		data = (data & 0xFF00) | ((data + 1) & 0xFF);
		pc |= page [PAGE_OFFSET( data )] << 8;
		goto loop;
	}
	
	case 0x00: // BRK
		goto handle_brk;
	
// Flags

	case 0x38: // SEC
		c = (unsigned) ~0;
		goto loop;
	
	case 0x18: // CLC
		c = 0;
		goto loop;
		
	case 0xB8: // CLV
		status &= ~st_v;
		goto loop;
	
	case 0xD8: // CLD
		status &= ~st_d;
		goto loop;
	
	case 0xF8: // SED
		status |= st_d;
		goto loop;
	
	case 0x58: // CLI
		if ( !(status & st_i) )
			goto loop;
		status &= ~st_i;
	handle_cli: {
		//dprintf( "CLI at %d\n", TIME );
		this->r.status = status; // update externally-visible I flag
		blargg_long delta = s.base - irq_time_;
		if ( delta <= 0 )
		{
			if ( TIME < irq_time_ )
				goto loop;
			goto delayed_cli;
		}
		s.base = irq_time_;
		s_time += delta;
		if ( s_time < 0 )
			goto loop;
		
		if ( delta >= s_time + 1 )
		{
			s.base += s_time + 1;
			s_time = -1;
			goto loop;
		}
		
		// TODO: implement
	delayed_cli:
		dprintf( "Delayed CLI not emulated\n" );
		goto loop;
	}
	
	case 0x78: // SEI
		if ( status & st_i )
			goto loop;
		status |= st_i;
	handle_sei: {
		this->r.status = status; // update externally-visible I flag
		blargg_long delta = s.base - end_time_;
		s.base = end_time_;
		s_time += delta;
		if ( s_time < 0 )
			goto loop;
		
		dprintf( "Delayed SEI not emulated\n" );
		goto loop;
	}
	
// Unofficial
	
	// SKW - Skip word
	case 0x1C: case 0x3C: case 0x5C: case 0x7C: case 0xDC: case 0xFC:
		HANDLE_PAGE_CROSSING( data + x );
	case 0x0C:
		pc++;
	// SKB - Skip byte
	case 0x74: case 0x04: case 0x14: case 0x34: case 0x44: case 0x54: case 0x64:
	case 0x80: case 0x82: case 0x89: case 0xC2: case 0xD4: case 0xE2: case 0xF4:
		pc++;
		goto loop;
	
	// NOP
	case 0xEA: case 0x1A: case 0x3A: case 0x5A: case 0x7A: case 0xDA: case 0xFA:
		goto loop;

	case bad_opcode: // HLT
		pc--;
		if ( pc > 0xFFFF )
		{
			// handle wrap-around (assumes caller has put page of HLT at 0x10000)
			pc &= 0xFFFF;
			goto loop;
		}
	case 0x02: case 0x12: case 0x22: case 0x32: case 0x42: case 0x52:
	case 0x62: case 0x72: case 0x92: case 0xB2: case 0xD2:
		goto stop;
	
// Unimplemented
	
	case 0xFF: // force 256-entry jump table for optimization purposes
		c |= 1;
	default:
		check( (unsigned) opcode <= 0xFF );
		// skip over proper number of bytes
		static unsigned char const illop_lens [8] = {
			0x40, 0x40, 0x40, 0x80, 0x40, 0x40, 0x80, 0xA0
		};
		fuint8 opcode = instr [-1];
		fint16 len = illop_lens [opcode >> 2 & 7] >> (opcode << 1 & 6) & 3;
		if ( opcode == 0x9C )
			len = 2;
		pc += len;
		error_count_++;
		
		if ( (opcode >> 4) == 0x0B )
		{
			if ( opcode == 0xB3 )
				data = READ_LOW( data );
			if ( opcode != 0xB7 )
				HANDLE_PAGE_CROSSING( data + y );
		}
		goto loop;
	}
	assert( false );
	
	int result_;
handle_brk:
	pc++;
	result_ = 4;
	
interrupt:
	{
		s_time += 7;
		
		WRITE_LOW( 0x100 | (sp - 1), pc >> 8 );
		WRITE_LOW( 0x100 | (sp - 2), pc );
		pc = GET_LE16( &READ_PROG( 0xFFFA ) + result_ );
		
		sp = (sp - 3) | 0x100;
		fuint8 temp;
		CALC_STATUS( temp );
		temp |= st_r;
		if ( result_ )
			temp |= st_b; // TODO: incorrectly sets B flag for IRQ
		WRITE_LOW( sp, temp );
		
		this->r.status = status |= st_i;
		blargg_long delta = s.base - end_time_;
		if ( delta >= 0 ) goto loop;
		s_time += delta;
		s.base = end_time_;
		goto loop;
	}
	
out_of_time:
	pc--;
	FLUSH_TIME();
	CPU_DONE( this, TIME, result_ );
	CACHE_TIME();
	if ( result_ >= 0 )
		goto interrupt;
	if ( s_time < 0 )
		goto loop;
	
stop:
	
	s.time = s_time;
	
	r.pc = pc;
	r.sp = GET_SP();
	r.a = a;
	r.x = x;
	r.y = y;
	
	{
		fuint8 temp;
		CALC_STATUS( temp );
		r.status = temp;
	}
	
	this->state_ = s;
	this->state = &this->state_;
	
	return s_time < 0;
}

