// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/

#include "Gb_Cpu.h"

#include <string.h>

//#include "gb_cpu_log.h"

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

#include "gb_cpu_io.h"

#include "blargg_source.h"

// Common instructions:
//
// 365880   FA      LD  A,IND16
// 355863   20      JR  NZ
// 313655   21      LD  HL,IMM
// 274580   28      JR  Z
// 252878   FE      CMP IMM
// 230541   7E      LD  A,(HL)
// 226209   2A      LD A,(HL+)
// 217467   CD      CALL
// 212034   C9      RET
// 208376   CB      CB prefix
//
//  27486   CB 7E   BIT 7,(HL)
//  15925   CB 76   BIT 6,(HL)
//  13035   CB 19   RR  C
//  11557   CB 7F   BIT 7,A
//  10898   CB 37   SWAP A
//  10208   CB 66   BIT 4,(HL)

#if BLARGG_NONPORTABLE
	#define PAGE_OFFSET( addr ) (addr)
#else
	#define PAGE_OFFSET( addr ) ((addr) & (page_size - 1))
#endif

inline void Gb_Cpu::set_code_page( int i, uint8_t* p )
{
	state->code_map [i] = p - PAGE_OFFSET( i * (blargg_long) page_size );
}

void Gb_Cpu::reset( void* unmapped )
{
	check( state == &state_ );
	state = &state_;
	
	state_.remain = 0;
	
	for ( int i = 0; i < page_count + 1; i++ )
		set_code_page( i, (uint8_t*) unmapped );
	
	memset( &r, 0, sizeof r );
	//interrupts_enabled = false;
	
	blargg_verify_byte_order();
}

void Gb_Cpu::map_code( gb_addr_t start, unsigned size, void* data )
{
	// address range must begin and end on page boundaries
	require( start % page_size == 0 );
	require( size % page_size == 0 );
	
	unsigned first_page = start / page_size;
	for ( unsigned i = size / page_size; i--; )
		set_code_page( first_page + i, (uint8_t*) data + i * page_size );
}

#define READ( addr )            CPU_READ( this, (addr), s.remain )
#define WRITE( addr, data )     {CPU_WRITE( this, (addr), (data), s.remain );}
#define READ_FAST( addr, out )  CPU_READ_FAST( this, (addr), s.remain, out )
#define READ_PROG( addr )       (s.code_map [(addr) >> page_shift] [PAGE_OFFSET( addr )])

unsigned const z_flag = 0x80;
unsigned const n_flag = 0x40;
unsigned const h_flag = 0x20;
unsigned const c_flag = 0x10;

