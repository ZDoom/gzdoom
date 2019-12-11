/*
	wildmidi_lib.h

	Midi Wavetable Processing library

    Copyright (C) Chris Ison 2001-2011
    Copyright (C) Bret Curtis 2013-2014

    This file is part of WildMIDI.

    WildMIDI is free software: you can redistribute and/or modify the player
    under the terms of the GNU General Public License and you can redistribute
    and/or modify the library under the terms of the GNU Lesser General Public
    License as published by the Free Software Foundation, either version 3 of
    the licenses, or(at your option) any later version.

    WildMIDI is distributed in the hope that it will be useful, but WITHOUT
    ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
    FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License and
    the GNU Lesser General Public License for more details.

    You should have received a copy of the GNU General Public License and the
    GNU Lesser General Public License along with WildMIDI.  If not,  see
    <http://www.gnu.org/licenses/>.
*/

#ifndef WILDMIDI_LIB_H
#define WILDMIDI_LIB_H

#include "../../music_common/fileio.h"

namespace WildMidi
{
enum EMixerOptions
{
	WM_MO_LOG_VOLUME	= 0x0001,
	WM_MO_ENHANCED_RESAMPLING = 0x0002,
	WM_MO_REVERB		= 0x0004,
	WM_MO_WHOLETEMPO	= 0x8000,
	WM_MO_ROUNDTEMPO	= 0x2000,
	
	WM_GS_VERSION		= 0x0001,
};


class SoundFontReaderInterface;

struct _WM_Info {
	char *copyright;
	unsigned long int current_sample;
	unsigned long int approx_total_samples;
	unsigned short int mixer_options;
	unsigned long int total_midi_time;
};

typedef void midi;
	
struct Instruments
{
	MusicIO::SoundFontReaderInterface *sfreader;

	struct _patch *patch[128] = {};
	float reverb_room_width = 16.875f;
	float reverb_room_length = 22.5f;
	
	float reverb_listen_posx = 8.4375f;
	float reverb_listen_posy = 16.875f;
	
	int fix_release = 0;
	int auto_amp = 0;
	int auto_amp_with_amp = 0;
	
	unsigned short int _WM_SampleRate;	// WildMidi makes the sample rate a property of the patches, not the renderer. Meaning that the instruments need to be reloaded when it changes... :?

	Instruments(MusicIO::SoundFontReaderInterface *reader, int samplerate)
	{
		sfreader = reader;
		_WM_SampleRate = samplerate;
	}
	~Instruments();
	
	int LoadConfig(const char *config_file);
	int load_sample(struct _patch *sample_patch);
	struct _patch *get_patch_data(unsigned short patchid);
	void load_patch(struct _mdi *mdi, unsigned short patchid);
	int GetSampleRate() { return _WM_SampleRate; }
	struct _sample * load_gus_pat(const char *filename);

private:
	void FreePatches(void);
};

const char * WildMidi_GetString (unsigned short int info);



class Renderer
{
	Instruments *instruments;

	signed int WM_MasterVolume = 948;
	unsigned int WM_MixerOptions = 0;

public:
	Renderer(Instruments *instr, unsigned mixOpt = 0);
	~Renderer();

	void ShortEvent(int status, int parm1, int parm2);
	void LongEvent(const unsigned char *data, int len);
	void ComputeOutput(float *buffer, int len);
	void LoadInstrument(int bank, int percussion, int instr);
	int GetVoiceCount();
	int SetOption(int opt, int set);
	
	void SetMasterVolume(unsigned char master_volume);
	midi * NewMidi();

private:
	void *handle;
	
	void AdjustNoteVolumes(struct _mdi *mdi, unsigned char ch, struct _note *nte);
	void AdjustChannelVolumes(struct _mdi *mdi, unsigned char ch);
	void do_note_on(struct _mdi *mdi, struct _event_data *data);
	void do_aftertouch(struct _mdi *mdi, struct _event_data *data);
	void do_control_channel_volume(struct _mdi *mdi, struct _event_data *data);
	void do_control_channel_balance(struct _mdi *mdi, struct _event_data *data);
	void do_control_channel_pan(struct _mdi *mdi, struct _event_data *data);
	void do_control_channel_expression(struct _mdi *mdi, struct _event_data *data);
	void do_control_channel_controllers_off(struct _mdi *mdi, struct _event_data *data);
	void do_pitch(struct _mdi *mdi, struct _event_data *data);
	void do_patch(struct _mdi *mdi, struct _event_data *data);
	void do_channel_pressure(struct _mdi *mdi, struct _event_data *data);
	void do_sysex_roland_drum_track(struct _mdi *mdi, struct _event_data *data);
	void do_sysex_gm_reset(struct _mdi *mdi, struct _event_data *data);
	void do_sysex_roland_reset(struct _mdi *mdi, struct _event_data *data);
	void do_sysex_yamaha_reset(struct _mdi *mdi, struct _event_data *data);
	struct _mdi *Init_MDI();
	unsigned long int get_inc(struct _mdi *mdi, struct _note *nte);
};

extern void (*wm_error_func)(const char *wmfmt, va_list args); 
}

#endif /* WILDMIDI_LIB_H */

