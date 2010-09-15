/*
** oalsound.cpp
** System interface for sound; uses OpenAL
**
**---------------------------------------------------------------------------
** Copyright 2008-2010 Chris Robinson
** All rights reserved.
**
** Redistribution and use in source and binary forms, with or without
** modification, are permitted provided that the following conditions
** are met:
**
** 1. Redistributions of source code must retain the above copyright
**    notice, this list of conditions and the following disclaimer.
** 2. Redistributions in binary form must reproduce the above copyright
**    notice, this list of conditions and the following disclaimer in the
**    documentation and/or other materials provided with the distribution.
** 3. The name of the author may not be used to endorse or promote products
**    derived from this software without specific prior written permission.
**
** THIS SOFTWARE IS PROVIDED BY THE AUTHOR ``AS IS'' AND ANY EXPRESS OR
** IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES
** OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
** IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR ANY DIRECT, INDIRECT,
** INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT
** NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
** DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
** THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
** (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF
** THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
**---------------------------------------------------------------------------
**
*/

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#define USE_WINDOWS_DWORD
#endif

#include "doomstat.h"
#include "templates.h"
#include "oalsound.h"
#include "c_cvars.h"
#include "c_dispatch.h"
#include "i_system.h"
#include "v_text.h"
#include "gi.h"
#include "actor.h"
#include "r_state.h"
#include "w_wad.h"
#include "i_music.h"
#include "i_musicinterns.h"
#include "tempfiles.h"


