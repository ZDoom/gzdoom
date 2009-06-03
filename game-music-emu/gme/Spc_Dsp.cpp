// snes_spc 0.9.0. http://www.slack.net/~ant/

#include "Spc_Dsp.h"

#include "blargg_endian.h"
#include <string.h>

/* Copyright (C) 2007 Shay Green. This module is free software; you
can redistribute it and/or modify it under the terms of the GNU Lesser
General Public License as published by the Free Software Foundation; either
version 2.1 of the License, or (at your option) any later version. This
module is distributed in the hope that it will be useful, but WITHOUT ANY
WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU Lesser General Public License for more
details. You should have received a copy of the GNU Lesser General Public
License along with this module; if not, write to the Free Software Foundation,
Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA */

#include "blargg_source.h"

#ifdef BLARGG_ENABLE_OPTIMIZER
	#include BLARGG_ENABLE_OPTIMIZER
#endif

#if INT_MAX < 0x7FFFFFFF
	#error "Requires that int type have at least 32 bits"
#endif


// TODO: add to blargg_endian.h
#define GET_LE16SA( addr )      ((BOOST::int16_t) GET_LE16( addr ))
#define GET_LE16A( addr )       GET_LE16( addr )
#define SET_LE16A( addr, data ) SET_LE16( addr, data )

static BOOST::uint8_t const initial_regs [Spc_Dsp::register_count] =
{
	0x45,0x8B,0x5A,0x9A,0xE4,0x82,0x1B,0x78,0x00,0x00,0xAA,0x96,0x89,0x0E,0xE0,0x80,
	0x2A,0x49,0x3D,0xBA,0x14,0xA0,0xAC,0xC5,0x00,0x00,0x51,0xBB,0x9C,0x4E,0x7B,0xFF,
	0xF4,0xFD,0x57,0x32,0x37,0xD9,0x42,0x22,0x00,0x00,0x5B,0x3C,0x9F,0x1B,0x87,0x9A,
	0x6F,0x27,0xAF,0x7B,0xE5,0x68,0x0A,0xD9,0x00,0x00,0x9A,0xC5,0x9C,0x4E,0x7B,0xFF,
	0xEA,0x21,0x78,0x4F,0xDD,0xED,0x24,0x14,0x00,0x00,0x77,0xB1,0xD1,0x36,0xC1,0x67,
	0x52,0x57,0x46,0x3D,0x59,0xF4,0x87,0xA4,0x00,0x00,0x7E,0x44,0x9C,0x4E,0x7B,0xFF,
	0x75,0xF5,0x06,0x97,0x10,0xC3,0x24,0xBB,0x00,0x00,0x7B,0x7A,0xE0,0x60,0x12,0x0F,
	0xF7,0x74,0x1C,0xE5,0x39,0x3D,0x73,0xC1,0x00,0x00,0x7A,0xB3,0xFF,0x4E,0x7B,0xFF
};

// if ( io < -32768 ) io = -32768;
// if ( io >  32767 ) io =  32767;
#define CLAMP16( io )\
{\
	if ( (int16_t) io != io )\
		io = (io >> 31) ^ 0x7FFF;\
}

// Access global DSP register
#define REG(n)      m.regs [r_##n]

// Access voice DSP register
#define VREG(r,n)   r [v_##n]

#define WRITE_SAMPLES( l, r, out ) \
{\
	out [0] = l;\
	out [1] = r;\
	out += 2;\
	if ( out >= m.out_end )\
	{\
		check( out == m.out_end );\
		check( m.out_end != &m.extra [extra_size] || \
			(m.extra <= m.out_begin && m.extra < &m.extra [extra_size]) );\
		out       = m.extra;\
		m.out_end = &m.extra [extra_size];\
	}\
}\

void Spc_Dsp::set_output( sample_t* out, int size )
{
	require( (size & 1) == 0 ); // must be even
	if ( !out )
	{
		out  = m.extra;
		size = extra_size;
	}
	m.out_begin = out;
	m.out       = out;
	m.out_end   = out + size;
}

// Volume registers and efb are signed! Easy to forget int8_t cast.
// Prefixes are to avoid accidental use of locals with same names.

