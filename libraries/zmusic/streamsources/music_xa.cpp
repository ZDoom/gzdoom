#include <algorithm>
#include "streamsource.h"
#include "../../libraries/music_common/fileio.h"

/**
 * PlayStation XA (ADPCM) source support for MultiVoc
 * Adapted and remixed from superxa2wav 
 *
 * taken from EDuke32 and adapted for GZDoom by Christoph Oelckers
 */


//#define NO_XA_HEADER

enum
{
kNumOfSamples   = 224,
kNumOfSGs       = 18,
TTYWidth        = 80,

kBufSize		= (kNumOfSGs*kNumOfSamples),
kSamplesMono	= (kNumOfSGs*kNumOfSamples),
kSamplesStereo	= (kNumOfSGs*kNumOfSamples/2),

/* ADPCM */
XA_DATA_START   = (0x44-48)
};

inline float constexpr DblToPCMF(double dt) { return float(dt) * (1.f/32768.f); }

typedef struct {
   MusicIO::FileInterface *reader;
   size_t committed;
   size_t length;
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
    auto &decodeBuf = xad->block;

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
                decodeBuf[count++] = DblToPCMF(xad->t1);
            }
        }
    }
}

static void decodeSoundSectStereo(XASector *ssct, xa_data * xad)
{
    size_t count = 0;
    int8_t snddat, filt, range;
    int8_t filt1, range1;
    int32_t unit, sample;
    int32_t sndgrp;
    double tmp2, tmp3, tmp4, tmp5;
    auto &decodeBuf = xad->block;

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

static void getNextXABlock(xa_data *xad, bool looping )
{
    XASector ssct;
    int coding;
	const int SUBMODE_REAL_TIME_SECTOR = (1 << 6);
	const int SUBMODE_FORM             = (1 << 5);
	const int SUBMODE_AUDIO_DATA       = (1 << 2);

    do
    {
        size_t bytes = xad->length - xad->reader->tell();

        if (sizeof(XASector) < bytes)
            bytes = sizeof(XASector);

		xad->reader->read(&ssct, (int)bytes);
    }
    while (ssct.sectorFiller[46] != (SUBMODE_REAL_TIME_SECTOR | SUBMODE_FORM | SUBMODE_AUDIO_DATA));

    coding = ssct.sectorFiller[47];

	xad->committed = 0;
	xad->blockIsMono = (coding & 3) == 0;
    xad->blockIs18K = (((coding >> 2) & 3) == 1);

    if (!xad->blockIsMono)
    {
        decodeSoundSectStereo(&ssct, xad);
    }
    else
    {
        decodeSoundSectMono(&ssct, xad);
    }

    if (xad->length == xad->reader->tell())
    {
        if (looping)
        {
			xad->reader->seek(XA_DATA_START, SEEK_SET);
            xad->t1 = xad->t2 = xad->t1_x = xad->t2_x = 0;
        }
        else
            xad->finished = true;
    }

    xad->finished = false;
}

//==========================================================================
//
// XASong
//
//==========================================================================

class XASong : public StreamSource
{
public:
	XASong(MusicIO::FileInterface *readr);
	SoundStreamInfo GetFormat() override;
	bool Start() override;
	bool GetData(void *buffer, size_t len) override;

protected:
	xa_data xad;
};

//==========================================================================
//
// XASong - Constructor
//
//==========================================================================

XASong::XASong(MusicIO::FileInterface * reader)
{
	reader->seek(0, SEEK_END);
	xad.length = reader->tell();
	reader->seek(XA_DATA_START, SEEK_SET);
	xad.reader = reader;
	xad.t1 = xad.t2 = xad.t1_x = xad.t2_x = 0;

	getNextXABlock(&xad, false);
}

SoundStreamInfo XASong::GetFormat()
{
	auto SampleRate = xad.blockIs18K? 18900 : 37800;
	return { 64*1024, SampleRate, 2};
}

//==========================================================================
//
// XASong :: Play
//
//==========================================================================

bool XASong::Start()
{
	if (xad.finished && m_Looping)
	{
		xad.reader->seek(XA_DATA_START, SEEK_SET);
		xad.t1 = xad.t2 = xad.t1_x = xad.t2_x = 0;
		xad.finished = false;
	}
	return true;
}

//==========================================================================
//
// XASong :: Read
//
//==========================================================================

bool XASong::GetData(void *vbuff, size_t len)
{
	float *dest = (float*)vbuff;
	while (len > 0)
	{
		auto ptr = xad.committed;
		auto block = xad.block + ptr;
		if (ptr < kBufSize)
		{
			// commit the data
			if (xad.blockIsMono)
			{
				size_t numsamples = len / 8;
				size_t availdata = kBufSize - ptr;
				
				for(size_t tocopy = std::min(numsamples, availdata); tocopy > 0; tocopy--)
				{
					float f = *block++;
					*dest++ = f;
					*dest++ = f;
					len -= 8;
					ptr++;
				}
				xad.committed = ptr;
			}
			else
			{
				size_t availdata = (kBufSize - ptr) * 4;
				size_t tocopy = std::min(availdata, len);
				memcpy(dest, block, tocopy);
				dest += tocopy / 4;
				len -= tocopy;
				xad.committed += tocopy / 4;
			}
		}
		if (xad.finished)
		{
			memset(dest, 0, len);
			return true;
		}
		if (len > 0)
		{
			// we ran out of data and need more
			getNextXABlock(&xad, m_Looping);
			// repeat until done.
		}
		else break;
		
	}
	return !xad.finished;
} 

//==========================================================================
//
// XA_OpenSong
//
//==========================================================================

StreamSource *XA_OpenSong(MusicIO::FileInterface *reader)
{
	return new XASong(reader);
}

