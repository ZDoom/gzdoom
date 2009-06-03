// Atari XL/XE SAP music file emulator

// Game_Music_Emu 0.5.2
#ifndef SAP_EMU_H
#define SAP_EMU_H

#include "Classic_Emu.h"
#include "Sap_Apu.h"
#include "Sap_Cpu.h"

class Sap_Emu : private Sap_Cpu, public Classic_Emu {
	typedef Sap_Cpu cpu;
public:
	static gme_type_t static_type() { return gme_sap_type; }
public:
	Sap_Emu();
	~Sap_Emu();
	struct info_t {
		byte const* rom_data;
		const char* warning;
		long init_addr;
		long play_addr;
		long music_addr;
		int  type;
		int  track_count;
		int  fastplay;
		bool stereo;
		char author    [256];
		char name      [256];
		char copyright [ 32];
	};
protected:
	blargg_err_t track_info_( track_info_t*, int track ) const;
	blargg_err_t load_mem_( byte const*, long );
	blargg_err_t start_track_( int );
	blargg_err_t run_clocks( blip_time_t&, int );
	void set_tempo_( double );
	void set_voice( int, Blip_Buffer*, Blip_Buffer*, Blip_Buffer* );
	void update_eq( blip_eq_t const& );
public: private: friend class Sap_Cpu;
	int cpu_read( sap_addr_t );
	void cpu_write( sap_addr_t, int );
	void cpu_write_( sap_addr_t, int );
private:
	info_t info;
	
	byte const* file_end;
	sap_time_t scanline_period;
	sap_time_t next_play;
	sap_time_t time_mask;
	Sap_Apu apu;
	Sap_Apu apu2;
	
	// large items
	struct {
		byte padding1 [0x100];
		byte ram [0x10000];
		byte padding2 [0x100];
	} mem;
	Sap_Apu_Impl apu_impl;
	
	sap_time_t play_period() const;
	void call_play();
	void cpu_jsr( sap_addr_t );
	void call_init( int track );
	void run_routine( sap_addr_t );
};

#endif
