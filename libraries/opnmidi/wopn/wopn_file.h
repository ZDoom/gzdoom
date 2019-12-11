/*
 * Wohlstand's OPN2 Bank File - a bank format to store OPN2 timbre data and setup
 *
 * Copyright (c) 2018 Vitaly Novichkov <admin@wohlnet.ru>
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

#ifndef WOPN_FILE_H
#define WOPN_FILE_H

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

/* Volume scaling model implemented in the libOPNMIDI */
typedef enum WOPN_VolumeModel
{
    WOPN_VM_Generic = 0
} WOPN_VolumeModel;

typedef enum WOPN_InstrumentFlags
{
    /* Is pseudo eight-operator (two 4-operator voices) instrument */
    WOPN_Ins_Pseudo8op  = 0x01,
    /* Is a blank instrument entry */
    WOPN_Ins_IsBlank    = 0x02,

    /* Mask of the flags range */
    WOPN_Ins_ALL_MASK   = 0x03
} WOPN_InstrumentFlags;

/* Error codes */
typedef enum WOPN_ErrorCodes
{
    WOPN_ERR_OK = 0,
    /* Magic number is not maching */
    WOPN_ERR_BAD_MAGIC,
    /* Too short file */
    WOPN_ERR_UNEXPECTED_ENDING,
    /* Zero banks count */
    WOPN_ERR_INVALID_BANKS_COUNT,
    /* Version of file is newer than supported by current version of library */
    WOPN_ERR_NEWER_VERSION,
    /* Out of memory */
    WOPN_ERR_OUT_OF_MEMORY,
    /* Given null pointer memory data */
    WOPN_ERR_NULL_POINTER
} WOPN_ErrorCodes;

/* OPN2 Oerators data  */
typedef struct WOPNOperator
{
    /* Detune and frequency multiplication register data */
    uint8_t dtfm_30;
    /* Total level register data */
    uint8_t level_40;
    /* Rate scale and attack register data */
    uint8_t rsatk_50;
    /* Amplitude modulation enable and Decay-1 register data */
    uint8_t amdecay1_60;
    /* Decay-2 register data */
    uint8_t decay2_70;
    /* Sustain and Release register data */
    uint8_t susrel_80;
    /* SSG-EG register data */
    uint8_t ssgeg_90;
} WOPNOperator;

/* Instrument entry */
typedef struct WOPNInstrument
{
    /* Title of the instrument */
    char    inst_name[34];
    /* MIDI note key (half-tone) offset for an instrument (or a first voice in pseudo-4-op mode) */
    int16_t note_offset;
    /* Reserved */
    int8_t  midi_velocity_offset;
    /* Percussion MIDI base tone number at which this drum will be played */
    uint8_t percussion_key_number;
    /* Enum WOPN_InstrumentFlags */
    uint8_t inst_flags;
    /* Feedback and Algorithm register data */
    uint8_t fbalg;
    /* LFO Sensitivity register data */
    uint8_t lfosens;
    /* Operators register data */
    WOPNOperator operators[4];
    /* Millisecond delay of sounding while key is on */
    uint16_t delay_on_ms;
    /* Millisecond delay of sounding after key off */
    uint16_t delay_off_ms;
} WOPNInstrument;

/* Bank entry */
typedef struct WOPNBank
{
    /* Name of bank */
    char    bank_name[33];
    /* MIDI Bank LSB code */
    uint8_t bank_midi_lsb;
    /* MIDI Bank MSB code */
    uint8_t bank_midi_msb;
    /* Instruments data of this bank */
    WOPNInstrument ins[128];
} WOPNBank;

/* Instrument data file */
typedef struct OPNIFile
{
    /* Version of instrument file */
    uint16_t        version;
    /* Is this a percussion instrument */
    uint8_t         is_drum;
    /* Instrument data */
    WOPNInstrument  inst;
} OPNIFile;

