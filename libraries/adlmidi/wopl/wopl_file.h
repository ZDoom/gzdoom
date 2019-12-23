/*
 * Wohlstand's OPL3 Bank File - a bank format to store OPL3 timbre data and setup
 *
 * Copyright (c) 2015-2018 Vitaly Novichkov <admin@wohlnet.ru>
 *
 * Permission is hereby granted, free of charge, to any person obtaining
 * a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the
 * rights to use, copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER
 * DEALINGS IN THE SOFTWARE.
 */

#ifndef WOPL_FILE_H
#define WOPL_FILE_H

#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if !defined(__STDC_VERSION__) || (defined(__STDC_VERSION__) && (__STDC_VERSION__ < 199901L)) \
  || defined(__STRICT_ANSI__) || !defined(__cplusplus)
typedef signed char int8_t;
typedef unsigned char uint8_t;
typedef signed short int int16_t;
typedef unsigned short int uint16_t;
#endif

/* Global OPL flags */
typedef enum WOPLFileFlags
{
    /* Enable Deep-Tremolo flag */
    WOPL_FLAG_DEEP_TREMOLO = 0x01,
    /* Enable Deep-Vibrato flag */
    WOPL_FLAG_DEEP_VIBRATO = 0x02
} WOPLFileFlags;

/* Volume scaling model implemented in the libADLMIDI */
typedef enum WOPL_VolumeModel
{
    WOPL_VM_Generic = 0,
    WOPL_VM_Native,
    WOPL_VM_DMX,
    WOPL_VM_Apogee,
    WOPL_VM_Win9x
} WOPL_VolumeModel;

typedef enum WOPL_InstrumentFlags
{
    /* Is two-operator single-voice instrument (no flags) */
    WOPL_Ins_2op        = 0x00,
    /* Is true four-operator instrument */
    WOPL_Ins_4op        = 0x01,
    /* Is pseudo four-operator (two 2-operator voices) instrument */
    WOPL_Ins_Pseudo4op  = 0x02,
    /* Is a blank instrument entry */
    WOPL_Ins_IsBlank    = 0x04,

    /* RythmMode flags mask */
    WOPL_RhythmModeMask  = 0x38,

    /* Mask of the flags range */
    WOPL_Ins_ALL_MASK   = 0x07
} WOPL_InstrumentFlags;

typedef enum WOPL_RhythmMode
{
    /* RythmMode: BassDrum */
    WOPL_RM_BassDrum  = 0x08,
    /* RythmMode: Snare */
    WOPL_RM_Snare     = 0x10,
    /* RythmMode: TomTom */
    WOPL_RM_TomTom    = 0x18,
    /* RythmMode: Cymbell */
    WOPL_RM_Cymbal   = 0x20,
    /* RythmMode: HiHat */
    WOPL_RM_HiHat     = 0x28
} WOPL_RhythmMode;

/* DEPRECATED: It has typo. Don't use it! */
typedef WOPL_RhythmMode WOPL_RythmMode;

/* Error codes */
typedef enum WOPL_ErrorCodes
{
    WOPL_ERR_OK = 0,
    /* Magic number is not maching */
    WOPL_ERR_BAD_MAGIC,
    /* Too short file */
    WOPL_ERR_UNEXPECTED_ENDING,
    /* Zero banks count */
    WOPL_ERR_INVALID_BANKS_COUNT,
    /* Version of file is newer than supported by current version of library */
    WOPL_ERR_NEWER_VERSION,
    /* Out of memory */
    WOPL_ERR_OUT_OF_MEMORY,
    /* Given null pointer memory data */
    WOPL_ERR_NULL_POINTER
} WOPL_ErrorCodes;

/* Operator indeces inside of Instrument Entry */
#define WOPL_OP_CARRIER1    0
#define WOPL_OP_MODULATOR1  1
#define WOPL_OP_CARRIER2    2
#define WOPL_OP_MODULATOR2  3

/* OPL3 Oerators data  */
typedef struct WOPLOperator
{
    /* AM/Vib/Env/Ksr/FMult characteristics */
    uint8_t avekf_20;
    /* Key Scale Level / Total level register data */
    uint8_t ksl_l_40;
    /* Attack / Decay */
    uint8_t atdec_60;
    /* Systain and Release register data */
    uint8_t susrel_80;
    /* Wave form */
    uint8_t waveform_E0;
} WOPLOperator;

/* Instrument entry */
typedef struct WOPLInstrument
{
    /* Title of the instrument */
    char    inst_name[34];
    /* MIDI note key (half-tone) offset for an instrument (or a first voice in pseudo-4-op mode) */
    int16_t note_offset1;
    /* MIDI note key (half-tone) offset for a second voice in pseudo-4-op mode */
    int16_t note_offset2;
    /* MIDI note velocity offset (taken from Apogee TMB format) */
    int8_t  midi_velocity_offset;
    /* Second voice detune level (taken from DMX OP2) */
    int8_t  second_voice_detune;
    /* Percussion MIDI base tone number at which this drum will be played */
    uint8_t percussion_key_number;
    /* Enum WOPL_InstrumentFlags */
    uint8_t inst_flags;
    /* Feedback&Connection register for first and second operators */
    uint8_t fb_conn1_C0;
    /* Feedback&Connection register for third and fourth operators */
    uint8_t fb_conn2_C0;
    /* Operators register data */
    WOPLOperator operators[4];
    /* Millisecond delay of sounding while key is on */
    uint16_t delay_on_ms;
    /* Millisecond delay of sounding after key off */
    uint16_t delay_off_ms;
} WOPLInstrument;

