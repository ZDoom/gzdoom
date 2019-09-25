#include "i_musicinterns.h"
/**
 * PlayStation XA (ADPCM) source support for MultiVoc
 * Adapted and remixed from superxa2wav (taken from EDuke32)
 */


//#define NO_XA_HEADER

#define kNumOfSamples   224
#define kNumOfSGs       18
#define TTYWidth        80

#define kBufSize (kNumOfSGs*kNumOfSamples)
#define kSamplesMono (kNumOfSGs*kNumOfSamples)
#define kSamplesStereo (kNumOfSGs*kNumOfSamples/2)

/* ADPCM */
#define XA_DATA_START   (0x44-48)

#define DblToPCMF(dt) ((dt) / 32768.f)

typedef struct {
   FileReader reader;
   size_t committed;
   bool blockIsMono;
   bool blockIs18K;
   bool finished;

   double t1, t2;
   double t1_x, t2_x;

   float block[kBufSize];
} xa_data;

typedef int8_t SoundGroup[128];

typedef struct XASector {
    int8_t     sectorFiller[48];
    SoundGroup SoundGroups[18];
} XASector;

static double K0[4] = {
    0.0,
    0.9375,
    1.796875,
    1.53125
};
static double K1[4] = {
    0.0,
    0.0,
    -0.8125,
    -0.859375
};



static int8_t getSoundData(int8_t *buf, int32_t unit, int32_t sample)
{
    int8_t ret;
    int8_t *p;
    int32_t offset, shift;

    p = buf;
    shift = (unit%2) * 4;

    offset = 16 + (unit / 2) + (sample * 4);
    p += offset;

    ret = (*p >> shift) & 0x0F;

    if (ret > 7) {
        ret -= 16;
    }
    return ret;
}

static int8_t getFilter(const int8_t *buf, int32_t unit)
{
    return (*(buf + 4 + unit) >> 4) & 0x03;
}


static int8_t getRange(const int8_t *buf, int32_t unit)
{
    return *(buf + 4 + unit) & 0x0F;
}


static void decodeSoundSectMono(XASector *ssct, xa_data * xad)
{
    size_t count = 0;
    int8_t snddat, filt, range;
    int32_t unit, sample;
    int32_t sndgrp;
    double tmp2, tmp3, tmp4, tmp5;
    int8_t &decodeBuf = xad->block;

    for (sndgrp = 0; sndgrp < kNumOfSGs; sndgrp++)
    {
        for (unit = 0; unit < 8; unit++)
        {
            range = getRange(ssct->SoundGroups[sndgrp], unit);
            filt = getFilter(ssct->SoundGroups[sndgrp], unit);
            for (sample = 0; sample < 28; sample++)
            {
                snddat = getSoundData(ssct->SoundGroups[sndgrp], unit, sample);
                tmp2 = (double)(1 << (12 - range));
                tmp3 = (double)snddat * tmp2;
                tmp4 = xad->t1 * K0[filt];
                tmp5 = xad->t2 * K1[filt];
                xad->t2 = xad->t1;
                xad->t1 = tmp3 + tmp4 + tmp5;
                decodeBuf[count++] = DblToPCM(xad->t1);
            }
        }
    }
}

static void decodeSoundSectStereo(XASector *ssct, xa_data * xad)
{
    size_t count = 0;
    int8_t snddat, filt, range;
    int8_t filt1, range1;
    int16_t decoded;
    int32_t unit, sample;
    int32_t sndgrp;
    double tmp2, tmp3, tmp4, tmp5;
    int8_t &decodeBuf = xad->block;

    for (sndgrp = 0; sndgrp < kNumOfSGs; sndgrp++)
    {
        for (unit = 0; unit < 8; unit+= 2)
        {
            range = getRange(ssct->SoundGroups[sndgrp], unit);
            filt = getFilter(ssct->SoundGroups[sndgrp], unit);
            range1 = getRange(ssct->SoundGroups[sndgrp], unit+1);
            filt1 = getFilter(ssct->SoundGroups[sndgrp], unit+1);

            for (sample = 0; sample < 28; sample++)
            {
                // Channel 1
                snddat = getSoundData(ssct->SoundGroups[sndgrp], unit, sample);
                tmp2 = (double)(1 << (12 - range));
                tmp3 = (double)snddat * tmp2;
                tmp4 = xad->t1 * K0[filt];
                tmp5 = xad->t2 * K1[filt];
                xad->t2 = xad->t1;
                xad->t1 = tmp3 + tmp4 + tmp5;
                decodeBuf[count++] = DblToPCMF(xad->t1);

                // Channel 2
                snddat = getSoundData(ssct->SoundGroups[sndgrp], unit+1, sample);
                tmp2 = (double)(1 << (12 - range1));
                tmp3 = (double)snddat * tmp2;
                tmp4 = xad->t1_x * K0[filt1];
                tmp5 = xad->t2_x * K1[filt1];
                xad->t2_x = xad->t1_x;
                xad->t1_x = tmp3 + tmp4 + tmp5;
                decodeBuf[count++] = DblToPCMF(xad->t1_x);
            }
        }
    }
}

