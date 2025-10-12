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

#ifndef OPAL_HHHH
#define OPAL_HHHH

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define OPAL_TRUE 1
#define OPAL_FALSE 0
typedef int opal_bool;

typedef struct OpalChannel_t OpalChannel;
typedef struct OpalOperator_t OpalOperator;
typedef struct Opal_t Opal;

/* Various constants */
typedef enum OpalEnum_t
{
    OpalOPL3SampleRate      = 49716,
    OpalNumChannels         = 18,
    OpalNumOperators        = 36
} OpalEnum;

/* A single FM operator */
struct OpalOperator_t
{
    Opal*           Master;             /* Master object */
    OpalChannel*    Chan;               /* Owning channel */
    uint32_t        Phase;              /* The current offset in the selected waveform */
    uint16_t        Waveform;           /* The waveform id this operator is using */
    uint16_t        FreqMultTimes2;     /* Frequency multiplier * 2 */
    int             EnvelopeStage;      /* Which stage the envelope is at (see Env* enums above) */
    int16_t         EnvelopeLevel;      /* 0 - $1FF, 0 being the loudest */
    uint16_t        OutputLevel;        /* 0 - $FF */
    uint16_t        AttackRate;
    uint16_t        DecayRate;
    uint16_t        SustainLevel;
    uint16_t        ReleaseRate;
    uint16_t        AttackShift;
    uint16_t        AttackMask;
    uint16_t        AttackAdd;
    const uint16_t* AttackTab;
    uint16_t        DecayShift;
    uint16_t        DecayMask;
    uint16_t        DecayAdd;
    const uint16_t* DecayTab;
    uint16_t        ReleaseShift;
    uint16_t        ReleaseMask;
    uint16_t        ReleaseAdd;
    const uint16_t* ReleaseTab;
    uint16_t        KeyScaleShift;
    uint16_t        KeyScaleLevel;
    int16_t         Out[2];
    opal_bool       KeyOn;
    opal_bool       KeyScaleRate;       /* Affects envelope rate scaling */
    opal_bool       SustainMode;        /* Whether to sustain during the sustain phase, or release instead */
    opal_bool       TremoloEnable;
    opal_bool       VibratoEnable;
};

/* A single channel, which can contain two or more operators */
struct OpalChannel_t
{
    OpalOperator*   Op[4];

    Opal*           Master;             /* Master object */
    uint16_t        Freq;               /* Frequency; actually it's a phase stepping value */
    uint16_t        Octave;             /* Also known as "block" in Yamaha parlance */
    uint32_t        PhaseStep;
    uint16_t        KeyScaleNumber;
    uint16_t        FeedbackShift;
    uint16_t        ModulationType;
    OpalChannel*    ChannelPair;
    opal_bool       Enable;
    opal_bool       LeftEnable, RightEnable;
    uint16_t        LeftPan, RightPan;
};


/*==================================================================================================
            Opal instance.
 ==================================================================================================*/
struct Opal_t
{
    int32_t             SampleRate;
    int32_t             SampleAccum;
    int16_t             LastOutput[2], CurrOutput[2];
    OpalChannel         Chan[OpalNumChannels];
    OpalOperator        Op[OpalNumOperators];
    uint16_t            Clock;
    uint16_t            TremoloClock;
    uint16_t            TremoloLevel;
    uint16_t            VibratoTick;
    uint16_t            VibratoClock;
    opal_bool           NoteSel;
    opal_bool           TremoloDepth;
    opal_bool           VibratoDepth;
};


/*==================================================================================================
            Public API.
 ==================================================================================================*/
/*!
 * \brief Initialize Opal with a given output sample rate
 * \param self Pointer to the Opal instance
 * \param sample_rate Desired output sample rate
 */
extern void Opal_Init(Opal *self, int sample_rate);

/*!
 * \brief Change the output sample rate of existing initialized instance
 * \param self Pointer to the Opal instance
 * \param sample_rate Desired output sample rate
 */
extern void Opal_SetSampleRate(Opal *self, int sample_rate);

/*!
 * \brief Send the register data to the Opal
 * \param self Pointer to the Opal instance
 * \param reg_num OPL3 Register address
 * \param val Value to write
 */
extern void Opal_Port(Opal *self, uint16_t reg_num, uint8_t val);

/*!
 * \brief Panorama level per channel
 * \param self Pointer to the Opal instance
 * \param reg_num Channel number (multiplied by 256)
 * \param pan Panorama level (0 - left, 64 - middle, 127 - right)
 */
extern void Opal_Pan(Opal *self, uint16_t reg_num, uint8_t pan);

/*!
 * \brief Generate one 16-bit PCM sample in system endian (there are two integers will be written)
 * \param self Pointer to the Opal instance
 * \param left Sample for the left chanel
 * \param right Sample for the right channel
 */
extern void Opal_Sample(Opal *self, int16_t* left, int16_t* right);


#ifdef __cplusplus
}
#endif

#endif /* OPAL_HHHH */
