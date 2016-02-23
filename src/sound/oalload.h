#ifndef OALDEF_H
#define OALDEF_H

#ifndef NO_OPENAL

#ifndef _WIN32
typedef void* FARPROC;
#endif

#define DEFINE_ENTRY(type, name) static type p_##name;
#include "oaldef.h"
#undef DEFINE_ENTRY
struct oalloadentry
{
	const char *name;
	FARPROC *funcaddr;
};
static oalloadentry oalfuncs[] = {
#define DEFINE_ENTRY(type, name) { #name, (FARPROC*)&p_##name },
#include "oaldef.h"
#undef DEFINE_ENTRY
{ NULL, 0 }
};


#define alEnable p_alEnable
#define alDisable p_alDisable
#define alIsEnabled p_alIsEnabled
#define alGetString p_alGetString
#define alGetBooleanv p_alGetBooleanv
#define alGetIntegerv p_alGetIntegerv
#define alGetFloatv p_alGetFloatv
#define alGetDoublev p_alGetDoublev
#define alGetBoolean p_alGetBoolean
#define alGetInteger p_alGetInteger
#define alGetFloat p_alGetFloat
#define alGetDouble p_alGetDouble
#define alGetError p_alGetError
#define alIsExtensionPresent p_alIsExtensionPresent
#define alGetProcAddress p_alGetProcAddress
#define alGetEnumValue p_alGetEnumValue
#define alListenerf p_alListenerf
#define alListener3f p_alListener3f
#define alListenerfv p_alListenerfv
#define alListeneri p_alListeneri
#define alListener3i p_alListener3i
#define alListeneriv p_alListeneriv
#define alGetListenerf p_alGetListenerf
#define alGetListener3f p_alGetListener3f
#define alGetListenerfv p_alGetListenerfv
#define alGetListeneri p_alGetListeneri
#define alGetListener3i p_alGetListener3i
#define alGetListeneriv p_alGetListeneriv
#define alGenSources p_alGenSources
#define alDeleteSources p_alDeleteSources
#define alIsSource p_alIsSource
#define alSourcef p_alSourcef
#define alSource3f p_alSource3f
#define alSourcefv p_alSourcefv
#define alSourcei p_alSourcei
#define alSource3i p_alSource3i
#define alSourceiv p_alSourceiv
#define alGetSourcef p_alGetSourcef
#define alGetSource3f p_alGetSource3f
#define alGetSourcefv p_alGetSourcefv
#define alGetSourcei p_alGetSourcei
#define alGetSource3i p_alGetSource3i
#define alGetSourceiv p_alGetSourceiv
#define alSourcePlayv p_alSourcePlayv
#define alSourceStopv p_alSourceStopv
#define alSourceRewindv p_alSourceRewindv
#define alSourcePausev p_alSourcePausev
#define alSourcePlay p_alSourcePlay
#define alSourceStop p_alSourceStop
#define alSourceRewind p_alSourceRewind
#define alSourcePause p_alSourcePause
#define alSourceQueueBuffers p_alSourceQueueBuffers
#define alSourceUnqueueBuffers p_alSourceUnqueueBuffers
#define alGenBuffers p_alGenBuffers
#define alDeleteBuffers p_alDeleteBuffers
#define alIsBuffer p_alIsBuffer
#define alBufferData p_alBufferData
#define alBufferf p_alBufferf
#define alBuffer3f p_alBuffer3f
#define alBufferfv p_alBufferfv
#define alBufferi p_alBufferi
#define alBuffer3i p_alBuffer3i
#define alBufferiv p_alBufferiv
#define alGetBufferf p_alGetBufferf
#define alGetBuffer3f p_alGetBuffer3f
#define alGetBufferfv p_alGetBufferfv
#define alGetBufferi p_alGetBufferi
#define alGetBuffer3i p_alGetBuffer3i
#define alGetBufferiv p_alGetBufferiv
#define alDopplerFactor p_alDopplerFactor
#define alDopplerVelocity p_alDopplerVelocity
#define alSpeedOfSound p_alSpeedOfSound
#define alDistanceModel p_alDistanceModel
#define alcCreateContext p_alcCreateContext
#define alcMakeContextCurrent p_alcMakeContextCurrent
#define alcProcessContext p_alcProcessContext
#define alcSuspendContext p_alcSuspendContext
#define alcDestroyContext p_alcDestroyContext
#define alcGetCurrentContext p_alcGetCurrentContext
#define alcGetContextsDevice p_alcGetContextsDevice
#define alcOpenDevice p_alcOpenDevice
#define alcCloseDevice p_alcCloseDevice
#define alcGetError p_alcGetError
#define alcIsExtensionPresent p_alcIsExtensionPresent
#define alcGetProcAddress p_alcGetProcAddress
#define alcGetEnumValue p_alcGetEnumValue
#define alcGetString p_alcGetString
#define alcGetIntegerv p_alcGetIntegerv
#define alcCaptureOpenDevice p_alcCaptureOpenDevice
#define alcCaptureCloseDevice p_alcCaptureCloseDevice
#define alcCaptureStart p_alcCaptureStart
#define alcCaptureStop p_alcCaptureStop
#define alcCaptureSamples p_alcCaptureSamples

#endif
#endif