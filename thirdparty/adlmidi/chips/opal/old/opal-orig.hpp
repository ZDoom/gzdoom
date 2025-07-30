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
#define OPAL_HAVE_SOFT_PANNING 1 /* libADLMIDI */



#include <stdint.h>



//==================================================================================================
// Opal class.
//==================================================================================================
class Opal {

    class Channel;

    // Various constants
    enum {
        OPL3SampleRate      = 49716,
        NumChannels         = 18,
        NumOperators        = 36,

        EnvOff              = -1,
        EnvAtt,
        EnvDec,
        EnvSus,
        EnvRel
    };

    // A single FM operator
    class Operator {

        public:
                            Operator();
            void            SetMaster(Opal *opal) {  Master = opal;  }
            void            SetChannel(Channel *chan) {  Chan = chan;  }

            int16_t         Output(uint16_t keyscalenum, uint32_t phase_step, int16_t vibrato, int16_t mod = 0, int16_t fbshift = 0);

            void            SetKeyOn(bool on);
            void            SetTremoloEnable(bool on);
            void            SetVibratoEnable(bool on);
            void            SetSustainMode(bool on);
            void            SetEnvelopeScaling(bool on);
            void            SetFrequencyMultiplier(uint16_t scale);
            void            SetKeyScale(uint16_t scale);
            void            SetOutputLevel(uint16_t level);
            void            SetAttackRate(uint16_t rate);
            void            SetDecayRate(uint16_t rate);
            void            SetSustainLevel(uint16_t level);
            void            SetReleaseRate(uint16_t rate);
            void            SetWaveform(uint16_t wave);

            void            ComputeRates();
            void            ComputeKeyScaleLevel();

        protected:
            Opal *          Master;             // Master object
            Channel *       Chan;               // Owning channel
            uint32_t        Phase;              // The current offset in the selected waveform
            uint16_t        Waveform;           // The waveform id this operator is using
            uint16_t        FreqMultTimes2;     // Frequency multiplier * 2
            int             EnvelopeStage;      // Which stage the envelope is at (see Env* enums above)
            int16_t         EnvelopeLevel;      // 0 - $1FF, 0 being the loudest
            uint16_t        OutputLevel;        // 0 - $FF
            uint16_t        AttackRate;
            uint16_t        DecayRate;
            uint16_t        SustainLevel;
            uint16_t        ReleaseRate;
            uint16_t        AttackShift;
            uint16_t        AttackMask;
            uint16_t        AttackAdd;
            const uint16_t *AttackTab;
            uint16_t        DecayShift;
            uint16_t        DecayMask;
            uint16_t        DecayAdd;
            const uint16_t *DecayTab;
            uint16_t        ReleaseShift;
            uint16_t        ReleaseMask;
            uint16_t        ReleaseAdd;
            const uint16_t *ReleaseTab;
            uint16_t        KeyScaleShift;
            uint16_t        KeyScaleLevel;
            int16_t         Out[2];
            bool            KeyOn;
            bool            KeyScaleRate;       // Affects envelope rate scaling
            bool            SustainMode;        // Whether to sustain during the sustain phase, or release instead
            bool            TremoloEnable;
            bool            VibratoEnable;
    };

    // A single channel, which can contain two or more operators
    class Channel {

        public:
                            Channel();
            void            SetMaster(Opal *opal) {  Master = opal;  }
            void            SetOperators(Operator *a, Operator *b, Operator *c, Operator *d) {
                Op[0] = a;
                Op[1] = b;
                Op[2] = c;
                Op[3] = d;
                if (a)
                    a->SetChannel(this);
                if (b)
                    b->SetChannel(this);
                if (c)
                    c->SetChannel(this);
                if (d)
                    d->SetChannel(this);
            }

            void            Output(int16_t &left, int16_t &right);
            void            SetEnable(bool on) {  Enable = on;  }
            void            SetChannelPair(Channel *pair) {  ChannelPair = pair;  }

            void            SetFrequencyLow(uint16_t freq);
            void            SetFrequencyHigh(uint16_t freq);
            void            SetKeyOn(bool on);
            void            SetOctave(uint16_t oct);
            void            SetLeftEnable(bool on);
            void            SetRightEnable(bool on);
            void            SetPan(uint8_t pan);
            void            SetFeedback(uint16_t val);
            void            SetModulationType(uint16_t type);

            uint16_t        GetFreq() const {  return Freq;  }
            uint16_t        GetOctave() const {  return Octave;  }
            uint16_t        GetKeyScaleNumber() const {  return KeyScaleNumber;  }
            uint16_t        GetModulationType() const {  return ModulationType;  }
            Channel *       GetChannelPair() const {  return ChannelPair;  } /* libADLMIDI */

            void            ComputeKeyScaleNumber();

        protected:
            void            ComputePhaseStep();

            Operator *      Op[4];

            Opal *          Master;             // Master object
            uint16_t        Freq;               // Frequency; actually it's a phase stepping value
            uint16_t        Octave;             // Also known as "block" in Yamaha parlance
            uint32_t        PhaseStep;
            uint16_t        KeyScaleNumber;
            uint16_t        FeedbackShift;
            uint16_t        ModulationType;
            Channel *       ChannelPair;
            bool            Enable;
            bool            LeftEnable, RightEnable;
            uint16_t        LeftPan, RightPan;
    };

