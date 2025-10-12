#ifndef FM_OP_H
#define FM_OP_H

#include <stdint.h>
#define false 0
#define true 1
#define Max(a,b) ((a>b)?a:b)
#define Min(a,b) ((a<b)?a:b)

/*  Types ---------------------------------------------------------------- */
typedef int32_t         Sample;
typedef int32_t         ISample;

/* ---------------------------------------------------------------------------
// Various implementation-specific constants.
// Most are used to either define the bit-width of counters, or the size
// of various constant tables.
*/
#define FM_LFOBITS      8
#define FM_LFOENTS      (1 << FM_LFOBITS)
#define FM_OPSINBITS    10
#define FM_OPSINENTS    (1 << FM_OPSINBITS)
#define FM_EGBITS       16
#define FM_EGCBITS      18
#define FM_LFOCBITS     14
#define FM_PGBITS       9
#define FM_RATIOBITS    12

typedef enum _EGPhase { next, attack, decay, sustain, release, off } EGPhase;

static inline int Limit(int v, int max, int min)
{
    return v > max ? max : (v < min ? min : v);
}

static inline int16_t Limit16(int a)
{
    if ((a+0x8000) & ~0xFFFF) return (a>>31) ^ 0x7FFF;
    else                      return a;
}

/*  ------------------------------------------------------------------------- */
struct _OPNA;
struct Channel4;

/*  Operator ---------------------------------------------------------------- */
typedef struct _FMOperator
{
    struct Channel4 *master;

    int32_t out, out2;

    /*  Phase Generator ----------------------------------------------------- */
    uint32_t dp;        /* Octave (used to define note in conjunction with bn, below). */
    uint8_t detune;     /* Detune */
    uint8_t multiple;   /* Multiple */
    uint32_t    pgcount;    /* Phase generator sweep value. Only the top 9 bits are relevant/used. */
    uint32_t    pgdcount;   /* Phase generator increment-per-clock value. Hopefully self-explanatory. */
    uint32_t    pgdcountl;  /* Phase generator detune increment value. Used in the implementation of vibrato. */
                            /* Equal to pgdcount >> 11. */

    /*  Envelope Generator -------------------------------------------------- */
    uint32_t bn;         /* Block/Note */
    uint32_t egout;
    int      eglevel;    /* EG ¤Î½ÐÎÏÃÍ */
    int      eglvnext;   /* ¼¡¤Î phase ¤Ë°Ü¤ëÃÍ */
    int32_t  egstep;     /* EG ¤Î¼¡¤ÎÊÑ°Ü¤Þ¤Ç¤Î»þ´Ö */
    int32_t  egstepd;    /* egstep ¤Î»þ´Öº¹Ê¬ */
    uint8_t  egtransa;   /* EG ÊÑ²½¤Î³ä¹ç (for attack) */
    uint8_t  egtransd;   /* EG ÊÑ²½¤Î³ä¹ç (for decay) */

    uint32_t ksr;        /* key scale rate */
    EGPhase phase;
    uint8_t ams;
    uint8_t ms;

    uint8_t keyon;      /* current key state */

    uint8_t tl;         /* Total Level   (0-127) */
    uint8_t tll;        /* Total Level Latch (for CSM mode) */
    uint8_t ar;         /* Attack Rate   (0-63) */
    uint8_t dr;         /* Decay Rate    (0-63) */
    uint8_t sr;         /* Sustain Rate  (0-63) */
    uint8_t sl;         /* Sustain Level (0-127) */
    uint8_t rr;         /* Release Rate  (0-63) */
    uint8_t ks;         /* Keyscale      (0-3) */
    uint8_t ssgtype;    /* SSG-Type Envelope Control */

    uint8_t amon;       /* enable Amplitude Modulation */
    uint8_t paramchanged;   /* Set whenever f-number or any ADSR constants */
                            /* are set in OPNASetReg(), as well as upon */
                            /* chip reset and chip "DAC" samplerate change. */
                            /* Causes the envelope generator to reset its */
                            /* internal state, also sets correct increments */
                            /* for the phase generator. */
    uint8_t mute;
} FMOperator;

/*  4-op Channel ------------------------------------------------------------ */
typedef struct Channel4
{
    struct _OPNA *master;
    uint32_t fb;
    int     buf[4];
    uint8_t idx[6];
    int *pms;
    uint16_t panl;
    uint16_t panr;
    FMOperator op[4];
} Channel4;

/*  OPNA Rhythm Generator --------------------------------------------------- */
typedef struct Rhythm {
    uint8_t pan;
    int8_t  level;
    int8_t  volume;
    int8_t*     sample;     /* Rhythm sample data */
    uint32_t    size;       /* Rhythm sample data size */
    uint32_t    pos;        /* Current index into rhytm sample data array */
    uint32_t    step;       /* Amount to increment the above by every time */
                            /* RhythmMix() gets called. */
    uint32_t    rate;       /* Samplerate of rhythm sample data */
                            /* (44100Hz in this implementation). */
} Rhythm;

#ifdef __cplusplus
extern "C" {
#endif

/* --------------------------------------------------------------------------
// Miscellaneous and probably irrelevant function prototypes. */
void OperatorInit(Channel4 *ch4, FMOperator *op);
void OperatorReset(FMOperator *op);
void OperatorPrepare(FMOperator *op);

static inline uint32_t IsOn(FMOperator *op) {
    return (op->phase - off);
}

#ifdef __cplusplus
}
#endif

#endif