bool Gb_Cpu::run( blargg_long cycle_count )
{
	state_.remain = blargg_ulong (cycle_count + clocks_per_instr) / clocks_per_instr;
	state_t s;
	this->state = &s;
	memcpy( &s, &this->state_, sizeof s );
	
#if BLARGG_BIG_ENDIAN
	#define R8( n ) (r8_ [n]) 
#elif BLARGG_LITTLE_ENDIAN
	#define R8( n ) (r8_ [(n) ^ 1]) 
#else
	#error "Byte order of CPU must be known"
#endif
	
	union {
		core_regs_t rg; // individual registers
		
		struct {
			uint16_t bc, de, hl, unused; // pairs
		} rp;
		
		uint8_t r8_ [8]; // indexed registers (use R8 macro due to endian dependence)
		uint16_t r16 [4]; // indexed pairs
	};
	BOOST_STATIC_ASSERT( sizeof rg == 8 && sizeof rp == 8 );
	
	rg = r;
	unsigned pc = r.pc;
	unsigned sp = r.sp;
	unsigned flags = r.flags;
	
loop:
	
	check( (unsigned long) pc < 0x10000 );
	check( (unsigned long) sp < 0x10000 );
	check( (flags & ~0xF0) == 0 );
	
	uint8_t const* instr = s.code_map [pc >> page_shift];
	unsigned op;
	
	// TODO: eliminate this special case
	#if BLARGG_NONPORTABLE
		op = instr [pc];
		pc++;
		instr += pc;
	#else
		instr += PAGE_OFFSET( pc );
		op = *instr++;
		pc++;
	#endif
	
#define GET_ADDR()  GET_LE16( instr )
	
	if ( !--s.remain )
		goto stop;
	
	unsigned data;
	data = *instr;
	
	#ifdef GB_CPU_LOG_H
		gb_cpu_log( "new", pc - 1, op, data, instr [1] );
	#endif
	
	switch ( op )
	{

// TODO: more efficient way to handle negative branch that wraps PC around
#define BRANCH( cond )\
{\
	pc++;\
	int offset = (int8_t) data;\
	if ( !(cond) ) goto loop;\
	pc = uint16_t (pc + offset);\
	goto loop;\
}

// Most Common

	case 0x20: // JR NZ
		BRANCH( !(flags & z_flag) )
	
	case 0x21: // LD HL,IMM (common)
		rp.hl = GET_ADDR();
		pc += 2;
		goto loop;
	
	case 0x28: // JR Z
		BRANCH( flags & z_flag )
	
	{
		unsigned temp;
	case 0xF0: // LD A,(0xFF00+imm)
		temp = data | 0xFF00;
		pc++;
		goto ld_a_ind_comm;
	
	case 0xF2: // LD A,(0xFF00+C)
		temp = rg.c | 0xFF00;
		goto ld_a_ind_comm;
	
	case 0x0A: // LD A,(BC)
		temp = rp.bc;
		goto ld_a_ind_comm;
	
	case 0x3A: // LD A,(HL-)
		temp = rp.hl;
		rp.hl = temp - 1;
		goto ld_a_ind_comm;
	
	case 0x1A: // LD A,(DE)
		temp = rp.de;
		goto ld_a_ind_comm;
	
	case 0x2A: // LD A,(HL+) (common)
		temp = rp.hl;
		rp.hl = temp + 1;
		goto ld_a_ind_comm;
		
	case 0xFA: // LD A,IND16 (common)
		temp = GET_ADDR();
		pc += 2;
	ld_a_ind_comm:
		READ_FAST( temp, rg.a );
		goto loop;
	}
	
	case 0xBE: // CMP (HL)
		data = READ( rp.hl );
		goto cmp_comm;
	
	case 0xB8: // CMP B
	case 0xB9: // CMP C
	case 0xBA: // CMP D
	case 0xBB: // CMP E
	case 0xBC: // CMP H
	case 0xBD: // CMP L
		data = R8( op & 7 );
		goto cmp_comm;
	
	case 0xFE: // CMP IMM
		pc++;
	cmp_comm:
		op = rg.a;
		data = op - data;
	sub_set_flags:
		flags = ((op & 15) - (data & 15)) & h_flag;
		flags |= (data >> 4) & c_flag;
		flags |= n_flag;
		if ( data & 0xFF )
			goto loop;
		flags |= z_flag;
		goto loop;

	case 0x46: // LD B,(HL)
	case 0x4E: // LD C,(HL)
	case 0x56: // LD D,(HL)
	case 0x5E: // LD E,(HL)
	case 0x66: // LD H,(HL)
	case 0x6E: // LD L,(HL)
	case 0x7E:{// LD A,(HL)
		unsigned addr = rp.hl;
		READ_FAST( addr, R8( (op >> 3) & 7 ) );
		goto loop;
	}
	
	case 0xC4: // CNZ (next-most-common)
		pc += 2;
		if ( flags & z_flag )
			goto loop;
	call:
		pc -= 2;
	case 0xCD: // CALL (most-common)
		data = pc + 2;
		pc = GET_ADDR();
	push:
		sp = (sp - 1) & 0xFFFF;
		WRITE( sp, data >> 8 );
		sp = (sp - 1) & 0xFFFF;
		WRITE( sp, data & 0xFF );
		goto loop;
	
	case 0xC8: // RNZ (next-most-common)
		if ( !(flags & z_flag) )
			goto loop;
	case 0xC9: // RET (most common)
	ret:
		pc = READ( sp );
		pc += 0x100 * READ( sp + 1 );
		sp = (sp + 2) & 0xFFFF;
		goto loop;
	
	case 0x00: // NOP
	case 0x40: // LD B,B
	case 0x49: // LD C,C
	case 0x52: // LD D,D
	case 0x5B: // LD E,E
	case 0x64: // LD H,H
	case 0x6D: // LD L,L
	case 0x7F: // LD A,A
		goto loop;
	
// CB Instructions

	case 0xCB:
		pc++;
		// now data is the opcode
		switch ( data ) {
			
		{
			int temp;
		case 0x46: // BIT b,(HL)
		case 0x4E:
		case 0x56:
		case 0x5E:
		case 0x66:
		case 0x6E:
		case 0x76:
		case 0x7E:
			{
				unsigned addr = rp.hl;
				READ_FAST( addr, temp );
				goto bit_comm;
			}
		
		case 0x40: case 0x41: case 0x42: case 0x43: // BIT b,r
		case 0x44: case 0x45: case 0x47: case 0x48:
		case 0x49: case 0x4A: case 0x4B: case 0x4C:
		case 0x4D: case 0x4F: case 0x50: case 0x51:
		case 0x52: case 0x53: case 0x54: case 0x55:
		case 0x57: case 0x58: case 0x59: case 0x5A:
		case 0x5B: case 0x5C: case 0x5D: case 0x5F:
		case 0x60: case 0x61: case 0x62: case 0x63:
		case 0x64: case 0x65: case 0x67: case 0x68:
		case 0x69: case 0x6A: case 0x6B: case 0x6C:
		case 0x6D: case 0x6F: case 0x70: case 0x71:
		case 0x72: case 0x73: case 0x74: case 0x75:
		case 0x77: case 0x78: case 0x79: case 0x7A:
		case 0x7B: case 0x7C: case 0x7D: case 0x7F:
			temp = R8( data & 7 );
		bit_comm:
			int bit = (~data >> 3) & 7;
			flags &= ~n_flag;
			flags |= h_flag | z_flag;
			flags ^= (temp << bit) & z_flag;
			goto loop;
		}
		
		case 0x86: // RES b,(HL)
		case 0x8E:
		case 0x96:
		case 0x9E:
		case 0xA6:
		case 0xAE:
		case 0xB6:
		case 0xBE:
		case 0xC6: // SET b,(HL)
		case 0xCE:
		case 0xD6:
		case 0xDE:
		case 0xE6:
		case 0xEE:
		case 0xF6:
		case 0xFE: {
			int temp = READ( rp.hl );
			int bit = 1 << ((data >> 3) & 7);
			temp &= ~bit;
			if ( !(data & 0x40) )
				bit = 0;
			WRITE( rp.hl, temp | bit );
			goto loop;
		}
		
		case 0xC0: case 0xC1: case 0xC2: case 0xC3: // SET b,r
		case 0xC4: case 0xC5: case 0xC7: case 0xC8:
		case 0xC9: case 0xCA: case 0xCB: case 0xCC:
		case 0xCD: case 0xCF: case 0xD0: case 0xD1:
		case 0xD2: case 0xD3: case 0xD4: case 0xD5:
		case 0xD7: case 0xD8: case 0xD9: case 0xDA:
		case 0xDB: case 0xDC: case 0xDD: case 0xDF:
		case 0xE0: case 0xE1: case 0xE2: case 0xE3:
		case 0xE4: case 0xE5: case 0xE7: case 0xE8:
		case 0xE9: case 0xEA: case 0xEB: case 0xEC:
		case 0xED: case 0xEF: case 0xF0: case 0xF1:
		case 0xF2: case 0xF3: case 0xF4: case 0xF5:
		case 0xF7: case 0xF8: case 0xF9: case 0xFA:
		case 0xFB: case 0xFC: case 0xFD: case 0xFF:
			R8( data & 7 ) |= 1 << ((data >> 3) & 7);
			goto loop;

		case 0x80: case 0x81: case 0x82: case 0x83: // RES b,r
		case 0x84: case 0x85: case 0x87: case 0x88:
		case 0x89: case 0x8A: case 0x8B: case 0x8C:
		case 0x8D: case 0x8F: case 0x90: case 0x91:
		case 0x92: case 0x93: case 0x94: case 0x95:
		case 0x97: case 0x98: case 0x99: case 0x9A:
		case 0x9B: case 0x9C: case 0x9D: case 0x9F:
		case 0xA0: case 0xA1: case 0xA2: case 0xA3:
		case 0xA4: case 0xA5: case 0xA7: case 0xA8:
		case 0xA9: case 0xAA: case 0xAB: case 0xAC:
		case 0xAD: case 0xAF: case 0xB0: case 0xB1:
		case 0xB2: case 0xB3: case 0xB4: case 0xB5:
		case 0xB7: case 0xB8: case 0xB9: case 0xBA:
		case 0xBB: case 0xBC: case 0xBD: case 0xBF:
			R8( data & 7 ) &= ~(1 << ((data >> 3) & 7));
			goto loop;
		
		{
			int temp;
		case 0x36: // SWAP (HL)
			temp = READ( rp.hl );
			goto swap_comm;
		
		case 0x30: // SWAP B
		case 0x31: // SWAP C
		case 0x32: // SWAP D
		case 0x33: // SWAP E
		case 0x34: // SWAP H
		case 0x35: // SWAP L
		case 0x37: // SWAP A
			temp = R8( data & 7 );
		swap_comm:
			op = (temp >> 4) | (temp << 4);
			flags = 0;
			goto shift_comm;
		}
		
// Shift/Rotate

		case 0x06: // RLC (HL)
		case 0x16: // RL (HL)
		case 0x26: // SLA (HL)
			op = READ( rp.hl );
			goto rl_comm;
		
		case 0x20: case 0x21: case 0x22: case 0x23: case 0x24: case 0x25: case 0x27: // SLA A
		case 0x00: case 0x01: case 0x02: case 0x03: case 0x04: case 0x05: case 0x07: // RLC A
		case 0x10: case 0x11: case 0x12: case 0x13: case 0x14: case 0x15: case 0x17: // RL A
			op = R8( data & 7 );
			goto rl_comm;
		
		case 0x3E: // SRL (HL)
			data += 0x10; // bump up to 0x4n to avoid preserving sign bit
		case 0x1E: // RR (HL)
		case 0x0E: // RRC (HL)
		case 0x2E: // SRA (HL)
			op = READ( rp.hl );
			goto rr_comm;
		
		case 0x38: case 0x39: case 0x3A: case 0x3B: case 0x3C: case 0x3D: case 0x3F: // SRL A
			data += 0x10; // bump up to 0x4n
		case 0x18: case 0x19: case 0x1A: case 0x1B: case 0x1C: case 0x1D: case 0x1F: // RR A
		case 0x08: case 0x09: case 0x0A: case 0x0B: case 0x0C: case 0x0D: case 0x0F: // RRC A
		case 0x28: case 0x29: case 0x2A: case 0x2B: case 0x2C: case 0x2D: case 0x2F: // SRA A
			op = R8( data & 7 );
			goto rr_comm;
		
	} // CB op
	assert( false ); // unhandled CB op

	case 0x07: // RLCA
	case 0x17: // RLA
		data = op;
		op = rg.a;
	rl_comm:
		op <<= 1;
		op |= ((data & flags) >> 4) & 1; // RL and carry is set
		flags = (op >> 4) & c_flag; // C = bit shifted out
		if ( data < 0x10 ) // RLC
			op |= op >> 8;
		// SLA doesn't fill lower bit
		goto shift_comm;
	
	case 0x0F: // RRCA
	case 0x1F: // RRA
		data = op;
		op = rg.a;
	rr_comm:
		op |= (data & flags) << 4; // RR and carry is set
		flags = (op << 4) & c_flag; // C = bit shifted out
		if ( data < 0x10 ) // RRC
			op |= op << 8;
		op >>= 1;
		if ( data & 0x20 ) // SRA propagates sign bit
			op |= (op << 1) & 0x80;
	shift_comm:
		data &= 7;
		if ( !(op & 0xFF) )
			flags |= z_flag;
		if ( data == 6 )
			goto write_hl_op_ff;
		R8( data ) = op;
		goto loop;

// Load

	case 0x70: // LD (HL),B
	case 0x71: // LD (HL),C
	case 0x72: // LD (HL),D
	case 0x73: // LD (HL),E
	case 0x74: // LD (HL),H
	case 0x75: // LD (HL),L
	case 0x77: // LD (HL),A
		op = R8( op & 7 );
	write_hl_op_ff:
		WRITE( rp.hl, op & 0xFF );
		goto loop;

	case 0x41: case 0x42: case 0x43: case 0x44: case 0x45: case 0x47: // LD r,r
	case 0x48: case 0x4A: case 0x4B: case 0x4C: case 0x4D: case 0x4F:
	case 0x50: case 0x51: case 0x53: case 0x54: case 0x55: case 0x57:
	case 0x58: case 0x59: case 0x5A: case 0x5C: case 0x5D: case 0x5F:
	case 0x60: case 0x61: case 0x62: case 0x63: case 0x65: case 0x67:
	case 0x68: case 0x69: case 0x6A: case 0x6B: case 0x6C: case 0x6F:
	case 0x78: case 0x79: case 0x7A: case 0x7B: case 0x7C: case 0x7D:
		R8( (op >> 3) & 7 ) = R8( op & 7 );
		goto loop;

	case 0x08: // LD IND16,SP
		data = GET_ADDR();
		pc += 2;
		WRITE( data, sp&0xFF );
		data++;
		WRITE( data, sp >> 8 );
		goto loop;
	
	case 0xF9: // LD SP,HL
		sp = rp.hl;
		goto loop;

	case 0x31: // LD SP,IMM
		sp = GET_ADDR();
		pc += 2;
		goto loop;
	
	case 0x01: // LD BC,IMM
	case 0x11: // LD DE,IMM
		r16 [op >> 4] = GET_ADDR();
		pc += 2;
		goto loop;
	
	{
		unsigned temp;
	case 0xE0: // LD (0xFF00+imm),A
		temp = data | 0xFF00;
		pc++;
		goto write_data_rg_a;
	
	case 0xE2: // LD (0xFF00+C),A
		temp = rg.c | 0xFF00;
		goto write_data_rg_a;

	case 0x32: // LD (HL-),A
		temp = rp.hl;
		rp.hl = temp - 1;
		goto write_data_rg_a;
	
	case 0x02: // LD (BC),A
		temp = rp.bc;
		goto write_data_rg_a;
	
	case 0x12: // LD (DE),A
		temp = rp.de;
		goto write_data_rg_a;
	
	case 0x22: // LD (HL+),A
		temp = rp.hl;
		rp.hl = temp + 1;
		goto write_data_rg_a;
		
	case 0xEA: // LD IND16,A (common)
		temp = GET_ADDR();
		pc += 2;
	write_data_rg_a:
		WRITE( temp, rg.a );
		goto loop;
	}
	
	case 0x06: // LD B,IMM
		rg.b = data;
		pc++;
		goto loop;
	
	case 0x0E: // LD C,IMM
		rg.c = data;
		pc++;
		goto loop;
	
	case 0x16: // LD D,IMM
		rg.d = data;
		pc++;
		goto loop;
	
	case 0x1E: // LD E,IMM
		rg.e = data;
		pc++;
		goto loop;
	
	case 0x26: // LD H,IMM
		rg.h = data;
		pc++;
		goto loop;
	
	case 0x2E: // LD L,IMM
		rg.l = data;
		pc++;
		goto loop;
	
	case 0x36: // LD (HL),IMM
		WRITE( rp.hl, data );
		pc++;
		goto loop;
	
	case 0x3E: // LD A,IMM
		rg.a = data;
		pc++;
		goto loop;

// Increment/Decrement

	case 0x03: // INC BC
	case 0x13: // INC DE
	case 0x23: // INC HL
		r16 [op >> 4]++;
		goto loop;
	
	case 0x33: // INC SP
		sp = (sp + 1) & 0xFFFF;
		goto loop;

	case 0x0B: // DEC BC
	case 0x1B: // DEC DE
	case 0x2B: // DEC HL
		r16 [op >> 4]--;
		goto loop;
	
	case 0x3B: // DEC SP
		sp = (sp - 1) & 0xFFFF;
		goto loop;
	
	case 0x34: // INC (HL)
		op = rp.hl;
		data = READ( op );
		data++;
		WRITE( op, data & 0xFF );
		goto inc_comm;
	
	case 0x04: // INC B
	case 0x0C: // INC C (common)
	case 0x14: // INC D
	case 0x1C: // INC E
	case 0x24: // INC H
	case 0x2C: // INC L
	case 0x3C: // INC A
		op = (op >> 3) & 7;
		R8( op ) = data = R8( op ) + 1;
	inc_comm:
		flags = (flags & c_flag) | (((data & 15) - 1) & h_flag) | ((data >> 1) & z_flag);
		goto loop;
	
	case 0x35: // DEC (HL)
		op = rp.hl;
		data = READ( op );
		data--;
		WRITE( op, data & 0xFF );
		goto dec_comm;
	
	case 0x05: // DEC B
	case 0x0D: // DEC C
	case 0x15: // DEC D
	case 0x1D: // DEC E
	case 0x25: // DEC H
	case 0x2D: // DEC L
	case 0x3D: // DEC A
		op = (op >> 3) & 7;
		data = R8( op ) - 1;
		R8( op ) = data;
	dec_comm:
		flags = (flags & c_flag) | n_flag | (((data & 15) + 0x31) & h_flag);
		if ( data & 0xFF )
			goto loop;
		flags |= z_flag;
		goto loop;

// Add 16-bit

	{
		blargg_ulong temp; // need more than 16 bits for carry
		unsigned prev;
		
	case 0xF8: // LD HL,SP+imm
		temp = int8_t (data); // sign-extend to 16 bits
		pc++;
		flags = 0;
		temp += sp;
		prev = sp;
		goto add_16_hl;
	
	case 0xE8: // ADD SP,IMM
		temp = int8_t (data); // sign-extend to 16 bits
		pc++;
		flags = 0;
		temp += sp;
		prev = sp;
		sp = temp & 0xFFFF;
		goto add_16_comm;

	case 0x39: // ADD HL,SP
		temp = sp;
		goto add_hl_comm;
	
	case 0x09: // ADD HL,BC
	case 0x19: // ADD HL,DE
	case 0x29: // ADD HL,HL
		temp = r16 [op >> 4];
	add_hl_comm:
		prev = rp.hl;
		temp += prev;
		flags &= z_flag;
	add_16_hl:
		rp.hl = temp;
	add_16_comm:
		flags |= (temp >> 12) & c_flag;
		flags |= (((temp & 0x0FFF) - (prev & 0x0FFF)) >> 7) & h_flag;
		goto loop;
	}
	
	case 0x86: // ADD (HL)
		data = READ( rp.hl );
		goto add_comm;
	
	case 0x80: // ADD B
	case 0x81: // ADD C
	case 0x82: // ADD D
	case 0x83: // ADD E
	case 0x84: // ADD H
	case 0x85: // ADD L
	case 0x87: // ADD A
		data = R8( op & 7 );
		goto add_comm;
	
	case 0xC6: // ADD IMM
		pc++;
	add_comm:
		flags = rg.a;
		data += flags;
		flags = ((data & 15) - (flags & 15)) & h_flag;
		flags |= (data >> 4) & c_flag;
		rg.a = data;
		if ( data & 0xFF )
			goto loop;
		flags |= z_flag;
		goto loop;

// Add/Subtract

	case 0x8E: // ADC (HL)
		data = READ( rp.hl );
		goto adc_comm;
	
	case 0x88: // ADC B
	case 0x89: // ADC C
	case 0x8A: // ADC D
	case 0x8B: // ADC E
	case 0x8C: // ADC H
	case 0x8D: // ADC L
	case 0x8F: // ADC A
		data = R8( op & 7 );
		goto adc_comm;
	
	case 0xCE: // ADC IMM
		pc++;
	adc_comm:
		data += (flags >> 4) & 1;
		data &= 0xFF; // to do: does carry get set when sum + carry = 0x100?
		goto add_comm;

	case 0x96: // SUB (HL)
		data = READ( rp.hl );
		goto sub_comm;
	
	case 0x90: // SUB B
	case 0x91: // SUB C
	case 0x92: // SUB D
	case 0x93: // SUB E
	case 0x94: // SUB H
	case 0x95: // SUB L
	case 0x97: // SUB A
		data = R8( op & 7 );
		goto sub_comm;
	
	case 0xD6: // SUB IMM
		pc++;
	sub_comm:
		op = rg.a;
		data = op - data;
		rg.a = data;
		goto sub_set_flags;

	case 0x9E: // SBC (HL)
		data = READ( rp.hl );
		goto sbc_comm;
	
	case 0x98: // SBC B
	case 0x99: // SBC C
	case 0x9A: // SBC D
	case 0x9B: // SBC E
	case 0x9C: // SBC H
	case 0x9D: // SBC L
	case 0x9F: // SBC A
		data = R8( op & 7 );
		goto sbc_comm;
	
	case 0xDE: // SBC IMM
		pc++;
	sbc_comm:
		data += (flags >> 4) & 1;
		data &= 0xFF; // to do: does carry get set when sum + carry = 0x100?
		goto sub_comm;

// Logical

	case 0xA0: // AND B
	case 0xA1: // AND C
	case 0xA2: // AND D
	case 0xA3: // AND E
	case 0xA4: // AND H
	case 0xA5: // AND L
		data = R8( op & 7 );
		goto and_comm;
	
	case 0xA6: // AND (HL)
		data = READ( rp.hl );
		pc--;
	case 0xE6: // AND IMM
		pc++;
	and_comm:
		rg.a &= data;
	case 0xA7: // AND A
		flags = h_flag | (((rg.a - 1) >> 1) & z_flag);
		goto loop;

	case 0xB0: // OR B
	case 0xB1: // OR C
	case 0xB2: // OR D
	case 0xB3: // OR E
	case 0xB4: // OR H
	case 0xB5: // OR L
		data = R8( op & 7 );
		goto or_comm;
	
	case 0xB6: // OR (HL)
		data = READ( rp.hl );
		pc--;
	case 0xF6: // OR IMM
		pc++;
	or_comm:
		rg.a |= data;
	case 0xB7: // OR A
		flags = ((rg.a - 1) >> 1) & z_flag;
		goto loop;

	case 0xA8: // XOR B
	case 0xA9: // XOR C
	case 0xAA: // XOR D
	case 0xAB: // XOR E
	case 0xAC: // XOR H
	case 0xAD: // XOR L
		data = R8( op & 7 );
		goto xor_comm;
	
	case 0xAE: // XOR (HL)
		data = READ( rp.hl );
		pc--;
	case 0xEE: // XOR IMM
		pc++;
	xor_comm:
		data ^= rg.a;
		rg.a = data;
		data--;
		flags = (data >> 1) & z_flag;
		goto loop;
	
	case 0xAF: // XOR A
		rg.a = 0;
		flags = z_flag;
		goto loop;

// Stack

	case 0xF1: // POP FA
	case 0xC1: // POP BC
	case 0xD1: // POP DE
	case 0xE1: // POP HL (common)
		data = READ( sp );
		r16 [(op >> 4) & 3] = data + 0x100 * READ( sp + 1 );
		sp = (sp + 2) & 0xFFFF;
		if ( op != 0xF1 )
			goto loop;
		flags = rg.flags & 0xF0;
		goto loop;
	
	case 0xC5: // PUSH BC
		data = rp.bc;
		goto push;
	
	case 0xD5: // PUSH DE
		data = rp.de;
		goto push;
	
	case 0xE5: // PUSH HL
		data = rp.hl;
		goto push;
	
	case 0xF5: // PUSH FA
		data = (flags << 8) | rg.a;
		goto push;

// Flow control
	
	case 0xFF:
		if ( pc == idle_addr + 1 )
			goto stop;
	case 0xC7: case 0xCF: case 0xD7: case 0xDF:  // RST
	case 0xE7: case 0xEF: case 0xF7:
		data = pc;
		pc = (op & 0x38) + rst_base;
		goto push;
	
	case 0xCC: // CZ
		pc += 2;
		if ( flags & z_flag )
			goto call;
		goto loop;
	
	case 0xD4: // CNC
		pc += 2;
		if ( !(flags & c_flag) )
			goto call;
		goto loop;
	
	case 0xDC: // CC
		pc += 2;
		if ( flags & c_flag )
			goto call;
		goto loop;

	case 0xD9: // RETI
		//interrupts_enabled = 1;
		goto ret;
	
	case 0xC0: // RZ
		if ( !(flags & z_flag) )
			goto ret;
		goto loop;
	
	case 0xD0: // RNC
		if ( !(flags & c_flag) )
			goto ret;
		goto loop;
	
	case 0xD8: // RC
		if ( flags & c_flag )
			goto ret;
		goto loop;

	case 0x18: // JR
		BRANCH( true )
	
	case 0x30: // JR NC
		BRANCH( !(flags & c_flag) )
	
	case 0x38: // JR C
		BRANCH( flags & c_flag )
	
	case 0xE9: // JP_HL
		pc = rp.hl;
		goto loop;

	case 0xC3: // JP (next-most-common)
		pc = GET_ADDR();
		goto loop;
	
	case 0xC2: // JP NZ
		pc += 2;
		if ( !(flags & z_flag) )
			goto jp_taken;
		goto loop;
	
	case 0xCA: // JP Z (most common)
		pc += 2;
		if ( !(flags & z_flag) )
			goto loop;
	jp_taken:
		pc -= 2;
		pc = GET_ADDR();
		goto loop;
	
	case 0xD2: // JP NC
		pc += 2;
		if ( !(flags & c_flag) )
			goto jp_taken;
		goto loop;
	
	case 0xDA: // JP C
		pc += 2;
		if ( flags & c_flag )
			goto jp_taken;
		goto loop;

// Flags

	case 0x2F: // CPL
		rg.a = ~rg.a;
		flags |= n_flag | h_flag;
		goto loop;

	case 0x3F: // CCF
		flags = (flags ^ c_flag) & ~(n_flag | h_flag);
		goto loop;

	case 0x37: // SCF
		flags = (flags | c_flag) & ~(n_flag | h_flag);
		goto loop;

	case 0xF3: // DI
		//interrupts_enabled = 0;
		goto loop;

	case 0xFB: // EI
		//interrupts_enabled = 1;
		goto loop;

// Special

	case 0xDD: case 0xD3: case 0xDB: case 0xE3: case 0xE4: // ?
	case 0xEB: case 0xEC: case 0xF4: case 0xFD: case 0xFC:
	case 0x10: // STOP
	case 0x27: // DAA (I'll have to implement this eventually...)
	case 0xBF:
	case 0xED: // Z80 prefix
	case 0x76: // HALT
		s.remain++;
		goto stop;
	}
	
	// If this fails then the case above is missing an opcode
	assert( false );
	
stop:
	pc--;
	
	// copy state back
	STATIC_CAST(core_regs_t&,r) = rg;
	r.pc = pc;
	r.sp = sp;
	r.flags = flags;
	
	this->state = &state_;
	memcpy( &this->state_, &s, sizeof this->state_ );
	
	return s.remain > 0;
}