    public:
                            Opal(int sample_rate);
                            ~Opal();

        void                SetSampleRate(int sample_rate);
        void                Port(uint16_t reg_num, uint8_t val);
        void                Pan(uint16_t reg_num, uint8_t pan);
        void                Sample(int16_t *left, int16_t *right);

    protected:
        void                Init(int sample_rate);
        void                Output(int16_t &left, int16_t &right);

        int32_t             SampleRate;
        int32_t             SampleAccum;
        int16_t             LastOutput[2], CurrOutput[2];
        Channel             Chan[NumChannels];
        Operator            Op[NumOperators];
//      uint16_t            ExpTable[256];
//      uint16_t            LogSinTable[256];
        uint16_t            Clock;
        uint16_t            TremoloClock;
        uint16_t            TremoloLevel;
        uint16_t            VibratoTick;
        uint16_t            VibratoClock;
        bool                NoteSel;
        bool                TremoloDepth;
        bool                VibratoDepth;

        static const uint16_t   RateTables[4][8];
        static const uint16_t   ExpTable[256];
        static const uint16_t   LogSinTable[256];
        static const uint16_t   PanLawTable[128];
};
//--------------------------------------------------------------------------------------------------
const uint16_t Opal::RateTables[4][8] = {
    {   1, 0, 1, 0, 1, 0, 1, 0  },
    {   1, 0, 1, 0, 0, 0, 1, 0  },
    {   1, 0, 0, 0, 1, 0, 0, 0  },
    {   1, 0, 0, 0, 0, 0, 0, 0  },
};
//--------------------------------------------------------------------------------------------------
const uint16_t Opal::ExpTable[0x100] = {
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
//--------------------------------------------------------------------------------------------------
const uint16_t Opal::LogSinTable[0x100] = {
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
//--------------------------------------------------------------------------------------------------
const uint16_t Opal::PanLawTable[128] =
{
    65535, 65529, 65514, 65489, 65454, 65409, 65354, 65289,
    65214, 65129, 65034, 64929, 64814, 64689, 64554, 64410,
    64255, 64091, 63917, 63733, 63540, 63336, 63123, 62901,
    62668, 62426, 62175, 61914, 61644, 61364, 61075, 60776,
    60468, 60151, 59825, 59489, 59145, 58791, 58428, 58057,
    57676, 57287, 56889, 56482, 56067, 55643, 55211, 54770,
    54320, 53863, 53397, 52923, 52441, 51951, 51453, 50947,
    50433, 49912, 49383, 48846, 48302, 47750, 47191,
    46340, // Center left
    46340, // Center right
    45472, 44885, 44291, 43690, 43083, 42469, 41848, 41221,
    40588, 39948, 39303, 38651, 37994, 37330, 36661, 35986,
    35306, 34621, 33930, 33234, 32533, 31827, 31116, 30400,
    29680, 28955, 28225, 27492, 26754, 26012, 25266, 24516,
    23762, 23005, 22244, 21480, 20713, 19942, 19169, 18392,
    17613, 16831, 16046, 15259, 14469, 13678, 12884, 12088,
    11291, 10492, 9691, 8888, 8085, 7280, 6473, 5666,
    4858, 4050, 3240, 2431, 1620, 810, 0
};



//==================================================================================================
// This is the temporary code for generating the above tables.  Maths and data from this nice
// reverse-engineering effort:
//
// https://docs.google.com/document/d/18IGx18NQY_Q1PJVZ-bHywao9bhsDoAqoIn1rIm42nwo/edit
//==================================================================================================
#if 0
#include <math.h>

void GenerateTables() {

    // Build the exponentiation table (reversed from the official OPL3 ROM)
    FILE *fd = fopen("exptab.txt", "wb");
    if (fd) {
        for (int i = 0; i < 0x100; i++) {
            int v = (pow(2, (0xFF - i) / 256.0) - 1) * 1024 + 0.5;
            if (i & 15)
                fprintf(fd, " %4d,", v);
            else
                fprintf(fd, "\n\t%4d,", v);
        }
        fclose(fd);
    }

    // Build the log-sin table
    fd = fopen("sintab.txt", "wb");
    if (fd) {
        for (int i = 0; i < 0x100; i++) {
            int v = -log(sin((i + 0.5) * 3.1415926535897933 / 256 / 2)) / log(2) * 256 + 0.5;
            if (i & 15)
                fprintf(fd, " %4d,", v);
            else
                fprintf(fd, "\n\t%4d,", v);
        }
        fclose(fd);
    }
}
#endif



//==================================================================================================
// Constructor/destructor.
//==================================================================================================
Opal::Opal(int sample_rate) {

    Init(sample_rate);
}
//--------------------------------------------------------------------------------------------------
Opal::~Opal() {
}



//==================================================================================================
// Initialise the emulation.
//==================================================================================================
void Opal::Init(int sample_rate) {

    Clock = 0;
    TremoloClock = 0;
    TremoloLevel = 0;
    VibratoTick = 0;
    VibratoClock = 0;
    NoteSel = false;
    TremoloDepth = false;
    VibratoDepth = false;

//  // Build the exponentiation table (reversed from the official OPL3 ROM)
//  for (int i = 0; i < 0x100; i++)
//      ExpTable[i] = (pow(2, (0xFF - i) / 256.0) - 1) * 1024 + 0.5;
//
//  // Build the log-sin table
//  for (int i = 0; i < 0x100; i++)
//      LogSinTable[i] = -log(sin((i + 0.5) * 3.1415926535897933 / 256 / 2)) / log(2) * 256 + 0.5;

    // Let sub-objects know where to find us
    for (int i = 0; i < NumOperators; i++)
        Op[i].SetMaster(this);

    for (int i = 0; i < NumChannels; i++)
        Chan[i].SetMaster(this);

    // Add the operators to the channels.  Note, some channels can't use all the operators
    // FIXME: put this into a separate routine
    const int chan_ops[] = {
        0, 1, 2, 6, 7, 8, 12, 13, 14, 18, 19, 20, 24, 25, 26, 30, 31, 32,
    };

    for (int i = 0; i < NumChannels; i++) {
        Channel *chan = &Chan[i];
        int op = chan_ops[i];
        if (i < 3 || (i >= 9 && i < 12))
            chan->SetOperators(&Op[op], &Op[op + 3], &Op[op + 6], &Op[op + 9]);
        else
            chan->SetOperators(&Op[op], &Op[op + 3], 0, 0);
    }

    // Initialise the operator rate data.  We can't do this in the Operator constructor as it
    // relies on referencing the master and channel objects
    for (int i = 0; i < NumOperators; i++)
        Op[i].ComputeRates();

    // Initialise channel panning at center.
    for (int i = 0; i < NumChannels; i++) {
        Chan[i].SetPan(64);
        Chan[i].SetLeftEnable(true);
        Chan[i].SetRightEnable(true);
    }

    SetSampleRate(sample_rate);
}



//==================================================================================================
// Change the sample rate.
//==================================================================================================
void Opal::SetSampleRate(int sample_rate) {

    // Sanity
    if (sample_rate == 0)
        sample_rate = OPL3SampleRate;

    SampleRate = sample_rate;
    SampleAccum = 0;
    LastOutput[0] = LastOutput[1] = 0;
    CurrOutput[0] = CurrOutput[1] = 0;
}



//==================================================================================================
// Write a value to an OPL3 register.
//==================================================================================================
void Opal::Port(uint16_t reg_num, uint8_t val) {

    const int op_lookup[] = {
    //  00  01  02  03  04  05  06  07  08  09  0A  0B  0C  0D  0E  0F
        0,  1,  2,  3,  4,  5,  -1, -1, 6,  7,  8,  9,  10, 11, -1, -1,
    //  10  11  12  13  14  15  16  17  18  19  1A  1B  1C  1D  1E  1F
        12, 13, 14, 15, 16, 17, -1, -1, -1, -1, -1, -1, -1, -1, -1, -1,
    };

    uint16_t type = reg_num & 0xE0;

    // Is it BD, the one-off register stuck in the middle of the register array?
    if (reg_num == 0xBD) {
        TremoloDepth = (val & 0x80) != 0;
        VibratoDepth = (val & 0x40) != 0;
        return;
    }

    // Global registers
    if (type == 0x00) {

        // 4-OP enables
        if (reg_num == 0x104) {

            // Enable/disable channels based on which 4-op enables
            uint8_t mask = 1;
            for (int i = 0; i < 6; i++, mask <<= 1) {

                // The 4-op channels are 0, 1, 2, 9, 10, 11
                uint16_t chan = i < 3 ? i : i + 6;
                Channel *primary = &Chan[chan];
                Channel *secondary = &Chan[chan + 3];

                if (val & mask) {

                    // Let primary channel know it's controlling the secondary channel
                    primary->SetChannelPair(secondary);

                    // Turn off the second channel in the pair
                    secondary->SetEnable(false);

                } else {

                    // Let primary channel know it's no longer controlling the secondary channel
                    primary->SetChannelPair(0);

                    // Turn on the second channel in the pair
                    secondary->SetEnable(true);
                }
            }

        // CSW / Note-sel
        } else if (reg_num == 0x08) {

            NoteSel = (val & 0x40) != 0;

            // Get the channels to recompute the Key Scale No. as this varies based on NoteSel
            for (int i = 0; i < NumChannels; i++)
                Chan[i].ComputeKeyScaleNumber();
        }

    // Channel registers
    } else if (type >= 0xA0 && type <= 0xC0) {

        // Convert to channel number
        int chan_num = reg_num & 15;

        // Valid channel?
        if (chan_num >= 9)
            return;

        // Is it the other bank of channels?
        if (reg_num & 0x100)
            chan_num += 9;

        Channel &chan = Chan[chan_num];

        /* libADLMIDI: registers Ax and Cx affect both channels */
        Channel *chans[2] = {&chan, chan.GetChannelPair()};
        unsigned numchans = chans[1] ? 2 : 1;

        // Do specific registers
        switch (reg_num & 0xF0) {

            // Frequency low
            case 0xA0: {
                for (unsigned i = 0; i < numchans; ++i) { /* libADLMIDI */
                    chans[i]->SetFrequencyLow(val);
                }
                break;
            }

            // Key-on / Octave / Frequency High
            case 0xB0: {
                for (unsigned i = 0; i < numchans; ++i) { /* libADLMIDI */
                    chans[i]->SetKeyOn((val & 0x20) != 0);
                    chans[i]->SetOctave(val >> 2 & 7);
                    chans[i]->SetFrequencyHigh(val & 3);
                }
                break;
            }

            // Right Stereo Channel Enable / Left Stereo Channel Enable / Feedback Factor / Modulation Type
            case 0xC0: {
                chan.SetRightEnable((val & 0x20) != 0);
                chan.SetLeftEnable((val & 0x10) != 0);
                chan.SetFeedback(val >> 1 & 7);
                chan.SetModulationType(val & 1);
                break;
            }
        }

    // Operator registers
    } else if ((type >= 0x20 && type <= 0x80) || type == 0xE0) {

        // Convert to operator number
        int op_num = op_lookup[reg_num & 0x1F];

        // Valid register?
        if (op_num < 0)
            return;

        // Is it the other bank of operators?
        if (reg_num & 0x100)
            op_num += 18;

        Operator &op = Op[op_num];

        // Do specific registers
        switch (type) {

            // Tremolo Enable / Vibrato Enable / Sustain Mode / Envelope Scaling / Frequency Multiplier
            case 0x20: {
                op.SetTremoloEnable((val & 0x80) != 0);
                op.SetVibratoEnable((val & 0x40) != 0);
                op.SetSustainMode((val & 0x20) != 0);
                op.SetEnvelopeScaling((val & 0x10) != 0);
                op.SetFrequencyMultiplier(val & 15);
                break;
            }

            // Key Scale / Output Level
            case 0x40: {
                op.SetKeyScale(val >> 6);
                op.SetOutputLevel(val & 0x3F);
                break;
            }

            // Attack Rate / Decay Rate
            case 0x60: {
                op.SetAttackRate(val >> 4);
                op.SetDecayRate(val & 15);
                break;
            }

            // Sustain Level / Release Rate
            case 0x80: {
                op.SetSustainLevel(val >> 4);
                op.SetReleaseRate(val & 15);
                break;
            }

            // Waveform
            case 0xE0: {
                op.SetWaveform(val & 7);
                break;
            }
        }
    }
}



//==================================================================================================
// Set panning on the channel designated by the register number.
// This is extended functionality.
//==================================================================================================
void Opal::Pan(uint16_t reg_num, uint8_t pan)
{
    uint8_t high = (reg_num >> 8) & 1;
    uint8_t regm = reg_num & 0xff;
    Chan[9 * high + (regm & 0x0f)].SetPan(pan);
}



//==================================================================================================
// Generate sample.  Every time you call this you will get two signed 16-bit samples (one for each
// stereo channel) which will sound correct when played back at the sample rate given when the
// class was constructed.
//==================================================================================================
void Opal::Sample(int16_t *left, int16_t *right) {

    // If the destination sample rate is higher than the OPL3 sample rate, we need to skip ahead
    while (SampleAccum >= SampleRate) {

        LastOutput[0] = CurrOutput[0];
        LastOutput[1] = CurrOutput[1];

        Output(CurrOutput[0], CurrOutput[1]);

        SampleAccum -= SampleRate;
    }

    // Mix with the partial accumulation
    int32_t omblend = SampleRate - SampleAccum;
    *left = (LastOutput[0] * omblend + CurrOutput[0] * SampleAccum) / SampleRate;
    *right = (LastOutput[1] * omblend + CurrOutput[1] * SampleAccum) / SampleRate;

    SampleAccum += OPL3SampleRate;
}



//==================================================================================================
// Produce final output from the chip.  This is at the OPL3 sample-rate.
//==================================================================================================
void Opal::Output(int16_t &left, int16_t &right) {

    int32_t leftmix = 0, rightmix = 0;

    // Sum the output of each channel
    for (int i = 0; i < NumChannels; i++) {

        int16_t chanleft, chanright;
        Chan[i].Output(chanleft, chanright);

        leftmix += chanleft;
        rightmix += chanright;
    }

    // Clamp
    if (leftmix < -0x8000)
        left = -0x8000;
    else if (leftmix > 0x7FFF)
        left = 0x7FFF;
    else
        left = leftmix;

    if (rightmix < -0x8000)
        right = -0x8000;
    else if (rightmix > 0x7FFF)
        right = 0x7FFF;
    else
        right = rightmix;

    Clock++;

    // Tremolo.  According to this post, the OPL3 tremolo is a 13,440 sample length triangle wave
    // with a peak at 26 and a trough at 0 and is simply added to the logarithmic level accumulator
    //      http://forums.submarine.org.uk/phpBB/viewtopic.php?f=9&t=1171
    TremoloClock = (TremoloClock + 1) % 13440;
    TremoloLevel = ((TremoloClock < 13440 / 2) ? TremoloClock : 13440 - TremoloClock) / 256;
    if (!TremoloDepth)
        TremoloLevel >>= 2;

    // Vibrato.  This appears to be a 8 sample long triangle wave with a magnitude of the three
    // high bits of the channel frequency, positive and negative, divided by two if the vibrato
    // depth is zero.  It is only cycled every 1,024 samples.
    VibratoTick++;
    if (VibratoTick >= 1024) {
        VibratoTick = 0;
        VibratoClock = (VibratoClock + 1) & 7;
    }
}



//==================================================================================================
// Channel constructor.
//==================================================================================================
Opal::Channel::Channel() {

    Master = 0;
    Freq = 0;
    Octave = 0;
    PhaseStep = 0;
    KeyScaleNumber = 0;
    FeedbackShift = 0;
    ModulationType = 0;
    ChannelPair = 0;
    Enable = true;
}



//==================================================================================================
// Produce output from channel.
//==================================================================================================
void Opal::Channel::Output(int16_t &left, int16_t &right) {

    // Has the channel been disabled?  This is usually a result of the 4-op enables being used to
    // disable the secondary channel in each 4-op pair
    if (!Enable) {
        left = right = 0;
        return;
    }

    int16_t vibrato = (Freq >> 7) & 7;
    if (!Master->VibratoDepth)
        vibrato >>= 1;

    // 0  3  7  3  0  -3  -7  -3
    uint16_t clk = Master->VibratoClock;
    if (!(clk & 3))
        vibrato = 0;                // Position 0 and 4 is zero
    else {
        if (clk & 1)
            vibrato >>= 1;          // Odd positions are half the magnitude
        if (clk & 4)
            vibrato = -vibrato;     // The second half positions are negative
    }

    vibrato <<= Octave;

    // Combine individual operator outputs
    int16_t out, acc;

    // Running in 4-op mode?
    if (ChannelPair) {

        // Get the secondary channel's modulation type.  This is the only thing from the secondary
        // channel that is used
        if (ChannelPair->GetModulationType() == 0) {

            if (ModulationType == 0) {

                // feedback -> modulator -> modulator -> modulator -> carrier
                out  = Op[0]->Output(KeyScaleNumber, PhaseStep, vibrato, 0, FeedbackShift);
                out  = Op[1]->Output(KeyScaleNumber, PhaseStep, vibrato, out, 0);
                out  = Op[2]->Output(KeyScaleNumber, PhaseStep, vibrato, out, 0);
                out  = Op[3]->Output(KeyScaleNumber, PhaseStep, vibrato, out, 0);

            } else {

                // (feedback -> carrier) + (modulator -> modulator -> carrier)
                out  = Op[0]->Output(KeyScaleNumber, PhaseStep, vibrato, 0, FeedbackShift);
                acc  = Op[1]->Output(KeyScaleNumber, PhaseStep, vibrato, 0, 0);
                acc  = Op[2]->Output(KeyScaleNumber, PhaseStep, vibrato, acc, 0);
                out += Op[3]->Output(KeyScaleNumber, PhaseStep, vibrato, acc, 0);
            }

        } else {

            if (ModulationType == 0) {

                // (feedback -> modulator -> carrier) + (modulator -> carrier)
                out  = Op[0]->Output(KeyScaleNumber, PhaseStep, vibrato, 0, FeedbackShift);
                out  = Op[1]->Output(KeyScaleNumber, PhaseStep, vibrato, out, 0);
                acc  = Op[2]->Output(KeyScaleNumber, PhaseStep, vibrato, 0, 0);
                out += Op[3]->Output(KeyScaleNumber, PhaseStep, vibrato, acc, 0);

            } else {

                // (feedback -> carrier) + (modulator -> carrier) + carrier
                out  = Op[0]->Output(KeyScaleNumber, PhaseStep, vibrato, 0, FeedbackShift);
                acc  = Op[1]->Output(KeyScaleNumber, PhaseStep, vibrato, 0, 0);
                out += Op[2]->Output(KeyScaleNumber, PhaseStep, vibrato, acc, 0);
                out += Op[3]->Output(KeyScaleNumber, PhaseStep, vibrato, 0, 0);
            }
        }

    } else {

        // Standard 2-op mode
        if (ModulationType == 0) {

            // Frequency modulation (well, phase modulation technically)
            out = Op[0]->Output(KeyScaleNumber, PhaseStep, vibrato, 0, FeedbackShift);
            out = Op[1]->Output(KeyScaleNumber, PhaseStep, vibrato, out, 0);

        } else {

            // Additive
            out = Op[0]->Output(KeyScaleNumber, PhaseStep, vibrato, 0, FeedbackShift);
            out += Op[1]->Output(KeyScaleNumber, PhaseStep, vibrato);
        }
    }

    left = LeftEnable ? out : 0;
    right = RightEnable ? out : 0;

    left = left * LeftPan / 65536;
    right = right * RightPan / 65536;
}



//==================================================================================================
// Set phase step for operators using this channel.
//==================================================================================================
void Opal::Channel::SetFrequencyLow(uint16_t freq) {

    Freq = (Freq & 0x300) | (freq & 0xFF);
    ComputePhaseStep();
}
//--------------------------------------------------------------------------------------------------
void Opal::Channel::SetFrequencyHigh(uint16_t freq) {

    Freq = (Freq & 0xFF) | ((freq & 3) << 8);
    ComputePhaseStep();

    // Only the high bits of Freq affect the Key Scale No.
    ComputeKeyScaleNumber();
}



//==================================================================================================
// Set the octave of the channel (0 to 7).
//==================================================================================================
void Opal::Channel::SetOctave(uint16_t oct) {

    Octave = oct & 7;
    ComputePhaseStep();
    ComputeKeyScaleNumber();
}



//==================================================================================================
// Keys the channel on/off.
//==================================================================================================
void Opal::Channel::SetKeyOn(bool on) {

    Op[0]->SetKeyOn(on);
    Op[1]->SetKeyOn(on);
}



//==================================================================================================
// Enable left stereo channel.
//==================================================================================================
void Opal::Channel::SetLeftEnable(bool on) {

    LeftEnable = on;
}



//==================================================================================================
// Enable right stereo channel.
//==================================================================================================
void Opal::Channel::SetRightEnable(bool on) {

    RightEnable = on;
}



//==================================================================================================
// Pan the channel to the position given.
//==================================================================================================
void Opal::Channel::SetPan(uint8_t pan)
{
    pan &= 127;
    LeftPan = PanLawTable[pan];
    RightPan = PanLawTable[127 - pan];
}



//==================================================================================================
// Set the channel feedback amount.
//==================================================================================================
void Opal::Channel::SetFeedback(uint16_t val) {

    FeedbackShift = val ? 9 - val : 0;
}



//==================================================================================================
// Set frequency modulation/additive modulation
//==================================================================================================
void Opal::Channel::SetModulationType(uint16_t type) {

    ModulationType = type;
}



//==================================================================================================
// Compute the stepping factor for the operator waveform phase based on the frequency and octave
// values of the channel.
//==================================================================================================
void Opal::Channel::ComputePhaseStep() {

    PhaseStep = uint32_t(Freq) << Octave;
}



//==================================================================================================
// Compute the key scale number and key scale levels.
//
// From the Yamaha data sheet this is the block/octave number as bits 3-1, with bit 0 coming from
// the MSB of the frequency if NoteSel is 1, and the 2nd MSB if NoteSel is 0.
//==================================================================================================
void Opal::Channel::ComputeKeyScaleNumber() {

    uint16_t lsb = Master->NoteSel ? Freq >> 9 : (Freq >> 8) & 1;
    KeyScaleNumber = Octave << 1 | lsb;

    // Get the channel operators to recompute their rates as they're dependent on this number.  They
    // also need to recompute their key scale level
    for (int i = 0; i < 4; i++) {

        if (!Op[i])
            continue;

        Op[i]->ComputeRates();
        Op[i]->ComputeKeyScaleLevel();
    }
}



//==================================================================================================
// Operator constructor.
//==================================================================================================
Opal::Operator::Operator() {

    Master = 0;
    Chan = 0;
    Phase = 0;
    Waveform = 0;
    FreqMultTimes2 = 1;
    EnvelopeStage = EnvOff;
    EnvelopeLevel = 0x1FF;
    AttackRate = 0;
    DecayRate = 0;
    SustainLevel = 0;
    ReleaseRate = 0;
    KeyScaleShift = 0;
    KeyScaleLevel = 0;
    Out[0] = Out[1] = 0;
    KeyOn = false;
    KeyScaleRate = false;
    SustainMode = false;
    TremoloEnable = false;
    VibratoEnable = false;
}



//==================================================================================================
// Produce output from operator.
//==================================================================================================
int16_t Opal::Operator::Output(uint16_t /*keyscalenum*/, uint32_t phase_step, int16_t vibrato, int16_t mod, int16_t fbshift) {

    // Advance wave phase
    if (VibratoEnable)
        phase_step += vibrato;
    Phase += (phase_step * FreqMultTimes2) / 2;

    uint16_t level = (EnvelopeLevel + OutputLevel + KeyScaleLevel + (TremoloEnable ? Master->TremoloLevel : 0)) << 3;

    switch (EnvelopeStage) {

        // Attack stage
        case EnvAtt: {
            uint16_t add = ((AttackAdd >> AttackTab[Master->Clock >> AttackShift & 7]) * ~EnvelopeLevel) >> 3;
            if (AttackRate == 0)
                add = 0;
            if (AttackMask && (Master->Clock & AttackMask))
                add = 0;
            EnvelopeLevel += add;
            if (EnvelopeLevel <= 0) {
                EnvelopeLevel = 0;
                EnvelopeStage = EnvDec;
            }
            break;
        }

        // Decay stage
        case EnvDec: {
            uint16_t add = DecayAdd >> DecayTab[Master->Clock >> DecayShift & 7];
            if (DecayRate == 0)
                add = 0;
            if (DecayMask && (Master->Clock & DecayMask))
                add = 0;
            EnvelopeLevel += add;
            if (EnvelopeLevel >= SustainLevel) {
                EnvelopeLevel = SustainLevel;
                EnvelopeStage = EnvSus;
            }
            break;
        }

        // Sustain stage
        case EnvSus: {
            if (SustainMode)
                break;
            // Note: fall-through!

        }//fallthrough

        // Release stage
        case EnvRel: {
            uint16_t add = ReleaseAdd >> ReleaseTab[Master->Clock >> ReleaseShift & 7];
            if (ReleaseRate == 0)
                add = 0;
            if (ReleaseMask && (Master->Clock & ReleaseMask))
                add = 0;
            EnvelopeLevel += add;
            if (EnvelopeLevel >= 0x1FF) {
                EnvelopeLevel = 0x1FF;
                EnvelopeStage = EnvOff;
                Out[0] = Out[1] = 0;
                return 0;
            }
            break;
        }

        // Envelope, and therefore the operator, is not running
        default:
            Out[0] = Out[1] = 0;
            return 0;
    }

    // Feedback?  In that case we modulate by a blend of the last two samples
    if (fbshift)
        mod += (Out[0] + Out[1]) >> fbshift;

    uint16_t phase = (Phase >> 10) + mod;
    uint16_t offset = phase & 0xFF;
    uint16_t logsin;
    bool negate = false;

    switch (Waveform) {

        //------------------------------------
        // Standard sine wave
        //------------------------------------
        case 0:
            if (phase & 0x100)
                offset ^= 0xFF;
            logsin = Master->LogSinTable[offset];
            negate = (phase & 0x200) != 0;
            break;

        //------------------------------------
        // Half sine wave
        //------------------------------------
        case 1:
            if (phase & 0x200)
                offset = 0;
            else if (phase & 0x100)
                offset ^= 0xFF;
            logsin = Master->LogSinTable[offset];
            break;

        //------------------------------------
        // Positive sine wave
        //------------------------------------
        case 2:
            if (phase & 0x100)
                offset ^= 0xFF;
            logsin =  Master->LogSinTable[offset];
            break;

        //------------------------------------
        // Quarter positive sine wave
        //------------------------------------
        case 3:
            if (phase & 0x100)
                offset = 0;
            logsin =  Master->LogSinTable[offset];
            break;

        //------------------------------------
        // Double-speed sine wave
        //------------------------------------
        case 4:
            if (phase & 0x200)
                offset = 0;

            else {

                if (phase & 0x80)
                    offset ^= 0xFF;

                offset = (offset + offset) & 0xFF;
                negate = (phase & 0x100) != 0;
            }

            logsin =  Master->LogSinTable[offset];
            break;

        //------------------------------------
        // Double-speed positive sine wave
        //------------------------------------
        case 5:
            if (phase & 0x200)
                offset = 0;

            else {

                offset = (offset + offset) & 0xFF;
                if (phase & 0x80)
                    offset ^= 0xFF;
            }

            logsin =  Master->LogSinTable[offset];
            break;

        //------------------------------------
        // Square wave
        //------------------------------------
        case 6:
            logsin = 0;
            negate = (phase & 0x200) != 0;
            break;

        //------------------------------------
        // Exponentiation wave
        //------------------------------------
        default:
            logsin = phase & 0x1FF;
            if (phase & 0x200) {
                logsin ^= 0x1FF;
                negate = true;
            }
            logsin <<= 3;
            break;
    }

    uint16_t mix = logsin + level;
    if (mix > 0x1FFF)
        mix = 0x1FFF;

    // From the OPLx decapsulated docs:
    // "When such a table is used for calculation of the exponential, the table is read at the
    // position given by the 8 LSB's of the input. The value + 1024 (the hidden bit) is then the
    // significand of the floating point output and the yet unused MSB's of the input are the
    // exponent of the floating point output."
    int16_t v = Master->ExpTable[mix & 0xFF] + 1024;
    v >>= mix >> 8;
    v += v;
    if (negate)
        v = ~v;

    // Keep last two results for feedback calculation
    Out[1] = Out[0];
    Out[0] = v;

    return v;
}



//==================================================================================================
// Trigger operator.
//==================================================================================================
void Opal::Operator::SetKeyOn(bool on) {

    // Already on/off?
    if (KeyOn == on)
        return;
    KeyOn = on;

    if (on) {

        // The highest attack rate is instant; it bypasses the attack phase
        if (AttackRate == 15) {
            EnvelopeStage = EnvDec;
            EnvelopeLevel = 0;
        } else
            EnvelopeStage = EnvAtt;

        Phase = 0;

    } else {

        // Stopping current sound?
        if (EnvelopeStage != EnvOff && EnvelopeStage != EnvRel)
            EnvelopeStage = EnvRel;
    }
}



//==================================================================================================
// Enable amplitude vibrato.
//==================================================================================================
void Opal::Operator::SetTremoloEnable(bool on) {

    TremoloEnable = on;
}



//==================================================================================================
// Enable frequency vibrato.
//==================================================================================================
void Opal::Operator::SetVibratoEnable(bool on) {

    VibratoEnable = on;
}



//==================================================================================================
// Sets whether we release or sustain during the sustain phase of the envelope.  'true' is to
// sustain, otherwise release.
//==================================================================================================
void Opal::Operator::SetSustainMode(bool on) {

    SustainMode = on;
}



//==================================================================================================
// Key scale rate.  Sets how much the Key Scaling Number affects the envelope rates.
//==================================================================================================
void Opal::Operator::SetEnvelopeScaling(bool on) {

    KeyScaleRate = on;
    ComputeRates();
}



//==================================================================================================
// Multiplies the phase frequency.
//==================================================================================================
void Opal::Operator::SetFrequencyMultiplier(uint16_t scale) {

    // Needs to be multiplied by two (and divided by two later when we use it) because the first
    // entry is actually .5
    const uint16_t mul_times_2[] = {
        1, 2, 4, 6, 8, 10, 12, 14, 16, 18, 20, 20, 24, 24, 30, 30,
    };

    FreqMultTimes2 = mul_times_2[scale & 15];
}



//==================================================================================================
// Attenuates output level towards higher pitch.
//==================================================================================================
void Opal::Operator::SetKeyScale(uint16_t scale) {

    /* libADLMIDI: KSL computation fix */
    const unsigned KeyScaleShiftTable[4] = {8, 1, 2, 0};
    KeyScaleShift = KeyScaleShiftTable[scale];

    ComputeKeyScaleLevel();
}



//==================================================================================================
// Sets the output level (volume) of the operator.
//==================================================================================================
void Opal::Operator::SetOutputLevel(uint16_t level) {

    OutputLevel = level * 4;
}



//==================================================================================================
// Operator attack rate.
//==================================================================================================
void Opal::Operator::SetAttackRate(uint16_t rate) {

    AttackRate = rate;

    ComputeRates();
}



//==================================================================================================
// Operator decay rate.
//==================================================================================================
void Opal::Operator::SetDecayRate(uint16_t rate) {

    DecayRate = rate;

    ComputeRates();
}



//==================================================================================================
// Operator sustain level.
//==================================================================================================
void Opal::Operator::SetSustainLevel(uint16_t level) {

    SustainLevel = level < 15 ? level : 31;
    SustainLevel *= 16;
}



//==================================================================================================
// Operator release rate.
//==================================================================================================
void Opal::Operator::SetReleaseRate(uint16_t rate) {

    ReleaseRate = rate;

    ComputeRates();
}



//==================================================================================================
// Assign the waveform this operator will use.
//==================================================================================================
void Opal::Operator::SetWaveform(uint16_t wave) {

    Waveform = wave & 7;
}



//==================================================================================================
// Compute actual rate from register rate.  From the Yamaha data sheet:
//
// Actual rate = Rate value * 4 + Rof, if Rate value = 0, actual rate = 0
//
// Rof is set as follows depending on the KSR setting:
//
//  Key scale   0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15
//  KSR = 0     0   0   0   0   1   1   1   1   2   2   2   2   3   3   3   3
//  KSR = 1     0   1   2   3   4   5   6   7   8   9   10  11  12  13  14  15
//
// Note: zero rates are infinite, and are treated separately elsewhere
//==================================================================================================
void Opal::Operator::ComputeRates() {

    int combined_rate = AttackRate * 4 + (Chan->GetKeyScaleNumber() >> (KeyScaleRate ? 0 : 2));
    int rate_high = combined_rate >> 2;
    int rate_low = combined_rate & 3;

    AttackShift = rate_high < 12 ? 12 - rate_high : 0;
    AttackMask = (1 << AttackShift) - 1;
    AttackAdd = (rate_high < 12) ? 1 : 1 << (rate_high - 12);
    AttackTab = Master->RateTables[rate_low];

    // Attack rate of 15 is always instant
    if (AttackRate == 15)
        AttackAdd = 0xFFF;

    combined_rate = DecayRate * 4 + (Chan->GetKeyScaleNumber() >> (KeyScaleRate ? 0 : 2));
    rate_high = combined_rate >> 2;
    rate_low = combined_rate & 3;

    DecayShift = rate_high < 12 ? 12 - rate_high : 0;
    DecayMask = (1 << DecayShift) - 1;
    DecayAdd = (rate_high < 12) ? 1 : 1 << (rate_high - 12);
    DecayTab = Master->RateTables[rate_low];

    combined_rate = ReleaseRate * 4 + (Chan->GetKeyScaleNumber() >> (KeyScaleRate ? 0 : 2));
    rate_high = combined_rate >> 2;
    rate_low = combined_rate & 3;

    ReleaseShift = rate_high < 12 ? 12 - rate_high : 0;
    ReleaseMask = (1 << ReleaseShift) - 1;
    ReleaseAdd = (rate_high < 12) ? 1 : 1 << (rate_high - 12);
    ReleaseTab = Master->RateTables[rate_low];
}



//==================================================================================================
// Compute the operator's key scale level.  This changes based on the channel frequency/octave and
// operator key scale value.
//==================================================================================================
void Opal::Operator::ComputeKeyScaleLevel() {

    static const uint16_t levtab[] = {
        0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,      0,
        0,      0,      0,      0,      0,      0,      0,      0,      0,      8,      12,     16,     20,     24,     28,     32,
        0,      0,      0,      0,      0,      12,     20,     28,     32,     40,     44,     48,     52,     56,     60,     64,
        0,      0,      0,      20,     32,     44,     52,     60,     64,     72,     76,     80,     84,     88,     92,     96,
        0,      0,      32,     52,     64,     76,     84,     92,     96,     104,    108,    112,    116,    120,    124,    128,
        0,      32,     64,     84,     96,     108,    116,    124,    128,    136,    140,    144,    148,    152,    156,    160,
        0,      64,     96,     116,    128,    140,    148,    156,    160,    168,    172,    176,    180,    184,    188,    192,
        0,      96,     128,    148,    160,    172,    180,    188,    192,    200,    204,    208,    212,    216,    220,    224,
    };

    // This uses a combined value of the top four bits of frequency with the octave/block
    uint16_t i = (Chan->GetOctave() << 4) | (Chan->GetFreq() >> 6);
    KeyScaleLevel = levtab[i] >> KeyScaleShift;
}