/* Bank data file */
typedef struct WOPNFile
{
    /* Version of bank file */
    uint16_t version;
    /* Count of melodic banks in this file */
    uint16_t banks_count_melodic;
    /* Count of percussion banks in this file */
    uint16_t banks_count_percussion;
    /* Chip global LFO enable flag and frequency register data */
    uint8_t lfo_freq;
    /* Reserved (Enum WOPN_VolumeModel) */
    uint8_t volume_model;
    /* dynamically allocated data Melodic banks array */
    WOPNBank *banks_melodic;
    /* dynamically allocated data Percussive banks array */
    WOPNBank *banks_percussive;
} WOPNFile;


/**
 * @brief Initialize blank WOPN data structure with allocated bank data
 * @param melodic_banks Count of melodic banks
 * @param percussive_banks Count of percussive banks
 * @return pointer to heap-allocated WOPN data structure or NULL when out of memory or incorrectly given banks counts
 */
extern WOPNFile *WOPN_Init(uint16_t melodic_banks, uint16_t percussive_banks);

/**
 * @brief Clean up WOPN data file (all allocated bank arrays will be fried too)
 * @param file pointer to heap-allocated WOPN data structure
 */
extern void WOPN_Free(WOPNFile *file);

/**
 * @brief Compare two bank entries
 * @param bank1 First bank
 * @param bank2 Second bank
 * @return 1 if banks are equal or 0 if there are different
 */
extern int WOPN_BanksCmp(const WOPNFile *bank1, const WOPNFile *bank2);


/**
 * @brief Load WOPN bank file from the memory.
 * WOPN data structure will be allocated. (don't forget to clear it with WOPN_Free() after use!)
 * @param mem Pointer to memory block contains raw WOPN bank file data
 * @param length Length of given memory block
 * @param error pointer to integer to return an error code. Pass NULL if you don't want to use error codes.
 * @return Heap-allocated WOPN file data structure or NULL if any error has occouped
 */
extern WOPNFile *WOPN_LoadBankFromMem(void *mem, size_t length, int *error);

/**
 * @brief Load WOPI instrument file from the memory.
 * You must allocate OPNIFile structure by yourself and give the pointer to it.
 * @param file Pointer to destinition OPNIFile structure to fill it with parsed data.
 * @param mem Pointer to memory block contains raw WOPI instrument file data
 * @param length Length of given memory block
 * @return 0 if no errors occouped, or an error code of WOPN_ErrorCodes enumeration
 */
extern int WOPN_LoadInstFromMem(OPNIFile *file, void *mem, size_t length);

/**
 * @brief Calculate the size of the output memory block
 * @param file Heap-allocated WOPN file data structure
 * @param version Destinition version of the file
 * @return Size of the raw WOPN file data
 */
extern size_t WOPN_CalculateBankFileSize(WOPNFile *file, uint16_t version);

/**
 * @brief Calculate the size of the output memory block
 * @param file Pointer to WOPI file data structure
 * @param version Destinition version of the file
 * @return Size of the raw WOPI file data
 */
extern size_t WOPN_CalculateInstFileSize(OPNIFile *file, uint16_t version);

/**
 * @brief Write raw WOPN into given memory block
 * @param file Heap-allocated WOPN file data structure
 * @param dest_mem Destinition memory block pointer
 * @param length Length of destinition memory block
 * @param version Wanted WOPN version
 * @param force_gm Force GM set in saved bank file
 * @return Error code or 0 on success
 */
extern int WOPN_SaveBankToMem(WOPNFile *file, void *dest_mem, size_t length, uint16_t version, uint16_t force_gm);

/**
 * @brief Write raw WOPI into given memory block
 * @param file Pointer to WOPI file data structure
 * @param dest_mem Destinition memory block pointer
 * @param length Length of destinition memory block
 * @param version Wanted WOPI version
 * @return Error code or 0 on success
 */
extern int WOPN_SaveInstToMem(OPNIFile *file, void *dest_mem, size_t length, uint16_t version);

#ifdef __cplusplus
}
#endif

#endif /* WOPN_FILE_H */
