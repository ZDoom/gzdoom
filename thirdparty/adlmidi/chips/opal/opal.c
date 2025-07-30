/*

    The Opal OPL3 emulator.

    Note: this is not a complete emulator, just enough for Reality Adlib Tracker tunes.

    Missing features compared to a real OPL3:

        - Timers/interrupts
        - OPL3 enable bit (it defaults to always on)
        - CSW mode
        - Test register
        - Percussion mode

*/

#include <stdint.h>
#include <limits.h>
#include "opal.h"

#ifndef INT_MAX
#   define INT_MAX  2147483647
#endif

#ifndef INT_MIN
#   define INT_MIN  -2147483648
#endif

/* Various constants */
typedef enum OpalEnumPriv_t
{
    OpalEnvOff = -1,
    OpalEnvAtt,
    OpalEnvDec,
    OpalEnvSus,
    OpalEnvRel
} OpalEnumPriv;

/*--------------------------------------------------------------------------------------------------*/
static const uint16_t RateTables[4][8] =
{
    {   1, 0, 1, 0, 1, 0, 1, 0  },
    {   1, 0, 1, 0, 0, 0, 1, 0  },
    {   1, 0, 0, 0, 1, 0, 0, 0  },
    {   1, 0, 0, 0, 0, 0, 0, 0  },
};

/*--------------------------------------------------------------------------------------------------*/
static const uint16_t ExpTable[0x100] =
{
    1018, 1013, 1007, 1002,  996,  991,  986,  980,  975,  969,  964,  959,  953,  948,  942,  937,
    932,  927,  921,  916,  911,  906,  900,  895,  890,  885,  880,  874,  869,  864,  859,  854,
    849,  844,  839,  834,  829,  824,  819,  814,  809,  804,  799,  794,  789,  784,  779,  774,
    770,  765,  760,  755,  750,  745,  741,  736,  731,  726,  722,  717,  712,  708,  703,  698,
    693,  689,  684,  680,  675,  670,  666,  661,  657,  652,  648,  643,  639,  634,  630,  625,
    621,  616,  612,  607,  603,  599,  594,  590,  585,  581,  577,  572,  568,  564,  560,  555,
    551,  547,  542,  538,  534,  530,  526,  521,  517,  513,  509,  505,  501,  496,  492,  488,
    484,  480,  476,  472,  468,  464,  460,  456,  452,  448,  444,  440,  436,  432,  428,  424,
    420,  416,  412,  409,  405,  401,  397,  393,  389,  385,  382,  378,  374,  370,  367,  363,
    359,  355,  352,  348,  344,  340,  337,  333,  329,  326,  322,  318,  315,  311,  308,  304,
    300,  297,  293,  290,  286,  283,  279,  276,  272,  268,  265,  262,  258,  255,  251,  248,
    244,  241,  237,  234,  231,  227,  224,  220,  217,  214,  210,  207,  204,  200,  197,  194,
    190,  187,  184,  181,  177,  174,  171,  168,  164,  161,  158,  155,  152,  148,  145,  142,
    139,  136,  133,  130,  126,  123,  120,  117,  114,  111,  108,  105,  102,   99,   96,   93,
    90,   87,   84,   81,   78,   75,   72,   69,   66,   63,   60,   57,   54,   51,   48,   45,
    42,   40,   37,   34,   31,   28,   25,   22,   20,   17,   14,   11,    8,    6,    3,    0,
};

/*--------------------------------------------------------------------------------------------------*/
static const uint16_t LogSinTable[0x100] =
{
    2137, 1731, 1543, 1419, 1326, 1252, 1190, 1137, 1091, 1050, 1013,  979,  949,  920,  894,  869,
    846,  825,  804,  785,  767,  749,  732,  717,  701,  687,  672,  659,  646,  633,  621,  609,
    598,  587,  576,  566,  556,  546,  536,  527,  518,  509,  501,  492,  484,  476,  468,  461,
    453,  446,  439,  432,  425,  418,  411,  405,  399,  392,  386,  380,  375,  369,  363,  358,
    352,  347,  341,  336,  331,  326,  321,  316,  311,  307,  302,  297,  293,  289,  284,  280,
    276,  271,  267,  263,  259,  255,  251,  248,  244,  240,  236,  233,  229,  226,  222,  219,
    215,  212,  209,  205,  202,  199,  196,  193,  190,  187,  184,  181,  178,  175,  172,  169,
    167,  164,  161,  159,  156,  153,  151,  148,  146,  143,  141,  138,  136,  134,  131,  129,
    127,  125,  122,  120,  118,  116,  114,  112,  110,  108,  106,  104,  102,  100,   98,   96,
    94,   92,   91,   89,   87,   85,   83,   82,   80,   78,   77,   75,   74,   72,   70,   69,
    67,   66,   64,   63,   62,   60,   59,   57,   56,   55,   53,   52,   51,   49,   48,   47,
    46,   45,   43,   42,   41,   40,   39,   38,   37,   36,   35,   34,   33,   32,   31,   30,
    29,   28,   27,   26,   25,   24,   23,   23,   22,   21,   20,   20,   19,   18,   17,   17,
    16,   15,   15,   14,   13,   13,   12,   12,   11,   10,   10,    9,    9,    8,    8,    7,
    7,    7,    6,    6,    5,    5,    5,    4,    4,    4,    3,    3,    3,    2,    2,    2,
    2,    1,    1,    1,    1,    1,    1,    1,    0,    0,    0,    0,    0,    0,    0,    0,
};

/*--------------------------------------------------------------------------------------------------*/
static const uint16_t PanLawTable[128] =
{
    65535, 65529, 65514, 65489, 65454, 65409, 65354, 65289,
    65214, 65129, 65034, 64929, 64814, 64689, 64554, 64410,
    64255, 64091, 63917, 63733, 63540, 63336, 63123, 62901,
    62668, 62426, 62175, 61914, 61644, 61364, 61075, 60776,
    60468, 60151, 59825, 59489, 59145, 58791, 58428, 58057,
    57676, 57287, 56889, 56482, 56067, 55643, 55211, 54770,
    54320, 53863, 53397, 52923, 52441, 51951, 51453, 50947,
    50433, 49912, 49383, 48846, 48302, 47750, 47191,
    46340, /* Center left */
    46340, /* Center right */
    45472, 44885, 44291, 43690, 43083, 42469, 41848, 41221,
    40588, 39948, 39303, 38651, 37994, 37330, 36661, 35986,
    35306, 34621, 33930, 33234, 32533, 31827, 31116, 30400,
    29680, 28955, 28225, 27492, 26754, 26012, 25266, 24516,
    23762, 23005, 22244, 21480, 20713, 19942, 19169, 18392,
    17613, 16831, 16046, 15259, 14469, 13678, 12884, 12088,
    11291, 10492, 9691, 8888, 8085, 7280, 6473, 5666,
    4858, 4050, 3240, 2431, 1620, 810, 0
};


