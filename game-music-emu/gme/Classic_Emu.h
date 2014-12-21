// Common aspects of emulators which use Blip_Buffer for sound output

// Game_Music_Emu 0.6.0
#ifndef CLASSIC_EMU_H
#define CLASSIC_EMU_H

#include "blargg_common.h"
#include "Blip_Buffer.h"
#include "Music_Emu.h"

class Classic_Emu : public Music_Emu {
public:
	Classic_Emu();
	~Classic_Emu();
	void set_buffer( Multi_Buffer* );
protected:
	// Services
	enum { wave_type = 0x100, noise_type = 0x200, mixed_type = wave_type | noise_type };
	void set_voice_types( int const* t ) { voice_types = t; }
	blargg_err_t setup_buffer( long clock_rate );
	long clock_rate() const { return clock_rate_; }
	void change_clock_rate( long ); // experimental
	
	// Overridable
	virtual void set_voice( int index, Blip_Buffer* center,
			Blip_Buffer* left, Blip_Buffer* right ) = 0;
	virtual void update_eq( blip_eq_t const& ) = 0;
	virtual blargg_err_t start_track_( int track ) = 0;
	virtual blargg_err_t run_clocks( blip_time_t& time_io, int msec ) = 0;
protected:
	blargg_err_t set_sample_rate_( long sample_rate );
	void mute_voices_( int );
	void set_equalizer_( equalizer_t const& );
	blargg_err_t play_( long, sample_t* );
private:
	Multi_Buffer* buf;
	Multi_Buffer* stereo_buffer; // NULL if using custom buffer
	long clock_rate_;
	unsigned buf_changed_count;
	int const* voice_types;
};

inline void Classic_Emu::set_buffer( Multi_Buffer* new_buf )
{
	assert( !buf && new_buf );
	buf = new_buf;
}

// ROM data handler, used by several Classic_Emu derivitives. Loads file data
// with padding on both sides, allowing direct use in bank mapping. The main purpose
// is to allow all file data to be loaded with only one read() call (for efficiency).

class Rom_Data_ {
public:
	typedef unsigned char byte;
protected:
	enum { pad_extra = 8 };
	blargg_vector<byte> rom;
	long file_size_;
	blargg_long rom_addr;
	blargg_long mask;
	blargg_long size_; // TODO: eliminate
	
	blargg_err_t load_rom_data_( Data_Reader& in, int header_size, void* header_out,
			int fill, long pad_size );
	void set_addr_( long addr, int unit );
};

template<int unit>
class Rom_Data : public Rom_Data_ {
	enum { pad_size = unit + pad_extra };
public:
	// Load file data, using already-loaded header 'h' if not NULL. Copy header
	// from loaded file data into *out and fill unmapped bytes with 'fill'.
	blargg_err_t load( Data_Reader& in, int header_size, void* header_out, int fill )
	{
		return load_rom_data_( in, header_size, header_out, fill, pad_size );
	}
	
	// Size of file data read in (excluding header)
	long file_size() const { return file_size_; }
	
	// Pointer to beginning of file data
	byte* begin() const { return rom.begin() + pad_size; }
	
	// Set address that file data should start at
	void set_addr( long addr ) { set_addr_( addr, unit ); }
	
	// Free data
	void clear() { rom.clear(); }
	
	// Size of data + start addr, rounded to a multiple of unit
	long size() const { return size_; }
	
	// Pointer to unmapped page filled with same value
	byte* unmapped() { return rom.begin(); }
	
	// Mask address to nearest power of two greater than size()
	blargg_long mask_addr( blargg_long addr ) const
	{
		#ifdef check
			check( addr <= mask );
		#endif
		return addr & mask;
	}
	
	// Pointer to page starting at addr. Returns unmapped() if outside data.
	byte* at_addr( blargg_long addr )
	{
		blargg_ulong offset = mask_addr( addr ) - rom_addr;
		if ( offset > blargg_ulong (rom.size() - pad_size) )
			offset = 0; // unmapped
		return &rom [offset];
	}
};

#ifndef GME_APU_HOOK
	#define GME_APU_HOOK( emu, addr, data ) ((void) 0)
#endif

#ifndef GME_FRAME_HOOK
	#define GME_FRAME_HOOK( emu ) ((void) 0)
#else
	#define GME_FRAME_HOOK_DEFINED 1
#endif

#endif
