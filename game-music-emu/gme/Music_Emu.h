// Common interface to game music file emulators

// Game_Music_Emu 0.5.2
#ifndef MUSIC_EMU_H
#define MUSIC_EMU_H

#include "Gme_File.h"
class Multi_Buffer;

struct Music_Emu : public Gme_File {
public:
// Basic functionality (see Gme_File.h for file loading/track info functions)

	// Set output sample rate. Must be called only once before loading file.
	blargg_err_t set_sample_rate( long sample_rate );
	
	// Start a track, where 0 is the first track. Also clears warning string.
	blargg_err_t start_track( int );
	
	// Generate 'count' samples info 'buf'. Output is in stereo. Any emulation
	// errors set warning string, and major errors also end track.
	typedef short sample_t;
	blargg_err_t play( long count, sample_t* buf );
	
// Informational
	
	// Sample rate sound is generated at
	long sample_rate() const;
	
	// Index of current track or -1 if one hasn't been started
	int current_track() const;
	
	// Number of voices used by currently loaded file
	int voice_count() const;
	
	// Names of voices
	const char** voice_names() const;
	
// Track status/control

	// Number of milliseconds (1000 msec = 1 second) played since beginning of track
	long tell() const;
	
	// Seek to new time in track. Seeking backwards or far forward can take a while.
	blargg_err_t seek( long msec );
	
	// Skip n samples
	blargg_err_t skip( long n );
	
	// True if a track has reached its end
	bool track_ended() const;
	
	// Set start time and length of track fade out. Once fade ends track_ended() returns
	// true. Fade time can be changed while track is playing.
	void set_fade( long start_msec, long length_msec = 8000 );
	
	// Disable automatic end-of-track detection and skipping of silence at beginning
	void ignore_silence( bool disable = true );
	
	// Info for current track
	using Gme_File::track_info;
	blargg_err_t track_info( track_info_t* out ) const;
	
// Sound customization
	
	// Adjust song tempo, where 1.0 = normal, 0.5 = half speed, 2.0 = double speed.
	// Track length as returned by track_info() assumes a tempo of 1.0.
	void set_tempo( double );
	
	// Mute/unmute voice i, where voice 0 is first voice
	void mute_voice( int index, bool mute = true );
	
	// Set muting state of all voices at once using a bit mask, where -1 mutes them all,
	// 0 unmutes them all, 0x01 mutes just the first voice, etc.
	void mute_voices( int mask );
	
	// Change overall output amplitude, where 1.0 results in minimal clamping.
	// Must be called before set_sample_rate().
	void set_gain( double );
	
	// Request use of custom multichannel buffer. Only supported by "classic" emulators;
	// on others this has no effect. Should be called only once *before* set_sample_rate().
	virtual void set_buffer( Multi_Buffer* ) { }
	
// Sound equalization (treble/bass)

	// Frequency equalizer parameters (see gme.txt)
	// See gme.h for definition of struct gme_equalizer_t.
	typedef gme_equalizer_t equalizer_t;
	
	// Current frequency equalizater parameters
	equalizer_t const& equalizer() const;
	
	// Set frequency equalizer parameters
	void set_equalizer( equalizer_t const& );
	
	// Equalizer settings for TV speaker
	static equalizer_t const tv_eq;
	
public:
	Music_Emu();
	~Music_Emu();
protected:
	void set_max_initial_silence( int n )       { max_initial_silence = n; }
	void set_silence_lookahead( int n )         { silence_lookahead = n; }
	void set_voice_count( int n )               { voice_count_ = n; }
	void set_voice_names( const char* const* names );
	void set_track_ended()                      { emu_track_ended_ = true; }
	double gain() const                         { return gain_; }
	double tempo() const                        { return tempo_; }
	void remute_voices();
	
	virtual blargg_err_t set_sample_rate_( long sample_rate ) = 0;
	virtual void set_equalizer_( equalizer_t const& ) { };
	virtual void mute_voices_( int mask ) = 0;
	virtual void set_tempo_( double ) = 0;
	virtual blargg_err_t start_track_( int ) = 0; // tempo is set before this
	virtual blargg_err_t play_( long count, sample_t* out ) = 0;
	virtual blargg_err_t skip_( long count );
protected:
	virtual void unload();
	virtual void pre_load();
	virtual void post_load_();
private:
	// general
	equalizer_t equalizer_;
	int max_initial_silence;
	const char** voice_names_;
	int voice_count_;
	int mute_mask_;
	double tempo_;
	double gain_;
	
	long sample_rate_;
	blargg_long msec_to_samples( blargg_long msec ) const;
	
	// track-specific
	int current_track_;
	blargg_long out_time;  // number of samples played since start of track
	blargg_long emu_time;  // number of samples emulator has generated since start of track
	bool emu_track_ended_; // emulator has reached end of track
	volatile bool track_ended_;
	void clear_track_vars();
	void end_track_if_error( blargg_err_t );
	
	// fading
	blargg_long fade_start;
	int fade_step;
	void handle_fade( long count, sample_t* out );
	
	// silence detection
	int silence_lookahead; // speed to run emulator when looking ahead for silence
	bool ignore_silence_;
	long silence_time;     // number of samples where most recent silence began
	long silence_count;    // number of samples of silence to play before using buf
	long buf_remain;       // number of samples left in silence buffer
	enum { buf_size = 2048 };
	blargg_vector<sample_t> buf;
	void fill_buf();
	void emu_play( long count, sample_t* out );
	
	Multi_Buffer* effects_buffer;
	friend GMEDLL Music_Emu* GMEAPI gme_new_emu( gme_type_t, int );
	friend GMEDLL void GMEAPI gme_set_stereo_depth( Music_Emu*, double );
};

// base class for info-only derivations
struct Gme_Info_ : Music_Emu
{
	virtual blargg_err_t set_sample_rate_( long sample_rate );
	virtual void set_equalizer_( equalizer_t const& );
	virtual void mute_voices_( int mask );
	virtual void set_tempo_( double );
	virtual blargg_err_t start_track_( int );
	virtual blargg_err_t play_( long count, sample_t* out );
	virtual void pre_load();
	virtual void post_load_();
};

inline blargg_err_t Music_Emu::track_info( track_info_t* out ) const
{
	return track_info( out, current_track_ );
}

inline long Music_Emu::sample_rate() const          { return sample_rate_; }
inline const char** Music_Emu::voice_names() const  { return voice_names_; }
inline int Music_Emu::voice_count() const           { return voice_count_; }
inline int Music_Emu::current_track() const         { return current_track_; }
inline bool Music_Emu::track_ended() const          { return track_ended_; }
inline const Music_Emu::equalizer_t& Music_Emu::equalizer() const { return equalizer_; }

inline void Music_Emu::set_tempo_( double t )       { tempo_ = t; }
inline void Music_Emu::remute_voices()              { mute_voices( mute_mask_ ); }
inline void Music_Emu::ignore_silence( bool b )     { ignore_silence_ = b; }
inline blargg_err_t Music_Emu::start_track_( int )  { return 0; }

inline void Music_Emu::set_voice_names( const char* const* names )
{
	// Intentional removal of const, so users don't have to remember obscure const in middle
	voice_names_ = const_cast<const char**> (names);
}

inline void Music_Emu::mute_voices_( int ) { }

inline void Music_Emu::set_gain( double g )
{
	assert( !sample_rate() ); // you must set gain before setting sample rate
	gain_ = g;
}

#endif