/*==================================================================================================
 * OpalOperator calls
 *=================================================================================================*/
static void OpalOperator_Init(OpalOperator *op);

static void OpalOperator_SetMaster(OpalOperator *self, Opal* opal);
static void OpalOperator_SetChannel(OpalOperator *self, OpalChannel* chan);

static int16_t OpalOperator_Output(OpalOperator *self, uint16_t keyscalenum, uint32_t phase_step, int16_t vibrato, int16_t mod /*= 0*/, int16_t fbshift /* = 0*/);

static void OpalOperator_SetKeyOn(OpalOperator *self, opal_bool on);
static void OpalOperator_SetTremoloEnable(OpalOperator *self, opal_bool on);
static void OpalOperator_SetVibratoEnable(OpalOperator *self, opal_bool on);
static void OpalOperator_SetSustainMode(OpalOperator *self, opal_bool on);
static void OpalOperator_SetEnvelopeScaling(OpalOperator *self, opal_bool on);
static void OpalOperator_SetFrequencyMultiplier(OpalOperator *self, uint16_t scale);
static void OpalOperator_SetKeyScale(OpalOperator *self, uint16_t scale);
static void OpalOperator_SetOutputLevel(OpalOperator *self, uint16_t level);
static void OpalOperator_SetAttackRate(OpalOperator *self, uint16_t rate);
static void OpalOperator_SetDecayRate(OpalOperator *self, uint16_t rate);
static void OpalOperator_SetSustainLevel(OpalOperator *self, uint16_t level);
static void OpalOperator_SetReleaseRate(OpalOperator *self, uint16_t rate);
static void OpalOperator_SetWaveform(OpalOperator *self, uint16_t wave);

static void OpalOperator_ComputeRates(OpalOperator *self);
static void OpalOperator_ComputeKeyScaleLevel(OpalOperator *self);


/*==================================================================================================
 * OpalChannel calls
 *=================================================================================================*/
static void OpalChannel_Init(OpalChannel *self);

/*public:*/
static void OpalChannel_SetMaster(OpalChannel *self, Opal* opal);
static void OpalChannel_SetOperators(OpalChannel *self, OpalOperator* a, OpalOperator* b, OpalOperator* c, OpalOperator* d);

static void OpalChannel_Output(OpalChannel *self, int16_t *left, int16_t *right);
static void OpalChannel_SetEnable(OpalChannel *self, opal_bool on);
static void OpalChannel_SetChannelPair(OpalChannel *self, OpalChannel* pair);

static void OpalChannel_SetFrequencyLow(OpalChannel *self, uint16_t freq);
static void OpalChannel_SetFrequencyHigh(OpalChannel *self, uint16_t freq);
static void OpalChannel_SetKeyOn(OpalChannel *self, opal_bool on);
static void OpalChannel_SetOctave(OpalChannel *self, uint16_t oct);
static void OpalChannel_SetLeftEnable(OpalChannel *self, opal_bool on);
static void OpalChannel_SetRightEnable(OpalChannel *self, opal_bool on);
static void OpalChannel_SetPan(OpalChannel *self, uint8_t pan);
static void OpalChannel_SetFeedback(OpalChannel *self, uint16_t val);
static void OpalChannel_SetModulationType(OpalChannel *self, uint16_t type);

static uint16_t OpalChannel_GetFreq(OpalChannel *self);
static uint16_t OpalChannel_GetOctave(OpalChannel *self);
static uint16_t OpalChannel_GetKeyScaleNumber(OpalChannel *self);
static uint16_t OpalChannel_GetModulationType(OpalChannel *self);
static OpalChannel* OpalChannel_GetChannelPair(OpalChannel *self); /* libADLMIDI */

static void OpalChannel_ComputeKeyScaleNumber(OpalChannel *self);

/*protected:*/
static void OpalChannel_ComputePhaseStep(OpalChannel *self);

/*==================================================================================================
 * Opal private calls
 *=================================================================================================*/
static void Opal_Output(Opal *self, int16_t *left, int16_t *right);




/*==================================================================================================
 *                                      Implementations                                            *
 *=================================================================================================*/


static const int chan_ops[] =
{
    0, 1, 2, 6, 7, 8, 12, 13, 14, 18, 19, 20, 24, 25, 26, 30, 31, 32,
};

/*==================================================================================================
 * Initialise the emulation.
 *=================================================================================================*/
void Opal_Init(Opal *self, int sample_rate)
{
    int i, op;
    OpalChannel* chan;

    self->Clock = 0;
    self->TremoloClock = 0;
    self->TremoloLevel = 0;
    self->VibratoTick = 0;
    self->VibratoClock = 0;
    self->NoteSel = OPAL_FALSE;
    self->TremoloDepth = OPAL_FALSE;
    self->VibratoDepth = OPAL_FALSE;

    for(i = 0; i < OpalNumOperators; i++)
        OpalOperator_Init(&self->Op[i]);

    for(i = 0; i < OpalNumChannels; i++)
        OpalChannel_Init(&self->Chan[i]);

    /*
    //  // Build the exponentiation table (reversed from the official OPL3 ROM)
    //  for (int i = 0; i < 0x100; i++)
    //      ExpTable[i] = (pow(2, (0xFF - i) / 256.0) - 1) * 1024 + 0.5;
    //
    //  // Build the log-sin table
    //  for (int i = 0; i < 0x100; i++)
    //      LogSinTable[i] = -log(sin((i + 0.5) * 3.1415926535897933 / 256 / 2)) / log(2) * 256 + 0.5;
    */

    /* Let sub-objects know where to find us */
    for(i = 0; i < OpalNumOperators; i++)
        OpalOperator_SetMaster(&self->Op[i], self);

    for(i = 0; i < OpalNumChannels; i++)
        OpalChannel_SetMaster(&self->Chan[i], self);

    /* Add the operators to the channels.  Note, some channels can't use all the operators */
    /* FIXME: put this into a separate routine */
    for(i = 0; i < OpalNumChannels; i++)
    {
        chan = &self->Chan[i];
        op = chan_ops[i];

        if(i < 3 || (i >= 9 && i < 12))
            OpalChannel_SetOperators(chan, &self->Op[op], &self->Op[op + 3], &self->Op[op + 6], &self->Op[op + 9]);
        else
            OpalChannel_SetOperators(chan, &self->Op[op], &self->Op[op + 3], 0, 0);
    }

    /* Initialise the operator rate data.  We can't do this in the Operator constructor as it
     * relies on referencing the master and channel objects */
    for(i = 0; i < OpalNumOperators; i++)
        OpalOperator_ComputeRates(&self->Op[i]);

    /* Initialise channel panning at center. */
    for(i = 0; i < OpalNumChannels; i++)
    {
        OpalChannel_SetPan(&self->Chan[i], 64);
        OpalChannel_SetLeftEnable(&self->Chan[i], OPAL_TRUE);
        OpalChannel_SetRightEnable(&self->Chan[i], OPAL_TRUE);
    }

    Opal_SetSampleRate(self, sample_rate);
}



