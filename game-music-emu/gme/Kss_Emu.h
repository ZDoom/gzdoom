// MSX computer KSS music file emulator

// Game_Music_Emu 0.5.2
#ifndef KSS_EMU_H
#define KSS_EMU_H

#include "Classic_Emu.h"
#include "Kss_Scc_Apu.h"
#include "Kss_Cpu.h"
#include "Sms_Apu.h"
#include "Ay_Apu.h"

class Kss_Emu : private Kss_Cpu, public Classic_Emu {
	typedef Kss_Cpu cpu;
public:
	// KSS file header
	enum { header_size = 0x10 };
	struct header_t
	{
		byte tag [4];
		byte load_addr [2];
		byte load_size [2];
		byte init_addr [2];
		byte play_addr [2];
		byte first_bank;
		byte bank_mode;
		byte extra_header;
		byte device_flags;
	};
	
	enum { ext_header_size = 0x10 };
	struct ext_header_t
	{
		byte data_size [4];
		byte unused [4];
		byte first_track [2];
		byte last_tack [2];
		byte psg_vol;
		byte scc_vol;
		byte msx_music_vol;
		byte msx_audio_vol;
	};
	
	struct composite_header_t : header_t, ext_header_t { };
	
	// Header for currently loaded file
	composite_header_t const& header() const { return header_; }
	
	static gme_type_t static_type() { return gme_kss_type; }
public:
	Kss_Emu();
	~Kss_Emu();
protected:
	blargg_err_t track_info_( track_info_t*, int track ) const;
	blargg_err_t load_( Data_Reader& );
	blargg_err_t start_track_( int );
	blargg_err_t run_clocks( blip_time_t&, int );
	void set_tempo_( double );
	void set_voice( int, Blip_Buffer*, Blip_Buffer*, Blip_Buffer* );
	void update_eq( blip_eq_t const& );
	void unload();
private:
	Rom_Data<page_size> rom;
	composite_header_t header_;
	
	bool scc_accessed;
	bool gain_updated;
	void update_gain();
	
	unsigned scc_enabled; // 0 or 0xC000
	byte const* bank_data;
	int bank_count;
	void set_bank( int logical, int physical );
	blargg_long bank_size() const { return (16 * 1024L) >> (header_.bank_mode >> 7 & 1); }
	
	blip_time_t play_period;
	blip_time_t next_play;
	int ay_latch;
	
	friend void kss_cpu_out( class Kss_Cpu*, cpu_time_t, unsigned addr, int data );
	friend int  kss_cpu_in( class Kss_Cpu*, cpu_time_t, unsigned addr );
	void cpu_write( unsigned addr, int data );
	friend void kss_cpu_write( class Kss_Cpu*, unsigned addr, int data );
	
	// large items
	enum { mem_size = 0x10000 };
	byte ram [mem_size + cpu_padding];
	
	Ay_Apu ay;
	Scc_Apu scc;
	Sms_Apu* sn;
	byte unmapped_read  [0x100];
	byte unmapped_write [page_size];
};

#endif