// Interleved gauss table (to improve cache coherency)
// interleved_gauss [i] = gauss [(i & 1) * 256 + 255 - (i >> 1 & 0xFF)]
static short const interleved_gauss [512] =
{
 370,1305, 366,1305, 362,1304, 358,1304, 354,1304, 351,1304, 347,1304, 343,1303,
 339,1303, 336,1303, 332,1302, 328,1302, 325,1301, 321,1300, 318,1300, 314,1299,
 311,1298, 307,1297, 304,1297, 300,1296, 297,1295, 293,1294, 290,1293, 286,1292,
 283,1291, 280,1290, 276,1288, 273,1287, 270,1286, 267,1284, 263,1283, 260,1282,
 257,1280, 254,1279, 251,1277, 248,1275, 245,1274, 242,1272, 239,1270, 236,1269,
 233,1267, 230,1265, 227,1263, 224,1261, 221,1259, 218,1257, 215,1255, 212,1253,
 210,1251, 207,1248, 204,1246, 201,1244, 199,1241, 196,1239, 193,1237, 191,1234,
 188,1232, 186,1229, 183,1227, 180,1224, 178,1221, 175,1219, 173,1216, 171,1213,
 168,1210, 166,1207, 163,1205, 161,1202, 159,1199, 156,1196, 154,1193, 152,1190,
 150,1186, 147,1183, 145,1180, 143,1177, 141,1174, 139,1170, 137,1167, 134,1164,
 132,1160, 130,1157, 128,1153, 126,1150, 124,1146, 122,1143, 120,1139, 118,1136,
 117,1132, 115,1128, 113,1125, 111,1121, 109,1117, 107,1113, 106,1109, 104,1106,
 102,1102, 100,1098,  99,1094,  97,1090,  95,1086,  94,1082,  92,1078,  90,1074,
  89,1070,  87,1066,  86,1061,  84,1057,  83,1053,  81,1049,  80,1045,  78,1040,
  77,1036,  76,1032,  74,1027,  73,1023,  71,1019,  70,1014,  69,1010,  67,1005,
  66,1001,  65, 997,  64, 992,  62, 988,  61, 983,  60, 978,  59, 974,  58, 969,
  56, 965,  55, 960,  54, 955,  53, 951,  52, 946,  51, 941,  50, 937,  49, 932,
  48, 927,  47, 923,  46, 918,  45, 913,  44, 908,  43, 904,  42, 899,  41, 894,
  40, 889,  39, 884,  38, 880,  37, 875,  36, 870,  36, 865,  35, 860,  34, 855,
  33, 851,  32, 846,  32, 841,  31, 836,  30, 831,  29, 826,  29, 821,  28, 816,
  27, 811,  27, 806,  26, 802,  25, 797,  24, 792,  24, 787,  23, 782,  23, 777,
  22, 772,  21, 767,  21, 762,  20, 757,  20, 752,  19, 747,  19, 742,  18, 737,
  17, 732,  17, 728,  16, 723,  16, 718,  15, 713,  15, 708,  15, 703,  14, 698,
  14, 693,  13, 688,  13, 683,  12, 678,  12, 674,  11, 669,  11, 664,  11, 659,
  10, 654,  10, 649,  10, 644,   9, 640,   9, 635,   9, 630,   8, 625,   8, 620,
   8, 615,   7, 611,   7, 606,   7, 601,   6, 596,   6, 592,   6, 587,   6, 582,
   5, 577,   5, 573,   5, 568,   5, 563,   4, 559,   4, 554,   4, 550,   4, 545,
   4, 540,   3, 536,   3, 531,   3, 527,   3, 522,   3, 517,   2, 513,   2, 508,
   2, 504,   2, 499,   2, 495,   2, 491,   2, 486,   1, 482,   1, 477,   1, 473,
   1, 469,   1, 464,   1, 460,   1, 456,   1, 451,   1, 447,   1, 443,   1, 439,
   0, 434,   0, 430,   0, 426,   0, 422,   0, 418,   0, 414,   0, 410,   0, 405,
   0, 401,   0, 397,   0, 393,   0, 389,   0, 385,   0, 381,   0, 378,   0, 374,
};


//// Counters

#define RATE( rate, div )\
	(rate >= div ? rate / div * 8 - 1 : rate - 1)

