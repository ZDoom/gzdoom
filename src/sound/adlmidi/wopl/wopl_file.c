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

#include "wopl_file.h"
#include <string.h>
#include <stdlib.h>

static const char       *wopl3_magic = "WOPL3-BANK\0";
static const char       *wopli_magic = "WOPL3-INST\0";

static const uint16_t   wopl_latest_version = 3;

#define WOPL_INST_SIZE_V2 62
#define WOPL_INST_SIZE_V3 66

static uint16_t toUint16LE(const uint8_t *arr)
{
    uint16_t num = arr[0];
    num |= ((arr[1] << 8) & 0xFF00);
    return num;
}

static uint16_t toUint16BE(const uint8_t *arr)
{
    uint16_t num = arr[1];
    num |= ((arr[0] << 8) & 0xFF00);
    return num;
}

static int16_t toSint16BE(const uint8_t *arr)
{
    int16_t num = *(const int8_t *)(&arr[0]);
    num *= 1 << 8;
    num |= arr[1];
    return num;
}

static void fromUint16LE(uint16_t in, uint8_t *arr)
{
    arr[0] =  in & 0x00FF;
    arr[1] = (in >> 8) & 0x00FF;
}

static void fromUint16BE(uint16_t in, uint8_t *arr)
{
    arr[1] =  in & 0x00FF;
    arr[0] = (in >> 8) & 0x00FF;
}

static void fromSint16BE(int16_t in, uint8_t *arr)
{
    arr[1] =  in & 0x00FF;
    arr[0] = ((uint16_t)in >> 8) & 0x00FF;
}


WOPLFile *WOPL_Init(uint16_t melodic_banks, uint16_t percussive_banks)
{
    WOPLFile *file = NULL;
    if(melodic_banks == 0)
        return NULL;
    if(percussive_banks == 0)
        return NULL;
    file = (WOPLFile*)calloc(1, sizeof(WOPLFile));
    if(!file)
        return NULL;
    file->banks_count_melodic = melodic_banks;
    file->banks_count_percussion = percussive_banks;
    file->banks_melodic = (WOPLBank*)calloc(1, sizeof(WOPLBank) * melodic_banks );
    file->banks_percussive = (WOPLBank*)calloc(1, sizeof(WOPLBank) * percussive_banks );
    return file;
}

void WOPL_Free(WOPLFile *file)
{
    if(file)
    {
        if(file->banks_melodic)
            free(file->banks_melodic);
        if(file->banks_percussive)
            free(file->banks_percussive);
        free(file);
    }
}

int WOPL_BanksCmp(const WOPLFile *bank1, const WOPLFile *bank2)
{
    int res = 1;

    res &= (bank1->version == bank2->version);
    res &= (bank1->opl_flags == bank2->opl_flags);
    res &= (bank1->volume_model == bank2->volume_model);
    res &= (bank1->banks_count_melodic == bank2->banks_count_melodic);
    res &= (bank1->banks_count_percussion == bank2->banks_count_percussion);

    if(res)
    {
        int i;
        for(i = 0; i < bank1->banks_count_melodic; i++)
            res &= (memcmp(&bank1->banks_melodic[i], &bank2->banks_melodic[i], sizeof(WOPLBank)) == 0);
        if(res)
        {
            for(i = 0; i < bank1->banks_count_percussion; i++)
                res &= (memcmp(&bank1->banks_percussive[i], &bank2->banks_percussive[i], sizeof(WOPLBank)) == 0);
        }
    }

    return res;
}

static void WOPL_parseInstrument(WOPLInstrument *ins, uint8_t *cursor, uint16_t version, uint8_t has_sounding_delays)
{
    int l;
    strncpy(ins->inst_name, (const char*)cursor, 32);
    ins->inst_name[32] = '\0';
    ins->note_offset1 = toSint16BE(cursor + 32);
    ins->note_offset2 = toSint16BE(cursor + 34);
    ins->midi_velocity_offset = (int8_t)cursor[36];
    ins->second_voice_detune = (int8_t)cursor[37];
    ins->percussion_key_number = cursor[38];
    ins->inst_flags = cursor[39];
    ins->fb_conn1_C0 = cursor[40];
    ins->fb_conn2_C0 = cursor[41];
    for(l = 0; l < 4; l++)
    {
        size_t off = 42 + (size_t)(l) * 5;
        ins->operators[l].avekf_20      = cursor[off + 0];
        ins->operators[l].ksl_l_40      = cursor[off + 1];
        ins->operators[l].atdec_60      = cursor[off + 2];
        ins->operators[l].susrel_80     = cursor[off + 3];
        ins->operators[l].waveform_E0   = cursor[off + 4];
    }
    if((version >= 3) && has_sounding_delays)
    {
        ins->delay_on_ms  = toUint16BE(cursor + 62);
        ins->delay_off_ms = toUint16BE(cursor + 64);
    }
}