/*==================================================================================================
 * Change the sample rate.
 *=================================================================================================*/
void Opal_SetSampleRate(Opal *self, int sample_rate)
{
    /* Sanity */
    if(sample_rate == 0)
        sample_rate = OpalOPL3SampleRate;

    self->SampleRate = sample_rate;
    self->SampleAccum = 0;
    self->LastOutput[0] = self->LastOutput[1] = 0;
    self->CurrOutput[0] = self->CurrOutput[1] = 0;
}


static const int op_lookup[] =
{
    /*  00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F */
    0,  1,  2,  3,  4,  5,  -1, -1, 6,  7,  8,  9,  10, 11, -1, -1,
    /*  10  11  12  13  14  15  16  17  18  19  1A  1B  1C  1D  1E  1F */
    12, 13, 14, 15, 16, 17, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
};

/*==================================================================================================
 * Write a value to an OPL3 register.
 *=================================================================================================*/
void Opal_Port(Opal *self, uint16_t reg_num, uint8_t val)
{
    int i;
    unsigned ui;
    uint8_t mask;
    unsigned numchans;
    uint16_t chan;
    int chan_num;
    int op_num;
    OpalChannel *primary, *secondary, *cchan;
    OpalChannel* chans[2];
    uint16_t type = reg_num & 0xE0;
    OpalOperator *op;

    /* Is it BD, the one-off register stuck in the middle of the register array? */
    if(reg_num == 0xBD)
    {
        self->TremoloDepth = (val & 0x80) != 0;
        self->VibratoDepth = (val & 0x40) != 0;
        return;
    }

    /* Global registers */
    if(type == 0x00)
    {

        /* 4-OP enables*/
        if(reg_num == 0x104)
        {

            /* Enable/disable channels based on which 4-op enables */
            mask = 1;

            for(i = 0; i < 6; i++, mask <<= 1)
            {
                /* The 4-op channels are 0, 1, 2, 9, 10, 11 */
                chan = i < 3 ? i : i + 6;
                primary = &self->Chan[chan];
                secondary = &self->Chan[chan + 3];

                if(val & mask)
                {
                    /* Let primary channel know it's controlling the secondary channel */
                    OpalChannel_SetChannelPair(primary, secondary);

                    /* Turn off the second channel in the pair */
                    OpalChannel_SetEnable(secondary, OPAL_FALSE);

                }
                else
                {
                    /* Let primary channel know it's no longer controlling the secondary channel */
                    OpalChannel_SetChannelPair(primary, 0);

                    /* Turn on the second channel in the pair */
                    OpalChannel_SetEnable(secondary, OPAL_TRUE);
                }
            }

            /* CSW / Note-sel */
        }
        else if(reg_num == 0x08)
        {
            self->NoteSel = (val & 0x40) != 0;

            /* Get the channels to recompute the Key Scale No. as this varies based on NoteSel */
            for(i = 0; i < OpalNumChannels; i++)
                OpalChannel_ComputeKeyScaleNumber(&self->Chan[i]);
        }

        /* Channel registers */
    }
    else if(type >= 0xA0 && type <= 0xC0)
    {
        /* Convert to channel number */
        chan_num = reg_num & 15;

        /* Valid channel? */
        if(chan_num >= 9)
            return;

        /* Is it the other bank of channels? */
        if(reg_num & 0x100)
            chan_num += 9;

        cchan = &self->Chan[chan_num];

        /* libADLMIDI: registers Ax and Cx affect both channels */
        chans[0] = cchan;
        chans[1] = OpalChannel_GetChannelPair(cchan);
        numchans = chans[1] ? 2 : 1;

        /* Do specific registers */
        switch(reg_num & 0xF0)
        {

        /* Frequency low */
        case 0xA0:
        {
            for(ui = 0; ui < numchans; ++ui)    /* libADLMIDI */
            {
                OpalChannel_SetFrequencyLow(chans[ui], val);
            }

            break;
        }

        /* Key-on / Octave / Frequency High */
        case 0xB0:
        {
            for(ui = 0; ui < numchans; ++ui)    /* libADLMIDI */
            {
                OpalChannel_SetKeyOn(chans[ui],(val & 0x20) != 0);
                OpalChannel_SetOctave(chans[ui], val >> 2 & 7);
                OpalChannel_SetFrequencyHigh(chans[ui], val & 3);
            }

            break;
        }

        /* Right Stereo Channel Enable / Left Stereo Channel Enable / Feedback Factor / Modulation Type */
        case 0xC0:
        {
            OpalChannel_SetRightEnable(cchan, (val & 0x20) != 0);
            OpalChannel_SetLeftEnable(cchan, (val & 0x10) != 0);
            OpalChannel_SetFeedback(cchan, val >> 1 & 7);
            OpalChannel_SetModulationType(cchan, val & 1);
            break;
        }
        }

        /* Operator registers */
    }
    else if((type >= 0x20 && type <= 0x80) || type == 0xE0)
    {
        /* Convert to operator number */
        op_num = op_lookup[reg_num & 0x1F];

        /* Valid register? */
        if(op_num < 0)
            return;

        /* Is it the other bank of operators? */
        if(reg_num & 0x100)
            op_num += 18;

        op = &self->Op[op_num];

        /* Do specific registers */
        switch(type)
        {

        /* Tremolo Enable / Vibrato Enable / Sustain Mode / Envelope Scaling / Frequency Multiplier */
        case 0x20:
        {
            OpalOperator_SetTremoloEnable(op, (val & 0x80) != 0);
            OpalOperator_SetVibratoEnable(op, (val & 0x40) != 0);
            OpalOperator_SetSustainMode(op, (val & 0x20) != 0);
            OpalOperator_SetEnvelopeScaling(op, (val & 0x10) != 0);
            OpalOperator_SetFrequencyMultiplier(op, val & 15);
            break;
        }

        /* Key Scale / Output Level */
        case 0x40:
        {
            OpalOperator_SetKeyScale(op, val >> 6);
            OpalOperator_SetOutputLevel(op, val & 0x3F);
            break;
        }

        /* Attack Rate / Decay Rate */
        case 0x60:
        {
            OpalOperator_SetAttackRate(op, val >> 4);
            OpalOperator_SetDecayRate(op, val & 15);
            break;
        }

        /* Sustain Level / Release Rate */
        case 0x80:
        {
            OpalOperator_SetSustainLevel(op, val >> 4);
            OpalOperator_SetReleaseRate(op, val & 15);
            break;
        }

        /* Waveform */
        case 0xE0:
        {
            OpalOperator_SetWaveform(op, val & 7);
            break;
        }
        }
    }
}