static unsigned const counter_mask [32] =
{
	RATE(   2,2), RATE(2048,4), RATE(1536,3),
	RATE(1280,5), RATE(1024,4), RATE( 768,3),
	RATE( 640,5), RATE( 512,4), RATE( 384,3),
	RATE( 320,5), RATE( 256,4), RATE( 192,3),
	RATE( 160,5), RATE( 128,4), RATE(  96,3),
	RATE(  80,5), RATE(  64,4), RATE(  48,3),
	RATE(  40,5), RATE(  32,4), RATE(  24,3),
	RATE(  20,5), RATE(  16,4), RATE(  12,3),
	RATE(  10,5), RATE(   8,4), RATE(   6,3),
	RATE(   5,5), RATE(   4,4), RATE(   3,3),
	              RATE(   2,4),
	              RATE(   1,4)
};
#undef RATE

inline void Spc_Dsp::init_counter()
{
	// counters start out with this synchronization
	m.counters [0] =     1;
	m.counters [1] =     0;
	m.counters [2] = -0x20;
	m.counters [3] =  0x0B;
	
	int n = 2;
	for ( int i = 1; i < 32; i++ )
	{
		m.counter_select [i] = &m.counters [n];
		if ( !--n )
			n = 3;
	}
	m.counter_select [ 0] = &m.counters [0];
	m.counter_select [30] = &m.counters [2];
}

inline void Spc_Dsp::run_counter( int i )
{
	int n = m.counters [i];
	if ( !(n-- & 7) )
		n -= 6 - i;
	m.counters [i] = n;
}

#define READ_COUNTER( rate )\
	(*m.counter_select [rate] & counter_mask [rate])


//// Emulation

