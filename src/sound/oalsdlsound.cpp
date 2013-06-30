/*
** oalsdlsound.cpp
** Interface for SDL_sound; uses OpenAL
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
#include "oalsdlsound.h"
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

#include <algorithm>
#include <memory>


struct RWSubFile {
private:
    FILE *fp;
    size_t start;
    size_t length;

public:
    RWSubFile(FILE *f, size_t offset, size_t len)
      : fp(f), start(offset), length(len)
    {
        fseek(fp, start, SEEK_SET);
    }
    ~RWSubFile()
    { fclose(fp); }

    static int seek(SDL_RWops *context, int offset, int whence)
    {
        RWSubFile *self = static_cast<RWSubFile*>(context->hidden.unknown.data1);

        if(whence == SEEK_END)
        {
            if(offset <= 0)
                offset = self->length + offset;
        }
        else if(whence == SEEK_CUR)
            offset = offset + ftell(self->fp) - self->start;
        else if(whence != SEEK_SET)
        {
            SDL_SetError("Invalid seek mode");
            return -1;
        }

        if(offset >= 0 && size_t(offset) <= self->length)
        {
            if(fseek(self->fp, offset + self->start, SEEK_SET) == 0)
                return offset;
        }

        SDL_SetError("Invalid file seek");
        return -1;
    }

    static int read(SDL_RWops *context, void *ptr, int size, int maxnum)
    {
        RWSubFile *self = static_cast<RWSubFile*>(context->hidden.unknown.data1);
        return fread(ptr, size, maxnum, self->fp);
    }

    static int write(SDL_RWops *context, const void *ptr, int size, int num)
    {
        RWSubFile *self = static_cast<RWSubFile*>(context->hidden.unknown.data1);
        return fwrite(ptr, size, num, self->fp);
    }

    static int close(SDL_RWops *context)
    {
        RWSubFile *self = static_cast<RWSubFile*>(context->hidden.unknown.data1);
        if(context->type != 0xdeadbeef)
        {
            SDL_SetError("Wrong kind of RWops for RWSubfile::close");
            return -1;
        }

        delete self;
        SDL_FreeRW(context);

        return 0;
    }
};


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


bool OpenALSoundStream::SetupSource()
{
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
    alSourcef(Source, AL_MAX_GAIN, Renderer->MusicVolume);
    alSourcef(Source, AL_GAIN, Volume*Renderer->MusicVolume);
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

    alGenBuffers(Buffers.size(), &Buffers[0]);
    return (getALError() == AL_NO_ERROR);
}

OpenALSoundStream::OpenALSoundStream(OpenALSoundRenderer *renderer)
    : Renderer(renderer), Sample(NULL), Source(0), Playing(false),
      Looping(false), Buffers(4), SampleRate(0), Format(0),
      NeedSwab(false), Volume(1.f)
{
    for(size_t i = 0;i < Buffers.size();i++)
        Buffers[i] = 0;
    Renderer->Streams.push_back(this);
}

OpenALSoundStream::~OpenALSoundStream()
{
    Playing = false;

    Sound_FreeSample(Sample);
    Sample = NULL;

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

    Renderer->Streams.erase(std::find(Renderer->Streams.begin(),
                                      Renderer->Streams.end(), this));
    Renderer = NULL;
}

bool OpenALSoundStream::Play(bool looping, float vol)
{
    if(Playing)
        return true;

    alSourceRewind(Source);
    alSourcei(Source, AL_BUFFER, 0);

    Looping = looping;
    SetVolume(vol);

    for(size_t i = 0;i < Buffers.size();i++)
    {
        size_t count = Sound_Decode(Sample);
        if(count == 0 && Looping)
        {
            Sound_Seek(Sample, 0);
            count = Sound_Decode(Sample);
        }
        alBufferData(Buffers[i], Format, GetData(count), count, SampleRate);
    }
    Playing = (getALError() == AL_NO_ERROR);
    if(Playing)
    {
        alSourceQueueBuffers(Source, Buffers.size(), &Buffers[0]);
        alSourcePlay(Source);
        Playing = (getALError() == AL_NO_ERROR);
    }
    return Playing;
}

void OpenALSoundStream::Stop()
{
    alSourceRewind(Source);
    alSourcei(Source, AL_BUFFER, 0);
}

bool OpenALSoundStream::SetPaused(bool paused)
{
    if(paused) alSourcePause(Source);
    else alSourcePlay(Source);
    return (getALError() == AL_NO_ERROR);
}

void OpenALSoundStream::SetVolume(float vol)
{
    if(vol >= 0.f) Volume = vol;
    alSourcef(Source, AL_GAIN, Volume*Renderer->MusicVolume);
}

unsigned int OpenALSoundStream::GetPosition()
{
    return 0;
}

bool OpenALSoundStream::SetPosition(unsigned int val)
{
    return false;
}

bool OpenALSoundStream::IsEnded()
{
    if(!Playing)
        return true;

    ALint processed, state, queued;
    alGetSourcei(Source, AL_SOURCE_STATE, &state);
    alGetSourcei(Source, AL_BUFFERS_QUEUED, &queued);
    alGetSourcei(Source, AL_BUFFERS_PROCESSED, &processed);
    while(processed-- > 0)
    {
        ALuint buf = 0;
        alSourceUnqueueBuffers(Source, 1, &buf);
        queued--;

        size_t count = Sound_Decode(Sample);
        if(count == 0 && Looping)
        {
            Sound_Seek(Sample, 0);
            count = Sound_Decode(Sample);
        }
        if(count > 0)
        {
            alBufferData(buf, Format, GetData(count), count, SampleRate);
            alSourceQueueBuffers(Source, 1, &buf);
            queued++;
        }
    }
    Playing = (getALError() == AL_NO_ERROR && queued > 0);
    if(Playing && state != AL_PLAYING && state != AL_PAUSED)
    {
        alSourcePlay(Source);
        Playing = (getALError() == AL_NO_ERROR);
    }

    return !Playing;
}

void *OpenALSoundStream::GetData(size_t bytes)
{
    void *data = Sample->buffer;
    if(NeedSwab)
    {
        short *samples = reinterpret_cast<short*>(data);
        size_t count = bytes >> 1;
        for(size_t i = 0;i < count;i++)
        {
            short smp = *samples;
            *(samples++) = ((smp>>8)&0x00FF) | ((smp<<8)*0xFF00);
        }
    }
    return data;
}

bool OpenALSoundStream::InitSample()
{
    UInt32 smpsize = 0;
    SampleRate = Sample->actual.rate;

    Format = AL_NONE;
    if(Sample->actual.format == AUDIO_U8)
    {
        if(Sample->actual.channels == 1)
            Format = AL_FORMAT_MONO8;
        else if(Sample->actual.channels == 2)
            Format = AL_FORMAT_STEREO8;
        smpsize = 1 * Sample->actual.channels;
    }
    else if(Sample->actual.format == AUDIO_S16LSB || Sample->actual.format == AUDIO_S16MSB)
    {
        NeedSwab = (Sample->actual.format != AUDIO_S16SYS);
        if(Sample->actual.channels == 1)
            Format = AL_FORMAT_MONO16;
        else if(Sample->actual.channels == 2)
            Format = AL_FORMAT_STEREO16;
        smpsize = 1 * Sample->actual.channels;
    }

    if(Format == AL_NONE)
    {
        Printf("Unsupported sound format (0x%04x, %d channels)\n", Sample->actual.format, Sample->actual.channels);
        return false;
    }

    Uint32 bufsize = (UInt32)(BufferTime*SampleRate) * smpsize;
    if(Sound_SetBufferSize(Sample, bufsize) == 0)
    {
        Printf("Failed to set buffer size to %u bytes: %s\n", bufsize, Sound_GetError());
        return false;
    }

    return true;
}

bool OpenALSoundStream::Init(const char *filename, int offset, int length)
{
    if(!SetupSource())
        return false;

    if(offset == 0)
        Sample = Sound_NewSampleFromFile(filename, NULL, 0);
    else
    {
        FILE *fp = fopen(filename, "rb");
        if(!fp)
        {
            Printf("Failed to open %s\n", filename);
            return false;
        }

        const char *ext = strrchr(filename, '.');
        if(ext) ext++;

        SDL_RWops *ops = SDL_AllocRW();
        ops->seek  = RWSubFile::seek;
        ops->read  = RWSubFile::read;
        ops->write = RWSubFile::write;
        ops->close = RWSubFile::close;
        ops->type  = 0xdeadbeef;
        ops->hidden.unknown.data1 = new RWSubFile(fp, offset, length);

        Sample = Sound_NewSample(ops, ext, NULL, 0);
    }
    if(!Sample)
    {
        Printf("Could not open audio in %s (%s)\n", filename, Sound_GetError());
        return false;
    }

    return InitSample();
}

bool OpenALSoundStream::Init(const BYTE *data, unsigned int datalen)
{
    if(!SetupSource())
        return false;

    Sample = Sound_NewSample(SDL_RWFromConstMem(data, datalen), NULL, NULL, 0);
    if(!Sample)
    {
        Printf("Could not read audio: %s\n", Sound_GetError());
        return false;
    }

    return InitSample();
}


Decoder::Decoder(const void* data, unsigned int datalen)
  : Sample(NULL), NeedSwab(false)
{
    Sample = Sound_NewSample(SDL_RWFromConstMem(data, datalen), NULL, NULL, 65536);
}


Decoder::~Decoder()
{
    Sound_FreeSample(Sample);
    Sample = NULL;
}

bool Decoder::GetFormat(ALenum *format, ALuint *rate)
{
    ALenum fmt = AL_NONE;
    if(Sample->actual.format == AUDIO_U8)
    {
        if(Sample->actual.channels == 1)
            fmt = AL_FORMAT_MONO8;
        else if(Sample->actual.channels == 2)
            fmt = AL_FORMAT_STEREO8;
    }
    else if(Sample->actual.format == AUDIO_S16LSB || Sample->actual.format == AUDIO_S16MSB)
    {
        NeedSwab = (Sample->actual.format != AUDIO_S16SYS);
        if(Sample->actual.channels == 1)
            fmt = AL_FORMAT_MONO16;
        else if(Sample->actual.channels == 2)
            fmt = AL_FORMAT_STEREO16;
    }

    if(fmt == AL_NONE)
    {
        Printf("Unsupported sound format (0x%04x, %d channels)\n", Sample->actual.format, Sample->actual.channels);
        return false;
    }

    *format = fmt;
    *rate = Sample->actual.rate;
    return true;
}

void* Decoder::GetData(ALsizei *size)
{
    UInt32 got = Sound_DecodeAll(Sample);
    if(got == 0)
    {
        *size = 0;
        return NULL;
    }

    void *data = Sample->buffer;
    if(NeedSwab)
    {
        short *samples = reinterpret_cast<short*>(data);
        size_t count = got >> 1;
        for(size_t i = 0;i < count;i++)
        {
            short smp = *samples;
            *(samples++) = ((smp>>8)&0x00FF) | ((smp<<8)*0xFF00);
        }
    }

    *size = got;
    return data;
}