static void WOPL_writeInstrument(WOPLInstrument *ins, uint8_t *cursor, uint16_t version, uint8_t has_sounding_delays)
{
    int l;
    strncpy((char*)cursor, ins->inst_name, 32);
    fromSint16BE(ins->note_offset1, cursor + 32);
    fromSint16BE(ins->note_offset2, cursor + 34);
    cursor[36] = (uint8_t)ins->midi_velocity_offset;
    cursor[37] = (uint8_t)ins->second_voice_detune;
    cursor[38] = ins->percussion_key_number;
    cursor[39] = ins->inst_flags;
    cursor[40] = ins->fb_conn1_C0;
    cursor[41] = ins->fb_conn2_C0;
    for(l = 0; l < 4; l++)
    {
        size_t off = 42 + (size_t)(l) * 5;
        cursor[off + 0] = ins->operators[l].avekf_20;
        cursor[off + 1] = ins->operators[l].ksl_l_40;
        cursor[off + 2] = ins->operators[l].atdec_60;
        cursor[off + 3] = ins->operators[l].susrel_80;
        cursor[off + 4] = ins->operators[l].waveform_E0;
    }
    if((version >= 3) && has_sounding_delays)
    {
        fromUint16BE(ins->delay_on_ms, cursor + 62);
        fromUint16BE(ins->delay_off_ms, cursor + 64);
    }
}

WOPLFile *WOPL_LoadBankFromMem(void *mem, size_t length, int *error)
{
    WOPLFile *outFile = NULL;
    uint16_t i = 0, j = 0, k = 0;
    uint16_t version = 0;
    uint16_t count_melodic_banks     = 1;
    uint16_t count_percusive_banks   = 1;
    uint8_t *cursor = (uint8_t *)mem;

    WOPLBank *bankslots[2];
    uint16_t  bankslots_sizes[2];

#define SET_ERROR(err) \
{\
    WOPL_Free(outFile);\
    if(error)\
    {\
        *error = err;\
    }\
}

#define GO_FORWARD(bytes) { cursor += bytes; length -= bytes; }

    if(!cursor)
    {
        SET_ERROR(WOPL_ERR_NULL_POINTER);
        return NULL;
    }

    {/* Magic number */
        if(length < 11)
        {
            SET_ERROR(WOPL_ERR_UNEXPECTED_ENDING);
            return NULL;
        }
        if(memcmp(cursor, wopl3_magic, 11) != 0)
        {
            SET_ERROR(WOPL_ERR_BAD_MAGIC);
            return NULL;
        }
        GO_FORWARD(11);
    }

    {/* Version code */
        if(length < 2)
        {
            SET_ERROR(WOPL_ERR_UNEXPECTED_ENDING);
            return NULL;
        }
        version = toUint16LE(cursor);
        if(version  > wopl_latest_version)
        {
            SET_ERROR(WOPL_ERR_NEWER_VERSION);
            return NULL;
        }
        GO_FORWARD(2);
    }

    {/* Header of WOPL */
        uint8_t head[6];
        if(length < 6)
        {
            SET_ERROR(WOPL_ERR_UNEXPECTED_ENDING);
            return NULL;
        }
        memcpy(head, cursor, 6);
        count_melodic_banks = toUint16BE(head);
        count_percusive_banks = toUint16BE(head + 2);
        GO_FORWARD(6);

        outFile = WOPL_Init(count_melodic_banks, count_percusive_banks);
        if(!outFile)
        {
            SET_ERROR(WOPL_ERR_OUT_OF_MEMORY);
            return NULL;
        }

        outFile->version        = version;
        outFile->opl_flags      = head[4];
        outFile->volume_model   = head[5];
    }    

    bankslots_sizes[0] = count_melodic_banks;
    bankslots[0] = outFile->banks_melodic;
    bankslots_sizes[1] = count_percusive_banks;
    bankslots[1] = outFile->banks_percussive;

    if(version >= 2) /* Bank names and LSB/MSB titles */
    {
        for(i = 0; i < 2; i++)
        {
            for(j = 0; j < bankslots_sizes[i]; j++)
            {
                if(length < 34)
                {
                    SET_ERROR(WOPL_ERR_UNEXPECTED_ENDING);
                    return NULL;
                }
                strncpy(bankslots[i][j].bank_name, (const char*)cursor, 32);
                bankslots[i][j].bank_name[32] = '\0';
                bankslots[i][j].bank_midi_lsb = cursor[32];
                bankslots[i][j].bank_midi_msb = cursor[33];
                GO_FORWARD(34);
            }
        }
    }

    {/* Read instruments data */
        uint16_t insSize = 0;
        if(version > 2)
            insSize = WOPL_INST_SIZE_V3;
        else
            insSize = WOPL_INST_SIZE_V2;
        for(i = 0; i < 2; i++)
        {
            if(length < (insSize * 128) * (size_t)bankslots_sizes[i])
            {
                SET_ERROR(WOPL_ERR_UNEXPECTED_ENDING);
                return NULL;
            }

            for(j = 0; j < bankslots_sizes[i]; j++)
            {
                for(k = 0; k < 128; k++)
                {
                    WOPLInstrument *ins = &bankslots[i][j].ins[k];
                    WOPL_parseInstrument(ins, cursor, version, 1);
                    GO_FORWARD(insSize);
                }
            }
        }
    }

#undef GO_FORWARD
#undef SET_ERROR

    return outFile;
}

