// Sinclair Spectrum AY music file emulator

// Game_Music_Emu 0.6.0
#ifndef AY_EMU_H
#define AY_EMU_H

#include "Classic_Emu.h"
#include "Ay_Apu.h"
#include "Ay_Cpu.h"

class Ay_Emu : private Ay_Cpu, public Classic_Emu {
	typedef Ay_Cpu cpu;
public:
	// AY file header
	enum { header_size = 0x14 };
	struct header_t
	{
		byte tag [8];
		byte vers;
		byte player;
		byte unused [2];
		byte author [2];
		byte comment [2];
		byte max_track;
		byte first_track;
		byte track_info [2];
	};
	
	static gme_type_t static_type() { return gme_ay_type; }
public:
	Ay_Emu();
	~Ay_Emu();
	struct file_t {
		header_t const* header;
		byte const* end;
		byte const* tracks;
	};
protected:
	blargg_err_t track_info_( track_info_t*, int track ) const;
	blargg_err_t load_mem_( byte const*, long );
	blargg_err_t start_track_( int );
	blargg_err_t run_clocks( blip_time_t&, int );
	void set_tempo_( double );
	void set_voice( int, Blip_Buffer*, Blip_Buffer*, Blip_Buffer* );
	void update_eq( blip_eq_t const& );
private:
	file_t file;
	
	cpu_time_t play_period;
	cpu_time_t next_play;
	Blip_Buffer* beeper_output;
	int beeper_delta;
	int last_beeper;
	int apu_addr;
	int cpc_latch;
	bool spectrum_mode;
	bool cpc_mode;
	
	// large items
	struct {
		byte padding1 [0x100];
		byte ram [0x10000 + 0x100];
	} mem;
	Ay_Apu apu;
	friend void ay_cpu_out( Ay_Cpu*, cpu_time_t, unsigned addr, int data );
	void cpu_out_misc( cpu_time_t, unsigned addr, int data );
};

#endif
