// Game_Music_Emu 0.6.0. http://www.slack.net/~ant/

#include "Hes_Cpu.h"

#include "blargg_endian.h"

//#include "hes_cpu_log.h"

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

// TODO: support T flag, including clearing it at appropriate times?

// all zero-page should really use whatever is at page 1, but that would
// reduce efficiency quite a bit
int const ram_addr = 0x2000;

#define FLUSH_TIME()    (void) (s.time = s_time)
#define CACHE_TIME()    (void) (s_time = s.time)

#include "hes_cpu_io.h"

#include "blargg_source.h"

#if BLARGG_NONPORTABLE
	#define PAGE_OFFSET( addr ) (addr)
#else
	#define PAGE_OFFSET( addr ) ((addr) & (page_size - 1))
#endif

// status flags
int const st_n = 0x80;
int const st_v = 0x40;
//unused: int const st_t = 0x20;
int const st_b = 0x10;
int const st_d = 0x08;
int const st_i = 0x04;
int const st_z = 0x02;
int const st_c = 0x01;

void Hes_Cpu::reset()
{
	check( state == &state_ );
	state = &state_;
	
	state_.time = 0;
	state_.base = 0;
	irq_time_   = future_hes_time;
	end_time_   = future_hes_time;
	
	r.status = st_i;
	r.sp     = 0;
	r.pc     = 0;
	r.a      = 0;
	r.x      = 0;
	r.y      = 0;
	
	blargg_verify_byte_order();
}

void Hes_Cpu::set_mmr( int reg, int bank )
{
	assert( (unsigned) reg <= page_count ); // allow page past end to be set
	assert( (unsigned) bank < 0x100 );
	mmr [reg] = bank;
	uint8_t const* code = CPU_SET_MMR( this, reg, bank );
	state->code_map [reg] = code - PAGE_OFFSET( reg << page_shift );
}

#define TIME    (s_time + s.base)

#define READ( addr )            CPU_READ( this, (addr), TIME )
#define WRITE( addr, data )     {CPU_WRITE( this, (addr), (data), TIME );}
#define READ_LOW( addr )        (ram [int (addr)])
#define WRITE_LOW( addr, data ) (void) (READ_LOW( addr ) = (data))
#define READ_PROG( addr )       (s.code_map [(addr) >> page_shift] [PAGE_OFFSET( addr )])

#define SET_SP( v )     (sp = ((v) + 1) | 0x100)
#define GET_SP()        ((sp - 1) & 0xFF)
#define PUSH( v )       ((sp = (sp - 1) | 0x100), WRITE_LOW( sp, v ))

// even on x86, using short and unsigned char was slower
typedef int         fint16;
typedef unsigned    fuint16;
typedef unsigned    fuint8;
typedef blargg_long fint32;

