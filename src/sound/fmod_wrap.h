
#ifndef FMOD_WRAP_H
#define FMOD_WRAP_H

#ifndef NO_FMOD

#if !defined(_WIN32) || defined(_MSC_VER)
// Use the real C++ interface if it's supported on this platform.
#include "fmod.hpp"
#else
// Use a wrapper C++ interface for non-Microsoft compilers on Windows.

#include "fmod.h"

// Create fake definitions for these structs so they can be subclassed.
struct FMOD_SYSTEM {};
struct FMOD_SOUND {};
struct FMOD_CHANNEL {};
struct FMOD_CHANNELGROUP {};
struct FMOD_SOUNDGROUP {};
struct FMOD_REVERB {};
struct FMOD_DSP {};
struct FMOD_DSPCONNECTION {};
struct FMOD_POLYGON {};
struct FMOD_GEOMETRY {};
struct FMOD_SYNCPOINT {};
/*
	Constant and defines
*/

/*
	FMOD Namespace
*/

namespace FMOD
{

	class System;
	class Sound;
	class Channel;
	class ChannelGroup;
	class SoundGroup;
	class Reverb;
	class DSP;
	class DSPConnection;
	class Geometry;

	/*
		FMOD global system functions (optional).
	*/
	inline FMOD_RESULT Memory_Initialize(void *poolmem, int poollen, FMOD_MEMORY_ALLOCCALLBACK useralloc, FMOD_MEMORY_REALLOCCALLBACK userrealloc, FMOD_MEMORY_FREECALLBACK userfree, FMOD_MEMORY_TYPE memtypeflags = (FMOD_MEMORY_NORMAL | FMOD_MEMORY_XBOX360_PHYSICAL)) { return FMOD_Memory_Initialize(poolmem, poollen, useralloc, userrealloc, userfree, memtypeflags); }
	//inline FMOD_RESULT Memory_GetStats  (int *currentalloced, int *maxalloced) { return FMOD_Memory_GetStats(currentalloced, maxalloced); }
	inline FMOD_RESULT Debug_SetLevel(FMOD_DEBUGLEVEL level)  { return FMOD_Debug_SetLevel(level); }
	inline FMOD_RESULT Debug_GetLevel(FMOD_DEBUGLEVEL *level) { return FMOD_Debug_GetLevel(level); }
	inline FMOD_RESULT File_SetDiskBusy(int busy) { return FMOD_File_SetDiskBusy(busy); }
	inline FMOD_RESULT File_GetDiskBusy(int *busy) { return FMOD_File_GetDiskBusy(busy); }

	/*
		FMOD System factory functions.
	*/
	inline FMOD_RESULT System_Create(System **system) { return FMOD_System_Create((FMOD_SYSTEM **)system); }

	/*
	   'System' API
	*/

	class System : FMOD_SYSTEM
	{
	  private:

		System();   /* Constructor made private so user cannot statically instance a System class.  
					   System_Create must be used. */
	  public:

		  FMOD_RESULT release                () { return FMOD_System_Release(this); }

		// Pre-init functions.
		  FMOD_RESULT setOutput              (FMOD_OUTPUTTYPE output) { return FMOD_System_SetOutput(this, output); }
		  FMOD_RESULT getOutput              (FMOD_OUTPUTTYPE *output) { return FMOD_System_GetOutput(this, output); }
		  FMOD_RESULT getNumDrivers          (int *numdrivers) { return FMOD_System_GetNumDrivers(this, numdrivers); }
		  FMOD_RESULT getDriverInfo          (int id, char *name, int namelen, FMOD_GUID *guid) { return FMOD_System_GetDriverInfo(this, id, name, namelen, guid); }
		  FMOD_RESULT getDriverCaps          (int id, FMOD_CAPS *caps, int *minfrequency, int *maxfrequency, FMOD_SPEAKERMODE *controlpanelspeakermode) { return FMOD_System_GetDriverCaps(this, id, caps, minfrequency, maxfrequency, controlpanelspeakermode); }
		  FMOD_RESULT setDriver              (int driver) { return FMOD_System_SetDriver(this, driver); }
		  FMOD_RESULT getDriver              (int *driver) { return FMOD_System_GetDriver(this, driver); }
		  FMOD_RESULT setHardwareChannels    (int min2d, int max2d, int min3d, int max3d) { return FMOD_System_SetHardwareChannels(this, min2d, max2d, min3d, max3d); }
		  FMOD_RESULT setSoftwareChannels    (int numsoftwarechannels) { return FMOD_System_SetSoftwareChannels(this, numsoftwarechannels); }
		  FMOD_RESULT getSoftwareChannels    (int *numsoftwarechannels) { return FMOD_System_GetSoftwareChannels(this, numsoftwarechannels); }
		  FMOD_RESULT setSoftwareFormat      (int samplerate, FMOD_SOUND_FORMAT format, int numoutputchannels, int maxinputchannels, FMOD_DSP_RESAMPLER resamplemethod) { return FMOD_System_SetSoftwareFormat(this, samplerate, format, numoutputchannels, maxinputchannels, resamplemethod); }
		  FMOD_RESULT getSoftwareFormat      (int *samplerate, FMOD_SOUND_FORMAT *format, int *numoutputchannels, int *maxinputchannels, FMOD_DSP_RESAMPLER *resamplemethod, int *bits) { return FMOD_System_GetSoftwareFormat(this, samplerate, format, numoutputchannels, maxinputchannels, resamplemethod, bits); }
		  FMOD_RESULT setDSPBufferSize       (unsigned int bufferlength, int numbuffers) { return FMOD_System_SetDSPBufferSize(this, bufferlength, numbuffers); }
		  FMOD_RESULT getDSPBufferSize       (unsigned int *bufferlength, int *numbuffers) { return FMOD_System_GetDSPBufferSize(this, bufferlength, numbuffers); }
		  FMOD_RESULT setFileSystem          (FMOD_FILE_OPENCALLBACK useropen, FMOD_FILE_CLOSECALLBACK userclose, FMOD_FILE_READCALLBACK userread, FMOD_FILE_SEEKCALLBACK userseek, int blockalign) { return FMOD_System_SetFileSystem(this, useropen, userclose, userread, userseek, blockalign); }
		  FMOD_RESULT attachFileSystem       (FMOD_FILE_OPENCALLBACK useropen, FMOD_FILE_CLOSECALLBACK userclose, FMOD_FILE_READCALLBACK userread, FMOD_FILE_SEEKCALLBACK userseek) { return FMOD_System_AttachFileSystem(this, useropen, userclose, userread, userseek); }
		  FMOD_RESULT setAdvancedSettings    (FMOD_ADVANCEDSETTINGS *settings) { return FMOD_System_SetAdvancedSettings(this, settings); }
		  FMOD_RESULT getAdvancedSettings    (FMOD_ADVANCEDSETTINGS *settings) { return FMOD_System_GetAdvancedSettings(this, settings); }
		  FMOD_RESULT setSpeakerMode         (FMOD_SPEAKERMODE speakermode) { return FMOD_System_SetSpeakerMode(this, speakermode); }
		  FMOD_RESULT getSpeakerMode         (FMOD_SPEAKERMODE *speakermode) { return FMOD_System_GetSpeakerMode(this, speakermode); }
		  FMOD_RESULT setCallback            (FMOD_SYSTEM_CALLBACK callback) { return FMOD_System_SetCallback(this, callback); }