/* Bank entry */
typedef struct WOPLBank
{
    /* Name of bank */
    char    bank_name[33];
    /* MIDI Bank LSB code */
    uint8_t bank_midi_lsb;
    /* MIDI Bank MSB code */
    uint8_t bank_midi_msb;
    /* Instruments data of this bank */
    WOPLInstrument ins[128];
} WOPLBank;

/* Instrument data file */
typedef struct WOPIFile
{
    /* Version of instrument file */
    uint16_t        version;
    /* Is this a percussion instrument */
    uint8_t         is_drum;
    /* Instrument data */
    WOPLInstrument  inst;
} WOPIFile;

/* Bank data file */
typedef struct WOPLFile
{
    /* Version of bank file */
    uint16_t version;
    /* Count of melodic banks in this file */
    uint16_t banks_count_melodic;
    /* Count of percussion banks in this file */
    uint16_t banks_count_percussion;
    /* Enum WOPLFileFlags */
    uint8_t  opl_flags;
    /* Enum WOPL_VolumeModel */
    uint8_t  volume_model;
    /* dynamically allocated data Melodic banks array */
    WOPLBank *banks_melodic;
    /* dynamically allocated data Percussive banks array */
    WOPLBank *banks_percussive;
} WOPLFile;


/**
 * @brief Initialize blank WOPL data structure with allocated bank data
 * @param melodic_banks Count of melodic banks
 * @param percussive_banks Count of percussive banks
 * @return pointer to heap-allocated WOPL data structure or NULL when out of memory or incorrectly given banks counts
 */
extern WOPLFile *WOPL_Init(uint16_t melodic_banks, uint16_t percussive_banks);

/**
 * @brief Clean up WOPL data file (all allocated bank arrays will be fried too)
 * @param file pointer to heap-allocated WOPL data structure
 */
extern void WOPL_Free(WOPLFile *file);

/**
 * @brief Compare two bank entries
 * @param bank1 First bank
 * @param bank2 Second bank
 * @return 1 if banks are equal or 0 if there are different
 */
extern int WOPL_BanksCmp(const WOPLFile *bank1, const WOPLFile *bank2);


/**
 * @brief Load WOPL bank file from the memory.
 * WOPL data structure will be allocated. (don't forget to clear it with WOPL_Free() after use!)
 * @param mem Pointer to memory block contains raw WOPL bank file data
 * @param length Length of given memory block
 * @param error pointer to integer to return an error code. Pass NULL if you don't want to use error codes.
 * @return Heap-allocated WOPL file data structure or NULL if any error has occouped
 */
extern WOPLFile *WOPL_LoadBankFromMem(void *mem, size_t length, int *error);

/**
 * @brief Load WOPI instrument file from the memory.
 * You must allocate WOPIFile structure by yourself and give the pointer to it.
 * @param file Pointer to destinition WOPIFile structure to fill it with parsed data.
 * @param mem Pointer to memory block contains raw WOPI instrument file data
 * @param length Length of given memory block
 * @return 0 if no errors occouped, or an error code of WOPL_ErrorCodes enumeration
 */
extern int WOPL_LoadInstFromMem(WOPIFile *file, void *mem, size_t length);

/**
 * @brief Calculate the size of the output memory block
 * @param file Heap-allocated WOPL file data structure
 * @param version Destinition version of the file
 * @return Size of the raw WOPL file data
 */
extern size_t WOPL_CalculateBankFileSize(WOPLFile *file, uint16_t version);

/**
 * @brief Calculate the size of the output memory block
 * @param file Pointer to WOPI file data structure
 * @param version Destinition version of the file
 * @return Size of the raw WOPI file data
 */
extern size_t WOPL_CalculateInstFileSize(WOPIFile *file, uint16_t version);

/**
 * @brief Write raw WOPL into given memory block
 * @param file Heap-allocated WOPL file data structure
 * @param dest_mem Destinition memory block pointer
 * @param length Length of destinition memory block
 * @param version Wanted WOPL version
 * @param force_gm Force GM set in saved bank file
 * @return Error code or 0 on success
 */
extern int WOPL_SaveBankToMem(WOPLFile *file, void *dest_mem, size_t length, uint16_t version, uint16_t force_gm);

/**
 * @brief Write raw WOPI into given memory block
 * @param file Pointer to WOPI file data structure
 * @param dest_mem Destinition memory block pointer
 * @param length Length of destinition memory block
 * @param version Wanted WOPI version
 * @return Error code or 0 on success
 */
extern int WOPL_SaveInstToMem(WOPIFile *file, void *dest_mem, size_t length, uint16_t version);

#ifdef __cplusplus
}
#endif

#endif /* WOPL_FILE_H */