bool Hes_Cpu::run( hes_time_t end_time )
{
	bool illegal_encountered = false;
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
branch_not_taken:
	s_time -= 2;
loop:
	
	#ifndef NDEBUG
	{
		hes_time_t correct = end_time_;
		if ( !(status & st_i) && correct > irq_time_ )
			correct = irq_time_;
		check( s.base == correct );
		/*
		static long count;
		if ( count == 1844 ) Debugger();
		if ( s.base != correct ) debug_printf( "%ld\n", count );
		count++;
		*/
	}
	#endif

	check( (unsigned) GET_SP() < 0x100 );
	check( (unsigned) a < 0x100 );
	check( (unsigned) x < 0x100 );
	
	uint8_t const* instr = s.code_map [pc >> page_shift];
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
	
	// TODO: each reference lists slightly different timing values, ugh
	static uint8_t const clock_table [256] =
	{// 0 1 2  3 4 5 6 7 8 9 A B C D E F
		1,7,3, 4,6,4,6,7,3,2,2,2,7,5,7,6,// 0
		4,7,7, 4,6,4,6,7,2,5,2,2,7,5,7,6,// 1
		7,7,3, 4,4,4,6,7,4,2,2,2,5,5,7,6,// 2
		4,7,7, 2,4,4,6,7,2,5,2,2,5,5,7,6,// 3
		7,7,3, 4,8,4,6,7,3,2,2,2,4,5,7,6,// 4
		4,7,7, 5,2,4,6,7,2,5,3,2,2,5,7,6,// 5
		7,7,2, 2,4,4,6,7,4,2,2,2,7,5,7,6,// 6
		4,7,7,17,4,4,6,7,2,5,4,2,7,5,7,6,// 7
		4,7,2, 7,4,4,4,7,2,2,2,2,5,5,5,6,// 8
		4,7,7, 8,4,4,4,7,2,5,2,2,5,5,5,6,// 9
		2,7,2, 7,4,4,4,7,2,2,2,2,5,5,5,6,// A
		4,7,7, 8,4,4,4,7,2,5,2,2,5,5,5,6,// B
		2,7,2,17,4,4,6,7,2,2,2,2,5,5,7,6,// C
		4,7,7,17,2,4,6,7,2,5,3,2,2,5,7,6,// D
		2,7,2,17,4,4,6,7,2,2,2,2,5,5,7,6,// E
		4,7,7,17,2,4,6,7,2,5,4,2,2,5,7,6 // F
	}; // 0x00 was 8
	
	fuint16 data;
	data = clock_table [opcode];
	if ( (s_time += data) >= 0 )
		goto possibly_out_of_time;
almost_out_of_time:
	
	data = *instr;
	
	#ifdef HES_CPU_LOG_H
		log_cpu( "new", pc - 1, opcode, instr [0], instr [1], instr [2],
				instr [3], instr [4], instr [5] );
		//log_opcode( opcode );
	#endif
	
	switch ( opcode )
	{
possibly_out_of_time:
		if ( s_time < (int) data )
			goto almost_out_of_time;
		s_time -= data;
		goto out_of_time;

// Macros

#define GET_MSB()           (instr [1])
#define ADD_PAGE( out )     (pc++, out = data + 0x100 * GET_MSB());
#define GET_ADDR()          GET_LE16( instr )

// TODO: is the penalty really always added? the original 6502 was much better
//#define PAGE_CROSS_PENALTY( lsb ) (void) (s_time += (lsb) >> 8)
#define PAGE_CROSS_PENALTY( lsb )

// Branch

// TODO: more efficient way to handle negative branch that wraps PC around
#define BRANCH( cond )\
{\
	fint16 offset = (BOOST::int8_t) data;\
	pc++;\
	if ( !(cond) ) goto branch_not_taken;\
	pc = BOOST::uint16_t (pc + offset);\
	goto loop;\
}

	case 0xF0: // BEQ
		BRANCH( !((uint8_t) nz) );
	
	case 0xD0: // BNE
		BRANCH( (uint8_t) nz );
	
	case 0x10: // BPL
		BRANCH( !IS_NEG );
	
	case 0x90: // BCC
		BRANCH( !(c & 0x100) )
	
	case 0x30: // BMI
		BRANCH( IS_NEG )
	
	case 0x50: // BVC
		BRANCH( !(status & st_v) )
	
	case 0x70: // BVS
		BRANCH( status & st_v )
	
	case 0xB0: // BCS
		BRANCH( c & 0x100 )
	
	case 0x80: // BRA
	branch_taken:
		BRANCH( true );
	
	case 0xFF:
		if ( pc == idle_addr + 1 )
			goto idle_done;
	case 0x0F: // BBRn
	case 0x1F:
	case 0x2F:
	case 0x3F:
	case 0x4F:
	case 0x5F:
	case 0x6F:
	case 0x7F:
	case 0x8F: // BBSn
	case 0x9F:
	case 0xAF:
	case 0xBF:
	case 0xCF:
	case 0xDF:
	case 0xEF: {
		fuint16 t = 0x101 * READ_LOW( data );
		t ^= 0xFF;
		pc++;
		data = GET_MSB();
		BRANCH( t & (1 << (opcode >> 4)) )
	}
	
	case 0x4C: // JMP abs
		pc = GET_ADDR();
		goto loop;
	
	case 0x7C: // JMP (ind+X)
		data += x;
	case 0x6C:{// JMP (ind)
		data += 0x100 * GET_MSB();
		pc = GET_LE16( &READ_PROG( data ) );
		goto loop;
	}
	
// Subroutine

	case 0x44: // BSR
		WRITE_LOW( 0x100 | (sp - 1), pc >> 8 );
		sp = (sp - 2) | 0x100;
		WRITE_LOW( sp, pc );
		goto branch_taken;
	
	case 0x20: { // JSR
		fuint16 temp = pc + 1;
		pc = GET_ADDR();
		WRITE_LOW( 0x100 | (sp - 1), temp >> 8 );
		sp = (sp - 2) | 0x100;
		WRITE_LOW( sp, temp );
		goto loop;
	}
	
	case 0x60: // RTS
		pc = 0x100 * READ_LOW( 0x100 | (sp - 0xFF) );
		pc += 1 + READ_LOW( sp );
		sp = (sp - 0xFE) | 0x100;
		goto loop;
	
	case 0x00: // BRK
		goto handle_brk;
	
// Common

	case 0xBD:{// LDA abs,X
		PAGE_CROSS_PENALTY( data + x );
		fuint16 addr = GET_ADDR() + x;
		pc += 2;
		CPU_READ_FAST( this, addr, TIME, nz );
		a = nz;
		goto loop;
	}
	
	case 0x9D:{// STA abs,X
		fuint16 addr = GET_ADDR() + x;
		pc += 2;
		CPU_WRITE_FAST( this, addr, a, TIME );
		goto loop;
	}
	
	case 0x95: // STA zp,x
		data = uint8_t (data + x);
	case 0x85: // STA zp
		pc++;
		WRITE_LOW( data, a );
		goto loop;
	
	case 0xAE:{// LDX abs
		fuint16 addr = GET_ADDR();
		pc += 2;
		CPU_READ_FAST( this, addr, TIME, nz );
		x = nz;
		goto loop;
	}
	
	case 0xA5: // LDA zp
		a = nz = READ_LOW( data );
		pc++;
		goto loop;
	
// Load/store
	
	{
		fuint16 addr;
	case 0x91: // STA (ind),Y
		addr = 0x100 * READ_LOW( uint8_t (data + 1) );
		addr += READ_LOW( data ) + y;
		pc++;
		goto sta_ptr;
	
	case 0x81: // STA (ind,X)
		data = uint8_t (data + x);
	case 0x92: // STA (ind)
		addr = 0x100 * READ_LOW( uint8_t (data + 1) );
		addr += READ_LOW( data );
		pc++;
		goto sta_ptr;
	
	case 0x99: // STA abs,Y
		data += y;
	case 0x8D: // STA abs
		addr = data + 0x100 * GET_MSB();
		pc += 2;
	sta_ptr:
		CPU_WRITE_FAST( this, addr, a, TIME );
		goto loop;
	}
	
	{
		fuint16 addr;
	case 0xA1: // LDA (ind,X)
		data = uint8_t (data + x);
	case 0xB2: // LDA (ind)
		addr = 0x100 * READ_LOW( uint8_t (data + 1) );
		addr += READ_LOW( data );
		pc++;
		goto a_nz_read_addr;
	
	case 0xB1:// LDA (ind),Y
		addr = READ_LOW( data ) + y;
		PAGE_CROSS_PENALTY( addr );
		addr += 0x100 * READ_LOW( (uint8_t) (data + 1) );
		pc++;
		goto a_nz_read_addr;
	
	case 0xB9: // LDA abs,Y
		data += y;
		PAGE_CROSS_PENALTY( data );
	case 0xAD: // LDA abs
		addr = data + 0x100 * GET_MSB();
		pc += 2;
	a_nz_read_addr:
		CPU_READ_FAST( this, addr, TIME, nz );
		a = nz;
		goto loop;
	}

	case 0xBE:{// LDX abs,y
		PAGE_CROSS_PENALTY( data + y );
		fuint16 addr = GET_ADDR() + y;
		pc += 2;
		FLUSH_TIME();
		x = nz = READ( addr );
		CACHE_TIME();
		goto loop;
	}
	
	case 0xB5: // LDA zp,x
		a = nz = READ_LOW( uint8_t (data + x) );
		pc++;
		goto loop;
	
	case 0xA9: // LDA #imm
		pc++;
		a  = data;
		nz = data;
		goto loop;

// Bit operations

	case 0x3C: // BIT abs,x
		data += x;
	case 0x2C:{// BIT abs
		fuint16 addr;
		ADD_PAGE( addr );
		FLUSH_TIME();
		nz = READ( addr );
		CACHE_TIME();
		goto bit_common;
	}
	case 0x34: // BIT zp,x
		data = uint8_t (data + x);
	case 0x24: // BIT zp
		data = READ_LOW( data );
	case 0x89: // BIT imm
		nz = data;
	bit_common:
		pc++;
		status &= ~st_v;
		status |= nz & st_v;
		if ( nz & a )
			goto loop; // Z should be clear, and nz must be non-zero if nz & a is
		nz <<= 8; // set Z flag without affecting N flag
		goto loop;
		
	{
		fuint16 addr;
		
	case 0xB3: // TST abs,x
		addr = GET_MSB() + x;
		goto tst_abs;
	
	case 0x93: // TST abs
		addr = GET_MSB();
	tst_abs:
		addr += 0x100 * instr [2];
		pc++;
		FLUSH_TIME();
		nz = READ( addr );
		CACHE_TIME();
		goto tst_common;
	}
	
	case 0xA3: // TST zp,x
		nz = READ_LOW( uint8_t (GET_MSB() + x) );
		goto tst_common;
	
	case 0x83: // TST zp
		nz = READ_LOW( GET_MSB() );
	tst_common:
		pc += 2;
		status &= ~st_v;
		status |= nz & st_v;
		if ( nz & data )
			goto loop; // Z should be clear, and nz must be non-zero if nz & data is
		nz <<= 8; // set Z flag without affecting N flag
		goto loop;
	
	{
		fuint16 addr;
	case 0x0C: // TSB abs
	case 0x1C: // TRB abs
		addr = GET_ADDR();
		pc++;
		goto txb_addr;
	
	// TODO: everyone lists different behaviors for the status flags, ugh
	case 0x04: // TSB zp
	case 0x14: // TRB zp
		addr = data + ram_addr;
	txb_addr:
		FLUSH_TIME();
		nz = a | READ( addr );
		if ( opcode & 0x10 )
			nz ^= a; // bits from a will already be set, so this clears them
		status &= ~st_v;
		status |= nz & st_v;
		pc++;
		WRITE( addr, nz );
		CACHE_TIME();
		goto loop;
	}
	
	case 0x07: // RMBn
	case 0x17:
	case 0x27:
	case 0x37:
	case 0x47:
	case 0x57:
	case 0x67:
	case 0x77:
		pc++;
		READ_LOW( data ) &= ~(1 << (opcode >> 4));
		goto loop;
	
	case 0x87: // SMBn
	case 0x97:
	case 0xA7:
	case 0xB7:
	case 0xC7:
	case 0xD7:
	case 0xE7:
	case 0xF7:
		pc++;
		READ_LOW( data ) |= 1 << ((opcode >> 4) - 8);
		goto loop;
	
// Load/store
	
	case 0x9E: // STZ abs,x
		data += x;
	case 0x9C: // STZ abs
		ADD_PAGE( data );
		pc++;
		FLUSH_TIME();
		WRITE( data, 0 );
		CACHE_TIME();
		goto loop;
	
	case 0x74: // STZ zp,x
		data = uint8_t (data + x);
	case 0x64: // STZ zp
		pc++;
		WRITE_LOW( data, 0 );
		goto loop;
	
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
		PAGE_CROSS_PENALTY( data );
	case 0xAC:{// LDY abs
		fuint16 addr = data + 0x100 * GET_MSB();
		pc += 2;
		FLUSH_TIME();
		y = nz = READ( addr );
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
		fuint16 addr = GET_ADDR();
		pc += 2;
		FLUSH_TIME();
		WRITE( addr, temp );
		CACHE_TIME();
		goto loop;
	}

// Compare

	case 0xEC:{// CPX abs
		fuint16 addr = GET_ADDR();
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
		fuint16 addr = GET_ADDR();
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

#define ARITH_ADDR_MODES( op )\
	case op - 0x04: /* (ind,x) */\
		data = uint8_t (data + x);\
	case op + 0x0D: /* (ind) */\
		data = 0x100 * READ_LOW( uint8_t (data + 1) ) + READ_LOW( data );\
		goto ptr##op;\
	case op + 0x0C:{/* (ind),y */\
		fuint16 temp = READ_LOW( data ) + y;\
		PAGE_CROSS_PENALTY( temp );\
		data = temp + 0x100 * READ_LOW( uint8_t (data + 1) );\
		goto ptr##op;\
	}\
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
		PAGE_CROSS_PENALTY( data );\
	case op + 0x08: /* abs */\
		ADD_PAGE( data );\
	ptr##op:\
		FLUSH_TIME();\
		data = READ( data );\
		CACHE_TIME();\
	case op + 0x04: /* imm */\
	imm##op:

	ARITH_ADDR_MODES( 0xC5 ) // CMP
		nz = a - data;
		pc++;
		c = ~nz;
		nz &= 0xFF;
		goto loop;
	
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
	
// Add/subtract

	ARITH_ADDR_MODES( 0xE5 ) // SBC
		data ^= 0xFF;
		goto adc_imm;
	
	ARITH_ADDR_MODES( 0x65 ) // ADC
	adc_imm: {
		if ( status & st_d )
			debug_printf( "Decimal mode not supported\n" );
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
		ADD_PAGE( data );
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
		ADD_PAGE( data );
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

#define INC_DEC_AXY( reg, n ) reg = uint8_t (nz = reg + n); goto loop;

	case 0x1A: // INA
		INC_DEC_AXY( a, +1 )
	
	case 0xE8: // INX
		INC_DEC_AXY( x, +1 )
	
	case 0xC8: // INY
		INC_DEC_AXY( y, +1 )

	case 0x3A: // DEA
		INC_DEC_AXY( a, -1 )
	
	case 0xCA: // DEX
		INC_DEC_AXY( x, -1 )
	
	case 0x88: // DEY
		INC_DEC_AXY( y, -1 )
	
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

	case 0xA8: // TAY
		y  = a;
		nz = a;
		goto loop;
	
	case 0x98: // TYA
		a  = y;
		nz = y;
		goto loop;
	
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
	
	#define SWAP_REGS( r1, r2 ) {\
		fuint8 t = r1;\
		r1 = r2;\
		r2 = t;\
		goto loop;\
	}
	
	case 0x02: // SXY
		SWAP_REGS( x, y );
	
	case 0x22: // SAX
		SWAP_REGS( a, x );
	
	case 0x42: // SAY
		SWAP_REGS( a, y );
	
	case 0x62: // CLA
		a = 0;
		goto loop;
	
	case 0x82: // CLX
		x = 0;
		goto loop;
	
	case 0xC2: // CLY
		y = 0;
		goto loop;
	
// Stack
	
	case 0x48: // PHA
		PUSH( a );
		goto loop;
		
	case 0xDA: // PHX
		PUSH( x );
		goto loop;
		
	case 0x5A: // PHY
		PUSH( y );
		goto loop;
		
	case 0x40:{// RTI
		fuint8 temp = READ_LOW( sp );
		pc  = READ_LOW( 0x100 | (sp - 0xFF) );
		pc |= READ_LOW( 0x100 | (sp - 0xFE) ) * 0x100;
		sp = (sp - 0xFD) | 0x100;
		data = status;
		SET_STATUS( temp );
		this->r.status = status; // update externally-visible I flag
		if ( (data ^ status) & st_i )
		{
			hes_time_t new_time = end_time_;
			if ( !(status & st_i) && new_time > irq_time_ )
				new_time = irq_time_;
			blargg_long delta = s.base - new_time;
			s.base = new_time;
			s_time += delta;
		}
		goto loop;
	}
	
	#define POP()  READ_LOW( sp ); sp = (sp - 0xFF) | 0x100
	
	case 0x68: // PLA
		a = nz = POP();
		goto loop;
	
	case 0xFA: // PLX
		x = nz = POP();
		goto loop;
	
	case 0x7A: // PLY
		y = nz = POP();
		goto loop;
	
	case 0x28:{// PLP
		fuint8 temp = POP();
		fuint8 changed = status ^ temp;
		SET_STATUS( temp );
		if ( !(changed & st_i) )
			goto loop; // I flag didn't change
		if ( status & st_i )
			goto handle_sei;
		goto handle_cli;
	}
	#undef POP
	
	case 0x08: { // PHP
		fuint8 temp;
		CALC_STATUS( temp );
		PUSH( temp | st_b );
		goto loop;
	}
	
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
			// delayed irq until after next instruction
			s.base += s_time + 1;
			s_time = -1;
			irq_time_ = s.base; // TODO: remove, as only to satisfy debug check in loop
			goto loop;
		}
	delayed_cli:
		debug_printf( "Delayed CLI not supported\n" ); // TODO: implement
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
		debug_printf( "Delayed SEI not supported\n" ); // TODO: implement
		goto loop;
	}
	
// Special
	
	case 0x53:{// TAM
		fuint8 const bits = data; // avoid using data across function call
		pc++;
		for ( int i = 0; i < 8; i++ )
			if ( bits & (1 << i) )
				set_mmr( i, a );
		goto loop;
	}
	
	case 0x43:{// TMA
		pc++;
		byte const* in = mmr;
		do
		{
			if ( data & 1 )
				a = *in;
			in++;
		}
		while ( (data >>= 1) != 0 );
		goto loop;
	}
	
	case 0x03: // ST0
	case 0x13: // ST1
	case 0x23:{// ST2
		fuint16 addr = opcode >> 4;
		if ( addr )
			addr++;
		pc++;
		FLUSH_TIME();
		CPU_WRITE_VDP( this, addr, data, TIME );
		CACHE_TIME();
		goto loop;
	}
	
	case 0xEA: // NOP
		goto loop;

	case 0x54: // CSL
		debug_printf( "CSL not supported\n" );
		illegal_encountered = true;
		goto loop;
	
	case 0xD4: // CSH
		goto loop;
	
	case 0xF4: { // SET
		//fuint16 operand = GET_MSB();
		debug_printf( "SET not handled\n" );
		//switch ( data )
		//{
		//}
		illegal_encountered = true;
		goto loop;
	}
	
// Block transfer

	{
		fuint16 in_alt;
		fint16 in_inc;
		fuint16 out_alt;
		fint16 out_inc;
		
	case 0xE3: // TIA
		in_alt  = 0;
		goto bxfer_alt;
	
	case 0xF3: // TAI
		in_alt  = 1;
	bxfer_alt:
		in_inc  = in_alt ^ 1;
		out_alt = in_inc;
		out_inc = in_alt;
		goto bxfer;
	
	case 0xD3: // TIN
		in_inc  = 1;
		out_inc = 0;
		goto bxfer_no_alt;
	
	case 0xC3: // TDD
		in_inc  = -1;
		out_inc = -1;
		goto bxfer_no_alt;
	
	case 0x73: // TII
		in_inc  = 1;
		out_inc = 1;
	bxfer_no_alt:
		in_alt  = 0;
		out_alt = 0;
	bxfer:
		fuint16 in    = GET_LE16( instr + 0 );
		fuint16 out   = GET_LE16( instr + 2 );
		int     count = GET_LE16( instr + 4 );
		if ( !count )
			count = 0x10000;
		pc += 6;
		WRITE_LOW( 0x100 | (sp - 1), y );
		WRITE_LOW( 0x100 | (sp - 2), a );
		WRITE_LOW( 0x100 | (sp - 3), x );
		FLUSH_TIME();
		do
		{
			// TODO: reads from $0800-$1400 in I/O page return 0 and don't access I/O
			fuint8 t = READ( in );
			in += in_inc;
			in &= 0xFFFF;
			s.time += 6;
			if ( in_alt )
				in_inc = -in_inc;
			WRITE( out, t );
			out += out_inc;
			out &= 0xFFFF;
			if ( out_alt )
				out_inc = -out_inc;
		}
		while ( --count );
		CACHE_TIME();
		goto loop;
	}

// Illegal

	default:
		assert( (unsigned) opcode <= 0xFF );
		debug_printf( "Illegal opcode $%02X at $%04X\n", (int) opcode, (int) pc - 1 );
		illegal_encountered = true;
		goto loop;
	}
	assert( false );
	
	int result_;
handle_brk:
	pc++;
	result_ = 6;
	
interrupt:
	{
		s_time += 7;
		
		WRITE_LOW( 0x100 | (sp - 1), pc >> 8 );
		WRITE_LOW( 0x100 | (sp - 2), pc );
		pc = GET_LE16( &READ_PROG( 0xFFF0 ) + result_ );
		
		sp = (sp - 3) | 0x100;
		fuint8 temp;
		CALC_STATUS( temp );
		if ( result_ == 6 )
			temp |= st_b;
		WRITE_LOW( sp, temp );
		
		status &= ~st_d;
		status |= st_i;
		this->r.status = status; // update externally-visible I flag
		
		blargg_long delta = s.base - end_time_;
		s.base = end_time_;
		s_time += delta;
		goto loop;
	}
	
idle_done:
	s_time = 0;
out_of_time:
	pc--;
	FLUSH_TIME();
	CPU_DONE( this, TIME, result_ );
	CACHE_TIME();
	if ( result_ > 0 )
		goto interrupt;
	if ( s_time < 0 )
		goto loop;
	
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
	
	return illegal_encountered;
}