/*==================================================================================================
 * Set panning on the channel designated by the register number.
 * This is extended functionality.
 *=================================================================================================*/
void Opal_Pan(Opal *self, uint16_t reg_num, uint8_t pan)
{
    uint8_t high = (reg_num >> 8) & 1;
    uint8_t regm = reg_num & 0xff;
    OpalChannel_SetPan(&self->Chan[9 * high + (regm & 0x0f)], pan);
}


static inline int32_t s_opal_MulDivR(int32_t a, int32_t b, int32_t c)
{
    int64_t ret = (((int64_t)(a) * b) + ( c / 2 )) / c;

    if(ret > INT_MAX)
        return INT_MAX;
    else if(ret < INT_MIN)
        return INT_MIN;
    else
        return (int32_t)(ret);
}


/*==================================================================================================
 * Generate sample.  Every time you call this you will get two signed 16-bit samples (one for each
 * stereo channel) which will sound correct when played back at the sample rate given when the
 * class was constructed.
 *=================================================================================================*/
void Opal_Sample(Opal *self, int16_t* left, int16_t* right)
{
    int32_t fract;

    /* If the destination sample rate is higher than the OPL3 sample rate, we need to skip ahead */
    while(self->SampleAccum >= self->SampleRate)
    {
        self->LastOutput[0] = self->CurrOutput[0];
        self->LastOutput[1] = self->CurrOutput[1];

        Opal_Output(self, &self->CurrOutput[0], &self->CurrOutput[1]);

        self->SampleAccum -= self->SampleRate;
    }

    /* Mix with the partial accumulation */
    fract = s_opal_MulDivR(self->SampleAccum, 65536, self->SampleRate);
    *left = (int16_t)(self->LastOutput[0] + ((fract * (self->CurrOutput[0] - self->LastOutput[0])) / 65536));
    *right = (int16_t)(self->LastOutput[1] + ((fract * (self->CurrOutput[1] - self->LastOutput[1])) / 65536));

    self->SampleAccum += OpalOPL3SampleRate;
}



/*==================================================================================================
 * Produce final output from the chip.  This is at the OPL3 sample-rate.
 *=================================================================================================*/
static void Opal_Output(Opal *self, int16_t *left, int16_t *right)
{
    int i;
    int16_t chanleft, chanright;
    int32_t leftmix = 0, rightmix = 0;

    /* Sum the output of each channel */
    for(i = 0; i < OpalNumChannels; i++)
    {
        OpalChannel_Output(&self->Chan[i], &chanleft, &chanright);
        leftmix += chanleft;
        rightmix += chanright;
    }

    /* Clamp */
    if(leftmix < -0x8000)
        *left = -0x8000;
    else if(leftmix > 0x7FFF)
        *left = 0x7FFF;
    else
        *left = leftmix;

    if(rightmix < -0x8000)
        *right = -0x8000;
    else if(rightmix > 0x7FFF)
        *right = 0x7FFF;
    else
        *right = rightmix;

    self->Clock++;

    /* Tremolo.  According to this post, the OPL3 tremolo is a 13,440 sample length triangle wave
     * with a peak at 26 and a trough at 0 and is simply added to the logarithmic level accumulator
     *      http://forums.submarine.org.uk/phpBB/viewtopic.php?f=9&t=1171 */
    self->TremoloClock = (self->TremoloClock + 1) % 13440;
    self->TremoloLevel = ((self->TremoloClock < 13440 / 2) ? self->TremoloClock : 13440 - self->TremoloClock) / 256;

    if(!self->TremoloDepth)
        self->TremoloLevel >>= 2;

    /* Vibrato.  This appears to be a 8 sample long triangle wave with a magnitude of the three
     * high bits of the channel frequency, positive and negative, divided by two if the vibrato
     * depth is zero.  It is only cycled every 1,024 samples. */
    self->VibratoTick++;

    if(self->VibratoTick >= 1024)
    {
        self->VibratoTick = 0;
        self->VibratoClock = (self->VibratoClock + 1) & 7;
    }
}



/*==================================================================================================
 * Channel constructor.
 *=================================================================================================*/
static void OpalChannel_Init(OpalChannel *self)
{
    self->Master = 0;
    self->Freq = 0;
    self->Octave = 0;
    self->PhaseStep = 0;
    self->KeyScaleNumber = 0;
    self->FeedbackShift = 0;
    self->ModulationType = 0;
    self->ChannelPair = 0;
    self->Enable = OPAL_TRUE;
    self->LeftPan = 46340;
    self->RightPan = 46340;
}

static void OpalChannel_SetMaster(OpalChannel *self, Opal* opal)
{
    self->Master = opal;
}


