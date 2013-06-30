#ifndef OALSDLSOUND_H
#define OALSDLSOUND_H

#include "oalsound.h"
#include "tempfiles.h"

#include "SDL_sound.h"

class OpenALSoundStream : public SoundStream
{
    OpenALSoundRenderer *Renderer;
    Sound_Sample *Sample;
    ALuint Source;

    bool Playing;
    bool Looping;

    static const ALfloat BufferTime = 0.2f;
    std::vector<ALuint> Buffers;

    ALuint SampleRate;
    ALenum Format;

    bool NeedSwab;
    bool NeedS8Conv;
    bool NeedS16Conv;
    void *GetData(size_t bytes);

    // General methods
    bool SetupSource();
    bool InitSample();

    ALfloat Volume;

public:
    OpenALSoundStream(OpenALSoundRenderer *renderer);
    virtual ~OpenALSoundStream();

    virtual bool Play(bool looping, float vol);
    virtual void Stop();
    virtual bool SetPaused(bool paused);

    virtual void SetVolume(float vol);

    virtual unsigned int GetPosition();
    virtual bool SetPosition(unsigned int val);

    virtual bool IsEnded();

    bool Init(const char *filename, int offset, int length);
    bool Init(const BYTE *data, unsigned int datalen);
};


class Decoder
{
    Sound_Sample *Sample;
    bool NeedSwab;
    bool NeedS8Conv;
    bool NeedS16Conv;

public:
    Decoder(const void *data, unsigned int datalen);
    virtual ~Decoder();

    bool GetFormat(ALenum *format, ALuint *rate);
    void *GetData(ALsizei *size);
};

#endif /* OALSDLSOUND_H */
