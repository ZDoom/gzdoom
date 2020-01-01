#pragma once

#include "mididefs.h"
#include "../../music_common/fileio.h"

enum EIntConfigKey
{
	zmusic_adl_chips_count,
	zmusic_adl_emulator_id,
	zmusic_adl_run_at_pcm_rate,
	zmusic_adl_fullpan,
	zmusic_adl_bank,
	zmusic_adl_use_custom_bank,
	zmusic_adl_volume_model,

	zmusic_fluid_reverb,
	zmusic_fluid_chorus,
	zmusic_fluid_voices,
	zmusic_fluid_interp,
	zmusic_fluid_samplerate,
	zmusic_fluid_threads,
	zmusic_fluid_chorus_voices,
	zmusic_fluid_chorus_type,

	zmusic_opl_numchips,
	zmusic_opl_core,
	zmusic_opl_fullpan,

	zmusic_opn_chips_count,
	zmusic_opn_emulator_id,
	zmusic_opn_run_at_pcm_rate,
	zmusic_opn_fullpan,
	zmusic_opn_use_custom_bank,

	zmusic_gus_dmxgus,
	zmusic_gus_midi_voices,
	zmusic_gus_memsize,

	zmusic_timidity_modulation_wheel,
	zmusic_timidity_portamento,
	zmusic_timidity_reverb,
	zmusic_timidity_reverb_level,
	zmusic_timidity_chorus,
	zmusic_timidity_surround_chorus,
	zmusic_timidity_channel_pressure,
	zmusic_timidity_lpf_def,
	zmusic_timidity_temper_control,
	zmusic_timidity_modulation_envelope,
	zmusic_timidity_overlap_voice_allow,
	zmusic_timidity_drum_effect,
	zmusic_timidity_pan_delay,
	zmusic_timidity_key_adjust,

	zmusic_wildmidi_reverb,
	zmusic_wildmidi_enhanced_resampling,

	zmusic_snd_midiprecache,

	zmusic_mod_samplerate,
	zmusic_mod_volramp,
	zmusic_mod_interp,
	zmusic_mod_autochip,
	zmusic_mod_autochip_size_force,
	zmusic_mod_autochip_size_scan,
	zmusic_mod_autochip_scan_threshold,

	zmusic_snd_streambuffersize,
	
	zmusic_snd_mididevice,
	zmusic_snd_outputrate,

	NUM_ZMUSIC_INT_CONFIGS
};

enum EFloatConfigKey
{
	zmusic_fluid_gain,
	zmusic_fluid_reverb_roomsize,
	zmusic_fluid_reverb_damping,
	zmusic_fluid_reverb_width,
	zmusic_fluid_reverb_level,
	zmusic_fluid_chorus_level,
	zmusic_fluid_chorus_speed,
	zmusic_fluid_chorus_depth,

	zmusic_timidity_drum_power,
	zmusic_timidity_tempo_adjust,
	zmusic_min_sustain_time,

	zmusic_gme_stereodepth,
	zmusic_mod_dumb_mastervolume,

	zmusic_snd_musicvolume,
	zmusic_relative_volume,
	zmusic_snd_mastervolume,

	NUM_FLOAT_CONFIGS
};

enum EStringConfigKey
{
	zmusic_adl_custom_bank,
	zmusic_fluid_lib,
	zmusic_fluid_patchset,
	zmusic_opn_custom_bank,
	zmusic_gus_config,
	zmusic_gus_patchdir,
	zmusic_timidity_config,
	zmusic_wildmidi_config,

	NUM_STRING_CONFIGS
};

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


#ifndef ZMUSIC_INTERNAL
#define DLL_IMPORT // _declspec(dllimport)
typedef struct { int zm1; } *ZMusic_MidiSource;
typedef struct { int zm2; } *ZMusic_MusicStream;
#endif