void Spc_Dsp::run( int clock_count )
{
	int new_phase = m.phase + clock_count;
	int count = new_phase >> 5;
	m.phase = new_phase & 31;
	if ( !count )
		return;
	
	uint8_t* const ram = m.ram;
	uint8_t const* const dir = &ram [REG(dir) * 0x100];
	int const slow_gaussian = (REG(pmon) >> 1) | REG(non);
	int const noise_rate = REG(flg) & 0x1F;
	
	// Global volume
	int mvoll = (int8_t) REG(mvoll);
	int mvolr = (int8_t) REG(mvolr);
	if ( mvoll * mvolr < m.surround_threshold )
		mvoll = -mvoll; // eliminate surround
	
	do
	{
		// KON/KOFF reading
		if ( (m.every_other_sample ^= 1) != 0 )
		{
			m.new_kon &= ~m.kon;
			m.kon    = m.new_kon;
			m.t_koff = REG(koff); 
		}
		
		run_counter( 1 );
		run_counter( 2 );
		run_counter( 3 );
		
		// Noise
		if ( !READ_COUNTER( noise_rate ) )
		{
			int feedback = (m.noise << 13) ^ (m.noise << 14);
			m.noise = (feedback & 0x4000) ^ (m.noise >> 1);
		}
		
		// Voices
		int pmon_input = 0;
		int main_out_l = 0;
		int main_out_r = 0;
		int echo_out_l = 0;
		int echo_out_r = 0;
		voice_t* v = m.voices;
		uint8_t* v_regs = m.regs;
		int vbit = 1;
		do
		{
			#define SAMPLE_PTR(i) GET_LE16A( &dir [VREG(v_regs,srcn) * 4 + i * 2] )
			
			int brr_header = ram [v->brr_addr];
			int kon_delay = v->kon_delay;
			
			// Pitch
			int pitch = GET_LE16A( &VREG(v_regs,pitchl) ) & 0x3FFF;
			if ( REG(pmon) & vbit )
				pitch += ((pmon_input >> 5) * pitch) >> 10;
			
			// KON phases
			if ( --kon_delay >= 0 )
			{
				v->kon_delay = kon_delay;
				
				// Get ready to start BRR decoding on next sample
				if ( kon_delay == 4 )
				{
					v->brr_addr   = SAMPLE_PTR( 0 );
					v->brr_offset = 1;
					v->buf_pos    = v->buf;
					brr_header    = 0; // header is ignored on this sample
				}
				
				// Envelope is never run during KON
				v->env        = 0;
				v->hidden_env = 0;
				
				// Disable BRR decoding until last three samples
				v->interp_pos = (kon_delay & 3 ? 0x4000 : 0);
				
				// Pitch is never added during KON
				pitch = 0;
			}
			
			int env = v->env;
			
			// Gaussian interpolation
			{
				int output = 0;
				VREG(v_regs,envx) = (uint8_t) (env >> 4);
				if ( env )
				{
					// Make pointers into gaussian based on fractional position between samples
					int offset = (unsigned) v->interp_pos >> 3 & 0x1FE;
					short const* fwd = interleved_gauss       + offset;
					short const* rev = interleved_gauss + 510 - offset; // mirror left half of gaussian
					
					int const* in = &v->buf_pos [(unsigned) v->interp_pos >> 12];
					
					if ( !(slow_gaussian & vbit) ) // 99%
					{
						// Faster approximation when exact sample value isn't necessary for pitch mod
						output = (fwd [0] * in [0] +
						          fwd [1] * in [1] +
						          rev [1] * in [2] +
						          rev [0] * in [3]) >> 11;
						output = (output * env) >> 11;
					}
					else
					{
						output = (int16_t) (m.noise * 2);
						if ( !(REG(non) & vbit) )
						{
							output  = (fwd [0] * in [0]) >> 11;
							output += (fwd [1] * in [1]) >> 11;
							output += (rev [1] * in [2]) >> 11;
							output = (int16_t) output;
							output += (rev [0] * in [3]) >> 11;
							
							CLAMP16( output );
							output &= ~1;
						}
						output = (output * env) >> 11 & ~1;
					}
					
					// Output
					int l = output * v->volume [0];
					int r = output * v->volume [1];
					
					main_out_l += l;
					main_out_r += r;
					
					if ( REG(eon) & vbit )
					{
						echo_out_l += l;
						echo_out_r += r;
					}
				}
				
				pmon_input = output;
				VREG(v_regs,outx) = (uint8_t) (output >> 8);
			}
			
			// Soft reset or end of sample
			if ( REG(flg) & 0x80 || (brr_header & 3) == 1 )
			{
				v->env_mode = env_release;
				env         = 0;
			}
			
			if ( m.every_other_sample )
			{
				// KOFF
				if ( m.t_koff & vbit )
					v->env_mode = env_release;
				
				// KON
				if ( m.kon & vbit )
				{
					v->kon_delay = 5;
					v->env_mode  = env_attack;
					REG(endx) &= ~vbit;
				}
			}
			
			// Envelope
			if ( !v->kon_delay )
			{
				if ( v->env_mode == env_release ) // 97%
				{
					env -= 0x8;
					v->env = env;
					if ( env <= 0 )
					{
						v->env = 0;
						goto skip_brr; // no BRR decoding for you!
					}
				}
				else // 3%
				{
					int rate;
					int const adsr0 = VREG(v_regs,adsr0);
					int env_data = VREG(v_regs,adsr1);
					if ( adsr0 >= 0x80 ) // 97% ADSR
					{
						if ( v->env_mode > env_decay ) // 89%
						{
							env--;
							env -= env >> 8;
							rate = env_data & 0x1F;
							
							// optimized handling
							v->hidden_env = env;
							if ( READ_COUNTER( rate ) )
								goto exit_env;
							v->env = env;
							goto exit_env;
						}
						else if ( v->env_mode == env_decay )
						{
							env--;
							env -= env >> 8;
							rate = (adsr0 >> 3 & 0x0E) + 0x10;
						}
						else // env_attack
						{
							rate = (adsr0 & 0x0F) * 2 + 1;
							env += rate < 31 ? 0x20 : 0x400;
						}
					}
					else // GAIN
					{
						int mode;
						env_data = VREG(v_regs,gain);
						mode = env_data >> 5;
						if ( mode < 4 ) // direct
						{
							env = env_data * 0x10;
							rate = 31;
						}
						else
						{
							rate = env_data & 0x1F;
							if ( mode == 4 ) // 4: linear decrease
							{
								env -= 0x20;
							}
							else if ( mode < 6 ) // 5: exponential decrease
							{
								env--;
								env -= env >> 8;
							}
							else // 6,7: linear increase
							{
								env += 0x20;
								if ( mode > 6 && (unsigned) v->hidden_env >= 0x600 )
									env += 0x8 - 0x20; // 7: two-slope linear increase
							}
						}
					}
					
					// Sustain level
					if ( (env >> 8) == (env_data >> 5) && v->env_mode == env_decay )
						v->env_mode = env_sustain;
					
					v->hidden_env = env;
					
					// unsigned cast because linear decrease going negative also triggers this
					if ( (unsigned) env > 0x7FF )
					{
						env = (env < 0 ? 0 : 0x7FF);
						if ( v->env_mode == env_attack )
							v->env_mode = env_decay;
					}
					
					if ( !READ_COUNTER( rate ) )
						v->env = env; // nothing else is controlled by the counter
				}
			}
		exit_env:
			
			{
				// Apply pitch
				int old_pos = v->interp_pos;
				int interp_pos = (old_pos & 0x3FFF) + pitch;
				if ( interp_pos > 0x7FFF )
					interp_pos = 0x7FFF;
				v->interp_pos = interp_pos;
				
				// BRR decode if necessary
				if ( old_pos >= 0x4000 )
				{
					// Arrange the four input nybbles in 0xABCD order for easy decoding
					int nybbles = ram [(v->brr_addr + v->brr_offset) & 0xFFFF] * 0x100 +
							ram [(v->brr_addr + v->brr_offset + 1) & 0xFFFF];
					
					// Advance read position
					int const brr_block_size = 9;
					int brr_offset = v->brr_offset;
					if ( (brr_offset += 2) >= brr_block_size )
					{
						// Next BRR block
						int brr_addr = (v->brr_addr + brr_block_size) & 0xFFFF;
						assert( brr_offset == brr_block_size );
						if ( brr_header & 1 )
						{
							brr_addr = SAMPLE_PTR( 1 );
							if ( !v->kon_delay )
								REG(endx) |= vbit;
						}
						v->brr_addr = brr_addr;
						brr_offset  = 1;
					}
					v->brr_offset = brr_offset;
					
					// Decode
					
					// 0: >>1  1: <<0  2: <<1 ... 12: <<11  13-15: >>4 <<11
					static unsigned char const shifts [16 * 2] = {
						13,12,12,12,12,12,12,12,12,12,12, 12, 12, 16, 16, 16,
						 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 11, 11, 11
					};
					int const scale = brr_header >> 4;
					int const right_shift = shifts [scale];
					int const left_shift  = shifts [scale + 16];
					
					// Write to next four samples in circular buffer
					int* pos = v->buf_pos;
					int* end;
					
					// Decode four samples
					for ( end = pos + 4; pos < end; pos++, nybbles <<= 4 )
					{
						// Extract upper nybble and scale appropriately
						int s = ((int16_t) nybbles >> right_shift) << left_shift;
						
						// Apply IIR filter (8 is the most commonly used)
						int const filter = brr_header & 0x0C;
						int const p1 = pos [brr_buf_size - 1];
						int const p2 = pos [brr_buf_size - 2] >> 1;
						if ( filter >= 8 )
						{
							s += p1;
							s -= p2;
							if ( filter == 8 ) // s += p1 * 0.953125 - p2 * 0.46875
							{
								s += p2 >> 4;
								s += (p1 * -3) >> 6;
							}
							else // s += p1 * 0.8984375 - p2 * 0.40625
							{
								s += (p1 * -13) >> 7;
								s += (p2 * 3) >> 4;
							}
						}
						else if ( filter ) // s += p1 * 0.46875
						{
							s += p1 >> 1;
							s += (-p1) >> 5;
						}
						
						// Adjust and write sample
						CLAMP16( s );
						s = (int16_t) (s * 2);
						pos [brr_buf_size] = pos [0] = s; // second copy simplifies wrap-around
					}
					
					if ( pos >= &v->buf [brr_buf_size] )
						pos = v->buf;
					v->buf_pos = pos;
				}
			}
skip_brr:
			// Next voice
			vbit <<= 1;
			v_regs += 0x10;
			v++;
		}
		while ( vbit < 0x100 );
		
		// Echo position
		int echo_offset = m.echo_offset;
		uint8_t* const echo_ptr = &ram [(REG(esa) * 0x100 + echo_offset) & 0xFFFF];
		if ( !echo_offset )
			m.echo_length = (REG(edl) & 0x0F) * 0x800;
		echo_offset += 4;
		if ( echo_offset >= m.echo_length )
			echo_offset = 0;
		m.echo_offset = echo_offset;
		
		// FIR
		int echo_in_l = GET_LE16SA( echo_ptr + 0 );
		int echo_in_r = GET_LE16SA( echo_ptr + 2 );
		
		int (*echo_hist_pos) [2] = m.echo_hist_pos;
		if ( ++echo_hist_pos >= &m.echo_hist [echo_hist_size] )
			echo_hist_pos = m.echo_hist;
		m.echo_hist_pos = echo_hist_pos;
		
		echo_hist_pos [0] [0] = echo_hist_pos [8] [0] = echo_in_l;
		echo_hist_pos [0] [1] = echo_hist_pos [8] [1] = echo_in_r;
		
		#define CALC_FIR_( i, in )  ((in) * (int8_t) REG(fir + i * 0x10))
		echo_in_l = CALC_FIR_( 7, echo_in_l );
		echo_in_r = CALC_FIR_( 7, echo_in_r );
		
		#define CALC_FIR( i, ch )   CALC_FIR_( i, echo_hist_pos [i + 1] [ch] )
		#define DO_FIR( i )\
			echo_in_l += CALC_FIR( i, 0 );\
			echo_in_r += CALC_FIR( i, 1 );
		DO_FIR( 0 );
		DO_FIR( 1 );
		DO_FIR( 2 );
		#if defined (__MWERKS__) && __MWERKS__ < 0x3200
			__eieio(); // keeps compiler from stupidly "caching" things in memory
		#endif
		DO_FIR( 3 );
		DO_FIR( 4 );
		DO_FIR( 5 );
		DO_FIR( 6 );
		
		// Echo out
		if ( !(REG(flg) & 0x20) )
		{
			int l = (echo_out_l >> 7) + ((echo_in_l * (int8_t) REG(efb)) >> 14);
			int r = (echo_out_r >> 7) + ((echo_in_r * (int8_t) REG(efb)) >> 14);
			
			// just to help pass more validation tests
			#if SPC_MORE_ACCURACY
				l &= ~1;
				r &= ~1;
			#endif
			
			CLAMP16( l );
			CLAMP16( r );
			
			SET_LE16A( echo_ptr + 0, l );
			SET_LE16A( echo_ptr + 2, r );
		}
		
		// Sound out
		int l = (((main_out_l * mvoll + echo_in_l * (int8_t) REG(evoll)) >> 14) * m.gain) >> 8;
		int r = (((main_out_r * mvolr + echo_in_r * (int8_t) REG(evolr)) >> 14) * m.gain) >> 8;
		
		CLAMP16( l );
		CLAMP16( r );
		
		if ( (REG(flg) & 0x40) )
		{
			l = 0;
			r = 0;
		}
		
		sample_t* out = m.out;
		WRITE_SAMPLES( l, r, out );
		m.out = out;
	}
	while ( --count );
}


