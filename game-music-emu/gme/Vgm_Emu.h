// Sega Master System/Mark III, Sega Genesis/Mega Drive, BBC Micro VGM music file emulator

// Game_Music_Emu 0.5.2
#ifndef VGM_EMU_H
#define VGM_EMU_H

#include "Vgm_Emu_Impl.h"

// Emulates VGM music using SN76489/SN76496 PSG, YM2612, and YM2413 FM sound chips.
// Supports custom sound buffer and frequency equalization when VGM uses just the PSG.
// FM sound chips can be run at their proper rates, or slightly higher to reduce
// aliasing on high notes. Currently YM2413 support requires that you supply a
// YM2413 sound chip emulator. I can provide one I've modified to work with the library.
class Vgm_Emu : public Vgm_Emu_Impl {
public:
	// True if custom buffer and custom equalization are supported
	// TODO: move into Music_Emu and rename to something like supports_custom_buffer()
	bool is_classic_emu() const { return !uses_fm; }
	
	// Disable running FM chips at higher than normal rate. Will result in slightly
	// more aliasing of high notes.
	void disable_oversampling( bool disable = true ) { disable_oversampling_ = disable; }
	
	// VGM header format
	enum { header_size = 0x40 };
	struct header_t
	{
		char tag [4];
		byte data_size [4];
		byte version [4];
		byte psg_rate [4];
		byte ym2413_rate [4];
		byte gd3_offset [4];
		byte track_duration [4];
		byte loop_offset [4];
		byte loop_duration [4];
		byte frame_rate [4];
		byte noise_feedback [2];
		byte noise_width;
		byte unused1;
		byte ym2612_rate [4];
		byte ym2151_rate [4];
		byte data_offset [4];
		byte unused2 [8];
	};
	
	// Header for currently loaded file
	header_t const& header() const { return *(header_t const*) data; }
	
	static gme_type_t static_type() { return gme_vgm_type; }
	
public:
	// deprecated
	Music_Emu::load;
	blargg_err_t load( header_t const& h, Data_Reader& in ) // use Remaining_Reader
			{ return load_remaining_( &h, sizeof h, in ); }
	byte const* gd3_data( int* size_out = 0 ) const; // use track_info()

public:
	Vgm_Emu();
	~Vgm_Emu();
protected:
	blargg_err_t track_info_( track_info_t*, int track ) const;
	blargg_err_t load_mem_( byte const*, long );
	blargg_err_t set_sample_rate_( long sample_rate );
	blargg_err_t start_track_( int );
	blargg_err_t play_( long count, sample_t* );
	blargg_err_t run_clocks( blip_time_t&, int );
	void set_tempo_( double );
	void mute_voices_( int mask );
	void set_voice( int, Blip_Buffer*, Blip_Buffer*, Blip_Buffer* );
	void update_eq( blip_eq_t const& );
private:
	// removed; use disable_oversampling() and set_tempo() instead
	Vgm_Emu( bool oversample, double tempo = 1.0 );
	double fm_rate;
	long psg_rate;
	long vgm_rate;
	bool disable_oversampling_;
	bool uses_fm;
	blargg_err_t setup_fm();
};

#endif
