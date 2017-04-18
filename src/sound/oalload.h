#ifndef OALDEF_H
#define OALDEF_H

#if !defined NO_OPENAL && defined DYN_OPENAL

#define DEFINE_ENTRY(type, name) static TReqProc<OpenALModule, type> p_##name{#name};
DEFINE_ENTRY(LPALENABLE,alEnable)
DEFINE_ENTRY(LPALDISABLE, alDisable)
DEFINE_ENTRY(LPALISENABLED, alIsEnabled)
DEFINE_ENTRY(LPALGETSTRING, alGetString)
DEFINE_ENTRY(LPALGETBOOLEANV, alGetBooleanv)
DEFINE_ENTRY(LPALGETINTEGERV, alGetIntegerv)
DEFINE_ENTRY(LPALGETFLOATV, alGetFloatv)
DEFINE_ENTRY(LPALGETDOUBLEV, alGetDoublev)
DEFINE_ENTRY(LPALGETBOOLEAN, alGetBoolean)
DEFINE_ENTRY(LPALGETINTEGER, alGetInteger)
DEFINE_ENTRY(LPALGETFLOAT, alGetFloat)
DEFINE_ENTRY(LPALGETDOUBLE, alGetDouble)
DEFINE_ENTRY(LPALGETERROR, alGetError)
DEFINE_ENTRY(LPALISEXTENSIONPRESENT, alIsExtensionPresent)
DEFINE_ENTRY(LPALGETPROCADDRESS, alGetProcAddress)
DEFINE_ENTRY(LPALGETENUMVALUE, alGetEnumValue)
DEFINE_ENTRY(LPALLISTENERF, alListenerf)
DEFINE_ENTRY(LPALLISTENER3F, alListener3f)
DEFINE_ENTRY(LPALLISTENERFV, alListenerfv)
DEFINE_ENTRY(LPALLISTENERI, alListeneri)
DEFINE_ENTRY(LPALLISTENER3I, alListener3i)
DEFINE_ENTRY(LPALLISTENERIV, alListeneriv)
DEFINE_ENTRY(LPALGETLISTENERF, alGetListenerf)
DEFINE_ENTRY(LPALGETLISTENER3F, alGetListener3f)
DEFINE_ENTRY(LPALGETLISTENERFV, alGetListenerfv)
DEFINE_ENTRY(LPALGETLISTENERI, alGetListeneri)
DEFINE_ENTRY(LPALGETLISTENER3I, alGetListener3i)
DEFINE_ENTRY(LPALGETLISTENERIV, alGetListeneriv)
DEFINE_ENTRY(LPALGENSOURCES, alGenSources)
DEFINE_ENTRY(LPALDELETESOURCES, alDeleteSources)
DEFINE_ENTRY(LPALISSOURCE, alIsSource)
DEFINE_ENTRY(LPALSOURCEF, alSourcef)
DEFINE_ENTRY(LPALSOURCE3F, alSource3f)
DEFINE_ENTRY(LPALSOURCEFV, alSourcefv)
DEFINE_ENTRY(LPALSOURCEI, alSourcei)
DEFINE_ENTRY(LPALSOURCE3I, alSource3i)
DEFINE_ENTRY(LPALSOURCEIV, alSourceiv)
DEFINE_ENTRY(LPALGETSOURCEF, alGetSourcef)
DEFINE_ENTRY(LPALGETSOURCE3F, alGetSource3f)
DEFINE_ENTRY(LPALGETSOURCEFV, alGetSourcefv)
DEFINE_ENTRY(LPALGETSOURCEI, alGetSourcei)
DEFINE_ENTRY(LPALGETSOURCE3I, alGetSource3i)
DEFINE_ENTRY(LPALGETSOURCEIV, alGetSourceiv)
DEFINE_ENTRY(LPALSOURCEPLAYV, alSourcePlayv)
DEFINE_ENTRY(LPALSOURCESTOPV, alSourceStopv)
DEFINE_ENTRY(LPALSOURCEREWINDV, alSourceRewindv)
DEFINE_ENTRY(LPALSOURCEPAUSEV, alSourcePausev)
DEFINE_ENTRY(LPALSOURCEPLAY, alSourcePlay)
DEFINE_ENTRY(LPALSOURCESTOP, alSourceStop)
DEFINE_ENTRY(LPALSOURCEREWIND, alSourceRewind)
DEFINE_ENTRY(LPALSOURCEPAUSE, alSourcePause)
DEFINE_ENTRY(LPALSOURCEQUEUEBUFFERS, alSourceQueueBuffers)
DEFINE_ENTRY(LPALSOURCEUNQUEUEBUFFERS, alSourceUnqueueBuffers)
DEFINE_ENTRY(LPALGENBUFFERS, alGenBuffers)
DEFINE_ENTRY(LPALDELETEBUFFERS, alDeleteBuffers)
DEFINE_ENTRY(LPALISBUFFER, alIsBuffer)
DEFINE_ENTRY(LPALBUFFERDATA, alBufferData)
DEFINE_ENTRY(LPALBUFFERF, alBufferf)
DEFINE_ENTRY(LPALBUFFER3F, alBuffer3f)
DEFINE_ENTRY(LPALBUFFERFV, alBufferfv)
DEFINE_ENTRY(LPALBUFFERI, alBufferi)
DEFINE_ENTRY(LPALBUFFER3I, alBuffer3i)
DEFINE_ENTRY(LPALBUFFERIV, alBufferiv)
DEFINE_ENTRY(LPALGETBUFFERF, alGetBufferf)
DEFINE_ENTRY(LPALGETBUFFER3F, alGetBuffer3f)
DEFINE_ENTRY(LPALGETBUFFERFV, alGetBufferfv)
DEFINE_ENTRY(LPALGETBUFFERI, alGetBufferi)
DEFINE_ENTRY(LPALGETBUFFER3I, alGetBuffer3i)
DEFINE_ENTRY(LPALGETBUFFERIV, alGetBufferiv)
DEFINE_ENTRY(LPALDOPPLERFACTOR, alDopplerFactor)
DEFINE_ENTRY(LPALDOPPLERVELOCITY, alDopplerVelocity)
DEFINE_ENTRY(LPALSPEEDOFSOUND, alSpeedOfSound)
DEFINE_ENTRY(LPALDISTANCEMODEL, alDistanceModel)
DEFINE_ENTRY(LPALCCREATECONTEXT, alcCreateContext)
DEFINE_ENTRY(LPALCMAKECONTEXTCURRENT, alcMakeContextCurrent)
DEFINE_ENTRY(LPALCPROCESSCONTEXT, alcProcessContext)
DEFINE_ENTRY(LPALCSUSPENDCONTEXT, alcSuspendContext)
DEFINE_ENTRY(LPALCDESTROYCONTEXT, alcDestroyContext)
DEFINE_ENTRY(LPALCGETCURRENTCONTEXT, alcGetCurrentContext)
DEFINE_ENTRY(LPALCGETCONTEXTSDEVICE, alcGetContextsDevice)
DEFINE_ENTRY(LPALCOPENDEVICE, alcOpenDevice)
DEFINE_ENTRY(LPALCCLOSEDEVICE, alcCloseDevice)
DEFINE_ENTRY(LPALCGETERROR, alcGetError)
DEFINE_ENTRY(LPALCISEXTENSIONPRESENT, alcIsExtensionPresent)
DEFINE_ENTRY(LPALCGETPROCADDRESS, alcGetProcAddress)
DEFINE_ENTRY(LPALCGETENUMVALUE, alcGetEnumValue)
DEFINE_ENTRY(LPALCGETSTRING, alcGetString)
DEFINE_ENTRY(LPALCGETINTEGERV, alcGetIntegerv)
DEFINE_ENTRY(LPALCCAPTUREOPENDEVICE, alcCaptureOpenDevice)
DEFINE_ENTRY(LPALCCAPTURECLOSEDEVICE, alcCaptureCloseDevice)
DEFINE_ENTRY(LPALCCAPTURESTART, alcCaptureStart)
DEFINE_ENTRY(LPALCCAPTURESTOP, alcCaptureStop)
DEFINE_ENTRY(LPALCCAPTURESAMPLES, alcCaptureSamples)
#undef DEFINE_ENTRY

#ifndef IN_IDE_PARSER
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
#endif
