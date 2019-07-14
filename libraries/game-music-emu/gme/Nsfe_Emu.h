// Nintendo NES/Famicom NSFE music file emulator

// Game_Music_Emu https://bitbucket.org/mpyne/game-music-emu/
#ifndef NSFE_EMU_H
#define NSFE_EMU_H

#include "blargg_common.h"
#include "Nsf_Emu.h"

// Allows reading info from NSFE file without creating emulator
class Nsfe_Info {
public:
	blargg_err_t load( Data_Reader&, Nsf_Emu* );
	
	struct info_t : Nsf_Emu::header_t
	{
		char game      [256];
		char author    [256];
		char copyright [256];
		char dumper    [256];
	} info;
	
	void disable_playlist( bool = true );
	
	blargg_err_t track_info_( track_info_t* out, int track ) const;
	
	int remap_track( int i ) const;
	
	void unload();
	
	Nsfe_Info();
	~Nsfe_Info();
private:
	blargg_vector<char> track_name_data;
	blargg_vector<const char*> track_names;
	blargg_vector<unsigned char> playlist;
	blargg_vector<char [4]> track_times;
	int actual_track_count_;
	bool playlist_disabled;
};

class Nsfe_Emu : public Nsf_Emu {
public:
	static gme_type_t static_type() { return gme_nsfe_type; }
	
public:
	// deprecated
	struct header_t { char tag [4]; };
	using Music_Emu::load;
	blargg_err_t load( header_t const& h, Data_Reader& in ) // use Remaining_Reader
			{ return load_remaining_( &h, sizeof h, in ); }
	void disable_playlist( bool = true ); // use clear_playlist()

public:
	Nsfe_Emu();
	~Nsfe_Emu();
protected:
	blargg_err_t load_( Data_Reader& );
	blargg_err_t track_info_( track_info_t*, int track ) const;
	blargg_err_t start_track_( int );
	void unload();
	void clear_playlist_();
private:
	Nsfe_Info info;
	bool loading;
};

#endif
