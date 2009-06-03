// Nintendo NES/Famicom NSF music file emulator

// Game_Music_Emu 0.5.2
#ifndef NSF_EMU_H
#define NSF_EMU_H

#include "Classic_Emu.h"
#include "Nes_Apu.h"
#include "Nes_Cpu.h"

class Nsf_Emu : private Nes_Cpu, public Classic_Emu {
	typedef Nes_Cpu cpu;
public:
	// Equalizer profiles for US NES and Japanese Famicom
	static equalizer_t const nes_eq;
	static equalizer_t const famicom_eq;
	
	// NSF file header
	enum { header_size = 0x80 };
	struct header_t
	{
		char tag [5];
		byte vers;
		byte track_count;
		byte first_track;
		byte load_addr [2];
		byte init_addr [2];
		byte play_addr [2];
		char game [32];
		char author [32];
		char copyright [32];
		byte ntsc_speed [2];
		byte banks [8];
		byte pal_speed [2];
		byte speed_flags;
		byte chip_flags;
		byte unused [4];
	};
	
	// Header for currently loaded file
	header_t const& header() const { return header_; }
	
	static gme_type_t static_type() { return gme_nsf_type; }
	
public:
	// deprecated
	Music_Emu::load;
	blargg_err_t load( header_t const& h, Data_Reader& in ) // use Remaining_Reader
			{ return load_remaining_( &h, sizeof h, in ); }

public:
	Nsf_Emu();
	~Nsf_Emu();
	Nes_Apu* apu_() { return &apu; }
protected:
	blargg_err_t track_info_( track_info_t*, int track ) const;
	blargg_err_t load_( Data_Reader& );
	blargg_err_t start_track_( int );
	blargg_err_t run_clocks( blip_time_t&, int );
	void set_tempo_( double );
	void set_voice( int, Blip_Buffer*, Blip_Buffer*, Blip_Buffer* );
	void update_eq( blip_eq_t const& );
	void unload();
protected:
	enum { bank_count = 8 };
	byte initial_banks [bank_count];
	nes_addr_t init_addr;
	nes_addr_t play_addr;
	double clock_rate_;
	bool pal_only;
	
	// timing
	Nes_Cpu::registers_t saved_state;
	nes_time_t next_play;
	nes_time_t play_period;
	int play_extra;
	int play_ready;
	
	enum { rom_begin = 0x8000 };
	enum { bank_select_addr = 0x5FF8 };
	enum { bank_size = 0x1000 };
	Rom_Data<bank_size> rom;
	
public: private: friend class Nes_Cpu;
	void cpu_jsr( nes_addr_t );
	int cpu_read( nes_addr_t );
	void cpu_write( nes_addr_t, int );
	void cpu_write_misc( nes_addr_t, int );
	enum { badop_addr = bank_select_addr };
	
private:
	class Nes_Namco_Apu* namco;
	class Nes_Vrc6_Apu*  vrc6;
	class Nes_Fme7_Apu*  fme7;
	Nes_Apu apu;
	static int pcm_read( void*, nes_addr_t );
	blargg_err_t init_sound();
	
	header_t header_;
	
	enum { sram_addr = 0x6000 };
	byte sram [0x2000];
	byte unmapped_code [Nes_Cpu::page_size + 8];
};

#endif