		// Plug-in support
		  FMOD_RESULT setPluginPath          (const char *path) { return FMOD_System_SetPluginPath(this, path); }
		  FMOD_RESULT loadPlugin             (const char *filename, unsigned int *handle, unsigned int priority = 0) { return FMOD_System_LoadPlugin(this, filename, handle, priority); }
		  FMOD_RESULT unloadPlugin           (unsigned int handle) { return FMOD_System_UnloadPlugin(this, handle); }
		  FMOD_RESULT getNumPlugins          (FMOD_PLUGINTYPE plugintype, int *numplugins) { return FMOD_System_GetNumPlugins(this, plugintype, numplugins); }
		  FMOD_RESULT getPluginHandle        (FMOD_PLUGINTYPE plugintype, int index, unsigned int *handle) { return FMOD_System_GetPluginHandle(this, plugintype, index, handle); }
		  FMOD_RESULT getPluginInfo          (unsigned int handle, FMOD_PLUGINTYPE *plugintype, char *name, int namelen, unsigned int *version) { return FMOD_System_GetPluginInfo(this, handle, plugintype, name, namelen, version); }
		  FMOD_RESULT setOutputByPlugin      (unsigned int handle) { return FMOD_System_SetOutputByPlugin(this, handle); }
		  FMOD_RESULT getOutputByPlugin      (unsigned int *handle) { return FMOD_System_GetOutputByPlugin(this, handle); }
		  FMOD_RESULT createCodec            (FMOD_CODEC_DESCRIPTION *description, unsigned int priority = 0) { return FMOD_System_CreateCodec(this, description, priority); }

		// Init/Close
		  FMOD_RESULT init                   (int maxchannels, FMOD_INITFLAGS flags, void *extradriverdata) { return FMOD_System_Init(this, maxchannels, flags, extradriverdata); }
		  FMOD_RESULT close                  () { return FMOD_System_Close(this); }

		// General post-init system functions
		  FMOD_RESULT update                 ()        /* IMPORTANT! CALL THIS ONCE PER FRAME! */ { return FMOD_System_Update(this); }

		  FMOD_RESULT set3DSettings          (float dopplerscale, float distancefactor, float rolloffscale) { return FMOD_System_Set3DSettings(this, dopplerscale, distancefactor, rolloffscale); }
		  FMOD_RESULT get3DSettings          (float *dopplerscale, float *distancefactor, float *rolloffscale) { return FMOD_System_Get3DSettings(this, dopplerscale, distancefactor, rolloffscale); }
		  FMOD_RESULT set3DNumListeners      (int numlisteners) { return FMOD_System_Set3DNumListeners(this, numlisteners); }
		  FMOD_RESULT get3DNumListeners      (int *numlisteners) { return FMOD_System_Get3DNumListeners(this, numlisteners); }
		  FMOD_RESULT set3DListenerAttributes(int listener, const FMOD_VECTOR *pos, const FMOD_VECTOR *vel, const FMOD_VECTOR *forward, const FMOD_VECTOR *up) { return FMOD_System_Set3DListenerAttributes(this, listener, pos, vel, forward, up); }
		  FMOD_RESULT get3DListenerAttributes(int listener, FMOD_VECTOR *pos, FMOD_VECTOR *vel, FMOD_VECTOR *forward, FMOD_VECTOR *up) { return FMOD_System_Get3DListenerAttributes(this, listener, pos, vel, forward, up); }
		  FMOD_RESULT set3DRolloffCallback   (FMOD_3D_ROLLOFFCALLBACK callback) { return FMOD_System_Set3DRolloffCallback(this, callback); }
		  FMOD_RESULT set3DSpeakerPosition   (FMOD_SPEAKER speaker, float x, float y, bool active) { return FMOD_System_Set3DSpeakerPosition(this, speaker, x, y, active); }
		  FMOD_RESULT get3DSpeakerPosition   (FMOD_SPEAKER speaker, float *x, float *y, bool *active) { FMOD_BOOL b; FMOD_RESULT res = FMOD_System_Get3DSpeakerPosition(this, speaker, x, y, &b); *active = b; return res; }

		  FMOD_RESULT setStreamBufferSize    (unsigned int filebuffersize, FMOD_TIMEUNIT filebuffersizetype) { return FMOD_System_SetStreamBufferSize(this, filebuffersize, filebuffersizetype); }
		  FMOD_RESULT getStreamBufferSize    (unsigned int *filebuffersize, FMOD_TIMEUNIT *filebuffersizetype) { return FMOD_System_GetStreamBufferSize(this, filebuffersize, filebuffersizetype); }

		// System information functions.
		  FMOD_RESULT getVersion             (unsigned int *version) { return FMOD_System_GetVersion(this, version); }
		  FMOD_RESULT getOutputHandle        (void **handle) { return FMOD_System_GetOutputHandle(this, handle); }
		  FMOD_RESULT getChannelsPlaying     (int *channels) { return FMOD_System_GetChannelsPlaying(this, channels); }
		  FMOD_RESULT getHardwareChannels    (int *num2d, int *num3d, int *total) { return FMOD_System_GetHardwareChannels(this, num2d, num3d, total); }
#if FMOD_VERSION < 0x42501
		  FMOD_RESULT getCPUUsage            (float *dsp, float *stream, float *update, float *total) { return FMOD_System_GetCPUUsage(this, dsp, stream, update, total); }
#else
		  FMOD_RESULT getCPUUsage            (float *dsp, float *stream, float *geometry, float *update, float *total) { return FMOD_System_GetCPUUsage(this, dsp, stream, geometry, update, total); }
#endif
		  FMOD_RESULT getSoundRAM            (int *currentalloced, int *maxalloced, int *total) { return FMOD_System_GetSoundRAM(this, currentalloced, maxalloced, total); }
		  FMOD_RESULT getNumCDROMDrives      (int *numdrives) { return FMOD_System_GetNumCDROMDrives(this, numdrives); }
		  FMOD_RESULT getCDROMDriveName      (int drive, char *drivename, int drivenamelen, char *scsiname, int scsinamelen, char *devicename, int devicenamelen) { return FMOD_System_GetCDROMDriveName(this, drive, drivename, drivenamelen, scsiname, scsinamelen, devicename, devicenamelen); }
		  FMOD_RESULT getSpectrum            (float *spectrumarray, int numvalues, int channeloffset, FMOD_DSP_FFT_WINDOW windowtype) { return FMOD_System_GetSpectrum(this, spectrumarray, numvalues, channeloffset, windowtype); }
		  FMOD_RESULT getWaveData            (float *wavearray, int numvalues, int channeloffset) { return FMOD_System_GetWaveData(this, wavearray, numvalues, channeloffset); }

		// Sound/DSP/Channel/FX creation and retrieval.
		  FMOD_RESULT createSound            (const char *name_or_data, FMOD_MODE mode, FMOD_CREATESOUNDEXINFO *exinfo, Sound **sound) { return FMOD_System_CreateSound(this, name_or_data, mode, exinfo, (FMOD_SOUND **)sound); }
		  FMOD_RESULT createStream           (const char *name_or_data, FMOD_MODE mode, FMOD_CREATESOUNDEXINFO *exinfo, Sound **sound) { return FMOD_System_CreateStream(this, name_or_data, mode, exinfo, (FMOD_SOUND **)sound); }
		  FMOD_RESULT createDSP              (FMOD_DSP_DESCRIPTION *description, DSP **dsp) { return FMOD_System_CreateDSP(this, description, (FMOD_DSP **)dsp); }
		  FMOD_RESULT createDSPByType        (FMOD_DSP_TYPE type, DSP **dsp) { return FMOD_System_CreateDSPByType(this, type, (FMOD_DSP **)dsp); }
		  FMOD_RESULT createChannelGroup     (const char *name, ChannelGroup **channelgroup) { return FMOD_System_CreateChannelGroup(this, name, (FMOD_CHANNELGROUP **)channelgroup); }
		  FMOD_RESULT createSoundGroup       (const char *name, SoundGroup **soundgroup) { return FMOD_System_CreateSoundGroup(this, name, (FMOD_SOUNDGROUP **)soundgroup); }
		  FMOD_RESULT createReverb           (Reverb **reverb) { return FMOD_System_CreateReverb(this, (FMOD_REVERB **)reverb); }