CVAR (String, snd_aldevice, "Default", CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CVAR (Bool, snd_efx, true, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
CUSTOM_CVAR (Float, snd_waterabsorption, 10.0f, CVAR_ARCHIVE|CVAR_GLOBALCONFIG)
{
	if(*self < 0.0f)
		self = 0.0f;
	else if(*self > 10.0f)
		self = 10.0f;
}

void I_BuildALDeviceList(FOptionValues *opt)
{
	opt->mValues.Resize(1);
	opt->mValues[0].TextValue = "Default";
	opt->mValues[0].Text = "Default";

#ifndef NO_OPENAL
	const ALCchar *names = (alcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT") ?
	                        alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER) :
	                        alcGetString(NULL, ALC_DEVICE_SPECIFIER));
	if(!names)
		Printf("Failed to get device list: %s\n", alcGetString(NULL, alcGetError(NULL)));
	else while(*names)
	{
		unsigned int i = opt->mValues.Reserve(1);
		opt->mValues[i].TextValue = names;
		opt->mValues[i].Text = names;

		names += strlen(names)+1;
	}
#endif
}

#ifndef NO_OPENAL

#include <algorithm>
#include <memory>
#include <string>
#include <vector>


EXTERN_CVAR (Int, snd_channels)
EXTERN_CVAR (Int, snd_samplerate)
EXTERN_CVAR (Bool, snd_waterreverb)
EXTERN_CVAR (Bool, snd_pitched)


#define foreach(type, name, vec) \
	for(std::vector<type>::iterator (name) = (vec).begin(), \
	    (_end_##name) = (vec).end(); \
	    (name) != (_end_##name);(name)++)


static ALenum checkALError(const char *fn, unsigned int ln)
{
    ALenum err = alGetError();
    if(err != AL_NO_ERROR)
    {
        if(strchr(fn, '/'))
            fn = strrchr(fn, '/')+1;
        else if(strchr(fn, '\\'))
            fn = strrchr(fn, '\\')+1;
        Printf(">>>>>>>>>>>> Received AL error %s (%#x), %s:%u\n", alGetString(err), err, fn, ln);
    }
    return err;
}
#define getALError() checkALError(__FILE__, __LINE__)

static ALCenum checkALCError(ALCdevice *device, const char *fn, unsigned int ln)
{
    ALCenum err = alcGetError(device);
    if(err != ALC_NO_ERROR)
    {
        if(strchr(fn, '/'))
            fn = strrchr(fn, '/')+1;
        else if(strchr(fn, '\\'))
            fn = strrchr(fn, '\\')+1;
        Printf(">>>>>>>>>>>> Received ALC error %s (%#x), %s:%u\n", alcGetString(device, err), err, fn, ln);
    }
    return err;
}
#define getALCError(d) checkALCError((d), __FILE__, __LINE__)

static ALsizei GetBufferLength(ALuint buffer, const char *fn, unsigned int ln)
{
	ALint bits, channels, size;
	alGetBufferi(buffer, AL_BITS, &bits);
	alGetBufferi(buffer, AL_CHANNELS, &channels);
	alGetBufferi(buffer, AL_SIZE, &size);
	if(checkALError(fn, ln) == AL_NO_ERROR)
		return (ALsizei)(size / (channels * bits / 8));
	return 0;
}
#define getBufferLength(b) GetBufferLength((b), __FILE__, __LINE__)

extern ReverbContainer *ForcedEnvironment;

#define PITCH_MULT (0.7937005f) /* Approx. 4 semitones lower; what Nash suggested */

#define PITCH(pitch) (snd_pitched ? (pitch)/128.f : 1.f)


static float GetRolloff(const FRolloffInfo *rolloff, float distance)
{
	if(distance <= rolloff->MinDistance)
		return 1.f;
	// Logarithmic rolloff has no max distance where it goes silent.
	if(rolloff->RolloffType == ROLLOFF_Log)
		return rolloff->MinDistance /
		       (rolloff->MinDistance + rolloff->RolloffFactor*(distance-rolloff->MinDistance));
	if(distance >= rolloff->MaxDistance)
		return 0.f;

	float volume = (rolloff->MaxDistance - distance) / (rolloff->MaxDistance - rolloff->MinDistance);
	if(rolloff->RolloffType == ROLLOFF_Linear)
		return volume;

	if(rolloff->RolloffType == ROLLOFF_Custom && S_SoundCurve != NULL)
		return S_SoundCurve[int(S_SoundCurveSize * (1.f - volume))] / 127.f;
	return (powf(10.f, volume) - 1.f) / 9.f;
}


/*** GStreamer start ***/
#include <gst/gst.h>
#include <gst/app/gstappsrc.h>
#include <gst/app/gstappsink.h>
#include <gst/app/gstappbuffer.h>
#include <gst/audio/multichannel.h>

/* Bad GStreamer, using enums for bit fields... */
static GstMessageType operator|(const GstMessageType &a, const GstMessageType &b)
{ return GstMessageType((unsigned)a|(unsigned)b); }
static GstSeekFlags operator|(const GstSeekFlags &a, const GstSeekFlags &b)
{ return GstSeekFlags((unsigned)a|(unsigned)b); }

static void PrintErrMsg(const char *str, GstMessage *msg)
{
	GError *error;
	gchar  *debug;

	gst_message_parse_error(msg, &error, &debug);
	Printf("%s: %s\n", str, error->message);
	DPrintf("%s\n", debug);

	g_error_free(error);
	g_free(debug);
}

static GstCaps *SupportedBufferFormatCaps(int forcebits=0)
{
	GstStructure *structure;
	GstCaps *caps;

	caps = gst_caps_new_empty();
	if(alIsExtensionPresent("AL_EXT_MCFORMATS"))
	{
		static const struct {
			gint count;
			GstAudioChannelPosition pos[8];
		} chans[] = {
			{ 1,
			    { GST_AUDIO_CHANNEL_POSITION_FRONT_MONO } },
			{ 2,
			    { GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
			      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT } },
			{ 4,
			    { GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
			      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
			      GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
			      GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT } },
			{ 6,
			    { GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
			      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
			      GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
			      GST_AUDIO_CHANNEL_POSITION_LFE,
			      GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
			      GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT } },
			{ 7,
			    { GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
			      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
			      GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
			      GST_AUDIO_CHANNEL_POSITION_LFE,
			      GST_AUDIO_CHANNEL_POSITION_REAR_CENTER,
			      GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT,
			      GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT } },
			{ 8,
			    { GST_AUDIO_CHANNEL_POSITION_FRONT_LEFT,
			      GST_AUDIO_CHANNEL_POSITION_FRONT_RIGHT,
			      GST_AUDIO_CHANNEL_POSITION_FRONT_CENTER,
			      GST_AUDIO_CHANNEL_POSITION_LFE,
			      GST_AUDIO_CHANNEL_POSITION_REAR_LEFT,
			      GST_AUDIO_CHANNEL_POSITION_REAR_RIGHT,
			      GST_AUDIO_CHANNEL_POSITION_SIDE_LEFT,
			      GST_AUDIO_CHANNEL_POSITION_SIDE_RIGHT } },
		};
		static const char *fmt32[] = {
			"AL_FORMAT_MONO_FLOAT32", "AL_FORMAT_STEREO_FLOAT32", "AL_FORMAT_QUAD32",
			"AL_FORMAT_51CHN32", "AL_FORMAT_61CHN32", "AL_FORMAT_71CHN32", NULL
		};
		static const char *fmt16[] = {
			"AL_FORMAT_MONO16", "AL_FORMAT_STEREO16", "AL_FORMAT_QUAD16",
			"AL_FORMAT_51CHN16", "AL_FORMAT_61CHN16", "AL_FORMAT_71CHN16", NULL
		};
		static const char *fmt8[] = {
			"AL_FORMAT_MONO8", "AL_FORMAT_STEREO8", "AL_FORMAT_QUAD8",
			"AL_FORMAT_51CHN8", "AL_FORMAT_61CHN8", "AL_FORMAT_71CHN8", NULL
		};

		if(alIsExtensionPresent("AL_EXT_FLOAT32"))
		{
			for(size_t i = 0;fmt32[i];i++)
			{
				if(forcebits && forcebits != 32)
					break;

				ALenum val = alGetEnumValue(fmt32[i]);
				if(getALError() != AL_NO_ERROR || val == 0 || val == -1)
					continue;

				structure = gst_structure_new("audio/x-raw-float",
				    "endianness", G_TYPE_INT, G_BYTE_ORDER,
				    "width", G_TYPE_INT, 32, NULL);
				gst_structure_set(structure, "channels", G_TYPE_INT,
				                  chans[i].count, NULL);
				if(chans[i].count > 2)
					gst_audio_set_channel_positions(structure, chans[i].pos);
				gst_caps_append_structure(caps, structure);
			}
		}
		for(size_t i = 0;fmt16[i];i++)
		{
			if(forcebits && forcebits != 16)
				break;

			ALenum val = alGetEnumValue(fmt16[i]);
			if(getALError() != AL_NO_ERROR || val == 0 || val == -1)
				continue;

			structure = gst_structure_new("audio/x-raw-int",
			    "endianness", G_TYPE_INT, G_BYTE_ORDER,
			    "width", G_TYPE_INT, 16,
			    "depth", G_TYPE_INT, 16,
			    "signed", G_TYPE_BOOLEAN, TRUE, NULL);
			gst_structure_set(structure, "channels", G_TYPE_INT,
			                  chans[i].count, NULL);
			if(chans[i].count > 2)
				gst_audio_set_channel_positions(structure, chans[i].pos);
			gst_caps_append_structure(caps, structure);
		}
		for(size_t i = 0;fmt8[i];i++)
		{
			if(forcebits && forcebits != 8)
				break;

			ALenum val = alGetEnumValue(fmt8[i]);
			if(getALError() != AL_NO_ERROR || val == 0 || val == -1)
				continue;

			structure = gst_structure_new("audio/x-raw-int",
			    "width", G_TYPE_INT, 8,
			    "depth", G_TYPE_INT, 8,
			    "signed", G_TYPE_BOOLEAN, FALSE, NULL);
			gst_structure_set(structure, "channels", G_TYPE_INT,
			                  chans[i].count, NULL);
			if(chans[i].count > 2)
				gst_audio_set_channel_positions(structure, chans[i].pos);
			gst_caps_append_structure(caps, structure);
		}
	}
	else
	{
		if(alIsExtensionPresent("AL_EXT_FLOAT32") &&
		   (!forcebits || forcebits == 32))
		{
			structure = gst_structure_new("audio/x-raw-float",
			    "endianness", G_TYPE_INT, G_BYTE_ORDER,
			    "width", G_TYPE_INT, 32,
			    "channels", GST_TYPE_INT_RANGE, 1, 2, NULL);
			gst_caps_append_structure(caps, structure);
		}
		if(!forcebits || forcebits == 16)
		{
			structure = gst_structure_new("audio/x-raw-int",
			    "endianness", G_TYPE_INT, G_BYTE_ORDER,
			    "width", G_TYPE_INT, 16,
			    "depth", G_TYPE_INT, 16,
			    "signed", G_TYPE_BOOLEAN, TRUE,
			    "channels", GST_TYPE_INT_RANGE, 1, 2, NULL);
			gst_caps_append_structure(caps, structure);
		}
		if(!forcebits || forcebits == 8)
		{
			structure = gst_structure_new("audio/x-raw-int",
			    "width", G_TYPE_INT, 8,
			    "depth", G_TYPE_INT, 8,
			    "signed", G_TYPE_BOOLEAN, FALSE,
			    "channels", GST_TYPE_INT_RANGE, 1, 2, NULL);
			gst_caps_append_structure(caps, structure);
		}
	}
	return caps;
}

static ALenum FormatFromDesc(int bits, int channels)
{
	if(bits == 8)
	{
		if(channels == 1) return AL_FORMAT_MONO8;
		if(channels == 2) return AL_FORMAT_STEREO8;
		if(channels == 4) return AL_FORMAT_QUAD8;
		if(channels == 6) return AL_FORMAT_51CHN8;
		if(channels == 7) return AL_FORMAT_61CHN8;
		if(channels == 8) return AL_FORMAT_71CHN8;
	}
	if(bits == 16)
	{
		if(channels == 1) return AL_FORMAT_MONO16;
		if(channels == 2) return AL_FORMAT_STEREO16;
		if(channels == 4) return AL_FORMAT_QUAD16;
		if(channels == 6) return AL_FORMAT_51CHN16;
		if(channels == 7) return AL_FORMAT_61CHN16;
		if(channels == 8) return AL_FORMAT_71CHN16;
	}
	if(bits == 32)
	{
		if(channels == 1) return AL_FORMAT_MONO_FLOAT32;
		if(channels == 2) return AL_FORMAT_STEREO_FLOAT32;
		if(channels == 4) return AL_FORMAT_QUAD32;
		if(channels == 6) return AL_FORMAT_51CHN32;
		if(channels == 7) return AL_FORMAT_61CHN32;
		if(channels == 8) return AL_FORMAT_71CHN32;
	}
	return AL_NONE;
}

class OpenALSoundStream : public SoundStream
{
	OpenALSoundRenderer *Renderer;
	GstElement *gstPipeline;
	GstTagList *TagList;
	gint64 LoopPts[2];
	ALuint Source;

	bool Playing;
	bool Looping;

	// Custom OpenAL sink; this is pretty crappy compared to the real
	// openalsink element, but it gets the job done
	static const ALsizei MaxSamplesQueued = 32768;
	std::vector<ALuint> Buffers;
	ALsizei SamplesQueued;
	ALsizei SampleRate;
	ALenum Format;

	static void sink_eos(GstAppSink *sink, gpointer user_data)
	{
		OpenALSoundStream *self = static_cast<OpenALSoundStream*>(user_data);

		if(!self->Playing)
			return;

		ALint state;
		do {
			g_usleep(10000);
			alGetSourcei(self->Source, AL_SOURCE_STATE, &state);
		} while(getALError() == AL_NO_ERROR && state == AL_PLAYING && self->Playing);

		alSourceRewind(self->Source);
		getALError();
	}

	static GstFlowReturn sink_preroll(GstAppSink *sink, gpointer user_data)
	{
		OpenALSoundStream *self = static_cast<OpenALSoundStream*>(user_data);

		// get the buffer from appsink
		GstBuffer *buffer = gst_app_sink_pull_preroll(sink);
		if(!buffer) return GST_FLOW_ERROR;

		GstCaps *caps = GST_BUFFER_CAPS(buffer);
		gint bits = 0, channels = 0, rate = 0, i;
		for(i = gst_caps_get_size(caps)-1;i >= 0;i--)
		{
			GstStructure *struc = gst_caps_get_structure(caps, i);
			if(gst_structure_has_field(struc, "width"))
				gst_structure_get_int(struc, "width", &bits);
			if(gst_structure_has_field(struc, "channels"))
				gst_structure_get_int(struc, "channels", &channels);
			if(gst_structure_has_field(struc, "rate"))
				gst_structure_get_int(struc, "rate", &rate);
		}

		self->SampleRate = rate;
		self->Format = FormatFromDesc(bits, channels);

		gst_buffer_unref(buffer);
		if(self->Format == AL_NONE || self->SampleRate <= 0)
			return GST_FLOW_ERROR;
		return GST_FLOW_OK;
	}

	static GstFlowReturn sink_buffer(GstAppSink *sink, gpointer user_data)
	{
		OpenALSoundStream *self = static_cast<OpenALSoundStream*>(user_data);

		GstBuffer *buffer = gst_app_sink_pull_buffer(sink);
		if(!buffer) return GST_FLOW_ERROR;

		if(GST_BUFFER_SIZE(buffer) == 0)
		{
			gst_buffer_unref(buffer);
			return GST_FLOW_OK;
		}

		ALint processed, state;
	next_buffer:
		do {
			alGetSourcei(self->Source, AL_SOURCE_STATE, &state);
			alGetSourcei(self->Source, AL_BUFFERS_PROCESSED, &processed);
			if(getALError() != AL_NO_ERROR)
			{
				gst_buffer_unref(buffer);
				return GST_FLOW_ERROR;
			}
			if(processed > 0 || self->SamplesQueued < MaxSamplesQueued ||
			   state != AL_PLAYING || !self->Playing)
				break;

			g_usleep(10000);
		} while(1);

		if(!self->Playing)
		{
			gst_buffer_unref(buffer);
			return GST_FLOW_OK;
		}

		ALuint bufID;
		if(processed == 0)
		{
			alGenBuffers(1, &bufID);
			if(getALError() != AL_NO_ERROR)
			{
				gst_buffer_unref(buffer);
				return GST_FLOW_ERROR;
			}
			self->Buffers.push_back(bufID);
		}
		else while(1)
		{
			alSourceUnqueueBuffers(self->Source, 1, &bufID);
			if(getALError() != AL_NO_ERROR)
			{
				gst_buffer_unref(buffer);
				return GST_FLOW_ERROR;
			}

			self->SamplesQueued -= getBufferLength(bufID);
			processed--;
			if(self->SamplesQueued < MaxSamplesQueued)
				break;
			if(processed == 0)
				goto next_buffer;
			self->Buffers.erase(find(self->Buffers.begin(), self->Buffers.end(), bufID));
			alDeleteBuffers(1, &bufID);
		}

		alBufferData(bufID, self->Format, GST_BUFFER_DATA(buffer),
		             GST_BUFFER_SIZE(buffer), self->SampleRate);
		alSourceQueueBuffers(self->Source, 1, &bufID);
		gst_buffer_unref(buffer);

		if(getALError() != AL_NO_ERROR)
			return GST_FLOW_ERROR;

		self->SamplesQueued += getBufferLength(bufID);
		if(state != AL_PLAYING && processed == 0)
		{
			alSourcePlay(self->Source);
			if(getALError() != AL_NO_ERROR)
				return GST_FLOW_ERROR;
		}
		return GST_FLOW_OK;
	}

	// Memory-based data source
	std::vector<guint8> MemData;
	size_t MemDataPos;

	static void need_memdata(GstAppSrc *appsrc, guint size, gpointer user_data)
	{
		OpenALSoundStream *self = static_cast<OpenALSoundStream*>(user_data);

		if(self->MemDataPos >= self->MemData.size())
		{
			gst_app_src_end_of_stream(appsrc);
			return;
		}

		// "read" the data it wants, up to the remaining amount
		guint8 *data = &self->MemData[self->MemDataPos];
		size = std::min<guint>(size, self->MemData.size() - self->MemDataPos);
		self->MemDataPos += size;

		GstBuffer *buffer = gst_buffer_new();
		GST_BUFFER_DATA(buffer) = data;
		GST_BUFFER_SIZE(buffer) = size;

		// this takes ownership of the buffer; don't unref
		gst_app_src_push_buffer(appsrc, buffer);
	}

	static gboolean seek_memdata(GstAppSrc *appsrc, guint64 position, gpointer user_data)
	{
		OpenALSoundStream *self = static_cast<OpenALSoundStream*>(user_data);

		if(position > self->MemData.size())
			return FALSE;
		self->MemDataPos = position;
		return TRUE;
	}

	static void memdata_source(GObject *object, GObject *orig, GParamSpec *pspec, OpenALSoundStream *self)
	{
		GstElement *elem;
		g_object_get(self->gstPipeline, "source", &elem, NULL);

		GstAppSrc *appsrc = GST_APP_SRC(elem);
		GstAppSrcCallbacks callbacks = {
		    need_memdata, NULL, seek_memdata
		};
		gst_app_src_set_callbacks(appsrc, &callbacks, self, NULL);
		gst_app_src_set_size(appsrc, self->MemData.size());
		gst_app_src_set_stream_type(appsrc, GST_APP_STREAM_TYPE_RANDOM_ACCESS);

		gst_object_unref(appsrc);
	}

	// Callback-based data source
	SoundStreamCallback Callback;
	void *UserData;
	int BufferBytes;
	GstCaps *SrcCaps;

	static void need_callback(GstAppSrc *appsrc, guint size, gpointer user_data)
	{
		OpenALSoundStream *self = static_cast<OpenALSoundStream*>(user_data);

		GstBuffer *buffer;
		if(!self->Playing)
			buffer = gst_buffer_new_and_alloc(0);
		else
		{
			buffer = gst_buffer_new_and_alloc(size);
			if(!self->Callback(self, GST_BUFFER_DATA(buffer), GST_BUFFER_SIZE(buffer), self->UserData))
			{
				gst_buffer_unref(buffer);

				gst_app_src_end_of_stream(appsrc);
				return;
			}
		}

		gst_app_src_push_buffer(appsrc, buffer);
	}

	static void callback_source(GObject *object, GObject *orig, GParamSpec *pspec, OpenALSoundStream *self)
	{
		GstElement *elem;
		g_object_get(self->gstPipeline, "source", &elem, NULL);

		GstAppSrc *appsrc = GST_APP_SRC(elem);
		GstAppSrcCallbacks callbacks = {
		    need_callback, NULL, NULL
		};
		gst_app_src_set_callbacks(appsrc, &callbacks, self, NULL);
		gst_app_src_set_size(appsrc, -1);
		gst_app_src_set_max_bytes(appsrc, self->BufferBytes);
		gst_app_src_set_stream_type(appsrc, GST_APP_STREAM_TYPE_STREAM);
		gst_app_src_set_caps(appsrc, self->SrcCaps);

		gst_object_unref(appsrc);
	}

	// General methods
	virtual bool SetupSource()
	{
		// We don't actually use this source if we have an openalsink. However,
		// holding on to it helps ensure we don't overrun our allotted voice
		// count.
		if(Renderer->FreeSfx.size() == 0)
		{
			FSoundChan *lowest = Renderer->FindLowestChannel();
			if(lowest) Renderer->StopChannel(lowest);

			if(Renderer->FreeSfx.size() == 0)
				return false;
		}
		Source = Renderer->FreeSfx.back();
		Renderer->FreeSfx.pop_back();

		alSource3f(Source, AL_DIRECTION, 0.f, 0.f, 0.f);
		alSource3f(Source, AL_VELOCITY, 0.f, 0.f, 0.f);
		alSource3f(Source, AL_POSITION, 0.f, 0.f, 0.f);
		alSourcef(Source, AL_MAX_GAIN, 1.f);
		alSourcef(Source, AL_GAIN, 1.f);
		alSourcef(Source, AL_PITCH, 1.f);
		alSourcef(Source, AL_ROLLOFF_FACTOR, 0.f);
		alSourcef(Source, AL_SEC_OFFSET, 0.f);
		alSourcei(Source, AL_SOURCE_RELATIVE, AL_TRUE);
		alSourcei(Source, AL_LOOPING, AL_FALSE);
		if(Renderer->EnvSlot)
		{
			alSourcef(Source, AL_ROOM_ROLLOFF_FACTOR, 0.f);
			alSourcef(Source, AL_AIR_ABSORPTION_FACTOR, 0.f);
			alSourcei(Source, AL_DIRECT_FILTER, AL_FILTER_NULL);
			alSource3i(Source, AL_AUXILIARY_SEND_FILTER, 0, 0, AL_FILTER_NULL);
		}

		return (getALError() == AL_NO_ERROR);
	}

	bool PipelineSetup()
	{
		TagList = gst_tag_list_new();
		g_return_val_if_fail(TagList != NULL, false);

		// Flags (can be combined):
		// 0x01: video        - Render the video stream
		// 0x02: audio        - Render the audio stream
		// 0x04: text         - Render subtitles
		// 0x08: vis          - Render visualisation when no video is present
		// 0x10: soft-volume  - Use software volume
		// 0x20: native-audio - Only use native audio formats
		// 0x40: native-video - Only use native video formats
		// 0x80: download     - Attempt progressive download buffering
		int flags = 0x02 | 0x10;

		gstPipeline = gst_element_factory_make("playbin2", NULL);
		g_return_val_if_fail(gstPipeline != NULL, false);

		GstElement *sink = gst_element_factory_make("openalsink", NULL);
		if(sink != NULL)
		{
			// Give the sink our device, so it can create its own context and
			// source to play with (and not risk cross-contaminating errors)
			g_object_set(sink, "device-handle", Renderer->Device, NULL);
		}
		else
		{
			static bool warned = false;
			if(!warned)
				g_warning("Could not create an openalsink\n");
			warned = true;

			sink = gst_element_factory_make("appsink", NULL);
			g_return_val_if_fail(sink != NULL, false);

			GstAppSink *appsink = GST_APP_SINK(sink);
			GstAppSinkCallbacks callbacks = {
				sink_eos, sink_preroll, sink_buffer, NULL
			};
			GstCaps *caps = SupportedBufferFormatCaps();

			gst_app_sink_set_callbacks(appsink, &callbacks, this, NULL);
			gst_app_sink_set_drop(appsink, FALSE);
			gst_app_sink_set_caps(appsink, caps);
			gst_caps_unref(caps);
		}

		// This takes ownership of the element; don't unref it
		g_object_set(gstPipeline, "audio-sink", sink, NULL);
		g_object_set(gstPipeline, "flags", flags, NULL);
		return true;
	}

	void HandleLoopTags()
	{
		// FIXME: Sample offsets assume a 44.1khz rate. Need to find some way
		// to get the actual rate of the file from GStreamer
		bool looppt_is_samples;
		unsigned int looppt;
		gchar *valstr;

		LoopPts[0] = 0;
		if(gst_tag_list_get_string(TagList, "LOOP_START", &valstr))
		{
			g_print("Got LOOP_START string: %s\n", valstr);
			if(!S_ParseTimeTag(valstr, &looppt_is_samples, &looppt))
				Printf("Invalid LOOP_START tag: '%s'\n", valstr);
			else
				LoopPts[0] = (looppt_is_samples ? ((gint64)looppt*1000000000/44100) :
				                                  ((gint64)looppt*1000000));
			g_free(valstr);
		}
		LoopPts[1] = -1;
		if(gst_tag_list_get_string(TagList, "LOOP_END", &valstr))
		{
			g_print("Got LOOP_END string: %s\n", valstr);
			if(!S_ParseTimeTag(valstr, &looppt_is_samples, &looppt))
				Printf("Invalid LOOP_END tag: '%s'\n", valstr);
			else
			{
				LoopPts[1] = (looppt_is_samples ? ((gint64)looppt*1000000000/44100) :
				                                  ((gint64)looppt*1000000));
				if(LoopPts[1] <= LoopPts[0])
					LoopPts[1] = -1;
			}
			g_free(valstr);
		}
	}

	bool PreparePipeline()
	{
		GstBus *bus = gst_element_get_bus(gstPipeline);
		if(!bus) return false;

		GstStateChangeReturn ret = gst_element_set_state(gstPipeline, GST_STATE_PAUSED);
		if(ret == GST_STATE_CHANGE_ASYNC)
		{
			const GstMessageType types = GST_MESSAGE_ERROR | GST_MESSAGE_TAG | GST_MESSAGE_ASYNC_DONE;
			GstMessage *msg;
			while((msg=gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, types)) != NULL)
			{
				if(GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
				{
					PrintErrMsg("Prepare Error", msg);
					ret = GST_STATE_CHANGE_FAILURE;
					break;
				}
				else if(GST_MESSAGE_TYPE(msg) == GST_MESSAGE_TAG)
				{
					GstTagList *tags = NULL;
					gst_message_parse_tag(msg, &tags);

					gst_tag_list_insert(TagList, tags, GST_TAG_MERGE_KEEP);

					gst_tag_list_free(tags);
				}
				else break;
				gst_message_unref(msg);
			}
		}
		else if(ret == GST_STATE_CHANGE_SUCCESS)
		{
			GstMessage *msg;
			while((msg=gst_bus_pop(bus)) != NULL)
			{
				if(GST_MESSAGE_TYPE(msg) == GST_MESSAGE_TAG)
				{
					GstTagList *tags = NULL;
					gst_message_parse_tag(msg, &tags);

					gst_tag_list_insert(TagList, tags, GST_TAG_MERGE_KEEP);

					gst_tag_list_free(tags);
				}
				gst_message_unref(msg);
			}
		}
		HandleLoopTags();

		gst_object_unref(bus);
		bus = NULL;

		return (ret != GST_STATE_CHANGE_FAILURE);
	}

public:
	FTempFileName tmpfile;
	ALfloat Volume;

	OpenALSoundStream(OpenALSoundRenderer *renderer)
		: Renderer(renderer), gstPipeline(NULL), TagList(NULL), Source(0),
		  Playing(false), Looping(false), SamplesQueued(0), Callback(NULL),
		  UserData(NULL), BufferBytes(0), SrcCaps(NULL), Volume(1.0f)
	{
		LoopPts[0] = LoopPts[1] = 0;
		Renderer->Streams.push_back(this);
	}

	virtual ~OpenALSoundStream()
	{
		Playing = false;

		if(SrcCaps)
			gst_caps_unref(SrcCaps);
		SrcCaps = NULL;

		if(gstPipeline)
		{
			gst_element_set_state(gstPipeline, GST_STATE_NULL);
			gst_object_unref(gstPipeline);
			gstPipeline = NULL;
		}

		if(TagList)
			gst_tag_list_free(TagList);
		TagList = NULL;

		if(Source)
		{
			alSourceRewind(Source);
			alSourcei(Source, AL_BUFFER, 0);

			Renderer->FreeSfx.push_back(Source);
			Source = 0;
		}

		if(Buffers.size() > 0)
		{
			alDeleteBuffers(Buffers.size(), &Buffers[0]);
			Buffers.clear();
		}
		getALError();

		Renderer->Streams.erase(find(Renderer->Streams.begin(),
		                             Renderer->Streams.end(), this));
		Renderer = NULL;
	}

	virtual bool Play(bool looping, float vol)
	{
		if(Playing)
			return true;

		GstBus *bus = gst_element_get_bus(gstPipeline);
		if(!bus) return false;

		Looping = looping;
		SetVolume(vol);

		if(Looping)
		{
			const GstSeekFlags flags = GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_SEGMENT;
			gst_element_seek(gstPipeline, 1.0, GST_FORMAT_TIME, flags,
			                 GST_SEEK_TYPE_NONE, 0, GST_SEEK_TYPE_SET, LoopPts[1]);
		}

		// Start playing the stream
		Playing = true;
		GstStateChangeReturn ret = gst_element_set_state(gstPipeline, GST_STATE_PLAYING);
		if(ret == GST_STATE_CHANGE_FAILURE)
			Playing = false;
		if(ret == GST_STATE_CHANGE_ASYNC)
		{
			const GstMessageType types = GST_MESSAGE_ERROR | GST_MESSAGE_ASYNC_DONE;
			GstMessage *msg;
			if((msg=gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, types)) != NULL)
			{
				if(GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
				{
					PrintErrMsg("Play Error", msg);
					Playing = false;
				}
				gst_message_unref(msg);
			}
		}

		gst_object_unref(bus);
		bus = NULL;

		return Playing;
	}

	virtual void Stop()
	{
		GstBus *bus = gst_element_get_bus(gstPipeline);
		if(!bus) return;

		// Stop the stream
		GstStateChangeReturn ret = gst_element_set_state(gstPipeline, GST_STATE_PAUSED);
		if(ret == GST_STATE_CHANGE_ASYNC)
		{
			Playing = false;
			// Wait for the state change before requesting a seek
			const GstMessageType types = GST_MESSAGE_ERROR | GST_MESSAGE_ASYNC_DONE;
			GstMessage *msg;
			if((msg=gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, types)) != NULL)
			{
				if(GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
					PrintErrMsg("Stop Error", msg);
				else if(GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ASYNC_DONE)
					ret = GST_STATE_CHANGE_SUCCESS;
				gst_message_unref(msg);
			}
		}
		if(ret == GST_STATE_CHANGE_SUCCESS)
		{
			Playing = false;

			alSourceRewind(Source);
			getALError();

			const GstSeekFlags flags = GST_SEEK_FLAG_FLUSH | GST_SEEK_FLAG_KEY_UNIT;
			gst_element_seek_simple(gstPipeline, GST_FORMAT_TIME, flags, 0);
		}

		gst_object_unref(bus);
		bus = NULL;
	}

	virtual bool SetPaused(bool paused)
	{
		GstBus *bus = gst_element_get_bus(gstPipeline);
		if(!bus) return false;

		GstStateChangeReturn ret;
		ret = gst_element_set_state(gstPipeline, (paused ? GST_STATE_PAUSED : GST_STATE_PLAYING));
		if(ret == GST_STATE_CHANGE_ASYNC)
		{
			const GstMessageType types = GST_MESSAGE_ERROR | GST_MESSAGE_ASYNC_DONE;
			GstMessage *msg;
			if((msg=gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, types)) != NULL)
			{
				if(GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
				{
					PrintErrMsg("Pause Error", msg);
					ret = GST_STATE_CHANGE_FAILURE;
				}
				gst_message_unref(msg);
			}
		}

		if(ret != GST_STATE_CHANGE_FAILURE && paused)
		{
			alSourcePause(Source);
			getALError();
		}

		gst_object_unref(bus);
		bus = NULL;

		return (ret != GST_STATE_CHANGE_FAILURE);
	}

	virtual void SetVolume(float vol)
	{
		Volume = vol;
		g_object_set(gstPipeline, "volume", (double)(Volume*Renderer->MusicVolume), NULL);
	}

	virtual unsigned int GetPosition()
	{
		GstFormat format = GST_FORMAT_TIME;
		gint64 pos;

		// Position will be handled in milliseconds; GStreamer's time format is in nanoseconds
		if(gst_element_query_position(gstPipeline, &format, &pos) && format == GST_FORMAT_TIME)
			return (unsigned int)(pos / 1000000);
		return 0;
	}

	virtual bool SetPosition(unsigned int val)
	{
		gint64 pos = (gint64)val * 1000000;
		return gst_element_seek_simple(gstPipeline, GST_FORMAT_TIME, GST_SEEK_FLAG_ACCURATE, pos);
	}

	virtual bool IsEnded()
	{
		GstBus *bus = gst_element_get_bus(gstPipeline);
		if(!bus) return true;

		GstMessage *msg;
		while((msg=gst_bus_pop(bus)) != NULL)
		{
			switch(GST_MESSAGE_TYPE(msg))
			{
			case GST_MESSAGE_SEGMENT_DONE:
			case GST_MESSAGE_EOS:
				Playing = false;
				if(Looping)
					Playing = gst_element_seek(gstPipeline, 1.0, GST_FORMAT_TIME, GST_SEEK_FLAG_FLUSH |
					                           GST_SEEK_FLAG_ACCURATE | GST_SEEK_FLAG_SEGMENT,
					                           GST_SEEK_TYPE_SET, LoopPts[0],
					                           GST_SEEK_TYPE_SET, LoopPts[1]);
				break;

			case GST_MESSAGE_ERROR:
				PrintErrMsg("Pipeline Error", msg);
				Playing = false;
				break;

			case GST_MESSAGE_WARNING:
				PrintErrMsg("Pipeline Warning", msg);
				break;

			default:
				break;
			}

			gst_message_unref(msg);
		}

		gst_object_unref(bus);
		bus = NULL;

		return !Playing;
	}

	bool Init(const char *filename)
	{
		if(!SetupSource() || !PipelineSetup())
			return false;

		GError *err = NULL;
		gchar *uri;
		if(g_path_is_absolute(filename))
			uri = g_filename_to_uri(filename, NULL, &err);
		else if(g_strrstr(filename, "://") != NULL)
			uri = g_strdup(filename);
		else
		{
			gchar *curdir = g_get_current_dir();
			gchar *absolute_path = g_strconcat(curdir, G_DIR_SEPARATOR_S, filename, NULL);
			uri = g_filename_to_uri(absolute_path, NULL, &err);
			g_free(absolute_path);
			g_free(curdir);
		}

		if(!uri)
		{
			if(err)
			{
				Printf("Failed to convert "TEXTCOLOR_BOLD"%s"TEXTCOLOR_NORMAL" to URI: %s\n",
				       filename, err->message);
				g_error_free(err);
			}
			return false;
		}

		g_object_set(gstPipeline, "uri", uri, NULL);
		g_free(uri);

		return PreparePipeline();
	}

	bool Init(const BYTE *data, unsigned int datalen)
	{
		// Can't keep the original pointer since the memory can apparently be
		// overwritten at some point (happens with MIDI data, at least)
		MemData.resize(datalen);
		memcpy(&MemData[0], data, datalen);
		MemDataPos = 0;

		if(!SetupSource() || !PipelineSetup())
			return false;

		g_object_set(gstPipeline, "uri", "appsrc://", NULL);
		g_signal_connect(gstPipeline, "deep-notify::source", G_CALLBACK(memdata_source), this);

		return PreparePipeline();
	}

	bool Init(SoundStreamCallback callback, int buffbytes, int flags, int samplerate, void *userdata)
	{
		Callback = callback;
		UserData = userdata;
		BufferBytes = buffbytes;

		GstStructure *structure;
		if((flags&Float))
			structure = gst_structure_new("audio/x-raw-float",
			    "endianness", G_TYPE_INT, G_BYTE_ORDER,
			    "width", G_TYPE_INT, 32, NULL);
		else if((flags&Bits32))
			structure = gst_structure_new("audio/x-raw-int",
			    "endianness", G_TYPE_INT, G_BYTE_ORDER,
			    "width", G_TYPE_INT, 32,
			    "depth", G_TYPE_INT, 32,
			    "signed", G_TYPE_BOOLEAN, TRUE, NULL);
		else if((flags&Bits8))
			structure = gst_structure_new("audio/x-raw-int",
			    "width", G_TYPE_INT, 8,
			    "depth", G_TYPE_INT, 8,
			    "signed", G_TYPE_BOOLEAN, TRUE, NULL);
		else
			structure = gst_structure_new("audio/x-raw-int",
			    "endianness", G_TYPE_INT, G_BYTE_ORDER,
			    "width", G_TYPE_INT, 16,
			    "depth", G_TYPE_INT, 16,
			    "signed", G_TYPE_BOOLEAN, TRUE, NULL);
		gst_structure_set(structure, "channels", G_TYPE_INT, (flags&Mono)?1:2, NULL);
		gst_structure_set(structure, "rate", G_TYPE_INT, samplerate, NULL);

		SrcCaps = gst_caps_new_full(structure, NULL);

		if(!SrcCaps || !SetupSource() || !PipelineSetup())
			return false;

		g_object_set(gstPipeline, "uri", "appsrc://", NULL);
		g_signal_connect(gstPipeline, "deep-notify::source", G_CALLBACK(callback_source), this);

		return PreparePipeline();
	}
};


class Decoder
{
	GstElement *gstPipeline, *gstSink;
	GstTagList *TagList;

	const guint8 *MemData;
	size_t MemDataSize;
	size_t MemDataPos;

	static void need_memdata(GstAppSrc *appsrc, guint size, gpointer user_data)
	{
		Decoder *self = static_cast<Decoder*>(user_data);
		GstFlowReturn ret;

		if(self->MemDataPos >= self->MemDataSize)
		{
			gst_app_src_end_of_stream(appsrc);
			return;
		}

		const guint8 *data = &self->MemData[self->MemDataPos];
		size = (std::min)(size, (guint)(self->MemDataSize - self->MemDataPos));
		self->MemDataPos += size;

		GstBuffer *buffer = gst_buffer_new();
		GST_BUFFER_DATA(buffer) = const_cast<guint8*>(data);
		GST_BUFFER_SIZE(buffer) = size;

		gst_app_src_push_buffer(appsrc, buffer);
	}

	static gboolean seek_memdata(GstAppSrc *appsrc, guint64 position, gpointer user_data)
	{
		Decoder *self = static_cast<Decoder*>(user_data);

		if(position > self->MemDataSize)
			return FALSE;
		self->MemDataPos = position;
		return TRUE;
	}

	static void memdata_source(GObject *object, GObject *orig, GParamSpec *pspec, Decoder *self)
	{
		GstElement *elem;
		g_object_get(self->gstPipeline, "source", &elem, NULL);

		GstAppSrc *appsrc = GST_APP_SRC(elem);
		GstAppSrcCallbacks callbacks = {
		    need_memdata, NULL, seek_memdata
		};
		gst_app_src_set_callbacks(appsrc, &callbacks, self, NULL);
		gst_app_src_set_size(appsrc, self->MemDataSize);
		gst_app_src_set_stream_type(appsrc, GST_APP_STREAM_TYPE_RANDOM_ACCESS);

		gst_object_unref(appsrc);
	}

	static GstFlowReturn sink_preroll(GstAppSink *sink, gpointer user_data)
	{
		Decoder *self = static_cast<Decoder*>(user_data);

		GstBuffer *buffer = gst_app_sink_pull_preroll(sink);
		if(!buffer) return GST_FLOW_ERROR;

		if(self->OutRate == 0)
		{
			GstCaps *caps = GST_BUFFER_CAPS(buffer);

			gint channels = 0, rate = 0, bits = 0, i;
			for(i = gst_caps_get_size(caps)-1;i >= 0;i--)
			{
				GstStructure *struc = gst_caps_get_structure(caps, i);
				if(gst_structure_has_field(struc, "channels"))
					gst_structure_get_int(struc, "channels", &channels);
				if(gst_structure_has_field(struc, "rate"))
					gst_structure_get_int(struc, "rate", &rate);
				if(gst_structure_has_field(struc, "width"))
					gst_structure_get_int(struc, "width", &bits);
			}

			self->OutChannels = channels;
			self->OutBits = bits;
			self->OutRate = rate;
		}

		gst_buffer_unref(buffer);
		if(self->OutRate <= 0)
			return GST_FLOW_ERROR;
		return GST_FLOW_OK;
	}

	static GstFlowReturn sink_buffer(GstAppSink *sink, gpointer user_data)
	{
		Decoder *self = static_cast<Decoder*>(user_data);

		GstBuffer *buffer = gst_app_sink_pull_buffer(sink);
		if(!buffer) return GST_FLOW_ERROR;

		guint newsize = GST_BUFFER_SIZE(buffer);
		size_t pos = self->OutData.size();
		self->OutData.resize(pos+newsize);

		memcpy(&self->OutData[pos], GST_BUFFER_DATA(buffer), newsize);

		gst_buffer_unref(buffer);
		return GST_FLOW_OK;
	}

	bool PipelineSetup(int forcebits)
	{
		if(forcebits && forcebits != 8 && forcebits != 16)
			return false;

		TagList = gst_tag_list_new();
		g_return_val_if_fail(TagList != NULL, false);

		gstPipeline = gst_element_factory_make("playbin2", NULL);
		g_return_val_if_fail(gstPipeline != NULL, false);

		gstSink = gst_element_factory_make("appsink", NULL);
		g_return_val_if_fail(gstSink != NULL, false);

		GstAppSink *appsink = GST_APP_SINK(gstSink);
		GstAppSinkCallbacks callbacks = {
			NULL, sink_preroll, sink_buffer, NULL
		};

		GstCaps *caps = SupportedBufferFormatCaps(forcebits);
		g_object_set(appsink, "sync", FALSE, NULL);
		gst_app_sink_set_callbacks(appsink, &callbacks, this, NULL);
		gst_app_sink_set_drop(appsink, FALSE);
		gst_app_sink_set_caps(appsink, caps);

		g_object_set(gstPipeline, "audio-sink", gst_object_ref(gstSink), NULL);
		g_object_set(gstPipeline, "flags", 0x02, NULL);

		gst_caps_unref(caps);
		return true;
	}

	void HandleLoopTags(unsigned int looppt[2], bool looppt_is_samples[2], bool has_looppt[2])
	{
		gchar *valstr;

		if(gst_tag_list_get_string(TagList, "LOOP_START", &valstr))
		{
			g_print("Got LOOP_START string: %s\n", valstr);
			has_looppt[0] = S_ParseTimeTag(valstr, &looppt_is_samples[0], &looppt[0]);
			if(!has_looppt[0])
				Printf("Invalid LOOP_START tag: '%s'\n", valstr);
			g_free(valstr);
		}
		if(gst_tag_list_get_string(TagList, "LOOP_END", &valstr))
		{
			g_print("Got LOOP_END string: %s\n", valstr);
			has_looppt[1] = S_ParseTimeTag(valstr, &looppt_is_samples[1], &looppt[1]);
			if(!has_looppt[1])
				Printf("Invalid LOOP_END tag: '%s'\n", valstr);
			g_free(valstr);
		}
	}

public:
	std::vector<BYTE> OutData;
	ALint LoopPts[2];
	ALsizei OutRate;
	ALuint OutChannels;
	ALuint OutBits;

	Decoder()
		: gstPipeline(NULL), gstSink(NULL), TagList(NULL),
		  OutRate(0), OutChannels(0), OutBits(0)
	{ LoopPts[0] = LoopPts[1] = 0; }

	virtual ~Decoder()
	{
		if(gstSink)
			gst_object_unref(gstSink);
		gstSink = NULL;

		if(gstPipeline)
		{
			gst_element_set_state(gstPipeline, GST_STATE_NULL);
			gst_object_unref(gstPipeline);
			gstPipeline = NULL;
		}

		if(TagList)
			gst_tag_list_free(TagList);
		TagList = NULL;
	}

	bool Decode(const void *data, unsigned int datalen, int forcebits=0)
	{
		MemData = static_cast<const guint8*>(data);
		MemDataSize = datalen;
		MemDataPos = 0;
		OutData.clear();

		if(!PipelineSetup(forcebits))
			return false;

		g_object_set(gstPipeline, "uri", "appsrc://", NULL);
		g_signal_connect(gstPipeline, "deep-notify::source", G_CALLBACK(memdata_source), this);


		GstBus *bus = gst_element_get_bus(gstPipeline);
		if(!bus) return false;
		GstMessage *msg;

		GstStateChangeReturn ret = gst_element_set_state(gstPipeline, GST_STATE_PLAYING);
		if(ret == GST_STATE_CHANGE_ASYNC)
		{
			const GstMessageType types = GST_MESSAGE_ERROR | GST_MESSAGE_TAG | GST_MESSAGE_ASYNC_DONE;
			while((msg=gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, types)) != NULL)
			{
				if(GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ASYNC_DONE)
				{
					ret = GST_STATE_CHANGE_SUCCESS;
					break;
				}
				else if(GST_MESSAGE_TYPE(msg) == GST_MESSAGE_TAG)
				{
					GstTagList *tags = NULL;
					gst_message_parse_tag(msg, &tags);

					gst_tag_list_insert(TagList, tags, GST_TAG_MERGE_KEEP);

					gst_tag_list_free(tags);
				}
				else if(GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
				{
					ret = GST_STATE_CHANGE_FAILURE;
					PrintErrMsg("Decoder Error", msg);
					break;
				}
				gst_message_unref(msg);
			}
		}

		bool err = true;
		if(ret == GST_STATE_CHANGE_SUCCESS)
		{
			const GstMessageType types = GST_MESSAGE_ERROR | GST_MESSAGE_TAG | GST_MESSAGE_EOS;
			while((msg=gst_bus_timed_pop_filtered(bus, GST_CLOCK_TIME_NONE, types)) != NULL)
			{
				if(GST_MESSAGE_TYPE(msg) == GST_MESSAGE_EOS)
				{
					err = false;
					break;
				}
				else if(GST_MESSAGE_TYPE(msg) == GST_MESSAGE_TAG)
				{
					GstTagList *tags = NULL;
					gst_message_parse_tag(msg, &tags);

					gst_tag_list_insert(TagList, tags, GST_TAG_MERGE_KEEP);

					gst_tag_list_free(tags);
				}
				else if(GST_MESSAGE_TYPE(msg) == GST_MESSAGE_ERROR)
				{
					PrintErrMsg("Decoder Error", msg);
					break;
				}
				gst_message_unref(msg);
			}
		}

		if(!err)
		{
			ALuint FrameSize = OutChannels*OutBits/8;
			if(OutData.size() >= FrameSize)
			{
				// HACK: Evilness abound. Seems GStreamer doesn't like (certain?)
				// wave files and produces an extra sample, which can cause an
				// audible click at the end. Cut it.
				OutData.resize(OutData.size() - FrameSize);
			}

			unsigned int looppt[2] = { 0, 0 };
			bool looppt_is_samples[2] = { true, true };
			bool has_looppt[2] = { false, false };
			HandleLoopTags(looppt, looppt_is_samples, has_looppt);

			if(has_looppt[0] || has_looppt[1])
			{
				if(!has_looppt[0])
					LoopPts[0] = 0;
				else if(looppt_is_samples[0])
					LoopPts[0] = (std::min)((ALint)looppt[0],
					                        (ALint)(OutData.size() / FrameSize));
				else
					LoopPts[0] = (std::min)((ALint)((gint64)looppt[0] * OutRate / 1000),
					                        (ALint)(OutData.size() / FrameSize));

				if(!has_looppt[1])
					LoopPts[1] = OutData.size() / FrameSize;
				else if(looppt_is_samples[1])
					LoopPts[1] = (std::min)((ALint)looppt[1],
					                        (ALint)(OutData.size() / FrameSize));
				else
					LoopPts[1] = (std::min)((ALint)((gint64)looppt[1] * OutRate / 1000),
					                        (ALint)(OutData.size() / FrameSize));
			}
		}

		gst_object_unref(bus);
		bus = NULL;

		return !err;
	}
};
/*** GStreamer end ***/

template<typename T>
static void LoadALFunc(const char *name, T *x)
{ *x = reinterpret_cast<T>(alGetProcAddress(name)); }

OpenALSoundRenderer::OpenALSoundRenderer()
	: Device(NULL), Context(NULL), SrcDistanceModel(false), SFXPaused(0),
	  PrevEnvironment(NULL), EnvSlot(0)
{
	EnvFilters[0] = EnvFilters[1] = 0;

	Printf("I_InitSound: Initializing OpenAL\n");

	static bool GSTInited = false;
	if(!GSTInited)
	{
		GError *error;
		if(!gst_init_check(NULL, NULL, &error))
		{
			Printf("Failed to initialize GStreamer: %s\n", error->message);
			g_error_free(error);
			return;
		}
		GSTInited = true;
	}

	if(snd_aldevice != "Default")
	{
		Device = alcOpenDevice(*snd_aldevice);
		if(!Device)
			Printf(TEXTCOLOR_BLUE" Failed to open device "TEXTCOLOR_BOLD"%s"TEXTCOLOR_BLUE". Trying default.\n", *snd_aldevice);
	}

	if(!Device)
	{
		Device = alcOpenDevice(NULL);
		if(!Device)
		{
			Printf(TEXTCOLOR_RED" Could not open audio device\n");
			return;
		}
	}

	Printf("  Opened device "TEXTCOLOR_ORANGE"%s\n", alcGetString(Device, ALC_DEVICE_SPECIFIER));
	ALCint major=0, minor=0;
	alcGetIntegerv(Device, ALC_MAJOR_VERSION, 1, &major);
	alcGetIntegerv(Device, ALC_MINOR_VERSION, 1, &minor);
	DPrintf("  ALC Version: "TEXTCOLOR_BLUE"%d.%d\n", major, minor);
	DPrintf("  ALC Extensions: "TEXTCOLOR_ORANGE"%s\n", alcGetString(Device, ALC_EXTENSIONS));

	DisconnectNotify = alcIsExtensionPresent(Device, "ALC_EXT_disconnect");

	std::vector<ALCint> attribs;
	if(*snd_samplerate > 0)
	{
		attribs.push_back(ALC_FREQUENCY);
		attribs.push_back(*snd_samplerate);
	}
	// Make sure one source is capable of stereo output with the rest doing
	// mono, without running out of voices
	attribs.push_back(ALC_MONO_SOURCES);
	attribs.push_back((std::max)(*snd_channels, 2) - 1);
	attribs.push_back(ALC_STEREO_SOURCES);
	attribs.push_back(1);
	// Other attribs..?
	attribs.push_back(0);

	Context = alcCreateContext(Device, &attribs[0]);
	if(!Context || alcMakeContextCurrent(Context) == ALC_FALSE)
	{
		Printf(TEXTCOLOR_RED"  Failed to setup context: %s\n", alcGetString(Device, alcGetError(Device)));
		if(Context)
			alcDestroyContext(Context);
		Context = NULL;
		alcCloseDevice(Device);
		Device = NULL;
		return;
	}
	attribs.clear();

	DPrintf("  Vendor: "TEXTCOLOR_ORANGE"%s\n", alGetString(AL_VENDOR));
	DPrintf("  Renderer: "TEXTCOLOR_ORANGE"%s\n", alGetString(AL_RENDERER));
	DPrintf("  Version: "TEXTCOLOR_ORANGE"%s\n", alGetString(AL_VERSION));
	DPrintf("  Extensions: "TEXTCOLOR_ORANGE"%s\n", alGetString(AL_EXTENSIONS));

	SrcDistanceModel = alIsExtensionPresent("AL_EXT_source_distance_model");
	LoopPoints = alIsExtensionPresent("AL_EXT_loop_points");

	alDopplerFactor(0.5f);
	alSpeedOfSound(343.3f * 96.0f);
	alDistanceModel(AL_INVERSE_DISTANCE);
	if(SrcDistanceModel)
		alEnable(AL_SOURCE_DISTANCE_MODEL);

	ALenum err = getALError();
	if(err != AL_NO_ERROR)
	{
		alcMakeContextCurrent(NULL);
		alcDestroyContext(Context);
		Context = NULL;
		alcCloseDevice(Device);
		Device = NULL;
		return;
	}

	ALCint numMono=0, numStereo=0;
	alcGetIntegerv(Device, ALC_MONO_SOURCES, 1, &numMono);
	alcGetIntegerv(Device, ALC_STEREO_SOURCES, 1, &numStereo);

	Sources.resize((std::min)((std::max)(*snd_channels, 2), numMono+numStereo));
	for(size_t i = 0;i < Sources.size();i++)
	{
		alGenSources(1, &Sources[i]);
		if(getALError() != AL_NO_ERROR)
		{
			Sources.resize(i);
			break;
		}
		FreeSfx.push_back(Sources[i]);
	}
	if(Sources.size() == 0)
	{
		Printf(TEXTCOLOR_RED" Error: could not generate any sound sources!\n");
		alcMakeContextCurrent(NULL);
		alcDestroyContext(Context);
		Context = NULL;
		alcCloseDevice(Device);
		Device = NULL;
		return;
	}
	DPrintf("  Allocated "TEXTCOLOR_BLUE"%u"TEXTCOLOR_NORMAL" sources\n", Sources.size());

	LastWaterAbsorb = 0.0f;
	if(*snd_efx && alcIsExtensionPresent(Device, "ALC_EXT_EFX"))
	{
		// EFX function pointers
#define LOAD_FUNC(x)  (LoadALFunc(#x, &x))
		LOAD_FUNC(alGenEffects);
		LOAD_FUNC(alDeleteEffects);
		LOAD_FUNC(alIsEffect);
		LOAD_FUNC(alEffecti);
		LOAD_FUNC(alEffectiv);
		LOAD_FUNC(alEffectf);
		LOAD_FUNC(alEffectfv);
		LOAD_FUNC(alGetEffecti);
		LOAD_FUNC(alGetEffectiv);
		LOAD_FUNC(alGetEffectf);
		LOAD_FUNC(alGetEffectfv);

		LOAD_FUNC(alGenFilters);
		LOAD_FUNC(alDeleteFilters);
		LOAD_FUNC(alIsFilter);
		LOAD_FUNC(alFilteri);
		LOAD_FUNC(alFilteriv);
		LOAD_FUNC(alFilterf);
		LOAD_FUNC(alFilterfv);
		LOAD_FUNC(alGetFilteri);
		LOAD_FUNC(alGetFilteriv);
		LOAD_FUNC(alGetFilterf);
		LOAD_FUNC(alGetFilterfv);

		LOAD_FUNC(alGenAuxiliaryEffectSlots);
		LOAD_FUNC(alDeleteAuxiliaryEffectSlots);
		LOAD_FUNC(alIsAuxiliaryEffectSlot);
		LOAD_FUNC(alAuxiliaryEffectSloti);
		LOAD_FUNC(alAuxiliaryEffectSlotiv);
		LOAD_FUNC(alAuxiliaryEffectSlotf);
		LOAD_FUNC(alAuxiliaryEffectSlotfv);
		LOAD_FUNC(alGetAuxiliaryEffectSloti);
		LOAD_FUNC(alGetAuxiliaryEffectSlotiv);
		LOAD_FUNC(alGetAuxiliaryEffectSlotf);
		LOAD_FUNC(alGetAuxiliaryEffectSlotfv);
#undef LOAD_FUNC
		if(getALError() == AL_NO_ERROR)
		{
			ALuint envReverb;
			alGenEffects(1, &envReverb);
			if(getALError() == AL_NO_ERROR)
			{
				alEffecti(envReverb, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
				if(alGetError() == AL_NO_ERROR)
					DPrintf("  EAX Reverb found\n");
				alEffecti(envReverb, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
				if(alGetError() == AL_NO_ERROR)
					DPrintf("  Standard Reverb found\n");

				alDeleteEffects(1, &envReverb);
				getALError();
			}

			alGenAuxiliaryEffectSlots(1, &EnvSlot);
			alGenFilters(2, EnvFilters);
			if(getALError() == AL_NO_ERROR)
			{
				alFilteri(EnvFilters[0], AL_FILTER_TYPE, AL_FILTER_LOWPASS);
				alFilteri(EnvFilters[1], AL_FILTER_TYPE, AL_FILTER_LOWPASS);
				if(getALError() == AL_NO_ERROR)
					DPrintf("  Lowpass found\n");
				else
				{
					alDeleteFilters(2, EnvFilters);
					EnvFilters[0] = EnvFilters[1] = 0;
					alDeleteAuxiliaryEffectSlots(1, &EnvSlot);
					EnvSlot = 0;
					getALError();
				}
			}
			else
			{
				alDeleteFilters(2, EnvFilters);
				alDeleteAuxiliaryEffectSlots(1, &EnvSlot);
				EnvFilters[0] = EnvFilters[1] = 0;
				EnvSlot = 0;
				getALError();
			}
		}
	}

	if(EnvSlot)
		Printf("  EFX enabled\n");

	snd_sfxvolume.Callback();
}

OpenALSoundRenderer::~OpenALSoundRenderer()
{
	if(!Device)
		return;

	while(Streams.size() > 0)
		delete Streams[0];

	alDeleteSources(Sources.size(), &Sources[0]);
	Sources.clear();
	FreeSfx.clear();
	SfxGroup.clear();
	PausableSfx.clear();
	ReverbSfx.clear();

	for(EffectMap::iterator i = EnvEffects.begin();i != EnvEffects.end();i++)
	{
		if(i->second)
			alDeleteEffects(1, &(i->second));
	}
	EnvEffects.clear();

	if(EnvSlot)
	{
		alDeleteAuxiliaryEffectSlots(1, &EnvSlot);
		alDeleteFilters(2, EnvFilters);
	}
	EnvSlot = 0;
	EnvFilters[0] = EnvFilters[1] = 0;

	alcMakeContextCurrent(NULL);
	alcDestroyContext(Context);
	Context = NULL;
	alcCloseDevice(Device);
	Device = NULL;
}

void OpenALSoundRenderer::SetSfxVolume(float volume)
{
	SfxVolume = volume;

	FSoundChan *schan = Channels;
	while(schan)
	{
		if(schan->SysChannel != NULL)
		{
			ALuint source = *((ALuint*)schan->SysChannel);
			volume = SfxVolume;

			alcSuspendContext(Context);
			alSourcef(source, AL_MAX_GAIN, volume);
			if(schan->ManualGain)
				volume *= GetRolloff(&schan->Rolloff, sqrt(schan->DistanceSqr));
			alSourcef(source, AL_GAIN, volume);
		}
		schan = schan->NextChan;
	}

	getALError();
}

void OpenALSoundRenderer::SetMusicVolume(float volume)
{
	MusicVolume = volume;
	foreach(OpenALSoundStream*, i, Streams)
		(*i)->SetVolume((*i)->Volume);
}

unsigned int OpenALSoundRenderer::GetMSLength(SoundHandle sfx)
{
	if(sfx.data)
	{
		ALuint buffer = *((ALuint*)sfx.data);
		if(alIsBuffer(buffer))
		{
			ALint bits, channels, freq, size;
			alGetBufferi(buffer, AL_BITS, &bits);
			alGetBufferi(buffer, AL_CHANNELS, &channels);
			alGetBufferi(buffer, AL_FREQUENCY, &freq);
			alGetBufferi(buffer, AL_SIZE, &size);
			if(getALError() == AL_NO_ERROR)
				return (unsigned int)(size / (channels*bits/8) * 1000. / freq);
		}
	}
	return 0;
}

unsigned int OpenALSoundRenderer::GetSampleLength(SoundHandle sfx)
{
	if(!sfx.data)
		return 0;
	return getBufferLength(*((ALuint*)sfx.data));
}

float OpenALSoundRenderer::GetOutputRate()
{
	ALCint rate = 44100; // Default, just in case
	alcGetIntegerv(Device, ALC_FREQUENCY, 1, &rate);
	return (float)rate;
}


SoundHandle OpenALSoundRenderer::LoadSoundRaw(BYTE *sfxdata, int length, int frequency, int channels, int bits, int loopstart)
{
	SoundHandle retval = { NULL };

	if(length == 0) return retval;

	if(bits == -8)
	{
		// Simple signed->unsigned conversion
		for(int i = 0;i < length;i++)
			sfxdata[i] ^= 0x80;
		bits = -bits;
	}

	ALenum format = AL_NONE;
	if(bits == 16)
	{
		if(channels == 1) format = AL_FORMAT_MONO16;
		if(channels == 2) format = AL_FORMAT_STEREO16;
	}
	else if(bits == 8)
	{
		if(channels == 1) format = AL_FORMAT_MONO8;
		if(channels == 2) format = AL_FORMAT_STEREO8;
	}

	if(format == AL_NONE || frequency <= 0)
	{
		Printf("Unhandled format: %d bit, %d channel, %d hz\n", bits, channels, frequency);
		return retval;
	}
	length -= length%(channels*bits/8);

	ALenum err;
	ALuint buffer = 0;
	alGenBuffers(1, &buffer);
	alBufferData(buffer, format, sfxdata, length, frequency);
	if((err=getALError()) != AL_NO_ERROR)
	{
		Printf("Failed to buffer data: %s\n", alGetString(err));
		alDeleteBuffers(1, &buffer);
		getALError();
		return retval;
	}

	if(loopstart > 0 && LoopPoints)
	{
		ALint loops[2] = { 
			loopstart,
			length / (channels*bits/8)
		};
		Printf("Setting loop points %d -> %d\n", loops[0], loops[1]);
		alBufferiv(buffer, AL_LOOP_POINTS, loops);
		getALError();
	}
	else if(loopstart > 0)
	{
		static bool warned = false;
		if(!warned)
			Printf("Loop points not supported!\n");
		warned = true;
	}

	retval.data = new ALuint(buffer);
	return retval;
}

SoundHandle OpenALSoundRenderer::LoadSound(BYTE *sfxdata, int length)
{
	SoundHandle retval = { NULL };

	Decoder decoder;
	if(!decoder.Decode(sfxdata, length))
	{
		DPrintf("Failed to decode sound\n");
		return retval;
	}

	ALenum format = FormatFromDesc(decoder.OutBits, decoder.OutChannels);
	if(format == AL_NONE)
	{
		Printf("Unhandled format: %d bit, %d channel\n", decoder.OutBits, decoder.OutChannels);
		return retval;
	}

	ALenum err;
	ALuint buffer = 0;
	alGenBuffers(1, &buffer);
	alBufferData(buffer, format, &decoder.OutData[0],
	             decoder.OutData.size(), decoder.OutRate);
	if((err=getALError()) != AL_NO_ERROR)
	{
		Printf("Failed to buffer data: %s\n", alGetString(err));
		alDeleteBuffers(1, &buffer);
		getALError();
		return retval;
	}

	if(LoopPoints && decoder.LoopPts[1] > decoder.LoopPts[0])
	{
		alBufferiv(buffer, AL_LOOP_POINTS, decoder.LoopPts);
		getALError();
	}
	else if(decoder.LoopPts[1] > decoder.LoopPts[0])
	{
		static bool warned = false;
		if(!warned)
			Printf("Loop points not supported!\n");
		warned = true;
	}

	retval.data = new ALuint(buffer);
	return retval;
}

void OpenALSoundRenderer::UnloadSound(SoundHandle sfx)
{
	if(!sfx.data)
		return;

	FSoundChan *schan = Channels;
	while(schan)
	{
		if(schan->SysChannel)
		{
			ALint bufID = 0;
			alGetSourcei(*((ALuint*)schan->SysChannel), AL_BUFFER, &bufID);
			if(bufID == *((ALint*)sfx.data))
			{
				FSoundChan *next = schan->NextChan;
				StopChannel(schan);
				schan = next;
				continue;
			}
		}
		schan = schan->NextChan;
	}

	alDeleteBuffers(1, ((ALuint*)sfx.data));
	getALError();
	delete ((ALuint*)sfx.data);
}

short *OpenALSoundRenderer::DecodeSample(int outlen, const void *coded, int sizebytes, ECodecType type)
{
	Decoder decoder;
	// Force 16-bit
	if(!decoder.Decode(coded, sizebytes, 16))
	{
		DPrintf("Failed to decode sample\n");
		return NULL;
	}
	if(decoder.OutChannels != 1)
	{
		DPrintf("Sample is not mono\n");
		return NULL;
	}

	short *samples = (short*)malloc(outlen);
	if((size_t)outlen > decoder.OutData.size())
	{
		memcpy(samples, &decoder.OutData[0], decoder.OutData.size());
		memset(&samples[decoder.OutData.size()/sizeof(short)], 0, outlen-decoder.OutData.size());
	}
	else
		memcpy(samples, &decoder.OutData[0], outlen);

	return samples;
}

SoundStream *OpenALSoundRenderer::CreateStream(SoundStreamCallback callback, int buffbytes, int flags, int samplerate, void *userdata)
{
	OpenALSoundStream *stream = new OpenALSoundStream(this);
	if(!stream->Init(callback, buffbytes, flags, samplerate, userdata))
	{
		delete stream;
		stream = NULL;
	}
	return stream;
}

SoundStream *OpenALSoundRenderer::OpenStream(const char *filename, int flags, int offset, int length)
{
	std::auto_ptr<OpenALSoundStream> stream(new OpenALSoundStream(this));

	if(offset > 0)
	{
		// If there's an offset to the start of the data, separate it into its
		// own temp file
		FILE *infile = fopen(filename, "rb");
		FILE *f = fopen(stream->tmpfile, "wb");
		if(!infile || !f || fseek(infile, offset, SEEK_SET) != 0)
		{
			if(infile) fclose(infile);
			if(f) fclose(f);
			return NULL;
		}
		BYTE buf[1024];
		size_t got;
		do {
			got = (std::min)(sizeof(buf), (size_t)length);
			got = fread(buf, 1, got, infile);
			if(got == 0)
				break;
		} while(fwrite(buf, 1, got, f) == got && (length-=got) > 0);
		fclose(f);
		fclose(infile);

		filename = stream->tmpfile;
	}

	bool ok = ((offset == -1) ? stream->Init((const BYTE*)filename, length) :
	                            stream->Init(filename));
	if(ok == false)
		return NULL;

	return stream.release();
}

FISoundChannel *OpenALSoundRenderer::StartSound(SoundHandle sfx, float vol, int pitch, int chanflags, FISoundChannel *reuse_chan)
{
	if(FreeSfx.size() == 0)
	{
		FSoundChan *lowest = FindLowestChannel();
		if(lowest) StopChannel(lowest);

		if(FreeSfx.size() == 0)
			return NULL;
	}

	ALuint buffer = *((ALuint*)sfx.data);
	ALuint &source = *find(Sources.begin(), Sources.end(), FreeSfx.back());
	alSource3f(source, AL_POSITION, 0.f, 0.f, 0.f);
	alSource3f(source, AL_VELOCITY, 0.f, 0.f, 0.f);
	alSource3f(source, AL_DIRECTION, 0.f, 0.f, 0.f);
	alSourcei(source, AL_SOURCE_RELATIVE, AL_TRUE);

	alSourcei(source, AL_LOOPING, (chanflags&SNDF_LOOP) ? AL_TRUE : AL_FALSE);

	alSourcef(source, AL_REFERENCE_DISTANCE, 1.f);
	alSourcef(source, AL_MAX_DISTANCE, 1000.f);
	alSourcef(source, AL_ROLLOFF_FACTOR, 0.f);
	alSourcef(source, AL_MAX_GAIN, SfxVolume);
	alSourcef(source, AL_GAIN, SfxVolume*vol);

	if(EnvSlot)
	{
		if(!(chanflags&SNDF_NOREVERB))
		{
			alSourcei(source, AL_DIRECT_FILTER, EnvFilters[0]);
			alSource3i(source, AL_AUXILIARY_SEND_FILTER, EnvSlot, 0, EnvFilters[1]);
			alSourcef(source, AL_AIR_ABSORPTION_FACTOR, LastWaterAbsorb);
		}
		else
		{
			alSourcei(source, AL_DIRECT_FILTER, AL_FILTER_NULL);
			alSource3i(source, AL_AUXILIARY_SEND_FILTER, 0, 0, AL_FILTER_NULL);
			alSourcef(source, AL_AIR_ABSORPTION_FACTOR, 0.f);
		}
		alSourcef(source, AL_ROOM_ROLLOFF_FACTOR, 0.f);
		alSourcef(source, AL_PITCH, PITCH(pitch));
	}
	else if(LastWaterAbsorb > 0.f && !(chanflags&SNDF_NOREVERB))
		alSourcef(source, AL_PITCH, PITCH(pitch)*PITCH_MULT);
	else
		alSourcef(source, AL_PITCH, PITCH(pitch));

	if(!reuse_chan)
		alSourcef(source, AL_SEC_OFFSET, 0.f);
	else
	{
		if((chanflags&SNDF_ABSTIME))
			alSourcef(source, AL_SEC_OFFSET, reuse_chan->StartTime.Lo/1000.f);
		else
		{
			// FIXME: set offset based on the current time and the StartTime
			alSourcef(source, AL_SEC_OFFSET, 0.f);
		}
	}
	if(getALError() != AL_NO_ERROR)
		return NULL;

	alSourcei(source, AL_BUFFER, buffer);
	if((chanflags&SNDF_NOPAUSE) || !SFXPaused)
		alSourcePlay(source);
	if(getALError() != AL_NO_ERROR)
	{
		alSourcei(source, AL_BUFFER, 0);
		getALError();
		return NULL;
	}

	if(!(chanflags&SNDF_NOREVERB))
		ReverbSfx.push_back(source);
	if(!(chanflags&SNDF_NOPAUSE))
		PausableSfx.push_back(source);
	SfxGroup.push_back(source);
	FreeSfx.pop_back();

	FISoundChannel *chan = reuse_chan;
	if(!chan) chan = S_GetChannel(&source);
	else chan->SysChannel = &source;

	chan->Rolloff.RolloffType = ROLLOFF_Linear;
	chan->Rolloff.MaxDistance = 2.f;
	chan->Rolloff.MinDistance = 1.f;
	chan->DistanceScale = 1.f;
	chan->DistanceSqr = (2.f-vol)*(2.f-vol);
	chan->ManualGain = true;

	return chan;
}

FISoundChannel *OpenALSoundRenderer::StartSound3D(SoundHandle sfx, SoundListener *listener, float vol,
	FRolloffInfo *rolloff, float distscale, int pitch, int priority, const FVector3 &pos, const FVector3 &vel,
	int channum, int chanflags, FISoundChannel *reuse_chan)
{
	float dist_sqr = (pos - listener->position).LengthSquared() *
	                 distscale*distscale;

	if(FreeSfx.size() == 0)
	{
		FSoundChan *lowest = FindLowestChannel();
		if(lowest)
		{
			if(lowest->Priority < priority || (lowest->Priority == priority &&
			                                   lowest->DistanceSqr > dist_sqr))
				StopChannel(lowest);
		}
		if(FreeSfx.size() == 0)
			return NULL;
	}

	float rolloffFactor, gain;
	bool manualGain = true;

	ALuint buffer = *((ALuint*)sfx.data);
	ALint channels = 1;
	alGetBufferi(buffer, AL_CHANNELS, &channels);

	ALuint &source = *find(Sources.begin(), Sources.end(), FreeSfx.back());
	alSource3f(source, AL_POSITION, pos[0], pos[1], -pos[2]);
	alSource3f(source, AL_VELOCITY, vel[0], vel[1], -vel[2]);
	alSource3f(source, AL_DIRECTION, 0.f, 0.f, 0.f);
	alSourcei(source, AL_SOURCE_RELATIVE, AL_FALSE);

	alSourcei(source, AL_LOOPING, (chanflags&SNDF_LOOP) ? AL_TRUE : AL_FALSE);

	// Multi-channel sources won't attenuate in OpenAL, and "area sounds" have
	// special rolloff properties (they have a panning radius of 32 units, but
	// start attenuating at MinDistance).
	if(channels == 1 && !(chanflags&SNDF_AREA))
	{
		if(rolloff->RolloffType == ROLLOFF_Log)
		{
			if(SrcDistanceModel)
				alSourcei(source, AL_DISTANCE_MODEL, AL_INVERSE_DISTANCE);
			alSourcef(source, AL_REFERENCE_DISTANCE, rolloff->MinDistance/distscale);
			alSourcef(source, AL_MAX_DISTANCE, (1000.f+rolloff->MinDistance)/distscale);
			rolloffFactor = rolloff->RolloffFactor;
			manualGain = false;
			gain = 1.f;
		}
		else if(rolloff->RolloffType == ROLLOFF_Linear && SrcDistanceModel)
		{
			alSourcei(source, AL_DISTANCE_MODEL, AL_LINEAR_DISTANCE);
			alSourcef(source, AL_REFERENCE_DISTANCE, rolloff->MinDistance/distscale);
			alSourcef(source, AL_MAX_DISTANCE, rolloff->MaxDistance/distscale);
			rolloffFactor = 1.f;
			manualGain = false;
			gain = 1.f;
		}
	}
	if(manualGain)
	{
		if(SrcDistanceModel)
			alSourcei(source, AL_DISTANCE_MODEL, AL_NONE);
		if((chanflags&SNDF_AREA) && rolloff->MinDistance < 32.f)
			alSourcef(source, AL_REFERENCE_DISTANCE, 32.f/distscale);
		else
			alSourcef(source, AL_REFERENCE_DISTANCE, rolloff->MinDistance/distscale);
		alSourcef(source, AL_MAX_DISTANCE, (1000.f+rolloff->MinDistance)/distscale);
		rolloffFactor = 0.f;
		gain = GetRolloff(rolloff, sqrt(dist_sqr));
	}
	alSourcef(source, AL_ROLLOFF_FACTOR, rolloffFactor);
	alSourcef(source, AL_MAX_GAIN, SfxVolume);
	alSourcef(source, AL_GAIN, SfxVolume * gain);

	if(EnvSlot)
	{
		if(!(chanflags&SNDF_NOREVERB))
		{
			alSourcei(source, AL_DIRECT_FILTER, EnvFilters[0]);
			alSource3i(source, AL_AUXILIARY_SEND_FILTER, EnvSlot, 0, EnvFilters[1]);
			alSourcef(source, AL_AIR_ABSORPTION_FACTOR, LastWaterAbsorb);
		}
		else
		{
			alSourcei(source, AL_DIRECT_FILTER, AL_FILTER_NULL);
			alSource3i(source, AL_AUXILIARY_SEND_FILTER, 0, 0, AL_FILTER_NULL);
			alSourcef(source, AL_AIR_ABSORPTION_FACTOR, 0.f);
		}
		alSourcef(source, AL_ROOM_ROLLOFF_FACTOR, rolloffFactor);
		alSourcef(source, AL_PITCH, PITCH(pitch));
	}
	else if(LastWaterAbsorb > 0.f && !(chanflags&SNDF_NOREVERB))
		alSourcef(source, AL_PITCH, PITCH(pitch)*PITCH_MULT);
	else
		alSourcef(source, AL_PITCH, PITCH(pitch));

	if(!reuse_chan)
		alSourcef(source, AL_SEC_OFFSET, 0.f);
	else
	{
		if((chanflags&SNDF_ABSTIME))
			alSourcef(source, AL_SEC_OFFSET, reuse_chan->StartTime.Lo/1000.f);
		else
		{
			// FIXME: set offset based on the current time and the StartTime
			alSourcef(source, AL_SAMPLE_OFFSET, 0.f);
		}
	}
	if(getALError() != AL_NO_ERROR)
		return NULL;

	alSourcei(source, AL_BUFFER, buffer);
	if((chanflags&SNDF_NOPAUSE) || !SFXPaused)
		alSourcePlay(source);
	if(getALError() != AL_NO_ERROR)
	{
		alSourcei(source, AL_BUFFER, 0);
		getALError();
		return NULL;
	}

	if(!(chanflags&SNDF_NOREVERB))
		ReverbSfx.push_back(source);
	if(!(chanflags&SNDF_NOPAUSE))
		PausableSfx.push_back(source);
	SfxGroup.push_back(source);
	FreeSfx.pop_back();

	FISoundChannel *chan = reuse_chan;
	if(!chan) chan = S_GetChannel(&source);
	else chan->SysChannel = &source;

	chan->Rolloff = *rolloff;
	chan->DistanceScale = distscale;
	chan->DistanceSqr = dist_sqr;
	chan->ManualGain = manualGain;

	return chan;
}

void OpenALSoundRenderer::StopChannel(FISoundChannel *chan)
{
	if(chan == NULL || chan->SysChannel == NULL)
		return;

	ALuint source = *((ALuint*)chan->SysChannel);
	// Release first, so it can be properly marked as evicted if it's being
	// forcefully killed
	S_ChannelEnded(chan);

	alSourceRewind(source);
	alSourcei(source, AL_BUFFER, 0);
	getALError();

	std::vector<ALuint>::iterator i;

	i = find(PausableSfx.begin(), PausableSfx.end(), source);
	if(i != PausableSfx.end()) PausableSfx.erase(i);
	i = find(ReverbSfx.begin(), ReverbSfx.end(), source);
	if(i != ReverbSfx.end()) ReverbSfx.erase(i);

	SfxGroup.erase(find(SfxGroup.begin(), SfxGroup.end(), source));
	FreeSfx.push_back(source);
}

unsigned int OpenALSoundRenderer::GetPosition(FISoundChannel *chan)
{
	if(chan == NULL || chan->SysChannel == NULL)
		return 0;

	ALint pos;
	alGetSourcei(*((ALuint*)chan->SysChannel), AL_SAMPLE_OFFSET, &pos);
	if(getALError() == AL_NO_ERROR)
		return pos;
	return 0;
}


void OpenALSoundRenderer::SetSfxPaused(bool paused, int slot)
{
	int oldslots = SFXPaused;

	if(paused)
	{
		SFXPaused |= 1 << slot;
		if(oldslots == 0 && PausableSfx.size() > 0)
		{
			alSourcePausev(PausableSfx.size(), &PausableSfx[0]);
			getALError();
			PurgeStoppedSources();
		}
	}
	else
	{
		SFXPaused &= ~(1 << slot);
		if(SFXPaused == 0 && oldslots != 0 && PausableSfx.size() > 0)
		{
			alSourcePlayv(PausableSfx.size(), &PausableSfx[0]);
			getALError();
		}
	}
}

void OpenALSoundRenderer::SetInactive(bool inactive)
{
}

void OpenALSoundRenderer::Sync(bool sync)
{
	if(sync)
	{
		if(SfxGroup.size() > 0)
		{
			alSourcePausev(SfxGroup.size(), &SfxGroup[0]);
			getALError();
			PurgeStoppedSources();
		}
	}
	else
	{
		// Might already be something to handle this; basically, get a vector
		// of all values in SfxGroup that are not also in PausableSfx (when
		// SFXPaused is non-0).
		std::vector<ALuint> toplay = SfxGroup;
		if(SFXPaused)
		{
			std::vector<ALuint>::iterator i = toplay.begin();
			while(i != toplay.end())
			{
				if(find(PausableSfx.begin(), PausableSfx.end(), *i) != PausableSfx.end())
					i = toplay.erase(i);
				else
					i++;
			}
		}
		if(toplay.size() > 0)
		{
			alSourcePlayv(toplay.size(), &toplay[0]);
			getALError();
		}
	}
}

void OpenALSoundRenderer::UpdateSoundParams3D(SoundListener *listener, FISoundChannel *chan, bool areasound, const FVector3 &pos, const FVector3 &vel)
{
	if(chan == NULL || chan->SysChannel == NULL)
		return;

	alcSuspendContext(Context);

	ALuint source = *((ALuint*)chan->SysChannel);
	alSource3f(source, AL_POSITION, pos[0], pos[1], -pos[2]);
	alSource3f(source, AL_VELOCITY, vel[0], vel[1], -vel[2]);

	chan->DistanceSqr = (pos - listener->position).LengthSquared() *
	                    chan->DistanceScale*chan->DistanceScale;
	// Not all sources can use the distance models provided by OpenAL.
	// For the ones that can't, apply the calculated attenuation as the
	// source gain. Positions still handle the panning,
	if(chan->ManualGain)
	{
		float gain = GetRolloff(&chan->Rolloff, sqrt(chan->DistanceSqr));
		alSourcef(source, AL_GAIN, SfxVolume*gain);
	}

	getALError();
}

void OpenALSoundRenderer::UpdateListener(SoundListener *listener)
{
	if(!listener->valid)
		return;

	alcSuspendContext(Context);

	float angle = listener->angle;
	ALfloat orient[6];
	// forward
	orient[0] = cos(angle);
	orient[1] = 0.f;
	orient[2] = -sin(angle);
	// up
	orient[3] = 0.f;
	orient[4] = 1.f;
	orient[5] = 0.f;

	alListenerfv(AL_ORIENTATION, orient);
	alListener3f(AL_POSITION, listener->position.X,
	                          listener->position.Y,
	                         -listener->position.Z);
	alListener3f(AL_VELOCITY, listener->velocity.X,
	                          listener->velocity.Y,
	                         -listener->velocity.Z);
	getALError();

	const ReverbContainer *env = ForcedEnvironment;
	if(!env)
	{
		env = listener->Environment;
		if(!env)
			env = DefaultEnvironments[0];
	}
	if(env != PrevEnvironment || env->Modified)
	{
		PrevEnvironment = env;
		DPrintf("Reverb Environment %s\n", env->Name);

		if(EnvSlot != 0)
			LoadReverb(env);

		const_cast<ReverbContainer*>(env)->Modified = false;
	}

	// NOTE: Moving into and out of water (and changing water absorption) will
	// undo pitch variations on sounds if either snd_waterreverb or EFX are
	// disabled.
	if(listener->underwater || env->SoftwareWater)
	{
		if(LastWaterAbsorb != *snd_waterabsorption)
		{
			LastWaterAbsorb = *snd_waterabsorption;

			if(EnvSlot != 0 && *snd_waterreverb)
			{
				// Find the "Underwater" reverb environment
				env = Environments;
				while(env && env->ID != 0x1600)
					env = env->Next;
				LoadReverb(env ? env : DefaultEnvironments[0]);

				alFilterf(EnvFilters[0], AL_LOWPASS_GAIN, 0.25f);
				alFilterf(EnvFilters[0], AL_LOWPASS_GAINHF, 0.75f);
				alFilterf(EnvFilters[1], AL_LOWPASS_GAIN, 1.f);
				alFilterf(EnvFilters[1], AL_LOWPASS_GAINHF, 1.f);

				// Apply the updated filters on the sources
				foreach(ALuint, i, ReverbSfx)
				{
					alSourcef(*i, AL_AIR_ABSORPTION_FACTOR, LastWaterAbsorb);
					alSourcei(*i, AL_DIRECT_FILTER, EnvFilters[0]);
					alSource3i(*i, AL_AUXILIARY_SEND_FILTER, EnvSlot, 0, EnvFilters[1]);
				}
			}
			else
			{
				foreach(ALuint, i, ReverbSfx)
					alSourcef(*i, AL_PITCH, PITCH_MULT);
			}
			getALError();
		}
	}
	else
	{
		if(LastWaterAbsorb > 0.f)
		{
			LastWaterAbsorb = 0.f;

			if(EnvSlot != 0)
			{
				LoadReverb(env);

				alFilterf(EnvFilters[0], AL_LOWPASS_GAIN, 1.f);
				alFilterf(EnvFilters[0], AL_LOWPASS_GAINHF, 1.f);
				alFilterf(EnvFilters[1], AL_LOWPASS_GAIN, 1.f);
				alFilterf(EnvFilters[1], AL_LOWPASS_GAINHF, 1.f);
				foreach(ALuint, i, ReverbSfx)
				{
					alSourcef(*i, AL_AIR_ABSORPTION_FACTOR, 0.f);
					alSourcei(*i, AL_DIRECT_FILTER, EnvFilters[0]);
					alSource3i(*i, AL_AUXILIARY_SEND_FILTER, EnvSlot, 0, EnvFilters[1]);
				}
			}
			else
			{
				foreach(ALuint, i, ReverbSfx)
					alSourcef(*i, AL_PITCH, 1.f);
			}
			getALError();
		}
	}
}

void OpenALSoundRenderer::UpdateSounds()
{
	alcProcessContext(Context);

	if(DisconnectNotify)
	{
		ALCint connected = ALC_TRUE;
		alcGetIntegerv(Device, ALC_CONNECTED, 1, &connected);
		if(connected == ALC_FALSE)
		{
			Printf("Sound device disconnected; restarting...\n");
			static char snd_reset[] = "snd_reset";
			AddCommandString(snd_reset);
			return;
		}
	}

	PurgeStoppedSources();
}

bool OpenALSoundRenderer::IsValid()
{
	return Device != NULL;
}

void OpenALSoundRenderer::MarkStartTime(FISoundChannel *chan)
{
	// FIXME: Get current time (preferably from the audio clock, but the system
	// time will have to do)
	chan->StartTime.AsOne = 0;
}

float OpenALSoundRenderer::GetAudibility(FISoundChannel *chan)
{
	if(chan == NULL || chan->SysChannel == NULL)
		return 0.f;

	ALuint source = *((ALuint*)chan->SysChannel);
	ALfloat volume = 0.f;

	if(!chan->ManualGain)
		volume = SfxVolume * GetRolloff(&chan->Rolloff, sqrt(chan->DistanceSqr));
	else
	{
		alGetSourcef(source, AL_GAIN, &volume);
		getALError();
	}
	return volume;
}


void OpenALSoundRenderer::PrintStatus()
{
	Printf("Output device: "TEXTCOLOR_ORANGE"%s\n", alcGetString(Device, ALC_DEVICE_SPECIFIER));
	getALCError(Device);

	ALCint frequency, major, minor, mono, stereo;
	alcGetIntegerv(Device, ALC_FREQUENCY, 1, &frequency);
	alcGetIntegerv(Device, ALC_MAJOR_VERSION, 1, &major);
	alcGetIntegerv(Device, ALC_MINOR_VERSION, 1, &minor);
	alcGetIntegerv(Device, ALC_MONO_SOURCES, 1, &mono);
	alcGetIntegerv(Device, ALC_STEREO_SOURCES, 1, &stereo);
	if(getALCError(Device) == AL_NO_ERROR)
	{
		Printf("Device sample rate: "TEXTCOLOR_BLUE"%d"TEXTCOLOR_NORMAL"hz\n", frequency);
		Printf("ALC Version: "TEXTCOLOR_BLUE"%d.%d\n", major, minor);
		Printf("ALC Extensions: "TEXTCOLOR_ORANGE"%s\n", alcGetString(Device, ALC_EXTENSIONS));
		Printf("Available sources: "TEXTCOLOR_BLUE"%d"TEXTCOLOR_NORMAL" ("TEXTCOLOR_BLUE"%d"TEXTCOLOR_NORMAL" mono, "TEXTCOLOR_BLUE"%d"TEXTCOLOR_NORMAL" stereo)\n", mono+stereo, mono, stereo);
	}
	if(!alcIsExtensionPresent(Device, "ALC_EXT_EFX"))
		Printf("EFX not found\n");
	else
	{
		ALCint sends;
		alcGetIntegerv(Device, ALC_EFX_MAJOR_VERSION, 1, &major);
		alcGetIntegerv(Device, ALC_EFX_MINOR_VERSION, 1, &minor);
		alcGetIntegerv(Device, ALC_MAX_AUXILIARY_SENDS, 1, &sends);
		if(getALCError(Device) == AL_NO_ERROR)
		{
			Printf("EFX Version: "TEXTCOLOR_BLUE"%d.%d\n", major, minor);
			Printf("Auxiliary sends: "TEXTCOLOR_BLUE"%d\n", sends);
		}
	}
	Printf("Vendor: "TEXTCOLOR_ORANGE"%s\n", alGetString(AL_VENDOR));
	Printf("Renderer: "TEXTCOLOR_ORANGE"%s\n", alGetString(AL_RENDERER));
	Printf("Version: "TEXTCOLOR_ORANGE"%s\n", alGetString(AL_VERSION));
	Printf("Extensions: "TEXTCOLOR_ORANGE"%s\n", alGetString(AL_EXTENSIONS));
	getALError();
}

FString OpenALSoundRenderer::GatherStats()
{
	ALCint updates = 1;
	alcGetIntegerv(Device, ALC_REFRESH, 1, &updates);
	getALCError(Device);

	ALuint total = Sources.size();
	ALuint used = SfxGroup.size()+Streams.size();
	ALuint unused = FreeSfx.size();
	FString out;
	out.Format("%u sources ("TEXTCOLOR_YELLOW"%u"TEXTCOLOR_NORMAL" active, "TEXTCOLOR_YELLOW"%u"TEXTCOLOR_NORMAL" free), Update interval: "TEXTCOLOR_YELLOW"%d"TEXTCOLOR_NORMAL"ms",
	           total, used, unused, 1000/updates);
	return out;
}

void OpenALSoundRenderer::PrintDriversList()
{
	const ALCchar *drivers = (alcIsExtensionPresent(NULL, "ALC_ENUMERATE_ALL_EXT") ?
	                          alcGetString(NULL, ALC_ALL_DEVICES_SPECIFIER) :
	                          alcGetString(NULL, ALC_DEVICE_SPECIFIER));
	const ALCchar *current = alcGetString(Device, ALC_DEVICE_SPECIFIER);
	if(drivers == NULL)
	{
		Printf(TEXTCOLOR_YELLOW"Failed to retrieve device list: %s\n", alcGetString(NULL, alcGetError(NULL)));
		return;
	}

	Printf("%c%s%2d. %s\n", ' ', ((snd_aldevice=="Default") ? TEXTCOLOR_BOLD : ""), 0,
	       "Default");
	for(int i = 1;*drivers;i++)
	{
		Printf("%c%s%2d. %s\n", ((strcmp(current, drivers)==0) ? '*' : ' '),
		       ((strcmp(*snd_aldevice, drivers)==0) ? TEXTCOLOR_BOLD : ""), i,
		       drivers);
		drivers += strlen(drivers)+1;
	}
}

void OpenALSoundRenderer::PurgeStoppedSources()
{
	// Release channels that are stopped
	foreach(ALuint, i, SfxGroup)
	{
		ALint state = AL_PLAYING;
		alGetSourcei(*i, AL_SOURCE_STATE, &state);
		if(state == AL_PLAYING || state == AL_PAUSED)
			continue;

		FSoundChan *schan = Channels;
		while(schan)
		{
			if(schan->SysChannel != NULL && *i == *((ALuint*)schan->SysChannel))
			{
				StopChannel(schan);
				break;
			}
			schan = schan->NextChan;
		}
	}
	getALError();
}

void OpenALSoundRenderer::LoadReverb(const ReverbContainer *env)
{
	ALuint &envReverb = EnvEffects[env->ID];
	bool doLoad = (env->Modified || !envReverb);

	if(!envReverb)
	{
		bool ok = false;
		alGenEffects(1, &envReverb);
		if(getALError() == AL_NO_ERROR)
		{
			alEffecti(envReverb, AL_EFFECT_TYPE, AL_EFFECT_EAXREVERB);
			ok = (alGetError() == AL_NO_ERROR);
			if(!ok)
			{
				alEffecti(envReverb, AL_EFFECT_TYPE, AL_EFFECT_REVERB);
				ok = (alGetError() == AL_NO_ERROR);
			}
			if(!ok)
			{
				alEffecti(envReverb, AL_EFFECT_TYPE, AL_EFFECT_NULL);
				ok = (alGetError() == AL_NO_ERROR);
			}
			if(!ok)
			{
				alDeleteEffects(1, &envReverb);
				getALError();
			}
		}
		if(!ok)
		{
			envReverb = 0;
			doLoad = false;
		}
	}

	if(doLoad)
	{
		const REVERB_PROPERTIES &props = env->Properties;
		ALint type = AL_EFFECT_NULL;

		alGetEffecti(envReverb, AL_EFFECT_TYPE, &type);
#define mB2Gain(x) ((float)pow(10., (x)/2000.))
		if(type == AL_EFFECT_EAXREVERB)
		{
			ALfloat reflectpan[3] = { props.ReflectionsPan0,
			                          props.ReflectionsPan1,
			                          props.ReflectionsPan2 };
			ALfloat latepan[3] = { props.ReverbPan0, props.ReverbPan1,
			                       props.ReverbPan2 };
#undef SETPARAM
#define SETPARAM(e,t,v) alEffectf((e), AL_EAXREVERB_##t, clamp((v), AL_EAXREVERB_MIN_##t, AL_EAXREVERB_MAX_##t))
			SETPARAM(envReverb, DENSITY, props.Density/100.f);
			SETPARAM(envReverb, DIFFUSION, props.Diffusion/100.f);
			SETPARAM(envReverb, GAIN, mB2Gain(props.Room));
			SETPARAM(envReverb, GAINHF, mB2Gain(props.RoomHF));
			SETPARAM(envReverb, GAINLF, mB2Gain(props.RoomLF));
			SETPARAM(envReverb, DECAY_TIME, props.DecayTime);
			SETPARAM(envReverb, DECAY_HFRATIO, props.DecayHFRatio);
			SETPARAM(envReverb, DECAY_LFRATIO, props.DecayLFRatio);
			SETPARAM(envReverb, REFLECTIONS_GAIN, mB2Gain(props.Reflections));
			SETPARAM(envReverb, REFLECTIONS_DELAY, props.ReflectionsDelay);
			alEffectfv(envReverb, AL_EAXREVERB_REFLECTIONS_PAN, reflectpan);
			SETPARAM(envReverb, LATE_REVERB_GAIN, mB2Gain(props.Reverb));
			SETPARAM(envReverb, LATE_REVERB_DELAY, props.ReverbDelay);
			alEffectfv(envReverb, AL_EAXREVERB_LATE_REVERB_PAN, latepan);
			SETPARAM(envReverb, ECHO_TIME, props.EchoTime);
			SETPARAM(envReverb, ECHO_DEPTH, props.EchoDepth);
			SETPARAM(envReverb, MODULATION_TIME, props.ModulationTime);
			SETPARAM(envReverb, MODULATION_DEPTH, props.ModulationDepth);
			SETPARAM(envReverb, AIR_ABSORPTION_GAINHF, mB2Gain(props.AirAbsorptionHF));
			SETPARAM(envReverb, HFREFERENCE, props.HFReference);
			SETPARAM(envReverb, LFREFERENCE, props.LFReference);
			SETPARAM(envReverb, ROOM_ROLLOFF_FACTOR, props.RoomRolloffFactor);
			alEffecti(envReverb, AL_EAXREVERB_DECAY_HFLIMIT,
			                     (props.Flags&REVERB_FLAGS_DECAYHFLIMIT)?AL_TRUE:AL_FALSE);
#undef SETPARAM
		}
		else if(type == AL_EFFECT_REVERB)
		{
#define SETPARAM(e,t,v) alEffectf((e), AL_REVERB_##t, clamp((v), AL_REVERB_MIN_##t, AL_REVERB_MAX_##t))
			SETPARAM(envReverb, DENSITY, props.Density/100.f);
			SETPARAM(envReverb, DIFFUSION, props.Diffusion/100.f);
			SETPARAM(envReverb, GAIN, mB2Gain(props.Room));
			SETPARAM(envReverb, GAINHF, mB2Gain(props.RoomHF));
			SETPARAM(envReverb, DECAY_TIME, props.DecayTime);
			SETPARAM(envReverb, DECAY_HFRATIO, props.DecayHFRatio);
			SETPARAM(envReverb, REFLECTIONS_GAIN, mB2Gain(props.Reflections));
			SETPARAM(envReverb, REFLECTIONS_DELAY, props.ReflectionsDelay);
			SETPARAM(envReverb, LATE_REVERB_GAIN, mB2Gain(props.Reverb));
			SETPARAM(envReverb, LATE_REVERB_DELAY, props.ReverbDelay);
			SETPARAM(envReverb, AIR_ABSORPTION_GAINHF, mB2Gain(props.AirAbsorptionHF));
			SETPARAM(envReverb, ROOM_ROLLOFF_FACTOR, props.RoomRolloffFactor);
			alEffecti(envReverb, AL_REVERB_DECAY_HFLIMIT,
			                     (props.Flags&REVERB_FLAGS_DECAYHFLIMIT)?AL_TRUE:AL_FALSE);
#undef SETPARAM
		}
#undef mB2Gain
	}

	alAuxiliaryEffectSloti(EnvSlot, AL_EFFECTSLOT_EFFECT, envReverb);
	getALError();
}

FSoundChan *OpenALSoundRenderer::FindLowestChannel()
{
	FSoundChan *schan = Channels;
	FSoundChan *lowest = NULL;
	while(schan)
	{
		if(schan->SysChannel != NULL)
		{
			if(!lowest || schan->Priority < lowest->Priority ||
			   (schan->Priority == lowest->Priority &&
			    schan->DistanceSqr > lowest->DistanceSqr))
				lowest = schan;
		}
		schan = schan->NextChan;
	}
	return lowest;
}

#endif // NO_OPENAL