//// Setup

void Spc_Dsp::mute_voices( int mask )
{
	m.mute_mask = mask;
	for ( int i = 0; i < voice_count; i++ )
	{
		m.voices [i].enabled = (mask >> i & 1) - 1;
		update_voice_vol( i * 0x10 );
	}
}

void Spc_Dsp::init( void* ram_64k )
{
	m.ram = (uint8_t*) ram_64k;
	set_gain( gain_unit );
	mute_voices( 0 );
	disable_surround( false );
	set_output( 0, 0 );
	reset();
	
	#ifndef NDEBUG
		// be sure this sign-extends
		assert( (int16_t) 0x8000 == -0x8000 );
		
		// be sure right shift preserves sign
		assert( (-1 >> 1) == -1 );
		
		// check clamp macro
		int i;
		i = +0x8000; CLAMP16( i ); assert( i == +0x7FFF );
		i = -0x8001; CLAMP16( i ); assert( i == -0x8000 );
		
		blargg_verify_byte_order();
	#endif
}

void Spc_Dsp::soft_reset_common()
{
	require( m.ram ); // init() must have been called already
	
	m.noise              = 0x4000;
	m.echo_hist_pos      = m.echo_hist;
	m.every_other_sample = 1;
	m.echo_offset        = 0;
	m.phase              = 0;
	
	init_counter();
}

void Spc_Dsp::soft_reset()
{
	REG(flg) = 0xE0;
	soft_reset_common();
}

void Spc_Dsp::load( uint8_t const regs [register_count] )
{
	memcpy( m.regs, regs, sizeof m.regs );
	memset( &m.regs [register_count], 0, offsetof (state_t,ram) - register_count );
	
	// Internal state
	int i;
	for ( i = voice_count; --i >= 0; )
	{
		voice_t& v = m.voices [i];
		v.brr_offset = 1;
		v.buf_pos    = v.buf;
	}
	m.new_kon = REG(kon);
	
	mute_voices( m.mute_mask );
	soft_reset_common();
}

void Spc_Dsp::reset() { load( initial_regs ); }