		  FMOD_RESULT playSound              (FMOD_CHANNELINDEX channelid, Sound *sound, bool paused, Channel **channel) { return FMOD_System_PlaySound(this, channelid, (FMOD_SOUND *)sound, paused, (FMOD_CHANNEL **)channel); }
		  FMOD_RESULT playDSP                (FMOD_CHANNELINDEX channelid, DSP *dsp, bool paused, Channel **channel) { return FMOD_System_PlayDSP(this, channelid, (FMOD_DSP *)dsp, paused, (FMOD_CHANNEL **)channel); }
		  FMOD_RESULT getChannel             (int channelid, Channel **channel) { return FMOD_System_GetChannel(this, channelid, (FMOD_CHANNEL **)channel); }
		  FMOD_RESULT getMasterChannelGroup  (ChannelGroup **channelgroup) { return FMOD_System_GetMasterChannelGroup(this, (FMOD_CHANNELGROUP **)channelgroup); }
		  FMOD_RESULT getMasterSoundGroup    (SoundGroup **soundgroup) { return FMOD_System_GetMasterSoundGroup(this, (FMOD_SOUNDGROUP **)soundgroup); }

		// Reverb API
		  FMOD_RESULT setReverbProperties    (const FMOD_REVERB_PROPERTIES *prop) { return FMOD_System_SetReverbProperties(this, prop); }
		  FMOD_RESULT getReverbProperties    (FMOD_REVERB_PROPERTIES *prop) { return FMOD_System_GetReverbProperties(this, prop); }
		  FMOD_RESULT setReverbAmbientProperties(FMOD_REVERB_PROPERTIES *prop) { return FMOD_System_SetReverbAmbientProperties(this, prop); }
		  FMOD_RESULT getReverbAmbientProperties(FMOD_REVERB_PROPERTIES *prop) { return FMOD_System_GetReverbAmbientProperties(this, prop); }

		// System level DSP access.
		  FMOD_RESULT getDSPHead             (DSP **dsp) { return FMOD_System_GetDSPHead(this, (FMOD_DSP **)dsp); }
		  FMOD_RESULT addDSP                 (DSP *dsp, DSPConnection **connection) { return FMOD_System_AddDSP(this, (FMOD_DSP *)dsp, (FMOD_DSPCONNECTION**)dsp); }
		  FMOD_RESULT lockDSP                () { return FMOD_System_LockDSP(this); }
		  FMOD_RESULT unlockDSP              () { return FMOD_System_UnlockDSP(this); }
		  FMOD_RESULT getDSPClock            (unsigned int *hi, unsigned int *lo) { return FMOD_System_GetDSPClock(this, hi, lo); }

		// Recording API.
		  FMOD_RESULT getRecordNumDrivers    (int *numdrivers) { return FMOD_System_GetRecordNumDrivers(this, numdrivers); }
		  FMOD_RESULT getRecordDriverInfo    (int id, char *name, int namelen, FMOD_GUID *guid) { return FMOD_System_GetRecordDriverInfo(this, id, name, namelen, guid); }
		  FMOD_RESULT getRecordDriverCaps    (int id, FMOD_CAPS *caps, int *minfrequency, int *maxfrequency) { return FMOD_System_GetRecordDriverCaps(this, id, caps, minfrequency, maxfrequency); }
		  FMOD_RESULT getRecordPosition      (int id, unsigned int *position) { return FMOD_System_GetRecordPosition(this, id, position); }

		  FMOD_RESULT recordStart            (int id, Sound *sound, bool loop) { return FMOD_System_RecordStart(this, id, (FMOD_SOUND *)sound, loop); }
		  FMOD_RESULT recordStop             (int id) { return FMOD_System_RecordStop(this, id); }
		  FMOD_RESULT isRecording            (int id, bool *recording) { FMOD_BOOL b; FMOD_RESULT res = FMOD_System_IsRecording(this, id, &b); *recording = b; return res; }

		// Geometry API.
		  FMOD_RESULT createGeometry         (int maxpolygons, int maxvertices, Geometry **geometry) { return FMOD_System_CreateGeometry(this, maxpolygons, maxvertices, (FMOD_GEOMETRY **)geometry); }
		  FMOD_RESULT setGeometrySettings    (float maxworldsize) { return FMOD_System_SetGeometrySettings(this, maxworldsize); }
		  FMOD_RESULT getGeometrySettings    (float *maxworldsize) { return FMOD_System_GetGeometrySettings(this, maxworldsize); }
		  FMOD_RESULT loadGeometry           (const void *data, int datasize, Geometry **geometry) { return FMOD_System_LoadGeometry(this, data, datasize, (FMOD_GEOMETRY **)geometry); }

		// Network functions.
		  FMOD_RESULT setNetworkProxy        (const char *proxy) { return FMOD_System_SetNetworkProxy(this, proxy); }
		  FMOD_RESULT getNetworkProxy        (char *proxy, int proxylen) { return FMOD_System_GetNetworkProxy(this, proxy, proxylen); }
		  FMOD_RESULT setNetworkTimeout      (int timeout) { return FMOD_System_SetNetworkTimeout(this, timeout); }
		  FMOD_RESULT getNetworkTimeout      (int *timeout) { return FMOD_System_GetNetworkTimeout(this, timeout); }

		// Userdata set/get.
		  FMOD_RESULT setUserData            (void *userdata) { return FMOD_System_SetUserData(this, userdata); }
		  FMOD_RESULT getUserData            (void **userdata) { return FMOD_System_GetUserData(this, userdata); }

		  FMOD_RESULT getMemoryInfo          (unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused, unsigned int *memoryused_array) { return FMOD_System_GetMemoryInfo(this, memorybits, event_memorybits, memoryused, memoryused_array); }
	};

	/*
		'Sound' API
	*/
	class Sound : FMOD_SOUND
	{
	  private:

		Sound();   /* Constructor made private so user cannot statically instance a Sound class.
					  Appropriate Sound creation or retrieval function must be used. */
	  public:

		  FMOD_RESULT release                () { return FMOD_Sound_Release(this); }
		  FMOD_RESULT getSystemObject        (System **system) { return FMOD_Sound_GetSystemObject(this, (FMOD_SYSTEM **)system); }