//==========================================================================
//
// Get one decoded block of data
//
//==========================================================================

static bool getNextXABlock(xa_data *xad, bool looping )
{
    XASector ssct;
    int coding;
	const int SUBMODE_REAL_TIME_SECTOR = (1 << 6);
	const int SUBMODE_FORM             = (1 << 5);
	const int SUBMODE_AUDIO_DATA       = (1 << 2);

    do
    {
        size_t bytes = xad->reader.GetLength() - xad->reader.Tell();

        if (sizeof(XASector) < bytes)
            bytes = sizeof(XASector);

		xad->reader.Read(&ssct, bytes);
    }
    while (ssct.sectorFiller[46] != (SUBMODE_REAL_TIME_SECTOR | SUBMODE_FORM | SUBMODE_AUDIO_DATA));

    coding = ssct.sectorFiller[47];

	xa->blockIsMono = (coding & 3) == 0;
    xa->blockIs18K = (((coding >> 2) & 3) == 1);

    uint32_t samples;

    if (!xa->blockIsMono)
    {
        decodeSoundSectStereo(&ssct, xad);
        samples = kSamplesStereo;
    }
    else
    {
        decodeSoundSectMono(&ssct, xad);
        samples = kSamplesMono;
    }

    if (xad->GetLength() == xad->Tell())
    {
        if (looping)
        {
            xad->pos = XA_DATA_START;
            xad->t1 = xad->t2 = xad->t1_x = xad->t2_x = 0;
        }
        else
            xa->finished = true;
    }

    xa->finished = false;
}

//==========================================================================
//
// XASong
//
//==========================================================================

class XASong : public StreamSong
{
public:
	GMESong(FileReader & readr);
	~GMESong();
	bool SetSubsong(int subsong);
	void Play(bool looping, int subsong);

protected:
	xa_data xad;

	static bool Read(SoundStream *stream, void *buff, int len, void *userdata);
};

//==========================================================================
//
// XASong - Constructor
//
//==========================================================================

XASong::XASong(FileReader &reader)
{
	ChannelConfig iChannels;
	SampleType Type;
	

	xad.ptr = std::move(reader);
	xad.pos = XA_DATA_START;
	xad.t1 = xad.t2 = xad.t1_x = xad.t2_x = 0;
	getNextXABlock(&xad, false);
	auto SampleRate = xad.blockIs18K? 18900 : 37800;

	const uint32_t sampleLength = (uint32_t)decoder->getSampleLength();
	Reader = std::move(reader);
	Decoder = decoder;
	Channels = 2;	// Since the format can theoretically switch between mono and stereo we need to output everything as stereo.
	m_Stream = GSnd->CreateStream(Read, 64 * 1024, 0, SampleRate, this);
}

//==========================================================================
//
// XASong - Destructor
//
//==========================================================================

XASong::~XASong()
{
	Stop();
	if (m_Stream != nullptr)
	{
		delete m_Stream;
		m_Stream = nullptr;
	}
}


//==========================================================================
//
// XASong :: Play
//
//==========================================================================

void XASong::Play(bool looping, int track)
{
	m_Status = STATE_Stopped;
	m_Looping = looping;
	if (xad.finished && looping)
	{
		xad.pos = XA_DATA_START;
		xad.t1 = xad.t2 = xad.t1_x = xad.t2_x = 0;
		xad.reader.Seek(0, FileReader::SeekSet);
		xad.finished = false;
	}
	if (m_Stream->Play(looping, 1))
	{
		m_Status = STATE_Playing;
	}
}

//==========================================================================
//
// XASong :: SetSubsong
//
//==========================================================================

bool XASong::SetSubsong(int track)
{
	return false;
}

//==========================================================================
//
// XASong :: Read													STATIC
//
//==========================================================================

bool XASong::Read(SoundStream *stream, void *vbuff, int ilen, void *userdata)
{
	auto self = (XASong*)userdata;
	while (ilen > 0)
	{
		if (self->xad.committed < kBufSize)
		{
			// commit the data
			if (self->xad.blockIsMono)
			{
			}
			else
			{
			}
		}
		if (self->xad.finished)
		{
			// fill the rest with 0's.
		}
		if (ilen > 0)
		{
			// we ran out of data and need more
			getNextXABlock(&self->xad, m_Looping);
			// repeat until done.
		}
		else break;
		
	}
	return !self->xad.finished;
} 

//==========================================================================
//
// XA_OpenSong
//
//==========================================================================

MusInfo *XA_OpenSong(FileReader &reader)
{
	return new XASong(reader);
}

