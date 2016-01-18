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

#define WM_MO_LOG_VOLUME	0x0001
#define WM_MO_ENHANCED_RESAMPLING 0x0002
#define WM_MO_REVERB		0x0004
#define WM_MO_WHOLETEMPO	0x8000
#define WM_MO_ROUNDTEMPO	0x2000

#define WM_GS_VERSION		0x0001

#define WM_SYMBOL	// we do not need this in ZDoom

/*
#if defined(__cplusplus)
extern "C" {
#endif
*/

struct _WM_Info {
	char *copyright;
	unsigned long int current_sample;
	unsigned long int approx_total_samples;
	unsigned short int mixer_options;
	unsigned long int total_midi_time;
};

typedef void midi;

WM_SYMBOL const char * WildMidi_GetString (unsigned short int info);
WM_SYMBOL int WildMidi_Init (const char * config_file, unsigned short int rate, unsigned short int options);
WM_SYMBOL int WildMidi_MasterVolume (unsigned char master_volume);
WM_SYMBOL int WildMidi_SetOption (midi * handle, unsigned short int options, unsigned short int setting);
WM_SYMBOL int WildMidi_Close (midi * handle);
WM_SYMBOL int WildMidi_Shutdown (void);
WM_SYMBOL int WildMidi_GetSampleRate (void);

/*
#if defined(__cplusplus)
}
#endif
*/

class WildMidi_Renderer
{
public:
	WildMidi_Renderer();
	~WildMidi_Renderer();

	void ShortEvent(int status, int parm1, int parm2);
	void LongEvent(const unsigned char *data, int len);
	void ComputeOutput(float *buffer, int len);
	void LoadInstrument(int bank, int percussion, int instr);
	int GetVoiceCount();
	void SetOption(int opt, int set);
private:
	void *handle;
};

#endif /* WILDMIDI_LIB_H */