		// Standard sound manipulation functions.
		  FMOD_RESULT lock                   (unsigned int offset, unsigned int length, void **ptr1, void **ptr2, unsigned int *len1, unsigned int *len2) { return FMOD_Sound_Lock(this, offset, length, ptr1, ptr2, len1, len2); }
		  FMOD_RESULT unlock                 (void *ptr1, void *ptr2, unsigned int len1, unsigned int len2) { return FMOD_Sound_Unlock(this, ptr1, ptr2, len1, len2); }
		  FMOD_RESULT setDefaults            (float frequency, float volume, float pan, int priority) { return FMOD_Sound_SetDefaults(this, frequency, volume, pan, priority); }
		  FMOD_RESULT getDefaults            (float *frequency, float *volume, float *pan, int *priority) { return FMOD_Sound_GetDefaults(this, frequency, volume, pan, priority); }
		  FMOD_RESULT setVariations          (float frequencyvar, float volumevar, float panvar) { return FMOD_Sound_SetVariations(this, frequencyvar, volumevar, panvar); }
		  FMOD_RESULT getVariations          (float *frequencyvar, float *volumevar, float *panvar) { return FMOD_Sound_GetVariations(this, frequencyvar, volumevar, panvar); }
		  FMOD_RESULT set3DMinMaxDistance    (float min, float max) { return FMOD_Sound_Set3DMinMaxDistance(this, min, max); }
		  FMOD_RESULT get3DMinMaxDistance    (float *min, float *max) { return FMOD_Sound_Get3DMinMaxDistance(this, min, max); }
		  FMOD_RESULT set3DConeSettings      (float insideconeangle, float outsideconeangle, float outsidevolume) { return FMOD_Sound_Set3DConeSettings(this, insideconeangle, outsideconeangle, outsidevolume); }
		  FMOD_RESULT get3DConeSettings      (float *insideconeangle, float *outsideconeangle, float *outsidevolume) { return FMOD_Sound_Get3DConeSettings(this, insideconeangle, outsideconeangle, outsidevolume); }
		  FMOD_RESULT set3DCustomRolloff     (FMOD_VECTOR *points, int numpoints) { return FMOD_Sound_Set3DCustomRolloff(this, points, numpoints); }
		  FMOD_RESULT get3DCustomRolloff     (FMOD_VECTOR **points, int *numpoints) { return FMOD_Sound_Get3DCustomRolloff(this, points, numpoints); }
		  FMOD_RESULT setSubSound            (int index, Sound *subsound) { return FMOD_Sound_SetSubSound(this, index, subsound); }
		  FMOD_RESULT getSubSound            (int index, Sound **subsound) { return FMOD_Sound_GetSubSound(this, index, (FMOD_SOUND **)subsound); }
		  FMOD_RESULT setSubSoundSentence    (int *subsoundlist, int numsubsounds) { return FMOD_Sound_SetSubSoundSentence(this, subsoundlist, numsubsounds); }
		  FMOD_RESULT getName                (char *name, int namelen) { return FMOD_Sound_GetName(this, name, namelen); }
		  FMOD_RESULT getLength              (unsigned int *length, FMOD_TIMEUNIT lengthtype) { return FMOD_Sound_GetLength(this, length, lengthtype); }
		  FMOD_RESULT getFormat              (FMOD_SOUND_TYPE *type, FMOD_SOUND_FORMAT *format, int *channels, int *bits) { return FMOD_Sound_GetFormat(this, type, format, channels, bits); }
		  FMOD_RESULT getNumSubSounds        (int *numsubsounds) { return FMOD_Sound_GetNumSubSounds(this, numsubsounds); }
		  FMOD_RESULT getNumTags             (int *numtags, int *numtagsupdated) { return FMOD_Sound_GetNumTags(this, numtags, numtagsupdated); }
		  FMOD_RESULT getTag                 (const char *name, int index, FMOD_TAG *tag) { return FMOD_Sound_GetTag(this, name, index, tag); }
		  FMOD_RESULT getOpenState           (FMOD_OPENSTATE *openstate, unsigned int *percentbuffered, bool *starving) { FMOD_BOOL b; FMOD_RESULT res = FMOD_Sound_GetOpenState(this, openstate, percentbuffered, &b); *starving = b; return res; }
		  FMOD_RESULT readData               (void *buffer, unsigned int lenbytes, unsigned int *read) { return FMOD_Sound_ReadData(this, buffer, lenbytes, read); }
		  FMOD_RESULT seekData               (unsigned int pcm) { return FMOD_Sound_SeekData(this, pcm); }

		  FMOD_RESULT setSoundGroup          (SoundGroup *soundgroup) { return FMOD_Sound_SetSoundGroup(this, (FMOD_SOUNDGROUP *)soundgroup); }
		  FMOD_RESULT getSoundGroup          (SoundGroup **soundgroup) { return FMOD_Sound_GetSoundGroup(this, (FMOD_SOUNDGROUP **)soundgroup); }

		// Synchronization point API.  These points can come from markers embedded in wav files, and can also generate channel callbacks.        
		  FMOD_RESULT getNumSyncPoints       (int *numsyncpoints) { return FMOD_Sound_GetNumSyncPoints(this, numsyncpoints); }
		  FMOD_RESULT getSyncPoint           (int index, FMOD_SYNCPOINT **point) { return FMOD_Sound_GetSyncPoint(this, index, point); }
		  FMOD_RESULT getSyncPointInfo       (FMOD_SYNCPOINT *point, char *name, int namelen, unsigned int *offset, FMOD_TIMEUNIT offsettype) { return FMOD_Sound_GetSyncPointInfo(this, point, name, namelen, offset, offsettype); }
		  FMOD_RESULT addSyncPoint           (unsigned int offset, FMOD_TIMEUNIT offsettype, const char *name, FMOD_SYNCPOINT **point) { return FMOD_Sound_AddSyncPoint(this, offset, offsettype, name, point); }
		  FMOD_RESULT deleteSyncPoint        (FMOD_SYNCPOINT *point) { return FMOD_Sound_DeleteSyncPoint(this, point); }

		// Functions also in Channel class but here they are the 'default' to save having to change it in Channel all the time.
		  FMOD_RESULT setMode                (FMOD_MODE mode) { return FMOD_Sound_SetMode(this, mode); }
		  FMOD_RESULT getMode                (FMOD_MODE *mode) { return FMOD_Sound_GetMode(this, mode); }
		  FMOD_RESULT setLoopCount           (int loopcount) { return FMOD_Sound_SetLoopCount(this, loopcount); }
		  FMOD_RESULT getLoopCount           (int *loopcount) { return FMOD_Sound_GetLoopCount(this, loopcount); }
		  FMOD_RESULT setLoopPoints          (unsigned int loopstart, FMOD_TIMEUNIT loopstarttype, unsigned int loopend, FMOD_TIMEUNIT loopendtype) { return FMOD_Sound_SetLoopPoints(this, loopstart, loopstarttype, loopend, loopendtype); }
		  FMOD_RESULT getLoopPoints          (unsigned int *loopstart, FMOD_TIMEUNIT loopstarttype, unsigned int *loopend, FMOD_TIMEUNIT loopendtype) { return FMOD_Sound_GetLoopPoints(this, loopstart, loopstarttype, loopend, loopendtype); }

		// Userdata set/get.
		  FMOD_RESULT setUserData            (void *userdata) { return FMOD_Sound_SetUserData(this, userdata); }
		  FMOD_RESULT getUserData            (void **userdata) { return FMOD_Sound_GetUserData(this, userdata); }

		  FMOD_RESULT getMemoryInfo          (unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused, unsigned int *memoryused_array) { return FMOD_Sound_GetMemoryInfo(this, memorybits, event_memorybits, memoryused, memoryused_array); }
	};

	/*
		'Channel' API.
	*/ 
	class Channel : FMOD_CHANNEL
	{
	  private:

		Channel();   /* Constructor made private so user cannot statically instance a Channel class.
						Appropriate Channel creation or retrieval function must be used. */
	  public:

		  FMOD_RESULT getSystemObject        (System **system) { return FMOD_Channel_GetSystemObject(this, (FMOD_SYSTEM **)system); }

