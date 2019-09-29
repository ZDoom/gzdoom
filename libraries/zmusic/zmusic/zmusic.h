#pragma once

#include "mididefs.h"
#include "../../music_common/fileio.h"

namespace ZMusic	// Namespaced because these conflict with the same-named CVARs
{

enum EIntConfigKey
{
	adl_chips_count,
	adl_emulator_id,
	adl_run_at_pcm_rate,
	adl_fullpan,
	adl_bank,
	adl_use_custom_bank,
	adl_volume_model,

	fluid_reverb,
	fluid_chorus,
	fluid_voices,
	fluid_interp,
	fluid_samplerate,
	fluid_threads,
	fluid_chorus_voices,
	fluid_chorus_type,

	opl_numchips,
	opl_core,
	opl_fullpan,

	opn_chips_count,
	opn_emulator_id,
	opn_run_at_pcm_rate,
	opn_fullpan,
	opn_use_custom_bank,

	gus_dmxgus,
	gus_midi_voices,
	gus_memsize,

	timidity_modulation_wheel,
	timidity_portamento,
	timidity_reverb,
	timidity_reverb_level,
	timidity_chorus,
	timidity_surround_chorus,
	timidity_channel_pressure,
	timidity_lpf_def,
	timidity_temper_control,
	timidity_modulation_envelope,
	timidity_overlap_voice_allow,
	timidity_drum_effect,
	timidity_pan_delay,
	timidity_key_adjust,

	wildmidi_reverb,
	wildmidi_enhanced_resampling,

	snd_midiprecache,

	mod_samplerate,
	mod_volramp,
	mod_interp,
	mod_autochip,
	mod_autochip_size_force,
	mod_autochip_size_scan,
	mod_autochip_scan_threshold,

	snd_streambuffersize,
	
	snd_mididevice,
	snd_outputrate,

	NUM_INT_CONFIGS
};

enum EFloatConfigKey
{
	fluid_gain,
	fluid_reverb_roomsize,
	fluid_reverb_damping,
	fluid_reverb_width,
	fluid_reverb_level,
	fluid_chorus_level,
	fluid_chorus_speed,
	fluid_chorus_depth,

	timidity_drum_power,
	timidity_tempo_adjust,
	min_sustain_time,

	gme_stereodepth,
	mod_dumb_mastervolume,

	snd_musicvolume,
	relative_volume,
	snd_mastervolume,

	NUM_FLOAT_CONFIGS
};

enum EStringConfigKey
{
	adl_custom_bank,
	fluid_lib,
	fluid_patchset,
	opn_custom_bank,
	gus_config,
	gus_patchdir,
	timidity_config,
	wildmidi_config,

	NUM_STRING_CONFIGS
};

}

struct Callbacks
{
	// Callbacks the client can install to capture messages from the backends
	// or to provide sound font data.
	
	// The message callbacks are optional, without them the output goes to stdout.
	void (*WildMidi_MessageFunc)(const char* wmfmt, va_list args) = nullptr;
	void (*GUS_MessageFunc)(int type, int verbosity_level, const char* fmt, ...) = nullptr;
	void (*Timidity_Messagefunc)(int type, int verbosity_level, const char* fmt, ...) = nullptr;
	int (*Fluid_MessageFunc)(const char *fmt, ...) = nullptr;
	
	// The sound font callbacks are for allowing the client to customize sound font management
	// Without them only paths to real files can be used.
	const char *(*PathForSoundfont)(const char *name, int type) = nullptr;
	MusicIO::SoundFontReaderInterface *(*OpenSoundFont)(const char* name, int type) = nullptr;
	
	// Used to handle client-specific path macros. If not set, the path may not contain any special tokens that may need expansion.
	std::string (*NicePath)(const char *path) = nullptr;

	// For playing modules with compressed samples.
	short* (*DumbVorbisDecode)(int outlen, const void* oggstream, int sizebytes);

};

// Sets callbacks for functionality that the client needs to provide.
void SetCallbacks(const Callbacks *callbacks);
// Sets GenMidi data for OPL playback. If this isn't provided the OPL synth will not work.
void SetGenMidi(const uint8_t* data);
// Set default bank for OPN. Without this OPN only works with custom banks.
void SetWgOpn(const void* data, unsigned len);
// Set DMXGUS data for running the GUS synth in actual GUS mode.
void SetDmxGus(const void* data, unsigned len);

// These exports is needed by the MIDI dumpers which need to remain on the client side.
class MIDISource;	// abstract for the client
EMIDIType IdentifyMIDIType(uint32_t *id, int size);
MIDISource *CreateMIDISource(const uint8_t *data, size_t length, EMIDIType miditype);

class MusInfo;
// Configuration interface. The return value specifies if a music restart is needed.
// RealValue should be written back to the CVAR or whatever other method the client uses to store configuration state.
bool ChangeMusicSetting(ZMusic::EIntConfigKey key, MusInfo *song, int value, int *pRealValue = nullptr);
bool ChangeMusicSetting(ZMusic::EFloatConfigKey key, MusInfo* song, float value, float *pRealValue = nullptr);
bool ChangeMusicSetting(ZMusic::EStringConfigKey key, MusInfo* song, const char *value);
