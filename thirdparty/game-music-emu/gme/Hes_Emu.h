// TurboGrafx-16/PC Engine HES music file emulator

// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/
#ifndef HES_EMU_H
#define HES_EMU_H

#include "Classic_Emu.h"
#include "Hes_Apu.h"
#include "Hes_Cpu.h"

class Hes_Emu : private Hes_Cpu, public Classic_Emu {
	typedef Hes_Cpu cpu;
public:
	// HES file header
	enum { header_size = 0x20 };
	struct header_t
	{
		byte tag [4];
		byte vers;
		byte first_track;
		byte init_addr [2];
		byte banks [8];
		byte data_tag [4];
		byte size [4];
		byte addr [4];
		byte unused [4];
	};
	
	// Header for currently loaded file
	header_t const& header() const { return header_; }
	
	static gme_type_t static_type() { return gme_hes_type; }

public:
	Hes_Emu();
	~Hes_Emu();
protected:
	blargg_err_t track_info_( track_info_t*, int track ) const;
	blargg_err_t load_( Data_Reader& );
	blargg_err_t start_track_( int );
	blargg_err_t run_clocks( blip_time_t&, int );
	void set_tempo_( double );
	void set_voice( int, Blip_Buffer*, Blip_Buffer*, Blip_Buffer* );
	void update_eq( blip_eq_t const& );
	void unload();
public: private: friend class Hes_Cpu;
	byte* write_pages [page_count + 1]; // 0 if unmapped or I/O space
	
	int cpu_read_( hes_addr_t );
	int cpu_read( hes_addr_t );
	void cpu_write_( hes_addr_t, int data );
	void cpu_write( hes_addr_t, int );
	void cpu_write_vdp( int addr, int data );
	byte const* cpu_set_mmr( int page, int bank );
	int cpu_done();
private:
	Rom_Data<page_size> rom;
	header_t header_;
	hes_time_t play_period;
	hes_time_t last_frame_hook;
	int timer_base;
	
	struct {
		hes_time_t last_time;
		blargg_long count;
		blargg_long load;
		int raw_load;
		byte enabled;
		byte fired;
	} timer;
	
	struct {
		hes_time_t next_vbl;
		byte latch;
		byte control;
	} vdp;
	
	struct {
		hes_time_t timer;
		hes_time_t vdp;
		byte disables;
	} irq;
	
	void recalc_timer_load();
	
	// large items
	Hes_Apu apu;
	byte sgx [3 * page_size + cpu_padding];
	
	void irq_changed();
	void run_until( hes_time_t );
};

#endif
