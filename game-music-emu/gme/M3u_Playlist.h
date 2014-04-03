// M3U playlist file parser, with support for subtrack information

// Game_Music_Emu 0.6.0
#ifndef M3U_PLAYLIST_H
#define M3U_PLAYLIST_H

#include "blargg_common.h"
#include "Data_Reader.h"

class M3u_Playlist {
public:
	// Load playlist data
	blargg_err_t load( const char* path );
	blargg_err_t load( Data_Reader& in );
	blargg_err_t load( void const* data, long size );
	
	// Line number of first parse error, 0 if no error. Any lines with parse
	// errors are ignored.
	int first_error() const { return first_error_; }
	
	struct info_t
	{
		const char* title;
		const char* composer;
		const char* engineer;
		const char* ripping;
		const char* tagging;
	};
	info_t const& info() const { return info_; }
	
	struct entry_t
	{
		const char* file; // filename without stupid ::TYPE suffix
		const char* type; // if filename has ::TYPE suffix, this will be "TYPE". "" if none.
		const char* name;
		bool decimal_track; // true if track was specified in hex
		// integers are -1 if not present
		int track;  // 1-based
		int length; // seconds
		int intro;
		int loop;
		int fade;
		int repeat; // count
	};
	entry_t const& operator [] ( int i ) const { return entries [i]; }
	int size() const { return int(entries.size()); }
	
	void clear();
	
private:
	blargg_vector<entry_t> entries;
	blargg_vector<char> data;
	int first_error_;
	info_t info_;
	
	blargg_err_t parse();
	blargg_err_t parse_();
};

inline void M3u_Playlist::clear()
{
	first_error_ = 0;
	entries.clear();
	data.clear();
}

#endif