int WOPL_LoadInstFromMem(WOPIFile *file, void *mem, size_t length)
{
    uint16_t version = 0;
    uint8_t *cursor = (uint8_t *)mem;
    uint16_t ins_size;

    if(!cursor)
        return WOPL_ERR_NULL_POINTER;

#define GO_FORWARD(bytes) { cursor += bytes; length -= bytes; }

    {/* Magic number */
        if(length < 11)
            return WOPL_ERR_UNEXPECTED_ENDING;
        if(memcmp(cursor, wopli_magic, 11) != 0)
            return WOPL_ERR_BAD_MAGIC;
        GO_FORWARD(11);
    }

    {/* Version code */
        if(length < 2)
            return WOPL_ERR_UNEXPECTED_ENDING;
        version = toUint16LE(cursor);
        if(version  > wopl_latest_version)
            return WOPL_ERR_NEWER_VERSION;
        GO_FORWARD(2);
    }

    {/* is drum flag */
        if(length < 1)
            return WOPL_ERR_UNEXPECTED_ENDING;
        file->is_drum = *cursor;
        GO_FORWARD(1);
    }

    if(version > 2)
        /* Skip sounding delays are not part of single-instrument file
         * two sizes of uint16_t will be subtracted */
        ins_size = WOPL_INST_SIZE_V3 - (sizeof(uint16_t) * 2);
    else
        ins_size = WOPL_INST_SIZE_V2;

    if(length < ins_size)
        return WOPL_ERR_UNEXPECTED_ENDING;

    WOPL_parseInstrument(&file->inst, cursor, version, 0);
    GO_FORWARD(ins_size);

    return WOPL_ERR_OK;
#undef GO_FORWARD
}

size_t WOPL_CalculateBankFileSize(WOPLFile *file, uint16_t version)
{
    size_t final_size = 0;
    size_t ins_size = 0;

    if(version == 0)
        version = wopl_latest_version;

    if(!file)
        return 0;
    final_size += 11 + 2 + 2 + 2 + 1 + 1;
    /*
     * Magic number,
     * Version,
     * Count of melodic banks,
     * Count of percussive banks,
     * Chip specific flags
     * Volume Model
     */

    if(version >= 2)
    {
        /* Melodic banks meta-data */
        final_size += (32 + 1 + 1) * file->banks_count_melodic;
        /* Percussive banks meta-data */
        final_size += (32 + 1 + 1) * file->banks_count_percussion;
    }

    if(version >= 3)
        ins_size = WOPL_INST_SIZE_V3;
    else
        ins_size = WOPL_INST_SIZE_V2;
    /* Melodic instruments */
    final_size += (ins_size * 128) * file->banks_count_melodic;
    /* Percusive instruments */
    final_size += (ins_size * 128) * file->banks_count_percussion;

    return final_size;
}

size_t WOPL_CalculateInstFileSize(WOPIFile *file, uint16_t version)
{
    size_t final_size = 0;
    size_t ins_size = 0;

    if(version == 0)
        version = wopl_latest_version;

    if(!file)
        return 0;
    final_size += 11 + 2 + 1;
    /*
     * Magic number,
     * version,
     * is percussive instrument
     */

    if(version >= 3)
        ins_size = WOPL_INST_SIZE_V3;
    else
        ins_size = WOPL_INST_SIZE_V2;
    final_size += ins_size * 128;

    return final_size;
}