		  FMOD_RESULT stop                   () { return FMOD_Channel_Stop(this); }
		  FMOD_RESULT setPaused              (bool paused) { return FMOD_Channel_SetPaused(this, paused); }
		  FMOD_RESULT getPaused              (bool *paused) { FMOD_BOOL b; FMOD_RESULT res = FMOD_Channel_GetPaused(this, &b); *paused = b; return res; }
		  FMOD_RESULT setVolume              (float volume) { return FMOD_Channel_SetVolume(this, volume); }
		  FMOD_RESULT getVolume              (float *volume) { return FMOD_Channel_GetVolume(this, volume); }
		  FMOD_RESULT setFrequency           (float frequency) { return FMOD_Channel_SetFrequency(this, frequency); }
		  FMOD_RESULT getFrequency           (float *frequency) { return FMOD_Channel_GetFrequency(this, frequency); }
		  FMOD_RESULT setPan                 (float pan) { return FMOD_Channel_SetPan(this, pan); }
		  FMOD_RESULT getPan                 (float *pan) { return FMOD_Channel_GetPan(this, pan); }
		  FMOD_RESULT setDelay               (FMOD_DELAYTYPE delaytype, unsigned int delayhi, unsigned int delaylo) { return FMOD_Channel_SetDelay(this, delaytype, delayhi, delaylo); }
		  FMOD_RESULT getDelay               (FMOD_DELAYTYPE delaytype, unsigned int *delayhi, unsigned int *delaylo) { return FMOD_Channel_GetDelay(this, delaytype, delayhi, delaylo); }
		  FMOD_RESULT setSpeakerMix          (float frontleft, float frontright, float center, float lfe, float backleft, float backright, float sideleft, float sideright) { return FMOD_Channel_SetSpeakerMix(this, frontleft, frontright, center, lfe, backleft, backright, sideleft, sideright); }
		  FMOD_RESULT getSpeakerMix          (float *frontleft, float *frontright, float *center, float *lfe, float *backleft, float *backright, float *sideleft, float *sideright) { return FMOD_Channel_GetSpeakerMix(this, frontleft, frontright, center, lfe, backleft, backright, sideleft, sideright); }
		  FMOD_RESULT setSpeakerLevels       (FMOD_SPEAKER speaker, float *levels, int numlevels) { return FMOD_Channel_SetSpeakerLevels(this, speaker, levels, numlevels); }
		  FMOD_RESULT getSpeakerLevels       (FMOD_SPEAKER speaker, float *levels, int numlevels) { return FMOD_Channel_GetSpeakerLevels(this, speaker, levels, numlevels); }
		  FMOD_RESULT setInputChannelMix     (float *levels, int numlevels) { return FMOD_Channel_SetInputChannelMix(this, levels, numlevels); }
		  FMOD_RESULT getInputChannelMix     (float *levels, int numlevels) { return FMOD_Channel_GetInputChannelMix(this, levels, numlevels); }
		  FMOD_RESULT setMute                (bool mute) { return FMOD_Channel_SetMute(this, mute); }
		  FMOD_RESULT getMute                (bool *mute) { FMOD_BOOL b; FMOD_RESULT res = FMOD_Channel_GetMute(this, &b); *mute = b; return res; }
		  FMOD_RESULT setPriority            (int priority) { return FMOD_Channel_SetPriority(this, priority); }
		  FMOD_RESULT getPriority            (int *priority) { return FMOD_Channel_GetPriority(this, priority); }
		  FMOD_RESULT setPosition            (unsigned int position, FMOD_TIMEUNIT postype) { return FMOD_Channel_SetPosition(this, position, postype); }
		  FMOD_RESULT getPosition            (unsigned int *position, FMOD_TIMEUNIT postype) { return FMOD_Channel_GetPosition(this, position, postype); }
		  FMOD_RESULT setReverbProperties    (const FMOD_REVERB_CHANNELPROPERTIES *prop) { return FMOD_Channel_SetReverbProperties(this, prop); }
		  FMOD_RESULT getReverbProperties    (FMOD_REVERB_CHANNELPROPERTIES *prop) { return FMOD_Channel_GetReverbProperties(this, prop); }

		  FMOD_RESULT setChannelGroup        (ChannelGroup *channelgroup) { return FMOD_Channel_SetChannelGroup(this, (FMOD_CHANNELGROUP *)channelgroup); }
		  FMOD_RESULT getChannelGroup        (ChannelGroup **channelgroup) { return FMOD_Channel_GetChannelGroup(this, (FMOD_CHANNELGROUP **)channelgroup); }
		  FMOD_RESULT setCallback            (FMOD_CHANNEL_CALLBACK callback) { return FMOD_Channel_SetCallback(this, callback); }
		  FMOD_RESULT setLowPassGain         (float gain) { return FMOD_Channel_SetLowPassGain(this, gain); }
		  FMOD_RESULT getLowPassGain         (float *gain) { return FMOD_Channel_GetLowPassGain(this, gain); }

		// 3D functionality.
		  FMOD_RESULT set3DAttributes        (const FMOD_VECTOR *pos, const FMOD_VECTOR *vel) { return FMOD_Channel_Set3DAttributes(this, pos, vel); }
		  FMOD_RESULT get3DAttributes        (FMOD_VECTOR *pos, FMOD_VECTOR *vel) { return FMOD_Channel_Get3DAttributes(this, pos, vel); }
		  FMOD_RESULT set3DMinMaxDistance    (float mindistance, float maxdistance) { return FMOD_Channel_Set3DMinMaxDistance(this, mindistance, maxdistance); }
		  FMOD_RESULT get3DMinMaxDistance    (float *mindistance, float *maxdistance) { return FMOD_Channel_Get3DMinMaxDistance(this, mindistance, maxdistance); }
		  FMOD_RESULT set3DConeSettings      (float insideconeangle, float outsideconeangle, float outsidevolume) { return FMOD_Channel_Set3DConeSettings(this, insideconeangle, outsideconeangle, outsidevolume); }
		  FMOD_RESULT get3DConeSettings      (float *insideconeangle, float *outsideconeangle, float *outsidevolume) { return FMOD_Channel_Get3DConeSettings(this, insideconeangle, outsideconeangle, outsidevolume); }
		  FMOD_RESULT set3DConeOrientation   (FMOD_VECTOR *orientation) { return FMOD_Channel_Set3DConeOrientation(this, orientation); }
		  FMOD_RESULT get3DConeOrientation   (FMOD_VECTOR *orientation) { return FMOD_Channel_Get3DConeOrientation(this, orientation); }
		  FMOD_RESULT set3DCustomRolloff     (FMOD_VECTOR *points, int numpoints) { return FMOD_Channel_Set3DCustomRolloff(this, points, numpoints); }
		  FMOD_RESULT get3DCustomRolloff     (FMOD_VECTOR **points, int *numpoints) { return FMOD_Channel_Get3DCustomRolloff(this, points, numpoints); }
		  FMOD_RESULT set3DOcclusion         (float directocclusion, float reverbocclusion) { return FMOD_Channel_Set3DOcclusion(this, directocclusion, reverbocclusion); }
		  FMOD_RESULT get3DOcclusion         (float *directocclusion, float *reverbocclusion) { return FMOD_Channel_Get3DOcclusion(this, directocclusion, reverbocclusion); }
		  FMOD_RESULT set3DSpread            (float angle) { return FMOD_Channel_Set3DSpread(this, angle); }
		  FMOD_RESULT get3DSpread            (float *angle) { return FMOD_Channel_Get3DSpread(this, angle); }
		  FMOD_RESULT set3DPanLevel          (float level) { return FMOD_Channel_Set3DPanLevel(this, level); }
		  FMOD_RESULT get3DPanLevel          (float *level) { return FMOD_Channel_Get3DPanLevel(this, level); }
		  FMOD_RESULT set3DDopplerLevel      (float level) { return FMOD_Channel_Set3DDopplerLevel(this, level); }
		  FMOD_RESULT get3DDopplerLevel      (float *level) { return FMOD_Channel_Get3DDopplerLevel(this, level); }

		// DSP functionality only for channels playing sounds created with FMOD_SOFTWARE.
		  FMOD_RESULT getDSPHead             (DSP **dsp) { return FMOD_Channel_GetDSPHead(this, (FMOD_DSP **)dsp); }
		  FMOD_RESULT addDSP                 (DSP *dsp, DSPConnection **connection) { return FMOD_Channel_AddDSP(this, (FMOD_DSP *)dsp, (FMOD_DSPCONNECTION **)connection); }

		// Information only functions.
		  FMOD_RESULT isPlaying              (bool *isplaying) { FMOD_BOOL b; FMOD_RESULT res = FMOD_Channel_IsPlaying(this, &b); *isplaying = b; return res; }
		  FMOD_RESULT isVirtual              (bool *isvirtual) { FMOD_BOOL b; FMOD_RESULT res = FMOD_Channel_IsVirtual(this, &b); *isvirtual = b; return res; }
		  FMOD_RESULT getAudibility          (float *audibility) { return FMOD_Channel_GetAudibility(this, audibility); }
		  FMOD_RESULT getCurrentSound        (Sound **sound) { return FMOD_Channel_GetCurrentSound(this, (FMOD_SOUND **)sound); }
		  FMOD_RESULT getSpectrum            (float *spectrumarray, int numvalues, int channeloffset, FMOD_DSP_FFT_WINDOW windowtype) { return FMOD_Channel_GetSpectrum(this, spectrumarray, numvalues, channeloffset, windowtype); }
		  FMOD_RESULT getWaveData            (float *wavearray, int numvalues, int channeloffset) { return FMOD_Channel_GetWaveData(this, wavearray, numvalues, channeloffset); }
		  FMOD_RESULT getIndex               (int *index) { return FMOD_Channel_GetIndex(this, index); }