static void OpalChannel_SetOperators(OpalChannel *self, OpalOperator* a, OpalOperator* b, OpalOperator* c, OpalOperator* d)
{
    self->Op[0] = a;
    self->Op[1] = b;
    self->Op[2] = c;
    self->Op[3] = d;

    if(a) OpalOperator_SetChannel(a, self);
    if(b) OpalOperator_SetChannel(b, self);
    if(c) OpalOperator_SetChannel(c, self);
    if(d) OpalOperator_SetChannel(d, self);
}


/*==================================================================================================
 * Produce output from channel.
 *==================================================================================================*/
static void OpalChannel_Output(OpalChannel *self, int16_t *left, int16_t *right)
{
    int16_t vibrato;
    uint16_t clk;
    /* Combine individual operator outputs */
    int16_t out, acc;

    /* Has the channel been disabled?  This is usually a result of the 4-op enables being used to
     * disable the secondary channel in each 4-op pair */
    if(!self->Enable)
    {
        *left = *right = 0;
        return;
    }

    vibrato = (self->Freq >> 7) & 7;

    if(!self->Master->VibratoDepth)
        vibrato >>= 1;

    /* 0  3  7  3  0  -3  -7  -3 */
    clk = self->Master->VibratoClock;

    if(!(clk & 3))
        vibrato = 0;                /* Position 0 and 4 is zero */
    else
    {
        if(clk & 1)
            vibrato >>= 1;          /* Odd positions are half the magnitude */

        if(clk & 4)
            vibrato = -vibrato;     /* The second half positions are negative */
    }

    vibrato <<= self->Octave;

    /* Combine individual operator outputs */

    /* Running in 4-op mode? */
    if(self->ChannelPair)
    {

        /* Get the secondary channel's modulation type.  This is the only thing from the secondary
         * channel that is used */
        if(OpalChannel_GetModulationType(self->ChannelPair) == 0)
        {
            if(self->ModulationType == 0)
            {
                /* feedback -> modulator -> modulator -> modulator -> carrier */
                out  = OpalOperator_Output(self->Op[0], self->KeyScaleNumber, self->PhaseStep, vibrato, 0, self->FeedbackShift);
                out  = OpalOperator_Output(self->Op[1], self->KeyScaleNumber, self->PhaseStep, vibrato, out, 0);
                out  = OpalOperator_Output(self->Op[2], self->KeyScaleNumber, self->PhaseStep, vibrato, out, 0);
                out  = OpalOperator_Output(self->Op[3], self->KeyScaleNumber, self->PhaseStep, vibrato, out, 0);

            }
            else
            {
                /* (feedback -> carrier) + (modulator -> modulator -> carrier) */
                out  = OpalOperator_Output(self->Op[0], self->KeyScaleNumber, self->PhaseStep, vibrato, 0, self->FeedbackShift);
                acc  = OpalOperator_Output(self->Op[1], self->KeyScaleNumber, self->PhaseStep, vibrato, 0, 0);
                acc  = OpalOperator_Output(self->Op[2], self->KeyScaleNumber, self->PhaseStep, vibrato, acc, 0);
                out += OpalOperator_Output(self->Op[3], self->KeyScaleNumber, self->PhaseStep, vibrato, acc, 0);
            }

        }
        else
        {

            if(self->ModulationType == 0)
            {
                /* (feedback -> modulator -> carrier) + (modulator -> carrier) */
                out  = OpalOperator_Output(self->Op[0], self->KeyScaleNumber, self->PhaseStep, vibrato, 0, self->FeedbackShift);
                out  = OpalOperator_Output(self->Op[1], self->KeyScaleNumber, self->PhaseStep, vibrato, out, 0);
                acc  = OpalOperator_Output(self->Op[2], self->KeyScaleNumber, self->PhaseStep, vibrato, 0, 0);
                out += OpalOperator_Output(self->Op[3], self->KeyScaleNumber, self->PhaseStep, vibrato, acc, 0);

            }
            else
            {
                /* (feedback -> carrier) + (modulator -> carrier) + carrier */
                out  = OpalOperator_Output(self->Op[0], self->KeyScaleNumber, self->PhaseStep, vibrato, 0, self->FeedbackShift);
                acc  = OpalOperator_Output(self->Op[1], self->KeyScaleNumber, self->PhaseStep, vibrato, 0, 0);
                out += OpalOperator_Output(self->Op[2], self->KeyScaleNumber, self->PhaseStep, vibrato, acc, 0);
                out += OpalOperator_Output(self->Op[3], self->KeyScaleNumber, self->PhaseStep, vibrato, 0, 0);
            }
        }

    }
    else
    {
        /* Standard 2-op mode */
        if(self->ModulationType == 0)
        {
            /* Frequency modulation (well, phase modulation technically) */
            out = OpalOperator_Output(self->Op[0], self->KeyScaleNumber, self->PhaseStep, vibrato, 0, self->FeedbackShift);
            out = OpalOperator_Output(self->Op[1], self->KeyScaleNumber, self->PhaseStep, vibrato, out, 0);

        }
        else
        {
            /* Additive */
            out =  OpalOperator_Output(self->Op[0], self->KeyScaleNumber, self->PhaseStep, vibrato, 0, self->FeedbackShift);
            out += OpalOperator_Output(self->Op[1], self->KeyScaleNumber, self->PhaseStep, vibrato, 0, 0);
        }
    }

    *left = self->LeftEnable ? out : 0;
    *right = self->RightEnable ? out : 0;

    *left = (*left * self->LeftPan) / 65536;
    *right = (*right * self->RightPan) / 65536;
}

static void OpalChannel_SetEnable(OpalChannel *self, opal_bool on)
{
    self->Enable = on;
}

static void OpalChannel_SetChannelPair(OpalChannel *self, OpalChannel* pair)
{
    self->ChannelPair = pair;
}



/*==================================================================================================
 * Set phase step for operators using this channel.
 *=================================================================================================*/
static void OpalChannel_SetFrequencyLow(OpalChannel *self, uint16_t freq)
{
    self->Freq = (self->Freq & 0x300) | (freq & 0xFF);
    OpalChannel_ComputePhaseStep(self);
}

/*-------------------------------------------------------------------------------------------------*/
static void OpalChannel_SetFrequencyHigh(OpalChannel *self, uint16_t freq)
{
    self->Freq = (self->Freq & 0xFF) | ((freq & 3) << 8);
    OpalChannel_ComputePhaseStep(self);

    /* Only the high bits of Freq affect the Key Scale No. */
    OpalChannel_ComputeKeyScaleNumber(self);
}



