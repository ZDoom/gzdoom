// Common interface to game music file loading and information

// Game_Music_Emu 0.5.2
#ifndef GME_FILE_H
#define GME_FILE_H

#include "gme.h"
#include "blargg_common.h"
#include "Data_Reader.h"
#include "M3u_Playlist.h"

// Error returned if file is wrong type
//extern const char gme_wrong_file_type []; // declared in gme.h

struct gme_type_t_
{
	const char* system;         /* name of system this music file type is generally for */
	int track_count;            /* non-zero for formats with a fixed number of tracks */
	Music_Emu* (*new_emu)();    /* Create new emulator for this type (useful in C++ only) */
	Music_Emu* (*new_info)();   /* Create new info reader for this type */
	
	/* internal */
	const char* extension_;
	int flags_;
};

struct track_info_t
{
	long track_count;
	
	/* times in milliseconds; -1 if unknown */
	long length;
	long intro_length;
	long loop_length;
	
	/* empty string if not available */
	char system    [256];
	char game      [256];
	char song      [256];
	char author    [256];
	char copyright [256];
	char comment   [256];
	char dumper    [256];
};
enum { gme_max_field = 255 };

struct Gme_File {
public:
// File loading
	
	// Each loads game music data from a file and returns an error if
	// file is wrong type or is seriously corrupt. They also set warning
	// string for minor problems.
	
	// Load from file on disk
	blargg_err_t load_file( const char* path );
	
	// Load from custom data source (see Data_Reader.h)
	blargg_err_t load( Data_Reader& );
	
	// Load from file already read into memory. Keeps pointer to data, so you
	// must not free it until you're done with the file.
	blargg_err_t load_mem( void const* data, long size );
	
	// Load an m3u playlist. Must be done after loading main music file.
	blargg_err_t load_m3u( const char* path );
	blargg_err_t load_m3u( Data_Reader& in );
	
	// Clears any loaded m3u playlist and any internal playlist that the music
	// format supports (NSFE for example).
	void clear_playlist();
	
// Informational
	
	// Type of emulator. For example if this returns gme_nsfe_type, this object
	// is an NSFE emulator, and you can cast to an Nsfe_Emu* if necessary.
	gme_type_t type() const;
	
	// Most recent warning string, or NULL if none. Clears current warning after
	// returning.
	const char* warning();
	
	// Number of tracks or 0 if no file has been loaded
	int track_count() const;
	
	// Get information for a track (length, name, author, etc.)
	// See gme.h for definition of struct track_info_t.
	blargg_err_t track_info( track_info_t* out, int track ) const;
	
// User data/cleanup
	
	// Set/get pointer to data you want to associate with this emulator.
	// You can use this for whatever you want.
	void set_user_data( void* p )       { user_data_ = p; }
	void* user_data() const             { return user_data_; }
	
	// Register cleanup function to be called when deleting emulator, or NULL to
	// clear it. Passes user_data to cleanup function.
	void set_user_cleanup( gme_user_cleanup_t func ) { user_cleanup_ = func; }
	
public:
	// deprecated
	int error_count() const; // use warning()
public:
	Gme_File();
	virtual ~Gme_File();
	BLARGG_DISABLE_NOTHROW
	typedef BOOST::uint8_t byte;
protected:
	// Services
	void set_track_count( int n )       { track_count_ = raw_track_count_ = n; }
	void set_warning( const char* s )   { warning_ = s; }
	void set_type( gme_type_t t )       { type_ = t; }
	blargg_err_t load_remaining_( void const* header, long header_size, Data_Reader& remaining );
	
	// Overridable
	virtual void unload();  // called before loading file and if loading fails
	virtual blargg_err_t load_( Data_Reader& ); // default loads then calls load_mem_()
	virtual blargg_err_t load_mem_( byte const* data, long size ); // use data in memory
	virtual blargg_err_t track_info_( track_info_t* out, int track ) const = 0;
	virtual void pre_load();
	virtual void post_load_();
	virtual void clear_playlist_() { }
	
public:
	blargg_err_t remap_track_( int* track_io ) const; // need by Music_Emu
private:
	// noncopyable
	Gme_File( const Gme_File& );
	Gme_File& operator = ( const Gme_File& );
	
	gme_type_t type_;
	int track_count_;
	int raw_track_count_;
	const char* warning_;
	void* user_data_;
	gme_user_cleanup_t user_cleanup_;
	M3u_Playlist playlist;
	char playlist_warning [64];
	blargg_vector<byte> file_data; // only if loaded into memory using default load
	
	blargg_err_t load_m3u_( blargg_err_t );
	blargg_err_t post_load( blargg_err_t err );
public:
	// track_info field copying
	enum { max_field_ = 255 };
	static void copy_field_( char* out, const char* in );
	static void copy_field_( char* out, const char* in, int len );
};
	
Music_Emu* gme_new_( Music_Emu*, long sample_rate );

#define GME_COPY_FIELD( in, out, name ) \
	{ Gme_File::copy_field_( out->name, in.name, sizeof in.name ); }

#ifndef GME_FILE_READER
	#ifdef HAVE_ZLIB_H
		#define GME_FILE_READER Gzip_File_Reader
	#else
		#define GME_FILE_READER Std_File_Reader
	#endif
#elif defined (GME_FILE_READER_INCLUDE)
	#include GME_FILE_READER_INCLUDE
#endif

inline gme_type_t Gme_File::type() const            { return type_; }
inline int Gme_File::error_count() const            { return warning_ != 0; }
inline int Gme_File::track_count() const            { return track_count_; }

inline const char* Gme_File::warning()
{
	const char* s = warning_;
	warning_ = 0;
	return s;
}

#endif