		// Functions also found in Sound class but here they can be set per channel.
		  FMOD_RESULT setMode                (FMOD_MODE mode) { return FMOD_Channel_SetMode(this, mode); }
		  FMOD_RESULT getMode                (FMOD_MODE *mode) { return FMOD_Channel_GetMode(this, mode); }
		  FMOD_RESULT setLoopCount           (int loopcount) { return FMOD_Channel_SetLoopCount(this, loopcount); }
		  FMOD_RESULT getLoopCount           (int *loopcount) { return FMOD_Channel_GetLoopCount(this, loopcount); }
		  FMOD_RESULT setLoopPoints          (unsigned int loopstart, FMOD_TIMEUNIT loopstarttype, unsigned int loopend, FMOD_TIMEUNIT loopendtype) { return FMOD_Channel_SetLoopPoints(this, loopstart, loopstarttype, loopend, loopendtype); }
		  FMOD_RESULT getLoopPoints          (unsigned int *loopstart, FMOD_TIMEUNIT loopstarttype, unsigned int *loopend, FMOD_TIMEUNIT loopendtype) { return FMOD_Channel_GetLoopPoints(this, loopstart, loopstarttype, loopend, loopendtype); }

		// Userdata set/get.
		  FMOD_RESULT setUserData            (void *userdata) { return FMOD_Channel_SetUserData(this, userdata); }
		  FMOD_RESULT getUserData            (void **userdata) { return FMOD_Channel_GetUserData(this, userdata); }

		  FMOD_RESULT getMemoryInfo          (unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused, unsigned int *memoryused_array) { return FMOD_Channel_GetMemoryInfo(this, memorybits, event_memorybits, memoryused, memoryused_array); }
	};

	/*
		'ChannelGroup' API
	*/
	class ChannelGroup : FMOD_CHANNELGROUP
	{
	  private:

		ChannelGroup();   /* Constructor made private so user cannot statically instance a ChannelGroup class.  
							 Appropriate ChannelGroup creation or retrieval function must be used. */
	  public:

		  FMOD_RESULT release                 () { return FMOD_ChannelGroup_Release(this); }
		  FMOD_RESULT getSystemObject         (System **system) { return FMOD_ChannelGroup_GetSystemObject(this, (FMOD_SYSTEM **)system); }

		// Channelgroup scale values.  (changes attributes relative to the channels, doesn't overwrite them)
		  FMOD_RESULT setVolume               (float volume) { return FMOD_ChannelGroup_SetVolume(this, volume); }
		  FMOD_RESULT getVolume               (float *volume) { return FMOD_ChannelGroup_GetVolume(this, volume); }
		  FMOD_RESULT setPitch                (float pitch) { return FMOD_ChannelGroup_SetPitch(this, pitch); }
		  FMOD_RESULT getPitch                (float *pitch) { return FMOD_ChannelGroup_GetPitch(this, pitch); }
		  FMOD_RESULT set3DOcclusion          (float directocclusion, float reverbocclusion) { return FMOD_ChannelGroup_Set3DOcclusion(this, directocclusion, reverbocclusion); }
		  FMOD_RESULT get3DOcclusion          (float *directocclusion, float *reverbocclusion) { return FMOD_ChannelGroup_Get3DOcclusion(this, directocclusion, reverbocclusion); }
		  FMOD_RESULT setPaused               (bool paused) { return FMOD_ChannelGroup_SetPaused(this, paused); }
		  FMOD_RESULT getPaused               (bool *paused) { FMOD_BOOL b; FMOD_RESULT res = FMOD_ChannelGroup_GetPaused(this, &b); *paused = b; return res; }
		  FMOD_RESULT setMute                 (bool mute) { return FMOD_ChannelGroup_SetMute(this, mute); }
		  FMOD_RESULT getMute                 (bool *mute) { FMOD_BOOL b; FMOD_RESULT res = FMOD_ChannelGroup_GetMute(this, &b); *mute = b; return res; }

		// Channelgroup override values.  (recursively overwrites whatever settings the channels had)
		  FMOD_RESULT stop                    () { return FMOD_ChannelGroup_Stop(this); }
		  FMOD_RESULT overrideVolume          (float volume) { return FMOD_ChannelGroup_OverrideVolume(this, volume); }
		  FMOD_RESULT overrideFrequency       (float frequency) { return FMOD_ChannelGroup_OverrideFrequency(this, frequency); }
		  FMOD_RESULT overridePan             (float pan) { return FMOD_ChannelGroup_OverridePan(this, pan); }
		  FMOD_RESULT overrideReverbProperties(const FMOD_REVERB_CHANNELPROPERTIES *prop) { return FMOD_ChannelGroup_OverrideReverbProperties(this, prop); }
		  FMOD_RESULT override3DAttributes    (const FMOD_VECTOR *pos, const FMOD_VECTOR *vel) { return FMOD_ChannelGroup_Override3DAttributes(this, pos, vel); }
		  FMOD_RESULT overrideSpeakerMix      (float frontleft, float frontright, float center, float lfe, float backleft, float backright, float sideleft, float sideright) { return FMOD_ChannelGroup_OverrideSpeakerMix(this, frontleft, frontright, center, lfe, backleft, backright, sideleft, sideright); }

		// Nested channel groups.
		  FMOD_RESULT addGroup                (ChannelGroup *group) { return FMOD_ChannelGroup_AddGroup(this, group); }
		  FMOD_RESULT getNumGroups            (int *numgroups) { return FMOD_ChannelGroup_GetNumGroups(this, numgroups); }
		  FMOD_RESULT getGroup                (int index, ChannelGroup **group) { return FMOD_ChannelGroup_GetGroup(this, index, (FMOD_CHANNELGROUP **)group); }
		  FMOD_RESULT getParentGroup          (ChannelGroup **group) { return FMOD_ChannelGroup_GetParentGroup(this, (FMOD_CHANNELGROUP **)group); }

		// DSP functionality only for channel groups playing sounds created with FMOD_SOFTWARE.
		  FMOD_RESULT getDSPHead              (DSP **dsp) { return FMOD_ChannelGroup_GetDSPHead(this, (FMOD_DSP **)dsp); }
		  FMOD_RESULT addDSP                  (DSP *dsp, DSPConnection **connection) { return FMOD_ChannelGroup_AddDSP(this, (FMOD_DSP *)dsp, (FMOD_DSPCONNECTION **)connection); }

		// Information only functions.
		  FMOD_RESULT getName                 (char *name, int namelen) { return FMOD_ChannelGroup_GetName(this, name, namelen); }
		  FMOD_RESULT getNumChannels          (int *numchannels) { return FMOD_ChannelGroup_GetNumChannels(this, numchannels); }
		  FMOD_RESULT getChannel              (int index, Channel **channel) { return FMOD_ChannelGroup_GetChannel(this, index, (FMOD_CHANNEL **)channel); }
		  FMOD_RESULT getSpectrum             (float *spectrumarray, int numvalues, int channeloffset, FMOD_DSP_FFT_WINDOW windowtype) { return FMOD_ChannelGroup_GetSpectrum(this, spectrumarray, numvalues, channeloffset, windowtype); }
		  FMOD_RESULT getWaveData             (float *wavearray, int numvalues, int channeloffset) { return FMOD_ChannelGroup_GetWaveData(this, wavearray, numvalues, channeloffset) ;}

		// Userdata set/get.
		  FMOD_RESULT setUserData             (void *userdata) { return FMOD_ChannelGroup_SetUserData(this, userdata); }
		  FMOD_RESULT getUserData             (void **userdata) { return FMOD_ChannelGroup_GetUserData(this, userdata); }

		  FMOD_RESULT getMemoryInfo           (unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused, unsigned int *memoryused_array) { return FMOD_ChannelGroup_GetMemoryInfo(this, memorybits, event_memorybits, memoryused, memoryused_array); }
	};

