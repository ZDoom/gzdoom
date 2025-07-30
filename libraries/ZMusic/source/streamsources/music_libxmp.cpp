/*
** music_libxmp.cpp
** libxmp module player.
**
**---------------------------------------------------------------------------
** Copyright 2024 Cacodemon345
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

#include <math.h>
#include <mutex>
#include <string>
#include <stdint.h>
#include <limits.h>
#include "streamsource.h"

#define LIBXMP_STATIC 1
#include "../libxmp/include/xmp.h"
#include "zmusic/m_swap.h"
#include "zmusic/mididefs.h"
#include "zmusic/midiconfig.h"
#include "fileio.h"

extern DumbConfig dumbConfig;

static unsigned long xmp_read(void *dest, unsigned long len, unsigned long nmemb, void *priv)
{
	if (len == 0 || nmemb == 0)
		return (unsigned long)0;

	MusicIO::FileInterface* interface = (MusicIO::FileInterface*)priv;

	auto origpos = interface->tell();
	auto length = interface->read(dest, (int32_t)(len * nmemb));

	if (length != len * nmemb)
	{
		// Let's hope the compiler doesn't misoptimize this.
		interface->seek(origpos + (length / len) * len, SEEK_SET);
	}
	return length / len;
}

static struct xmp_callbacks callbacks =
{
	xmp_read,
	[](void *priv, long offset, int whence) -> int { return ((MusicIO::FileInterface*)priv)->seek(offset, whence); },
	[](void *priv) -> long { return ((MusicIO::FileInterface*)priv)->tell(); },
	[](void *priv) -> int { return 0; }
};

class XMPSong : public StreamSource
{
private:
	xmp_context context = nullptr;
	int samplerate = 44100;
	int subsong = 0;

	// libxmp can't output in float.
	std::vector<int16_t> int16_buffer;

public:
	XMPSong(xmp_context ctx, int samplerate);
	~XMPSong();
	bool SetSubsong(int subsong) override;
	bool Start() override;
	SoundStreamInfoEx GetFormatEx() override;

protected:
	bool GetData(void *buffer, size_t len) override;
};

XMPSong::XMPSong(xmp_context ctx, int rate)
{
	context = ctx;
	samplerate = (dumbConfig.mod_samplerate != 0) ? dumbConfig.mod_samplerate : rate;
	xmp_set_player(context, XMP_PLAYER_VOLUME, 100);
	xmp_set_player(context, XMP_PLAYER_INTERP, dumbConfig.mod_interp);

	int16_buffer.reserve(16 * 1024);
}

XMPSong::~XMPSong()
{
	xmp_end_player(context);
	xmp_free_context(context);
}

SoundStreamInfoEx XMPSong::GetFormatEx()
{
	return { 32 * 1024, samplerate, SampleType_Float32, ChannelConfig_Stereo };
}

bool XMPSong::SetSubsong(int subsong)
{
	this->subsong = subsong;
	if (xmp_get_player(context, XMP_PLAYER_STATE) >= XMP_STATE_PLAYING)
		return xmp_set_position(context, subsong) >= 0;
	return true;
}

bool XMPSong::GetData(void *buffer, size_t len)
{
	if ((len / 4) > int16_buffer.size())
		int16_buffer.resize(len / 4);

	int ret = xmp_play_buffer(context, (void*)int16_buffer.data(), len / 2, m_Looping? INT_MAX : 0);
	xmp_set_player(context, XMP_PLAYER_INTERP, dumbConfig.mod_interp);

	if (ret >= 0)
	{
		float* soundbuffer = (float*)buffer;
		for (unsigned int i = 0; i < len / 4; i++)
		{
			soundbuffer[i] = ((int16_buffer[i] < 0.) ? (int16_buffer[i] / 32768.) : (int16_buffer[i] / 32767.)) * dumbConfig.mod_dumb_mastervolume;
		}
	}

	if (ret < 0 && m_Looping)
	{
		xmp_restart_module(context);
		xmp_set_position(context, subsong);
		return true;
	}

	return ret >= 0;
}

bool XMPSong::Start()
{
	int ret = xmp_start_player(context, samplerate, 0);
	if (ret >= 0)
		xmp_set_position(context, subsong);
	return ret >= 0;
}

StreamSource* XMP_OpenSong(MusicIO::FileInterface* reader, int samplerate)
{
	if (xmp_test_module_from_callbacks((void*)reader, callbacks, nullptr) < 0)
		return nullptr;

	xmp_context ctx = xmp_create_context();
	if (!ctx)
		return nullptr;

	reader->seek(0, SEEK_SET);

	if (xmp_load_module_from_callbacks(ctx, (void*)reader, callbacks) < 0)
	{
		return nullptr;
	}

	return new XMPSong(ctx, samplerate);
}