int WOPL_SaveBankToMem(WOPLFile *file, void *dest_mem, size_t length, uint16_t version, uint16_t force_gm)
{
    uint8_t *cursor = (uint8_t *)dest_mem;
    uint16_t ins_size = 0;
    uint16_t i, j, k;
    uint16_t banks_melodic = force_gm ? 1 : file->banks_count_melodic;
    uint16_t banks_percusive = force_gm ? 1 : file->banks_count_percussion;

    WOPLBank *bankslots[2];
    uint16_t  bankslots_sizes[2];

    if(version == 0)
        version = wopl_latest_version;

#define GO_FORWARD(bytes) { cursor += bytes; length -= bytes; }

    if(length < 11)
        return WOPL_ERR_UNEXPECTED_ENDING;
    memcpy(cursor, wopl3_magic, 11);
    GO_FORWARD(11);

    if(length < 2)
        return WOPL_ERR_UNEXPECTED_ENDING;
    fromUint16LE(version, cursor);
    GO_FORWARD(2);

    if(length < 2)
        return WOPL_ERR_UNEXPECTED_ENDING;
    fromUint16BE(banks_melodic, cursor);
    GO_FORWARD(2);

    if(length < 2)
        return WOPL_ERR_UNEXPECTED_ENDING;
    fromUint16BE(banks_percusive, cursor);
    GO_FORWARD(2);

    if(length < 2)
        return WOPL_ERR_UNEXPECTED_ENDING;
    cursor[0] = file->opl_flags;
    cursor[1] = file->volume_model;
    GO_FORWARD(2);

    bankslots[0]        = file->banks_melodic;
    bankslots_sizes[0]  = banks_melodic;
    bankslots[1]        = file->banks_percussive;
    bankslots_sizes[1]  = banks_percusive;

    if(version >= 2)
    {
        for(i = 0; i < 2; i++)
        {
            for(j = 0; j < bankslots_sizes[i]; j++)
            {
                if(length < 34)
                    return WOPL_ERR_UNEXPECTED_ENDING;
                strncpy((char*)cursor, bankslots[i][j].bank_name, 32);
                cursor[32] = bankslots[i][j].bank_midi_lsb;
                cursor[33] = bankslots[i][j].bank_midi_msb;
                GO_FORWARD(34);
            }
        }
    }

    {/* Write instruments data */
        if(version >= 3)
            ins_size = WOPL_INST_SIZE_V3;
        else
            ins_size = WOPL_INST_SIZE_V2;
        for(i = 0; i < 2; i++)
        {
            if(length < (ins_size * 128) * (size_t)bankslots_sizes[i])
                return WOPL_ERR_UNEXPECTED_ENDING;

            for(j = 0; j < bankslots_sizes[i]; j++)
            {
                for(k = 0; k < 128; k++)
                {
                    WOPLInstrument *ins = &bankslots[i][j].ins[k];
                    WOPL_writeInstrument(ins, cursor, version, 1);
                    GO_FORWARD(ins_size);
                }
            }
        }
    }

    return WOPL_ERR_OK;
#undef GO_FORWARD
}

int WOPL_SaveInstToMem(WOPIFile *file, void *dest_mem, size_t length, uint16_t version)
{
    uint8_t *cursor = (uint8_t *)dest_mem;
    uint16_t ins_size;

    if(!cursor)
        return WOPL_ERR_NULL_POINTER;

    if(version == 0)
        version = wopl_latest_version;

#define GO_FORWARD(bytes) { cursor += bytes; length -= bytes; }

    {/* Magic number */
        if(length < 11)
            return WOPL_ERR_UNEXPECTED_ENDING;
        memcpy(cursor, wopli_magic, 11);
        GO_FORWARD(11);
    }

    {/* Version code */
        if(length < 2)
            return WOPL_ERR_UNEXPECTED_ENDING;
        fromUint16LE(version, cursor);
        GO_FORWARD(2);
    }

    {/* is drum flag */
        if(length < 1)
            return WOPL_ERR_UNEXPECTED_ENDING;
        *cursor = file->is_drum;
        GO_FORWARD(1);
    }

    if(version > 2)
        /* Skip sounding delays are not part of single-instrument file
         * two sizes of uint16_t will be subtracted */
        ins_size = WOPL_INST_SIZE_V3 - (sizeof(uint16_t) * 2);
    else
        ins_size = WOPL_INST_SIZE_V2;

    if(length < ins_size)
        return WOPL_ERR_UNEXPECTED_ENDING;

    WOPL_writeInstrument(&file->inst, cursor, version, 0);
    GO_FORWARD(ins_size);

    return WOPL_ERR_OK;
#undef GO_FORWARD
}