	/*
		'SoundGroup' API
	*/
	class SoundGroup : FMOD_SOUNDGROUP
	{
	  private:

		SoundGroup();       /* Constructor made private so user cannot statically instance a SoundGroup class.  
							   Appropriate SoundGroup creation or retrieval function must be used. */
	  public:

		  FMOD_RESULT release                () { return FMOD_SoundGroup_Release(this); }
		  FMOD_RESULT getSystemObject        (System **system) { return FMOD_SoundGroup_GetSystemObject(this, (FMOD_SYSTEM **)system); }

		// SoundGroup control functions.
		  FMOD_RESULT setMaxAudible          (int maxaudible) { return FMOD_SoundGroup_SetMaxAudible(this, maxaudible); }
		  FMOD_RESULT getMaxAudible          (int *maxaudible) { return FMOD_SoundGroup_GetMaxAudible(this, maxaudible); }
		  FMOD_RESULT setMaxAudibleBehavior  (FMOD_SOUNDGROUP_BEHAVIOR behavior) { return FMOD_SoundGroup_SetMaxAudibleBehavior(this, behavior); }
		  FMOD_RESULT getMaxAudibleBehavior  (FMOD_SOUNDGROUP_BEHAVIOR *behavior) { return FMOD_SoundGroup_GetMaxAudibleBehavior(this, behavior); }
		  FMOD_RESULT setMuteFadeSpeed       (float speed) { return FMOD_SoundGroup_SetMuteFadeSpeed(this, speed); }
		  FMOD_RESULT getMuteFadeSpeed       (float *speed) { return FMOD_SoundGroup_GetMuteFadeSpeed(this, speed); }
		  FMOD_RESULT setVolume              (float volume) { return FMOD_SoundGroup_SetVolume(this, volume); }
		  FMOD_RESULT getVolume              (float *volume) { return FMOD_SoundGroup_GetVolume(this, volume); }
		  FMOD_RESULT stop                   () { return FMOD_SoundGroup_Stop(this); }

		// Information only functions.
		  FMOD_RESULT getName                (char *name, int namelen) { return FMOD_SoundGroup_GetName(this, name, namelen); }
		  FMOD_RESULT getNumSounds           (int *numsounds) { return FMOD_SoundGroup_GetNumSounds(this, numsounds); }
		  FMOD_RESULT getSound               (int index, Sound **sound) { return FMOD_SoundGroup_GetSound(this, index, (FMOD_SOUND **)sound); }
		  FMOD_RESULT getNumPlaying          (int *numplaying) { return FMOD_SoundGroup_GetNumPlaying(this, numplaying); }

		// Userdata set/get.
		  FMOD_RESULT setUserData            (void *userdata) { return FMOD_SoundGroup_SetUserData(this, userdata); }
		  FMOD_RESULT getUserData            (void **userdata) { return FMOD_SoundGroup_GetUserData(this, userdata); }

		  FMOD_RESULT getMemoryInfo          (unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused, unsigned int *memoryused_array) { return FMOD_SoundGroup_GetMemoryInfo(this, memorybits, event_memorybits, memoryused, memoryused_array); }
	};

	/*
		'DSP' API
	*/
	class DSP : FMOD_DSP
	{
	  private:

		DSP();   /* Constructor made private so user cannot statically instance a DSP class.  
					Appropriate DSP creation or retrieval function must be used. */
	  public:

		  FMOD_RESULT release                () { return FMOD_DSP_Release(this); }
		  FMOD_RESULT getSystemObject        (System **system) { return FMOD_DSP_GetSystemObject(this, (FMOD_SYSTEM **)system); }

		// Connection / disconnection / input and output enumeration.
		  FMOD_RESULT addInput               (DSP *target, DSPConnection **connection) { return FMOD_DSP_AddInput(this, target, (FMOD_DSPCONNECTION **)connection); }
		  FMOD_RESULT disconnectFrom         (DSP *target) { return FMOD_DSP_DisconnectFrom(this, target); }
		  FMOD_RESULT disconnectAll          (bool inputs, bool outputs) { return FMOD_DSP_DisconnectAll(this, inputs, outputs); }
		  FMOD_RESULT remove                 () { return FMOD_DSP_Remove(this); }
		  FMOD_RESULT getNumInputs           (int *numinputs) { return FMOD_DSP_GetNumInputs(this, numinputs); }
		  FMOD_RESULT getNumOutputs          (int *numoutputs) { return FMOD_DSP_GetNumOutputs(this, numoutputs); }
		  FMOD_RESULT getInput               (int index, DSP **input, DSPConnection **inputconnection) { return FMOD_DSP_GetInput(this, index, (FMOD_DSP **)input, (FMOD_DSPCONNECTION **)inputconnection); }
		  FMOD_RESULT getOutput              (int index, DSP **output, DSPConnection **outputconnection) { return FMOD_DSP_GetOutput(this, index, (FMOD_DSP **)output, (FMOD_DSPCONNECTION **)outputconnection); }

		// DSP unit control.
		  FMOD_RESULT setActive              (bool active) { return FMOD_DSP_SetActive(this, active); }
		  FMOD_RESULT getActive              (bool *active) { FMOD_BOOL b; FMOD_RESULT res = FMOD_DSP_GetActive(this, &b); *active = b; return res; }
		  FMOD_RESULT setBypass              (bool bypass) { return FMOD_DSP_SetBypass(this, bypass); }
		  FMOD_RESULT getBypass              (bool *bypass) { FMOD_BOOL b; FMOD_RESULT res = FMOD_DSP_GetBypass(this, &b); *bypass = b; return res; }
		  FMOD_RESULT setSpeakerActive       (FMOD_SPEAKER speaker, bool active) { return FMOD_DSP_SetSpeakerActive(this, speaker, active); }
		  FMOD_RESULT getSpeakerActive       (FMOD_SPEAKER speaker, bool *active) { FMOD_BOOL b; FMOD_RESULT res = FMOD_DSP_GetSpeakerActive(this, speaker, &b); *active = b; return res; }
		  FMOD_RESULT reset                  ()  { return FMOD_DSP_Reset(this); }

		// DSP parameter control.
		  FMOD_RESULT setParameter           (int index, float value) { return FMOD_DSP_SetParameter(this, index, value); }
		  FMOD_RESULT getParameter           (int index, float *value, char *valuestr, int valuestrlen) { return FMOD_DSP_GetParameter(this, index, value, valuestr, valuestrlen); }
		  FMOD_RESULT getNumParameters       (int *numparams) { return FMOD_DSP_GetNumParameters(this, numparams); }
		  FMOD_RESULT getParameterInfo       (int index, char *name, char *label, char *description, int descriptionlen, float *min, float *max) { return FMOD_DSP_GetParameterInfo(this, index, name, label, description, descriptionlen, min, max); }
		  FMOD_RESULT showConfigDialog       (void *hwnd, bool show) { return FMOD_DSP_ShowConfigDialog(this, hwnd, show); }
		
		// DSP attributes.        
		  FMOD_RESULT getInfo                (char *name, unsigned int *version, int *channels, int *configwidth, int *configheight) { return FMOD_DSP_GetInfo(this, name, version, channels, configwidth, configheight); }
		  FMOD_RESULT getType                (FMOD_DSP_TYPE *type) { return FMOD_DSP_GetType(this, type); }
		  FMOD_RESULT setDefaults            (float frequency, float volume, float pan, int priority) { return FMOD_DSP_SetDefaults(this, frequency, volume, pan, priority); }
		  FMOD_RESULT getDefaults            (float *frequency, float *volume, float *pan, int *priority) { return FMOD_DSP_GetDefaults(this, frequency, volume, pan, priority) ;}

		// Userdata set/get.
		  FMOD_RESULT setUserData            (void *userdata) { return FMOD_DSP_SetUserData(this, userdata); }
		  FMOD_RESULT getUserData            (void **userdata) { return FMOD_DSP_GetUserData(this, userdata); }