/*==================================================================================================
 * Set the octave of the channel (0 to 7).
 *=================================================================================================*/
static void OpalChannel_SetOctave(OpalChannel *self, uint16_t oct)
{
    self->Octave = oct & 7;
    OpalChannel_ComputePhaseStep(self);
    OpalChannel_ComputeKeyScaleNumber(self);
}



/*==================================================================================================
 * Keys the channel on/off.
 *=================================================================================================*/
static void OpalChannel_SetKeyOn(OpalChannel *self, opal_bool on)
{
    OpalOperator_SetKeyOn(self->Op[0], on);
    OpalOperator_SetKeyOn(self->Op[1], on);
}



/*==================================================================================================
 * Enable left stereo channel.
 *=================================================================================================*/
static void OpalChannel_SetLeftEnable(OpalChannel *self, opal_bool on)
{
    self->LeftEnable = on;
}



/*==================================================================================================
 * Enable right stereo channel.
 *=================================================================================================*/
static void OpalChannel_SetRightEnable(OpalChannel *self, opal_bool on)
{
    self->RightEnable = on;
}



/*==================================================================================================
 * Pan the channel to the position given.
 *=================================================================================================*/
static void OpalChannel_SetPan(OpalChannel *self, uint8_t pan)
{
    pan &= 127;
    self->LeftPan = PanLawTable[pan];
    self->RightPan = PanLawTable[127 - pan];
}



/*==================================================================================================
 * Set the channel feedback amount.
 *=================================================================================================*/
static void OpalChannel_SetFeedback(OpalChannel *self, uint16_t val)
{
    self->FeedbackShift = val ? 9 - val : 0;
}



/*==================================================================================================
 * Set frequency modulation/additive modulation
 *=================================================================================================*/
static void OpalChannel_SetModulationType(OpalChannel *self, uint16_t type)
{
    self->ModulationType = type;
}

static uint16_t OpalChannel_GetFreq(OpalChannel *self)
{
    return self->Freq;
}

static uint16_t OpalChannel_GetOctave(OpalChannel *self)
{
    return self->Octave;
}

static uint16_t OpalChannel_GetKeyScaleNumber(OpalChannel *self)
{
    return self->KeyScaleNumber;
}

static uint16_t OpalChannel_GetModulationType(OpalChannel *self)
{
    return self->ModulationType;
}

static OpalChannel* OpalChannel_GetChannelPair(OpalChannel *self) /* libADLMIDI */
{
    return self->ChannelPair;
}

/*==================================================================================================
  Compute the stepping factor for the operator waveform phase based on the frequency and octave
  values of the channel.
 ==================================================================================================*/
static void OpalChannel_ComputePhaseStep(OpalChannel *self)
{
    self->PhaseStep = (uint32_t)(self->Freq) << self->Octave;
}



/*==================================================================================================
  Compute the key scale number and key scale levels.

  From the Yamaha data sheet this is the block/octave number as bits 3-1, with bit 0 coming from
  the MSB of the frequency if NoteSel is 1, and the 2nd MSB if NoteSel is 0.
 ==================================================================================================*/
static void OpalChannel_ComputeKeyScaleNumber(OpalChannel *self)
{
    int i;
    uint16_t lsb = self->Master->NoteSel ? self->Freq >> 9 : (self->Freq >> 8) & 1;
    self->KeyScaleNumber = self->Octave << 1 | lsb;

    /* Get the channel operators to recompute their rates as they're dependent on this number.
     * They also need to recompute their key scale level */
    for(i = 0; i < 4; i++)
    {
        if(!self->Op[i])
            continue;

        OpalOperator_ComputeRates(self->Op[i]);
        OpalOperator_ComputeKeyScaleLevel(self->Op[i]);
    }
}


/*==================================================================================================
 * Operator constructor.
 *=================================================================================================*/
static void OpalOperator_Init(OpalOperator *op)
{
    op->Master = 0;
    op->Chan = 0;
    op->Phase = 0;
    op->Waveform = 0;
    op->FreqMultTimes2 = 1;
    op->EnvelopeStage = OpalEnvOff;
    op->EnvelopeLevel = 0x1FF;
    op->AttackRate = 0;
    op->DecayRate = 0;
    op->SustainLevel = 0;
    op->ReleaseRate = 0;
    op->KeyScaleShift = 0;
    op->KeyScaleLevel = 0;
    op->Out[0] = op->Out[1] = 0;
    op->KeyOn = OPAL_FALSE;
    op->KeyScaleRate = OPAL_FALSE;
    op->SustainMode = OPAL_FALSE;
    op->TremoloEnable = OPAL_FALSE;
    op->VibratoEnable = OPAL_FALSE;
}

static void OpalOperator_SetMaster(OpalOperator *self, Opal* opal)
{
    self->Master = opal;
}

static void OpalOperator_SetChannel(OpalOperator *self, OpalChannel* chan)
{
    self->Chan = chan;
}


/*==================================================================================================
 * Produce output from operator.
 *=================================================================================================*/
