#ifndef __SNDINT_H
#define __SNDINT_H

#include <vector>
#include <stdio.h>

#include "basictypes.h"
#include "vectors.h"

// For convenience, this structure matches FMOD_REVERB_PROPERTIES.
// Since I can't very well #include system-specific stuff in the
// main game files, I duplicate it here.
struct REVERB_PROPERTIES
{                
	int			 Instance;
    int			 Environment;
    float        EnvSize;
    float        EnvDiffusion;
    int          Room;
    int          RoomHF;
    int          RoomLF;
    float        DecayTime;
    float        DecayHFRatio;
    float        DecayLFRatio;
    int          Reflections;
    float        ReflectionsDelay;
    float        ReflectionsPan0;
	float        ReflectionsPan1;
	float        ReflectionsPan2;
    int          Reverb;
    float        ReverbDelay;
    float        ReverbPan0;
	float        ReverbPan1;
	float        ReverbPan2;
    float        EchoTime;
    float        EchoDepth;
    float        ModulationTime;
    float        ModulationDepth;
    float        AirAbsorptionHF;
    float        HFReference;
    float        LFReference;
    float        RoomRolloffFactor;
    float        Diffusion;
    float        Density;
    unsigned int Flags;
};

#define REVERB_FLAGS_DECAYTIMESCALE        0x00000001
#define REVERB_FLAGS_REFLECTIONSSCALE      0x00000002
#define REVERB_FLAGS_REFLECTIONSDELAYSCALE 0x00000004
#define REVERB_FLAGS_REVERBSCALE           0x00000008
#define REVERB_FLAGS_REVERBDELAYSCALE      0x00000010
#define REVERB_FLAGS_DECAYHFLIMIT          0x00000020
#define REVERB_FLAGS_ECHOTIMESCALE         0x00000040
#define REVERB_FLAGS_MODULATIONTIMESCALE   0x00000080

struct ReverbContainer
{
	ReverbContainer *Next;
	const char *Name;
	WORD ID;
	bool Builtin;
	bool Modified;
	REVERB_PROPERTIES Properties;
	bool SoftwareWater;
};

struct SoundListener
{
	FVector3 position;
	FVector3 velocity;
	float angle;
	bool underwater;
	bool valid;
	ReverbContainer *Environment;
};

// Default rolloff information.
struct FRolloffInfo
{
	int RolloffType;
	float MinDistance;
	union { float MaxDistance; float RolloffFactor; };
};

struct SoundHandle
{
	void *data;

	bool isValid() const { return data != NULL; }
	void Clear() { data = NULL; }
};

struct FISoundChannel
{
	void		*SysChannel;	// Channel information from the system interface.
	QWORD_UNION	StartTime;		// Sound start time in DSP clocks.

	// The sound interface doesn't use these directly but it needs to pass them to a
	// callback that can't be passed a sound channel pointer
	FRolloffInfo Rolloff;
	float		DistanceScale;
	float		DistanceSqr;
	bool		ManualGain;
};


enum SampleType
{
    SampleType_UInt8,
    SampleType_Int16
};
enum ChannelConfig
{
    ChannelConfig_Mono,
    ChannelConfig_Stereo
};

struct SoundDecoder
{
    virtual void getInfo(int *samplerate, ChannelConfig *chans, SampleType *type) = 0;

    virtual size_t read(char *buffer, size_t bytes) = 0;
    virtual std::vector<char> readAll();
    virtual bool seek(size_t ms_offset) = 0;
    virtual size_t getSampleOffset() = 0;

    SoundDecoder() { }
    virtual ~SoundDecoder() { }

protected:
    virtual bool open(const char *data, size_t length) = 0;
    virtual bool open(const char *fname, size_t offset, size_t length) = 0;
    friend class SoundRenderer;

private:
    // Make non-copyable
    SoundDecoder(const SoundDecoder &rhs);
    SoundDecoder& operator=(const SoundDecoder &rhs);
};

#ifdef HAVE_MPG123
#include "mpg123.h"
struct MPG123Decoder : public SoundDecoder
{
    virtual void getInfo(int *samplerate, ChannelConfig *chans, SampleType *type);

    virtual size_t read(char *buffer, size_t bytes);
    virtual bool seek(size_t ms_offset);
    virtual size_t getSampleOffset();

    MPG123Decoder() : MPG123(0) { }
    virtual ~MPG123Decoder();

protected:
    virtual bool open(const char *data, size_t length);
    virtual bool open(const char *fname, size_t offset, size_t length);

private:
    mpg123_handle *MPG123;
    bool Done;

    const char *MemData;
    size_t MemLength;
    size_t MemPos;
    static off_t mem_lseek(void *handle, off_t offset, int whence);
    static ssize_t mem_read(void *handle, void *buffer, size_t bytes);

    // Make non-copyable
    MPG123Decoder(const MPG123Decoder &rhs);
    MPG123Decoder& operator=(const MPG123Decoder &rhs);
};
#endif
#ifdef HAVE_SNDFILE
#include "sndfile.h"
struct SndFileDecoder : public SoundDecoder
{
    virtual void getInfo(int *samplerate, ChannelConfig *chans, SampleType *type);

    virtual size_t read(char *buffer, size_t bytes);
    virtual std::vector<char> readAll();
    virtual bool seek(size_t ms_offset);
    virtual size_t getSampleOffset();

    SndFileDecoder() : SndFile(0) { }
    virtual ~SndFileDecoder();

protected:
    virtual bool open(const char *data, size_t length);
    virtual bool open(const char *fname, size_t offset, size_t length);

private:
    SNDFILE *SndFile;
    SF_INFO SndInfo;

    FILE *File;
    size_t FileLength;
    size_t FileOffset;
    static sf_count_t file_get_filelen(void *user_data);
    static sf_count_t file_seek(sf_count_t offset, int whence, void *user_data);
    static sf_count_t file_read(void *ptr, sf_count_t count, void *user_data);
    static sf_count_t file_write(const void *ptr, sf_count_t count, void *user_data);
    static sf_count_t file_tell(void *user_data);

    const char *MemData;
    size_t MemLength;
    size_t MemPos;
    static sf_count_t mem_get_filelen(void *user_data);
    static sf_count_t mem_seek(sf_count_t offset, int whence, void *user_data);
    static sf_count_t mem_read(void *ptr, sf_count_t count, void *user_data);
    static sf_count_t mem_write(const void *ptr, sf_count_t count, void *user_data);
    static sf_count_t mem_tell(void *user_data);

    // Make non-copyable
    SndFileDecoder(const SndFileDecoder &rhs);
    SndFileDecoder& operator=(const SndFileDecoder &rhs);
};
#endif

#endif