#ifdef __cplusplus
extern "C"
{
#endif
	DLL_IMPORT const char* ZMusic_GetLastError();

	// Sets callbacks for functionality that the client needs to provide.
	DLL_IMPORT void ZMusic_SetCallbacks(const Callbacks* callbacks);
	// Sets GenMidi data for OPL playback. If this isn't provided the OPL synth will not work.
	DLL_IMPORT void ZMusic_SetGenMidi(const uint8_t* data);
	// Set default bank for OPN. Without this OPN only works with custom banks.
	DLL_IMPORT void ZMusic_SetWgOpn(const void* data, unsigned len);
	// Set DMXGUS data for running the GUS synth in actual GUS mode.
	DLL_IMPORT void ZMusic_SetDmxGus(const void* data, unsigned len);

	// These exports are needed by the MIDI dumpers which need to remain on the client side because the need access to the client's file system.
	DLL_IMPORT EMIDIType ZMusic_IdentifyMIDIType(uint32_t* id, int size);
	DLL_IMPORT ZMusic_MidiSource ZMusic_CreateMIDISource(const uint8_t* data, size_t length, EMIDIType miditype);
	DLL_IMPORT bool ZMusic_MIDIDumpWave(ZMusic_MidiSource source, EMidiDevice devtype, const char* devarg, const char* outname, int subsong, int samplerate);

	DLL_IMPORT ZMusic_MusicStream ZMusic_OpenSong(MusicIO::FileInterface* reader, EMidiDevice device, const char* Args);
	DLL_IMPORT ZMusic_MusicStream ZMusic_OpenCDSong(int track, int cdid = 0);

	DLL_IMPORT bool ZMusic_FillStream(ZMusic_MusicStream stream, void* buff, int len);
	DLL_IMPORT bool ZMusic_Start(ZMusic_MusicStream song, int subsong, bool loop);
	DLL_IMPORT void ZMusic_Pause(ZMusic_MusicStream song);
	DLL_IMPORT void ZMusic_Resume(ZMusic_MusicStream song);
	DLL_IMPORT void ZMusic_Update(ZMusic_MusicStream song);
	DLL_IMPORT bool ZMusic_IsPlaying(ZMusic_MusicStream song);
	DLL_IMPORT void ZMusic_Stop(ZMusic_MusicStream song);
	DLL_IMPORT void ZMusic_Close(ZMusic_MusicStream song);
	DLL_IMPORT bool ZMusic_SetSubsong(ZMusic_MusicStream song, int subsong);
	DLL_IMPORT bool ZMusic_IsLooping(ZMusic_MusicStream song);
	DLL_IMPORT bool ZMusic_IsMIDI(ZMusic_MusicStream song);
	DLL_IMPORT void ZMusic_VolumeChanged(ZMusic_MusicStream song);
	DLL_IMPORT bool ZMusic_WriteSMF(ZMusic_MidiSource source, const char* fn, int looplimit);
	DLL_IMPORT void ZMusic_GetStreamInfo(ZMusic_MusicStream song, SoundStreamInfo *info);
	// Configuration interface. The return value specifies if a music restart is needed.
	// RealValue should be written back to the CVAR or whatever other method the client uses to store configuration state.
	DLL_IMPORT bool ChangeMusicSettingInt(EIntConfigKey key, ZMusic_MusicStream song, int value, int* pRealValue = nullptr);
	DLL_IMPORT bool ChangeMusicSettingFloat(EFloatConfigKey key, ZMusic_MusicStream song, float value, float* pRealValue = nullptr);
	DLL_IMPORT bool ChangeMusicSettingString(EStringConfigKey key, ZMusic_MusicStream song, const char* value);
	DLL_IMPORT const char *ZMusic_GetStats(ZMusic_MusicStream song);

#ifdef __cplusplus
}

inline bool ChangeMusicSetting(EIntConfigKey key, ZMusic_MusicStream song, int value, int* pRealValue = nullptr)
{
	return ChangeMusicSettingInt(key, song, value, pRealValue);
}
inline bool ChangeMusicSetting(EFloatConfigKey key, ZMusic_MusicStream song, float value, float* pRealValue = nullptr)
{
	return ChangeMusicSettingFloat(key, song, value, pRealValue);
}
inline bool ChangeMusicSetting(EStringConfigKey key, ZMusic_MusicStream song, const char* value)
{
	return ChangeMusicSettingString(key, song, value);
}

#endif