		  FMOD_RESULT getMemoryInfo          (unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused, unsigned int *memoryused_array) { return FMOD_DSP_GetMemoryInfo(this, memorybits, event_memorybits, memoryused, memoryused_array); }
	};


	/*
		'DSPConnection' API
	*/
	class DSPConnection : FMOD_DSPCONNECTION
	{
	  private:

		DSPConnection();    /* Constructor made private so user cannot statically instance a DSPConnection class.  
							   Appropriate DSPConnection creation or retrieval function must be used. */

	  public:

		  FMOD_RESULT getInput              (DSP **input) { return FMOD_DSPConnection_GetInput(this, (FMOD_DSP **)input); }
		  FMOD_RESULT getOutput             (DSP **output) { return FMOD_DSPConnection_GetOutput(this, (FMOD_DSP **)output); }
		  FMOD_RESULT setMix                (float volume) { return FMOD_DSPConnection_SetMix(this, volume); }
		  FMOD_RESULT getMix                (float *volume) { return FMOD_DSPConnection_GetMix(this, volume); }
		  FMOD_RESULT setLevels             (FMOD_SPEAKER speaker, float *levels, int numlevels) { return FMOD_DSPConnection_SetLevels(this, speaker, levels, numlevels); }
		  FMOD_RESULT getLevels             (FMOD_SPEAKER speaker, float *levels, int numlevels) { return FMOD_DSPConnection_GetLevels(this, speaker, levels, numlevels); }

		// Userdata set/get.
		  FMOD_RESULT setUserData           (void *userdata) { return FMOD_DSPConnection_SetUserData(this, userdata); }
		  FMOD_RESULT getUserData           (void **userdata) { return FMOD_DSPConnection_GetUserData(this, userdata); }

		  FMOD_RESULT getMemoryInfo         (unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused, unsigned int *memoryused_array) { return FMOD_DSPConnection_GetMemoryInfo(this, memorybits, event_memorybits, memoryused, memoryused_array); }
	};


	/*
		'Geometry' API
	*/
	class Geometry : FMOD_GEOMETRY
	{
	  private:

		Geometry();   /* Constructor made private so user cannot statically instance a Geometry class.  
						 Appropriate Geometry creation or retrieval function must be used. */

	  public:

		  FMOD_RESULT release                () { return FMOD_Geometry_Release(this); }

		// Polygon manipulation.
		  FMOD_RESULT addPolygon             (float directocclusion, float reverbocclusion, bool doublesided, int numvertices, const FMOD_VECTOR *vertices, int *polygonindex) { return FMOD_Geometry_AddPolygon(this, directocclusion, reverbocclusion, doublesided, numvertices, vertices, polygonindex); }
		  FMOD_RESULT getNumPolygons         (int *numpolygons) { return FMOD_Geometry_GetNumPolygons(this, numpolygons); }
		  FMOD_RESULT getMaxPolygons         (int *maxpolygons, int *maxvertices) { return FMOD_Geometry_GetMaxPolygons(this, maxpolygons, maxvertices); }
		  FMOD_RESULT getPolygonNumVertices  (int index, int *numvertices) { return FMOD_Geometry_GetPolygonNumVertices(this, index, numvertices); }
		  FMOD_RESULT setPolygonVertex       (int index, int vertexindex, const FMOD_VECTOR *vertex) { return FMOD_Geometry_SetPolygonVertex(this, index, vertexindex, vertex); }
		  FMOD_RESULT getPolygonVertex       (int index, int vertexindex, FMOD_VECTOR *vertex) { return FMOD_Geometry_GetPolygonVertex(this, index, vertexindex, vertex); }
		  FMOD_RESULT setPolygonAttributes   (int index, float directocclusion, float reverbocclusion, bool doublesided) { return FMOD_Geometry_SetPolygonAttributes(this, index, directocclusion, reverbocclusion, doublesided); }
		  FMOD_RESULT getPolygonAttributes   (int index, float *directocclusion, float *reverbocclusion, bool *doublesided) { FMOD_BOOL b; FMOD_RESULT res = FMOD_Geometry_GetPolygonAttributes(this, index, directocclusion, reverbocclusion, &b); *doublesided = b; return res; }

		// Object manipulation.
		  FMOD_RESULT setActive              (bool active) { return FMOD_Geometry_SetActive(this, active); }
		  FMOD_RESULT getActive              (bool *active) { FMOD_BOOL b; FMOD_RESULT res = FMOD_Geometry_GetActive(this, &b); *active = b; return res; }
		  FMOD_RESULT setRotation            (const FMOD_VECTOR *forward, const FMOD_VECTOR *up) { return FMOD_Geometry_SetRotation(this, forward, up); }
		  FMOD_RESULT getRotation            (FMOD_VECTOR *forward, FMOD_VECTOR *up) { return FMOD_Geometry_GetRotation(this, forward, up); }
		  FMOD_RESULT setPosition            (const FMOD_VECTOR *position) { return FMOD_Geometry_SetPosition(this, position); }
		  FMOD_RESULT getPosition            (FMOD_VECTOR *position) { return FMOD_Geometry_GetPosition(this, position); }
		  FMOD_RESULT setScale               (const FMOD_VECTOR *scale) { return FMOD_Geometry_SetScale(this, scale); }
		  FMOD_RESULT getScale               (FMOD_VECTOR *scale) { return FMOD_Geometry_GetScale(this, scale); }
		  FMOD_RESULT save                   (void *data, int *datasize) { return FMOD_Geometry_Save(this, data, datasize); }

		// Userdata set/get.
		  FMOD_RESULT setUserData            (void *userdata) { return FMOD_Geometry_SetUserData(this, userdata); }
		  FMOD_RESULT getUserData            (void **userdata) { return FMOD_Geometry_GetUserData(this, userdata); }

		  FMOD_RESULT getMemoryInfo          (unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused, unsigned int *memoryused_array) { return FMOD_Geometry_GetMemoryInfo(this, memorybits, event_memorybits, memoryused, memoryused_array); }
	};


	/*
		'Reverb' API
	*/
	class Reverb : FMOD_REVERB
	{
	  private:

		Reverb();    /*  Constructor made private so user cannot statically instance a Reverb class.  
						 Appropriate Reverb creation or retrieval function must be used. */

	  public:    

		  FMOD_RESULT release                () { return FMOD_Reverb_Release(this); }

		// Reverb manipulation.
		  FMOD_RESULT set3DAttributes        (const FMOD_VECTOR *position, float mindistance, float maxdistance) { return FMOD_Reverb_Set3DAttributes(this, position, mindistance, maxdistance); }
		  FMOD_RESULT get3DAttributes        (FMOD_VECTOR *position, float *mindistance, float *maxdistance) { return FMOD_Reverb_Get3DAttributes(this, position, mindistance, maxdistance); }
		  FMOD_RESULT setProperties          (const FMOD_REVERB_PROPERTIES *properties) { return FMOD_Reverb_SetProperties(this, properties); }
		  FMOD_RESULT getProperties          (FMOD_REVERB_PROPERTIES *properties) { return FMOD_Reverb_GetProperties(this, properties); }
		  FMOD_RESULT setActive              (bool active) { return FMOD_Reverb_SetActive(this, active); }
		  FMOD_RESULT getActive              (bool *active) { FMOD_BOOL b; FMOD_RESULT res = FMOD_Reverb_GetActive(this, &b); *active = b; return res; }

		// Userdata set/get.
		  FMOD_RESULT setUserData            (void *userdata) { return FMOD_Reverb_SetUserData(this, userdata); }
		  FMOD_RESULT getUserData            (void **userdata) { return FMOD_Reverb_GetUserData(this, userdata); }

		  FMOD_RESULT getMemoryInfo          (unsigned int memorybits, unsigned int event_memorybits, unsigned int *memoryused, unsigned int *memoryused_array) { return FMOD_Reverb_GetMemoryInfo(this, memorybits, event_memorybits, memoryused, memoryused_array); }
	};
}

#endif
#endif
#endif
