// Combination of Fir_Resampler and Blip_Buffer mixing. Used by Sega FM emulators.

// Game_Music_Emu 0.5.2
#ifndef DUAL_RESAMPLER_H
#define DUAL_RESAMPLER_H

#include "Fir_Resampler.h"
#include "Blip_Buffer.h"

class Dual_Resampler {
public:
	Dual_Resampler();
	virtual ~Dual_Resampler();
	
	typedef short dsample_t;
	
	double setup( double oversample, double rolloff, double gain );
	blargg_err_t reset( int max_pairs );
	void resize( int pairs_per_frame );
	void clear();
	
	void dual_play( long count, dsample_t* out, Blip_Buffer& );
	
protected:
	virtual int play_frame( blip_time_t, int pcm_count, dsample_t* pcm_out ) = 0;
private:
	
	blargg_vector<dsample_t> sample_buf;
	int sample_buf_size;
	int oversamples_per_frame;
	int buf_pos;
	int resampler_size;
	
	Fir_Resampler<12> resampler;
	void mix_samples( Blip_Buffer&, dsample_t* );
	void play_frame_( Blip_Buffer&, dsample_t* );
};

inline double Dual_Resampler::setup( double oversample, double rolloff, double gain )
{
	return resampler.time_ratio( oversample, rolloff, gain * 0.5 );
}

inline void Dual_Resampler::clear()
{
	buf_pos = sample_buf_size;
	resampler.clear();
}

#endif