static int16_t OpalOperator_Output(OpalOperator *self, uint16_t keyscalenum, uint32_t phase_step, int16_t vibrato, int16_t mod, int16_t fbshift)
{
    int16_t v;
    uint16_t mix, phase, offset, logsin, add, level, negate;

    (void)keyscalenum;

    /* Advance wave phase */
    if(self->VibratoEnable)
        phase_step += vibrato;

    self->Phase += (phase_step * self->FreqMultTimes2) / 2;

    level = (self->EnvelopeLevel + self->OutputLevel + self->KeyScaleLevel + (self->TremoloEnable ? self->Master->TremoloLevel : 0)) << 3;

    switch(self->EnvelopeStage)
    {

    /* Attack stage */
    case OpalEnvAtt:
    {
        add = ((self->AttackAdd >> self->AttackTab[self->Master->Clock >> self->AttackShift & 7]) * ~self->EnvelopeLevel) >> 3;

        if(self->AttackRate == 0)
            add = 0;

        if(self->AttackMask && (self->Master->Clock & self->AttackMask))
            add = 0;

        self->EnvelopeLevel += add;

        if(self->EnvelopeLevel <= 0)
        {
            self->EnvelopeLevel = 0;
            self->EnvelopeStage = OpalEnvDec;
        }

        break;
    }

    /* Decay stage */
    case OpalEnvDec:
    {
        add = self->DecayAdd >> self->DecayTab[self->Master->Clock >> self->DecayShift & 7];

        if(self->DecayRate == 0)
            add = 0;

        if(self->DecayMask && (self->Master->Clock & self->DecayMask))
            add = 0;

        self->EnvelopeLevel += add;

        if(self->EnvelopeLevel >= self->SustainLevel)
        {
            self->EnvelopeLevel = self->SustainLevel;
            self->EnvelopeStage = OpalEnvSus;
        }

        break;
    }

    /* Sustain stage */
    case OpalEnvSus:
    {
        if(self->SustainMode)
            break;

        /* Note: fall-through! */

    } /* fallthrough */

    /* Release stage */
    case OpalEnvRel:
    {
        add = self->ReleaseAdd >> self->ReleaseTab[self->Master->Clock >> self->ReleaseShift & 7];

        if(self->ReleaseRate == 0)
            add = 0;

        if(self->ReleaseMask && (self->Master->Clock & self->ReleaseMask))
            add = 0;

        self->EnvelopeLevel += add;

        if(self->EnvelopeLevel >= 0x1FF)
        {
            self->EnvelopeLevel = 0x1FF;
            self->EnvelopeStage = OpalEnvOff;
            self->Out[0] = self->Out[1] = 0;
            return 0;
        }

        break;
    }

    /* Envelope, and therefore the operator, is not running */
    default:
        self->Out[0] = self->Out[1] = 0;
        return 0;
    }

    /* Feedback?  In that case we modulate by a blend of the last two samples */
    if(fbshift)
        mod += (self->Out[0] + self->Out[1]) >> fbshift;

    phase = (self->Phase >> 10) + mod;
    offset = phase & 0xFF;
    negate = OPAL_FALSE;

    switch(self->Waveform)
    {

    /*------------------------------------
     * Standard sine wave
     *------------------------------------*/
    case 0:
        if(phase & 0x100)
            offset ^= 0xFF;

        logsin = LogSinTable[offset];
        negate = (phase & 0x200) != 0;
        break;

    /*------------------------------------
     * Half sine wave
     *------------------------------------*/
    case 1:
        if(phase & 0x200)
            offset = 0;
        else if(phase & 0x100)
            offset ^= 0xFF;

        logsin = LogSinTable[offset];
        break;

    /*------------------------------------
     * Positive sine wave
     *------------------------------------*/
    case 2:
        if(phase & 0x100)
            offset ^= 0xFF;

        logsin =  LogSinTable[offset];
        break;

    /*------------------------------------
     * Quarter positive sine wave
     *------------------------------------*/
    case 3:
        if(phase & 0x100)
            offset = 0;

        logsin = LogSinTable[offset];
        break;

    /*------------------------------------
     * Double-speed sine wave
     *------------------------------------*/
    case 4:
        if(phase & 0x200)
            offset = 0;

        else
        {

            if(phase & 0x80)
                offset ^= 0xFF;

            offset = (offset + offset) & 0xFF;
            negate = (phase & 0x100) != 0;
        }

        logsin = LogSinTable[offset];
        break;

    /*------------------------------------
     * Double-speed positive sine wave
     *------------------------------------*/
    case 5:
        if(phase & 0x200)
            offset = 0;

        else
        {

            offset = (offset + offset) & 0xFF;

            if(phase & 0x80)
                offset ^= 0xFF;
        }

        logsin = LogSinTable[offset];
        break;

    /*------------------------------------
     * Square wave
     *------------------------------------*/
    case 6:
        logsin = 0;
        negate = (phase & 0x200) != 0;
        break;

    /*------------------------------------
     * Exponentiation wave
     *------------------------------------*/
    default:
        logsin = phase & 0x1FF;

        if(phase & 0x200)
        {
            logsin ^= 0x1FF;
            negate = OPAL_TRUE;
        }

        logsin <<= 3;
        break;
    }

    mix = logsin + level;

    if(mix > 0x1FFF)
        mix = 0x1FFF;

    /* From the OPLx decapsulated docs:
     * "When such a table is used for calculation of the exponential, the table is read at the
     * position given by the 8 LSB's of the input. The value + 1024 (the hidden bit) is then the
     * significand of the floating point output and the yet unused MSB's of the input are the
     * exponent of the floating point output." */
    v = ExpTable[mix & 0xFF] + 1024;
    v >>= mix >> 8;
    v += v;

    if(negate)
        v = ~v;

    /* Keep last two results for feedback calculation */
    self->Out[1] = self->Out[0];
    self->Out[0] = v;

    return v;
}



/*==================================================================================================
 * Trigger operator.
 *=================================================================================================*/
void OpalOperator_SetKeyOn(OpalOperator* self, opal_bool on)
{
    /* Already on/off? */
    if(self->KeyOn == on)
        return;

    self->KeyOn = on;

    if(on)
    {
        /* The highest attack rate is instant; it bypasses the attack phase */
        if(self->AttackRate == 15)
        {
            self->EnvelopeStage = OpalEnvDec;
            self->EnvelopeLevel = 0;
        }
        else
            self->EnvelopeStage = OpalEnvAtt;

        self->Phase = 0;

    }
    else
    {
        /* Stopping current sound? */
        if(self->EnvelopeStage != OpalEnvOff && self->EnvelopeStage != OpalEnvRel)
            self->EnvelopeStage = OpalEnvRel;
    }
}


/*==================================================================================================
 * Enable amplitude vibrato.
 *=================================================================================================*/
static void OpalOperator_SetTremoloEnable(OpalOperator *self, opal_bool on)
{
    self->TremoloEnable = on;
}



/*==================================================================================================
 * Enable frequency vibrato.
 *=================================================================================================*/
static void OpalOperator_SetVibratoEnable(OpalOperator *self, opal_bool on)
{
    self->VibratoEnable = on;
}



/*==================================================================================================
 * Sets whether we release or sustain during the sustain phase of the envelope.  'true' is to
 * sustain, otherwise release.
 *=================================================================================================*/
static void OpalOperator_SetSustainMode(OpalOperator *self, opal_bool on)
{
    self->SustainMode = on;
}



/*==================================================================================================
 * Key scale rate.  Sets how much the Key Scaling Number affects the envelope rates.
 *==================================================================================================*/
static void OpalOperator_SetEnvelopeScaling(OpalOperator *self, opal_bool on)
{
    self->KeyScaleRate = on;
    OpalOperator_ComputeRates(self);
}


