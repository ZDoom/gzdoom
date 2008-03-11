/* SNES SPC-700 DSP emulator C interface (also usable from C++) */

/* snes_spc 0.9.0 */
#ifndef DSP_H
#define DSP_H

#include <stddef.h>

#ifdef __cplusplus
	extern "C" {
#endif

typedef struct SPC_DSP SPC_DSP;

/* Creates new DSP emulator. NULL if out of memory. */
SPC_DSP* spc_dsp_new( void );

/* Frees DSP emulator */
void spc_dsp_delete( SPC_DSP* );

/* Initializes DSP and has it use the 64K RAM provided */
void spc_dsp_init( SPC_DSP*, void* ram_64k );

/* Sets destination for output samples. If out is NULL or out_size is 0,
doesn't generate any. */
typedef short spc_dsp_sample_t;
void spc_dsp_set_output( SPC_DSP*, spc_dsp_sample_t* out, int out_size );

/* Number of samples written to output since it was last set, always
a multiple of 2. Undefined if more samples were generated than
output buffer could hold. */
int spc_dsp_sample_count( SPC_DSP const* );


/**** Emulation *****/

/* Resets DSP to power-on state */
void spc_dsp_reset( SPC_DSP* );

/* Emulates pressing reset switch on SNES */
void spc_dsp_soft_reset( SPC_DSP* );

/* Reads/writes DSP registers. For accuracy, you must first call spc_dsp_run() */
/* to catch the DSP up to present. */
int  spc_dsp_read ( SPC_DSP const*, int addr );
void spc_dsp_write( SPC_DSP*, int addr, int data );

/* Runs DSP for specified number of clocks (~1024000 per second). Every 32 clocks */
/* a pair of samples is be generated. */
void spc_dsp_run( SPC_DSP*, int clock_count );


/**** Sound control *****/

/* Mutes voices corresponding to non-zero bits in mask. Reduces emulation accuracy. */
enum { spc_dsp_voice_count = 8 };
void spc_dsp_mute_voices( SPC_DSP*, int mask );

/* If true, prevents channels and global volumes from being phase-negated.
Only supported by fast DSP; has no effect on accurate DSP. */
void spc_dsp_disable_surround( SPC_DSP*, int disable );


/**** State save/load *****/

/* Resets DSP and uses supplied values to initialize registers */
enum { spc_dsp_register_count = 128 };
void spc_dsp_load( SPC_DSP*, unsigned char const regs [spc_dsp_register_count] );

/* Saves/loads exact emulator state (accurate DSP only) */
enum { spc_dsp_state_size = 640 }; /* maximum space needed when saving */
typedef void (*spc_dsp_copy_func_t)( unsigned char** io, void* state, size_t );
void spc_dsp_copy_state( SPC_DSP*, unsigned char** io, spc_dsp_copy_func_t );

/* Returns non-zero if new key-on events occurred since last call (accurate DSP only) */
int spc_dsp_check_kon( SPC_DSP* );


#ifdef __cplusplus
	}
#endif

#endif