/* Needs to be multiplied by two (and divided by two later when we use it) because
 * the first entry is actually .5 */
static const uint16_t mul_times_2[] =
{
    1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30,
};

/*==================================================================================================
 * Multiplies the phase frequency.
 *==================================================================================================*/
static void OpalOperator_SetFrequencyMultiplier(OpalOperator *self, uint16_t scale)
{
    self->FreqMultTimes2 = mul_times_2[scale & 15];
}



/*==================================================================================================
 * Attenuates output level towards higher pitch.
 *==================================================================================================*/
static void OpalOperator_SetKeyScale(OpalOperator *self, uint16_t scale)
{
    /* libADLMIDI: KSL computation fix */
    const unsigned KeyScaleShiftTable[4] = {8, 1, 2, 0};
    self->KeyScaleShift = KeyScaleShiftTable[scale];

    OpalOperator_ComputeKeyScaleLevel(self);
}



/*==================================================================================================
 * Sets the output level (volume) of the operator.
 *=================================================================================================*/
static void OpalOperator_SetOutputLevel(OpalOperator *self, uint16_t level)
{
    self->OutputLevel = level * 4;
}



/*==================================================================================================
 * Operator attack rate.
 *=================================================================================================*/
static void OpalOperator_SetAttackRate(OpalOperator *self, uint16_t rate)
{
    self->AttackRate = rate;
    OpalOperator_ComputeRates(self);
}



/*==================================================================================================
 * Operator decay rate.
 *=================================================================================================*/
static void OpalOperator_SetDecayRate(OpalOperator *self, uint16_t rate)
{
    self->DecayRate = rate;
    OpalOperator_ComputeRates(self);
}



/*==================================================================================================
 * Operator sustain level.
 *=================================================================================================*/
static void OpalOperator_SetSustainLevel(OpalOperator *self, uint16_t level)
{
    self->SustainLevel = level < 15 ? level : 31;
    self->SustainLevel *= 16;
}



/*==================================================================================================
 * Operator release rate.
 *=================================================================================================*/
static void OpalOperator_SetReleaseRate(OpalOperator *self, uint16_t rate)
{
    self->ReleaseRate = rate;
    OpalOperator_ComputeRates(self);
}



/*==================================================================================================
 * Assign the waveform this operator will use.
 *=================================================================================================*/
static void OpalOperator_SetWaveform(OpalOperator *self, uint16_t wave)
{
    self->Waveform = wave & 7;
}



/*==================================================================================================
 * Compute actual rate from register rate.  From the Yamaha data sheet:
 *
 * Actual rate = Rate value * 4 + Rof, if Rate value = 0, actual rate = 0
 *
 * Rof is set as follows depending on the KSR setting:
 *
 *  Key scale   0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15
 *  KSR = 0     0   0   0   0   1   1   1   1   2   2   2   2   3   3   3   3
 *  KSR = 1     0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15
 *
 * Note: zero rates are infinite, and are treated separately elsewhere
 *=================================================================================================*/
static void OpalOperator_ComputeRates(OpalOperator *self)
{
    int combined_rate = self->AttackRate * 4 + (OpalChannel_GetKeyScaleNumber(self->Chan) >> (self->KeyScaleRate ? 0 : 2));
    int rate_high = combined_rate >> 2;
    int rate_low = combined_rate & 3;

    self->AttackShift = rate_high < 12 ? 12 - rate_high : 0;
    self->AttackMask = (1 << self->AttackShift) - 1;
    self->AttackAdd = (rate_high < 12) ? 1 : 1 << (rate_high - 12);
    self->AttackTab = RateTables[rate_low];

    /* Attack rate of 15 is always instant */
    if(self->AttackRate == 15)
        self->AttackAdd = 0xFFF;

    combined_rate = self->DecayRate * 4 + (OpalChannel_GetKeyScaleNumber(self->Chan) >> (self->KeyScaleRate ? 0 : 2));
    rate_high = combined_rate >> 2;
    rate_low = combined_rate & 3;

    self->DecayShift = rate_high < 12 ? 12 - rate_high : 0;
    self->DecayMask = (1 << self->DecayShift) - 1;
    self->DecayAdd = (rate_high < 12) ? 1 : 1 << (rate_high - 12);
    self->DecayTab = RateTables[rate_low];

    combined_rate = self->ReleaseRate * 4 + (OpalChannel_GetKeyScaleNumber(self->Chan) >> (self->KeyScaleRate ? 0 : 2));
    rate_high = combined_rate >> 2;
    rate_low = combined_rate & 3;

    self->ReleaseShift = rate_high < 12 ? 12 - rate_high : 0;
    self->ReleaseMask = (1 << self->ReleaseShift) - 1;
    self->ReleaseAdd = (rate_high < 12) ? 1 : 1 << (rate_high - 12);
    self->ReleaseTab = RateTables[rate_low];
}


static const uint16_t levtab[] =
{
    0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
    0,      0,      0,      0,      0,      0,      0,      0,      0,      8,      12,     16,     20,     24,     28,     32,
    0,      0,      0,      0,      0,      12,     20,     28,     32,     40,     44,     48,     52,     56,     60,     64,
    0,      0,      0,      20,     32,     44,     52,     60,     64,     72,     76,     80,     84,     88,     92,     96,
    0,      0,      32,     52,     64,     76,     84,     92,     96,     104,    108,    112,    116,    120,    124,    128,
    0,      32,     64,     84,     96,     108,    116,    124,    128,    136,    140,    144,    148,    152,    156,    160,
    0,      64,     96,     116,    128,    140,    148,    156,    160,    168,    172,    176,    180,    184,    188,    192,
    0,      96,     128,    148,    160,    172,    180,    188,    192,    200,    204,    208,    212,    216,    220,    224,
};

/*=================================================================================================*
 * Compute the operator's key scale level.  This changes based on the channel frequency/octave and *
 * operator key scale value.                                                                       *
 *=================================================================================================*/
static void OpalOperator_ComputeKeyScaleLevel(OpalOperator *self)
{
    /* This uses a combined value of the top four bits of frequency with the octave/block */
    uint16_t i = (OpalChannel_GetOctave(self->Chan) << 4) | (OpalChannel_GetFreq(self->Chan) >> 6);
    self->KeyScaleLevel = levtab[i] >> self->KeyScaleShift;
}
